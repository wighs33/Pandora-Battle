"""POST /auth/steam, POST /auth/dev: 플레이어를 확인하고 백엔드 세션 토큰을 발급한다.

Steam 로그인은 클라이언트가 GetAuthTicketForWebApi로 받은 티켓을 Steam Web API로 검증한다.
개발용 로그인은 한 PC에서 클라이언트 여러 개를 테스트할 때만 쓰며, ALLOW_DEV_LOGIN=true인 스테이지에서만 열린다.
"""

import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request

from common import (
    ApiError,
    ParameterMissing,
    api_handler,
    aws_client,
    get_secure_parameter,
    issue_token,
    json_response,
    parse_json_body,
    player_pk,
    PROFILE_SK,
    sanitize_display_name,
    token_issuer,
    token_secret,
    utc_now_iso,
    ddb_s,
)

STEAM_AUTHENTICATE_URL = "https://partner.steam-api.com/ISteamUserAuth/AuthenticateUserTicket/v1/"
TICKET_PATTERN = re.compile(r"^[0-9A-Fa-f]{16,4096}$")
DEV_ID_PATTERN = re.compile(r"^[A-Za-z0-9_-]{1,32}$")


def verify_steam_ticket(ticket_hex, api_key, app_id, identity, opener=urllib.request.urlopen):
    """Steam Web API로 티켓을 검증하고 SteamID64 문자열을 돌려준다."""
    if not isinstance(ticket_hex, str) or not TICKET_PATTERN.match(ticket_hex):
        raise ApiError(400, "invalid_ticket", "Steam ticket must be a hex string.")

    query = urllib.parse.urlencode(
        {"key": api_key, "appid": app_id, "ticket": ticket_hex, "identity": identity}
    )
    # 요청 URL에는 API 키가 들어 있으므로 로그에는 상태 코드와 응답 본문만 남긴다.
    try:
        with opener(STEAM_AUTHENTICATE_URL + "?" + query, timeout=10) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        detail = error.read(300).decode("utf-8", "replace")
        print("Steam AuthenticateUserTicket HTTP %d: %s" % (error.code, detail))
        if error.code in (401, 403):
            raise ApiError(502, "steam_key_rejected", "Steam rejected the Web API key.") from error
        raise ApiError(502, "steam_unavailable", "Steam ticket verification failed.") from error
    except (urllib.error.URLError, TimeoutError, ValueError) as error:
        print("Steam AuthenticateUserTicket failed: %s" % type(error).__name__)
        raise ApiError(502, "steam_unavailable", "Steam ticket verification failed.") from error

    params = (payload.get("response") or {}).get("params")
    if not params or params.get("result") != "OK" or not params.get("steamid"):
        raise ApiError(401, "steam_ticket_rejected", "Steam rejected the ticket.")
    if params.get("publisherbanned"):
        raise ApiError(403, "player_banned", "This account is banned.")
    return str(params["steamid"])


def upsert_profile(dynamodb, table_name, player_id, display_name, provider, now_iso):
    """로그인할 때마다 표시 이름과 로그인 시각을 갱신하고, 처음이면 생성 시각을 남긴다."""
    dynamodb.update_item(
        TableName=table_name,
        Key={"PK": ddb_s(player_pk(player_id)), "SK": ddb_s(PROFILE_SK)},
        UpdateExpression=(
            "SET DisplayName = :name, LoginProvider = :provider, LastLoginAt = :now, "
            "CreatedAt = if_not_exists(CreatedAt, :now), EntityType = :type"
        ),
        ExpressionAttributeValues={
            ":name": ddb_s(display_name),
            ":provider": ddb_s(provider),
            ":now": ddb_s(now_iso),
            ":type": ddb_s("Player"),
        },
    )


def _login_response(player_id, display_name):
    token = issue_token(token_secret(), player_id, display_name, token_issuer())
    return json_response(
        200,
        {"token": token, "playerId": player_id, "displayName": display_name},
    )


def _dynamodb():
    return aws_client("dynamodb")


def steam_login(event):
    body = parse_json_body(event)
    try:
        api_key = get_secure_parameter(os.environ["STEAM_KEY_PARAM"])
    except ParameterMissing as error:
        raise ApiError(503, "steam_login_not_configured", "Steam Web API key is not configured.") from error

    steam_id = verify_steam_ticket(
        body.get("ticket"),
        api_key=api_key,
        app_id=os.environ["STEAM_APP_ID"],
        identity=os.environ["STEAM_TICKET_IDENTITY"],
    )
    player_id = "steam:" + steam_id
    display_name = sanitize_display_name(body.get("displayName"))
    upsert_profile(_dynamodb(), os.environ["TABLE_NAME"], player_id, display_name, "steam", utc_now_iso())
    return _login_response(player_id, display_name)


def dev_login(event):
    if os.environ.get("ALLOW_DEV_LOGIN") != "true":
        raise ApiError(404, "not_found", "Not found.")

    body = parse_json_body(event)
    dev_id = body.get("devId")
    if not isinstance(dev_id, str) or not DEV_ID_PATTERN.match(dev_id):
        raise ApiError(400, "invalid_dev_id", "devId must be 1-32 characters of letters, digits, '_' or '-'.")
    player_id = "dev:" + dev_id
    display_name = sanitize_display_name(body.get("displayName"), fallback=dev_id)
    upsert_profile(_dynamodb(), os.environ["TABLE_NAME"], player_id, display_name, "dev", utc_now_iso())
    return _login_response(player_id, display_name)


@api_handler
def handler(event, context):
    route = event.get("routeKey")
    if route == "POST /auth/steam":
        return steam_login(event)
    if route == "POST /auth/dev":
        return dev_login(event)
    raise ApiError(404, "not_found", "Not found.")
