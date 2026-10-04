"""POST /server/match-result: 전용 서버가 경기 결과를 보고하면 전적을 한 트랜잭션으로 기록한다.

이 경로는 API Gateway의 IAM 인증(SigV4)으로만 호출할 수 있다. GameLift 플릿의 인스턴스 역할이나
개발자의 IAM 자격 증명을 가진 서버만 결과를 쓸 수 있고, 게임 클라이언트는 호출할 수 없다.

같은 경기 ID로 다시 보고하면 경기 결과 항목의 조건부 쓰기가 실패하므로 전적이 두 번 더해지지 않는다.
"""

import os
import re

from common import (
    ApiError,
    api_handler,
    aws_client,
    ddb_n,
    ddb_s,
    json_response,
    match_pk,
    MATCH_RESULT_SK,
    parse_json_body,
    player_match_sk,
    player_pk,
    PROFILE_SK,
    sanitize_display_name,
    utc_now_iso,
)

END_REASONS = ("completed", "player_exit")
RESULTS = ("win", "lose", "draw")
RESULT_STAT = {"win": "Wins", "lose": "Losses", "draw": "Draws"}
MAX_PLAYERS = 16
MAX_COUNT = 100000
MATCH_ID_PATTERN = re.compile(r"^[A-Za-z0-9:/_.\-]{1,256}$")
PLAYER_ID_PATTERN = re.compile(r"^(steam:[0-9]{1,20}|dev:[A-Za-z0-9_-]{1,32})$")
MAP_KEY_PATTERN = re.compile(r"^[A-Za-z0-9_.\-]{0,64}$")


