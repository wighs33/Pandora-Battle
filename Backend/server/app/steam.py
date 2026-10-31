"""Steam 로그인 티켓 검증. 클라이언트가 GetAuthTicketForWebApi로 받은 티켓을 Steam Web API로 확인한다."""

import json
import logging
import urllib.error
import urllib.parse
import urllib.request

from .errors import ApiError

STEAM_AUTHENTICATE_URL = "https://partner.steam-api.com/ISteamUserAuth/AuthenticateUserTicket/v1/"

logger = logging.getLogger(__name__)


def verify_steam_ticket(ticket_hex, api_key, app_id, identity, opener=urllib.request.urlopen):
    """티켓을 검증하고 SteamID64 문자열을 돌려준다. 티켓 형식(16진수)은 요청 모델이 이미 확인했다."""
    query = urllib.parse.urlencode({"key": api_key, "appid": app_id, "ticket": ticket_hex, "identity": identity})
    # 요청 URL에는 API 키가 들어 있으므로 로그에는 상태 코드와 응답 본문만 남긴다.
    try:
        with opener(STEAM_AUTHENTICATE_URL + "?" + query, timeout=10) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        detail = error.read(300).decode("utf-8", "replace")
        logger.warning("Steam AuthenticateUserTicket HTTP %d: %s", error.code, detail)
        if error.code in (401, 403):
            raise ApiError(502, "steam_key_rejected", "Steam rejected the Web API key.") from error
        raise ApiError(502, "steam_unavailable", "Steam ticket verification failed.") from error
    except (urllib.error.URLError, TimeoutError, ValueError) as error:
        logger.warning("Steam AuthenticateUserTicket failed: %s", type(error).__name__)
        raise ApiError(502, "steam_unavailable", "Steam ticket verification failed.") from error

    params = (payload.get("response") or {}).get("params")
    if not params or params.get("result") != "OK" or not params.get("steamid"):
        raise ApiError(401, "steam_ticket_rejected", "Steam rejected the ticket.")
    if params.get("publisherbanned"):
        raise ApiError(403, "player_banned", "This account is banned.")
    return str(params["steamid"])
