"""POST /match/join: 로그인한 플레이어에게 GameLift 게임 세션 자리를 잡아 주고 접속 정보를 돌려준다.

빈 자리가 있는 세션을 먼저 찾고, 없으면 새 세션을 만든 뒤 ACTIVE가 될 때까지 기다린다.
클라이언트는 받은 주소로 접속하면서 PlayerSessionId를 URL 옵션으로 보내고, 게임 서버가 AcceptPlayerSession으로 검증한다.

요청 본문의 mode로 세션 종류를 나누고, 같은 값을 게임 속성 mode에 넣어 검색과 서버 분기에 쓴다.
- match(기본): 로비에서 인원을 모아 한 경기를 하는 PvP 세션. 경기가 시작되면 서버가 새 참가를 막는다.
- rpg: 경기 끝 없이 들어오고 나가는 공유 월드. 서버는 RPG 맵을 연 뒤에 세션을 활성화한다.

동시에 여러 명이 빈 서버에 요청하면 각자 새 세션을 만들 수 있다. 인원을 모아 한 세션에 넣는 일은
FlexMatch로 옮길 때 해결한다(Docs/GameLift_Backend.md 참고).
"""

import json
import os
import time

from common import ApiError, api_handler, json_response, parse_json_body, require_player

# Lambda 제한(29초) 안에서 기다린다. RPG 세션은 서버가 맵을 연 뒤 활성화하므로 경기 세션보다 오래 걸린다.
ACTIVE_WAIT_SECONDS = 24.0
POLL_INTERVAL_SECONDS = 1.0

MODE_PROPERTY = "mode"
MATCH_MODE = "match"
RPG_MODE = "rpg"
SESSION_MODES = (MATCH_MODE, RPG_MODE)


def read_mode(body):
    mode = body.get("mode", MATCH_MODE)
    if mode not in SESSION_MODES:
        raise ApiError(400, "invalid_mode", "mode must be one of: %s." % ", ".join(SESSION_MODES))
    return mode


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


def try_create_player_session(gamelift, game_session_id, player_id, player_data):
    """자리가 없거나 세션이 더 이상 참가를 받지 않으면 None을 돌려준다."""
    try:
        return gamelift.create_player_session(
            GameSessionId=game_session_id,
            PlayerId=player_id,
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
    player_id,
    display_name,
    max_players,
    mode=MATCH_MODE,
    sleep=time.sleep,
    clock=time.monotonic,
    timeout=ACTIVE_WAIT_SECONDS,
):
    player_data = json.dumps({"displayName": display_name}, ensure_ascii=False)

    for session in search_joinable_sessions(gamelift, fleet_id, location, mode):
        player_session = try_create_player_session(gamelift, session["GameSessionId"], player_id, player_data)
        if player_session:
            return connection_info(player_session)

    request = {
        "FleetId": fleet_id,
        "MaximumPlayerSessionCount": max_players,
        "Name": "labproject-%s" % mode,
        "GameProperties": [{"Key": MODE_PROPERTY, "Value": mode}],
    }
    if location:
        request["Location"] = location
    try:
        game_session = gamelift.create_game_session(**request)["GameSession"]
    except gamelift.exceptions.FleetCapacityExceededException as error:
        raise ApiError(503, "no_server_available", "No game server is available right now.") from error

    wait_until_active(gamelift, game_session["GameSessionId"], sleep, clock, timeout)
    player_session = try_create_player_session(gamelift, game_session["GameSessionId"], player_id, player_data)
    if not player_session:
        raise ApiError(503, "session_unavailable", "The new game session did not accept the player.")
    return connection_info(player_session)


@api_handler
def handler(event, context):
    claims = require_player(event)
    mode = read_mode(parse_json_body(event))
    fleet_id = os.environ.get("GAMELIFT_FLEET_ID", "")
    if not fleet_id:
        raise ApiError(503, "fleet_not_configured", "GameLift fleet is not configured for this stage.")

    import boto3

    max_players_variable = "MAX_PLAYERS_PER_RPG_SESSION" if mode == RPG_MODE else "MAX_PLAYERS_PER_SESSION"
    result = join_or_create(
        boto3.client("gamelift"),
        fleet_id=fleet_id,
        location=os.environ.get("GAMELIFT_LOCATION", ""),
        player_id=claims["sub"],
        display_name=claims.get("name", ""),
        max_players=int(os.environ.get(max_players_variable, "4")),
        mode=mode,
    )
    return json_response(200, result)
