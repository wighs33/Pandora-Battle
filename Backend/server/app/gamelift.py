"""GameLift 게임 세션 자리 잡기.

빈 자리가 있는 세션을 먼저 찾고, 없으면 새 세션을 만든 뒤 ACTIVE가 될 때까지 기다린다.
같은 모드로 아직 활성화 중인 세션이 있으면 새로 만들지 않고 그 세션을 기다린다.
시간 안에 활성화되지 않으면 session_timeout을 돌려주고, 클라이언트가 같은 요청을 다시 보내 그 세션을 이어서 기다린다.
클라이언트는 받은 주소로 접속하면서 PlayerSessionId를 보내고, 게임 서버가 AcceptPlayerSession으로 검증한다.

mode는 게임 속성 mode에 넣어 검색과 서버 분기에 쓴다.
- match: 로비에서 인원을 모아 한 경기를 하는 PvP 세션
- bossraid: 들어오고 나가는 보스 레이드 세션. 서버는 레이드 맵을 연 뒤에 세션을 활성화한다.
"""

import json
import time

from .errors import ApiError

# 클라이언트 요청 하나가 기다리는 시간. 보스 레이드 세션은 맵을 연 뒤 활성화하므로 경기 세션보다 오래 걸린다.
ACTIVE_WAIT_SECONDS = 24.0
POLL_INTERVAL_SECONDS = 1.0

MODE_PROPERTY = "mode"
MATCH_MODE = "match"
BOSS_RAID_MODE = "bossraid"


def session_mode(session):
    for prop in session.get("GameProperties", []):
        if prop.get("Key") == MODE_PROPERTY:
            return prop.get("Value", "")
    return MATCH_MODE


def search_joinable_sessions(gamelift, fleet_id, location, mode):
    request = {
        "FleetId": fleet_id,
        # 문자열은 작은따옴표로 감싼다(큰따옴표는 InvalidRequestException).
        "FilterExpression": "hasAvailablePlayerSessions=true AND gameSessionProperties.%s = '%s'" % (MODE_PROPERTY, mode),
        "SortExpression": "creationTimeMillis ASC",
        "Limit": 10,
    }
    if location:
        request["Location"] = location
    sessions = gamelift.search_game_sessions(**request).get("GameSessions", [])
    if sessions:
        return sessions

    # 검색 색인은 새 세션을 몇 초 늦게 반영한다. 방금 만든 세션을 놓치지 않도록 즉시 반영되는 조회로 한 번 더 찾는다.
    describe = {"FleetId": fleet_id, "StatusFilter": "ACTIVE", "Limit": 20}
    if location:
        describe["Location"] = location
    return [
        session
        for session in gamelift.describe_game_sessions(**describe).get("GameSessions", [])
        if session_mode(session) == mode
        and session.get("PlayerSessionCreationPolicy", "ACCEPT_ALL") == "ACCEPT_ALL"
        and session.get("CurrentPlayerSessionCount", 0) < session.get("MaximumPlayerSessionCount", 0)
    ]


def find_activating_session(gamelift, fleet_id, location, mode):
    """같은 모드로 활성화 중인 세션. 앞 요청이 만들었지만 기다리는 시간 안에 활성화되지 못한 세션을 이어서 기다릴 때 쓴다."""
    describe = {"FleetId": fleet_id, "StatusFilter": "ACTIVATING", "Limit": 20}
    if location:
        describe["Location"] = location
    for session in gamelift.describe_game_sessions(**describe).get("GameSessions", []):
        if session_mode(session) == mode:
            return session
    return None


def try_create_player_session(gamelift, game_session_id, account_id, player_data):
    """자리가 없거나 세션이 더 이상 참가를 받지 않으면 None을 돌려준다."""
    try:
        return gamelift.create_player_session(
            GameSessionId=game_session_id,
            PlayerId=account_id,
            PlayerData=player_data,
        )["PlayerSession"]
    except (
        gamelift.exceptions.GameSessionFullException,
        gamelift.exceptions.InvalidGameSessionStatusException,
    ):
        return None


def wait_until_active(gamelift, game_session_id, sleep, clock, timeout):
    deadline = clock() + timeout
    while True:
        sessions = gamelift.describe_game_sessions(GameSessionId=game_session_id).get("GameSessions", [])
        status = sessions[0]["Status"] if sessions else "MISSING"
        if status == "ACTIVE":
            return
        if status != "ACTIVATING":
            raise ApiError(503, "session_failed", "Game session could not be started (%s)." % status)
        if clock() >= deadline:
            raise ApiError(504, "session_timeout", "Game session did not become active in time.")
        sleep(POLL_INTERVAL_SECONDS)


def connection_info(player_session):
    return {
        "ipAddress": player_session.get("IpAddress", ""),
        "dnsName": player_session.get("DnsName", ""),
        "port": player_session.get("Port", 0),
        "playerSessionId": player_session["PlayerSessionId"],
    }


def join_or_create(
    gamelift,
    fleet_id,
    location,
    account_id,
    display_name,
    max_players,
    mode=MATCH_MODE,
    sleep=time.sleep,
    clock=time.monotonic,
    timeout=ACTIVE_WAIT_SECONDS,
):
    player_data = json.dumps({"displayName": display_name}, ensure_ascii=False)

    for session in search_joinable_sessions(gamelift, fleet_id, location, mode):
        player_session = try_create_player_session(gamelift, session["GameSessionId"], account_id, player_data)
        if player_session:
            return connection_info(player_session)

    pending_session = find_activating_session(gamelift, fleet_id, location, mode)
    if pending_session:
        game_session_id = pending_session["GameSessionId"]
    else:
        request = {
            "FleetId": fleet_id,
            "MaximumPlayerSessionCount": max_players,
            "Name": "labproject-%s" % mode,
            "GameProperties": [{"Key": MODE_PROPERTY, "Value": mode}],
        }
        if location:
            request["Location"] = location
        try:
            game_session_id = gamelift.create_game_session(**request)["GameSession"]["GameSessionId"]
        except gamelift.exceptions.FleetCapacityExceededException as error:
            raise ApiError(503, "no_server_available", "No game server is available right now.") from error

    wait_until_active(gamelift, game_session_id, sleep, clock, timeout)
    player_session = try_create_player_session(gamelift, game_session_id, account_id, player_data)
    if not player_session:
        raise ApiError(503, "session_unavailable", "The new game session did not accept the player.")
    return connection_info(player_session)


def join_session(gamelift, settings, account_id, display_name, mode):
    if not settings.gamelift_fleet_id:
        raise ApiError(503, "fleet_not_configured", "GameLift fleet is not configured for this stage.")
    max_players = (
        settings.max_players_per_boss_raid_session if mode == BOSS_RAID_MODE else settings.max_players_per_session
    )
    return join_or_create(
        gamelift,
        fleet_id=settings.gamelift_fleet_id,
        location=settings.gamelift_location,
        account_id=account_id,
        display_name=display_name,
        max_players=max_players,
        mode=mode,
    )
