"""로그인 세션 토큰(JWT HS256). 외부 라이브러리 없이 표준 라이브러리로 서명하고 검증한다."""

import base64
import hashlib
import hmac
import json
import time

from .errors import ApiError

TOKEN_TTL_SECONDS = 12 * 60 * 60


def _b64url_encode(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _b64url_decode(text):
    padding = "=" * (-len(text) % 4)
    return base64.urlsafe_b64decode(text + padding)


def _sign(secret, signing_input):
    return hmac.new(secret, signing_input.encode("ascii"), hashlib.sha256).digest()


def issue_token(secret, account_id, display_name, issuer, now=None, ttl=TOKEN_TTL_SECONDS):
    """sub는 로그인 ID("steam:<SteamID64>", "dev:<이름>")다. GameLift 플레이어 세션과 전용 서버 보고도 같은 ID를 쓴다."""
    issued_at = int(time.time() if now is None else now)
    header = {"alg": "HS256", "typ": "JWT"}
    claims = {"iss": issuer, "sub": account_id, "name": display_name, "iat": issued_at, "exp": issued_at + ttl}
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
