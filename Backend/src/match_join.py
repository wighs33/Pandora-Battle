"""POST /match/join: 로그인한 플레이어에게 GameLift 게임 세션 자리를 잡아 주고 접속 정보를 돌려준다.

빈 자리가 있는 세션을 먼저 찾고, 없으면 새 세션을 만든 뒤 ACTIVE가 될 때까지 기다린다.
클라이언트는 받은 주소로 접속하면서 PlayerSessionId를 URL 옵션으로 보내고, 게임 서버가 AcceptPlayerSession으로 검증한다.

동시에 여러 명이 빈 서버에 요청하면 각자 새 세션을 만들 수 있다. 인원을 모아 한 세션에 넣는 일은
FlexMatch로 옮길 때 해결한다(Docs/GameLift_Backend.md 참고).
"""

import json
import os
import time

from common import ApiError, api_handler, json_response, require_player

ACTIVE_WAIT_SECONDS = 20.0
POLL_INTERVAL_SECONDS = 1.0


def search_joinable_sessions(gamelift, fleet_id, location):
    request = {
        "FleetId": fleet_id,
        "FilterExpression": "hasAvailablePlayerSessions=true",
        "SortExpression": "creationTimeMillis ASC",
        "Limit": 10,
    }
    if location:
        request["Location"] = location
    return gamelift.search_game_sessions(**request).get("GameSessions", [])


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
    sleep=time.sleep,
    clock=time.monotonic,
    timeout=ACTIVE_WAIT_SECONDS,
):
    player_data = json.dumps({"displayName": display_name}, ensure_ascii=False)

    for session in search_joinable_sessions(gamelift, fleet_id, location):
        player_session = try_create_player_session(gamelift, session["GameSessionId"], player_id, player_data)
        if player_session:
            return connection_info(player_session)

    request = {
        "FleetId": fleet_id,
        "MaximumPlayerSessionCount": max_players,
        "Name": "labproject-match",
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
    fleet_id = os.environ.get("GAMELIFT_FLEET_ID", "")
    if not fleet_id:
        raise ApiError(503, "fleet_not_configured", "GameLift fleet is not configured for this stage.")

    import boto3

    result = join_or_create(
        boto3.client("gamelift"),
        fleet_id=fleet_id,
        location=os.environ.get("GAMELIFT_LOCATION", ""),
        player_id=claims["sub"],
        display_name=claims.get("name", ""),
        max_players=int(os.environ.get("MAX_PLAYERS_PER_SESSION", "4")),
    )
    return json_response(200, result)
