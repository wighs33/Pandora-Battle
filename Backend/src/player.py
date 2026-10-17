"""GET /player/me: 로그인한 플레이어의 프로필, 누적 전적과 최근 경기 기록을 돌려준다."""

import os

from common import api_handler, aws_client, json_response, player_pk, require_player, PROFILE_SK

RECENT_MATCH_LIMIT = 20
STAT_FIELDS = ("Matches", "Wins", "Losses", "Draws", "Kills", "Deaths")


def _s(item, name, default=""):
    return item.get(name, {}).get("S", default)


def _n(item, name):
    return int(item.get(name, {}).get("N", "0"))


def build_profile(player_id, items):
    """PK 하나를 SK 내림차순으로 읽은 결과를 응답 형태로 바꾼다. PROFILE이 MATCH#보다 먼저 온다."""
    profile = next((item for item in items if _s(item, "SK") == PROFILE_SK), {})
    matches = [item for item in items if _s(item, "SK").startswith("MATCH#")]
    return {
        "playerId": player_id,
        "displayName": _s(profile, "DisplayName"),
        "stats": {field[0].lower() + field[1:]: _n(profile, field) for field in STAT_FIELDS},
        "recentMatches": [
            {
                "matchId": _s(item, "MatchId"),
                "recordedAt": _s(item, "RecordedAt"),
                "mapKey": _s(item, "MapKey"),
                "result": _s(item, "Result"),
                "team": _n(item, "Team"),
                "kills": _n(item, "Kills"),
                "deaths": _n(item, "Deaths"),
            }
            for item in matches
        ],
    }


@api_handler
def handler(event, context):
    claims = require_player(event)
    player_id = claims["sub"]

    # 같은 파티션의 레이드 행(RAID#)은 PROFILE 뒤에 정렬되므로, PROFILE부터 거꾸로 읽어 프로필과 경기 기록만 받는다.
    response = aws_client("dynamodb").query(
        TableName=os.environ["TABLE_NAME"],
        KeyConditionExpression="PK = :pk AND SK <= :profile",
        ExpressionAttributeValues={":pk": {"S": player_pk(player_id)}, ":profile": {"S": PROFILE_SK}},
        ScanIndexForward=False,
        Limit=RECENT_MATCH_LIMIT + 1,
    )
    return json_response(200, build_profile(player_id, response.get("Items", [])))