def _require_int(value, name, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise ApiError(400, "invalid_report", "%s must be an integer in [%d, %d]." % (name, minimum, maximum))
    return value


def validate_report(body):
    """보고 형식을 검증하고 정규화한 사본을 돌려준다. 백엔드 ID가 없는 플레이어는 경기 기록에만 남는다."""
    match_id = body.get("matchId")
    if not isinstance(match_id, str) or not MATCH_ID_PATTERN.match(match_id):
        raise ApiError(400, "invalid_report", "matchId is missing or malformed.")

    map_key = body.get("mapKey", "")
    if not isinstance(map_key, str) or not MAP_KEY_PATTERN.match(map_key):
        raise ApiError(400, "invalid_report", "mapKey is malformed.")

    end_reason = body.get("endReason")
    if end_reason not in END_REASONS:
        raise ApiError(400, "invalid_report", "endReason must be one of %s." % ", ".join(END_REASONS))

    winner_team = _require_int(body.get("winnerTeam", -1), "winnerTeam", -1, 64)

    players = body.get("players")
    if not isinstance(players, list) or not 1 <= len(players) <= MAX_PLAYERS:
        raise ApiError(400, "invalid_report", "players must contain 1-%d entries." % MAX_PLAYERS)

    normalized_players = []
    seen_player_ids = set()
    for player in players:
        if not isinstance(player, dict):
            raise ApiError(400, "invalid_report", "Each player must be an object.")
        player_id = player.get("playerId") or ""
        if player_id:
            if not isinstance(player_id, str) or not PLAYER_ID_PATTERN.match(player_id):
                raise ApiError(400, "invalid_report", "playerId is malformed.")
            if player_id in seen_player_ids:
                raise ApiError(400, "invalid_report", "playerId appears more than once.")
            seen_player_ids.add(player_id)
        result = player.get("result")
        if result not in RESULTS:
            raise ApiError(400, "invalid_report", "result must be one of %s." % ", ".join(RESULTS))
        normalized_players.append(
            {
                "playerId": player_id,
                "displayName": sanitize_display_name(player.get("displayName")),
                "team": _require_int(player.get("team", -1), "team", -1, 64),
                "kills": _require_int(player.get("kills", 0), "kills", 0, MAX_COUNT),
                "deaths": _require_int(player.get("deaths", 0), "deaths", 0, MAX_COUNT),
                "result": result,
            }
        )

    return {
        "matchId": match_id,
        "mapKey": map_key,
        "endReason": end_reason,
        "winnerTeam": winner_team,
        "players": normalized_players,
    }


def build_transaction(report, table_name, recorded_at, reported_by):
    """경기 결과 1건 + 플레이어마다 누적 전적 갱신과 경기 기록 추가를 한 트랜잭션 항목으로 만든다."""
    match_id = report["matchId"]
    items = [
        {
            "Put": {
                "TableName": table_name,
                "Item": {
                    "PK": ddb_s(match_pk(match_id)),
                    "SK": ddb_s(MATCH_RESULT_SK),
                    "EntityType": ddb_s("Match"),
                    "MatchId": ddb_s(match_id),
                    "MapKey": ddb_s(report["mapKey"]),
                    "EndReason": ddb_s(report["endReason"]),
                    "WinnerTeam": ddb_n(report["winnerTeam"]),
                    "RecordedAt": ddb_s(recorded_at),
                    "ReportedBy": ddb_s(reported_by or "unknown"),
                    "Players": {
                        "L": [
                            {
                                "M": {
                                    "PlayerId": ddb_s(player["playerId"]),
                                    "DisplayName": ddb_s(player["displayName"]),
                                    "Team": ddb_n(player["team"]),
                                    "Kills": ddb_n(player["kills"]),
                                    "Deaths": ddb_n(player["deaths"]),
                                    "Result": ddb_s(player["result"]),
                                }
                            }
                            for player in report["players"]
                        ]
                    },
                },
                "ConditionExpression": "attribute_not_exists(PK)",
            }
        }
    ]

    for player in report["players"]:
        if not player["playerId"]:
            continue
        pk = ddb_s(player_pk(player["playerId"]))
        result_stat = RESULT_STAT[player["result"]]
        items.append(
            {
                "Update": {
                    "TableName": table_name,
                    "Key": {"PK": pk, "SK": ddb_s(PROFILE_SK)},
                    "UpdateExpression": (
                        "SET LastMatchAt = :now, EntityType = :type, "
                        "DisplayName = if_not_exists(DisplayName, :name) "
                        "ADD Matches :one, Kills :kills, Deaths :deaths, #result :one"
                    ),
                    "ExpressionAttributeNames": {"#result": result_stat},
                    "ExpressionAttributeValues": {
                        ":now": ddb_s(recorded_at),
                        ":type": ddb_s("Player"),
                        ":name": ddb_s(player["displayName"]),
                        ":one": ddb_n(1),
                        ":kills": ddb_n(player["kills"]),
                        ":deaths": ddb_n(player["deaths"]),
                    },
                }
            }
        )
        items.append(
            {
                "Put": {
                    "TableName": table_name,
                    "Item": {
                        "PK": pk,
                        "SK": ddb_s(player_match_sk(recorded_at, match_id)),
                        "EntityType": ddb_s("PlayerMatch"),
                        "MatchId": ddb_s(match_id),
                        "MapKey": ddb_s(report["mapKey"]),
                        "RecordedAt": ddb_s(recorded_at),
                        "Result": ddb_s(player["result"]),
                        "Team": ddb_n(player["team"]),
                        "Kills": ddb_n(player["kills"]),
                        "Deaths": ddb_n(player["deaths"]),
                    },
                }
            }
        )
    return items


def is_duplicate_report(error):
    """첫 항목(경기 결과)의 조건부 쓰기만 실패했다면 이미 기록된 경기다."""
    response = getattr(error, "response", {}) or {}
    if response.get("Error", {}).get("Code") != "TransactionCanceledException":
        return False
    reasons = response.get("CancellationReasons") or []
    return bool(reasons) and reasons[0].get("Code") == "ConditionalCheckFailed"


def record_report(dynamodb, table_name, report, reported_by, recorded_at=None):
    items = build_transaction(report, table_name, recorded_at or utc_now_iso(), reported_by)
    try:
        dynamodb.transact_write_items(TransactItems=items)
    except Exception as error:  # noqa: BLE001 - botocore ClientError만 중복 여부를 판단한다.
        if is_duplicate_report(error):
            return False
        raise
    return True


@api_handler
def handler(event, context):
    identity = ((event.get("requestContext") or {}).get("authorizer") or {}).get("iam") or {}
    reported_by = identity.get("userArn") or identity.get("callerId") or ""
    report = validate_report(parse_json_body(event))

    recorded = record_report(aws_client("dynamodb"), os.environ["TABLE_NAME"], report, reported_by)
    print("match-result matchId=%s recorded=%s reportedBy=%s" % (report["matchId"], recorded, reported_by))
    return json_response(200, {"recorded": recorded, "duplicate": not recorded})
