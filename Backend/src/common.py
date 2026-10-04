"""Lambda 공통 코드: 응답 형식, 요청 파싱, 세션 토큰, SSM 비밀 값, DynamoDB 키.

외부 라이브러리 없이 Lambda 기본 런타임(boto3 포함)만 사용한다. boto3는 실제로 AWS를 호출할 때 처음 가져오므로
로컬 단위 테스트는 boto3 없이 실행할 수 있고, 클라이언트는 실행 환경마다 한 번 만들어 요청 사이에 다시 쓴다.
"""

import base64
import hashlib
import hmac
import json
import os
import re
import time

TOKEN_TTL_SECONDS = 12 * 60 * 60
DISPLAY_NAME_MAX_LENGTH = 32
PARAMETER_CACHE_SECONDS = 5 * 60

PROFILE_SK = "PROFILE"
MATCH_RESULT_SK = "RESULT"

_parameter_cache = {}


class ApiError(Exception):
    """클라이언트에 그대로 돌려줄 오류. code는 클라이언트가 분기할 수 있는 고정 문자열이다."""

    def __init__(self, status, code, message):
        super().__init__(message)
        self.status = status
        self.code = code
        self.message = message


# 응답 ----------------------------------------------------------------------------------------------------------------

def json_response(status, body):
    return {
        "statusCode": status,
        "headers": {"Content-Type": "application/json"},
        "body": json.dumps(body, ensure_ascii=False),
    }


def api_handler(func):
    """ApiError는 정해진 오류 응답으로, 예상하지 못한 예외는 로그를 남기고 500으로 바꾼다."""

    def wrapper(event, context):
        try:
            return func(event, context)
        except ApiError as error:
            return json_response(error.status, {"error": error.code, "message": error.message})
        except Exception:  # noqa: BLE001 - Lambda 경계에서 모든 예외를 응답으로 바꾼다.
            import traceback

            traceback.print_exc()
            return json_response(500, {"error": "internal_error", "message": "Internal server error."})

    wrapper.__name__ = func.__name__
    return wrapper


# 요청 ----------------------------------------------------------------------------------------------------------------

def parse_json_body(event):
    body = event.get("body") or ""
    if event.get("isBase64Encoded"):
        body = base64.b64decode(body).decode("utf-8")
    if not body:
        return {}
    try:
        parsed = json.loads(body)
    except ValueError as error:
        raise ApiError(400, "invalid_json", "Request body is not valid JSON.") from error
    if not isinstance(parsed, dict):
        raise ApiError(400, "invalid_json", "Request body must be a JSON object.")
    return parsed


def sanitize_display_name(value, fallback="Player"):
    """표시 이름은 화면 출력에만 쓰므로 제어 문자를 지우고 길이만 제한한다."""
    if not isinstance(value, str):
        return fallback
    cleaned = "".join(ch for ch in value if ch.isprintable()).strip()
    return cleaned[:DISPLAY_NAME_MAX_LENGTH] or fallback


def utc_now_iso(now=None):
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(time.time() if now is None else now))


# 비밀 값 --------------------------------------------------------------------------------------------------------------

class ParameterMissing(Exception):
    pass


_aws_clients = {}


def aws_client(service):
    """서비스별 boto3 클라이언트를 실행 환경마다 한 번만 만든다. 따뜻한 호출은 만들어 둔 클라이언트와 연결을 그대로 쓴다."""
    client = _aws_clients.get(service)
    if client is None:
        import boto3

        client = _aws_clients[service] = boto3.client(service)
    return client


def get_secure_parameter(name):
    """SSM SecureString 값을 읽는다. 키를 바꾸면 재배포 없이 반영되도록 5분만 캐시한다."""
    cached = _parameter_cache.get(name)
    if cached and time.monotonic() - cached[1] < PARAMETER_CACHE_SECONDS:
        return cached[0]

    client = aws_client("ssm")
    try:
        value = client.get_parameter(Name=name, WithDecryption=True)["Parameter"]["Value"].strip()
    except client.exceptions.ParameterNotFound as error:
        raise ParameterMissing(name) from error
    _parameter_cache[name] = (value, time.monotonic())
    return value


def token_secret():
    return get_secure_parameter(os.environ["TOKEN_SECRET_PARAM"]).encode("utf-8")


# 세션 토큰 (JWT HS256) -------------------------------------------------------------------------------------------------

def _b64url_encode(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _b64url_decode(text):
    padding = "=" * (-len(text) % 4)
    return base64.urlsafe_b64decode(text + padding)


def _sign(secret, signing_input):
    return hmac.new(secret, signing_input.encode("ascii"), hashlib.sha256).digest()


def issue_token(secret, player_id, display_name, issuer, now=None, ttl=TOKEN_TTL_SECONDS):
    issued_at = int(time.time() if now is None else now)
    header = {"alg": "HS256", "typ": "JWT"}
    claims = {
        "iss": issuer,
        "sub": player_id,
        "name": display_name,
        "iat": issued_at,
        "exp": issued_at + ttl,
    }
    signing_input = ".".join(
        _b64url_encode(json.dumps(part, separators=(",", ":")).encode("utf-8")) for part in (header, claims)
    )
    return signing_input + "." + _b64url_encode(_sign(secret, signing_input))


def verify_token(secret, token, issuer, now=None):
    """서명·발급자·만료를 확인하고 claims를 돌려준다. 실패하면 401 ApiError를 던진다."""
    unauthorized = ApiError(401, "invalid_token", "Session token is invalid or expired.")
    parts = token.split(".") if isinstance(token, str) else []
    if len(parts) != 3:
        raise unauthorized
    signing_input = parts[0] + "." + parts[1]
    try:
        header = json.loads(_b64url_decode(parts[0]))
        claims = json.loads(_b64url_decode(parts[1]))
        signature = _b64url_decode(parts[2])
    except (ValueError, UnicodeDecodeError) as error:
        raise unauthorized from error
    if header.get("alg") != "HS256" or not hmac.compare_digest(signature, _sign(secret, signing_input)):
        raise unauthorized
    current = int(time.time() if now is None else now)
    if claims.get("iss") != issuer or not isinstance(claims.get("exp"), int) or claims["exp"] <= current:
        raise unauthorized
    if not isinstance(claims.get("sub"), str) or not claims["sub"]:
        raise unauthorized
    return claims


def token_issuer():
    return "labproject-" + os.environ.get("STAGE", "dev")


def require_player(event, secret=None):
    """Authorization: Bearer 토큰을 검증하고 플레이어 claims를 돌려준다."""
    headers = event.get("headers") or {}
    authorization = headers.get("authorization") or headers.get("Authorization") or ""
    match = re.match(r"^Bearer\s+(\S+)$", authorization)
    if not match:
        raise ApiError(401, "missing_token", "Authorization header with a Bearer token is required.")
    return verify_token(secret if secret is not None else token_secret(), match.group(1), token_issuer())


# DynamoDB 키 ---------------------------------------------------------------------------------------------------------
# 한 테이블에 플레이어 프로필, 플레이어별 경기 기록, 경기 결과를 함께 둔다.
#   PK=PLAYER#<id>  SK=PROFILE                     프로필과 누적 전적
#   PK=PLAYER#<id>  SK=MATCH#<기록 시각>#<경기 ID>   플레이어별 경기 기록(최신순 조회)
#   PK=MATCH#<id>   SK=RESULT                      경기 전체 결과(중복 보고 방지)

def player_pk(player_id):
    return "PLAYER#" + player_id


def player_match_sk(recorded_at, match_id):
    return "MATCH#" + recorded_at + "#" + match_id


def match_pk(match_id):
    return "MATCH#" + match_id


def ddb_s(value):
    return {"S": value}


def ddb_n(value):
    return {"N": str(int(value))}
