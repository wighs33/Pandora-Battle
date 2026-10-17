"""POST /server/raid-damage: 전용 서버가 레이드 보스에게 들어간 딜을 보고하면 전역 체력을 깎고 플레이어별 누적 딜을 더한다.

match-result와 같이 API Gateway의 IAM 인증(SigV4)으로만 호출할 수 있다. 딜은 서버가 집계하므로 클라이언트는 바꿀 수 없다.

서버는 레이드 월드를 열 때 빈 보고로 지금 회차와 전역 체력을 받아 오고, 그 뒤로는 일정 간격과 도전이 끝날 때마다
그사이 쌓인 딜을 보낸다. 응답의 전역 체력으로 자기 월드의 보스 체력을 맞추므로 다른 서버가 깎은 체력도 함께 보인다.

- 서버마다 보고 순번(seq)을 늘려 보낸다. 이미 받은 순번은 다시 반영하지 않고 지금 체력만 돌려준다(재전송 대비).
- 여러 서버가 동시에 마지막 일격을 보고하면 전역 체력이 0 아래로 내려갈 수 있다. 0 이하가 되면 회차를 격파로 바꾼다.
- 격파된 뒤에 도착한 딜도 누적 딜에는 더한다. 서버가 격파를 알기 전에 실제로 넣은 딜이기 때문이다.
"""

import os
import re

from boss_raid import STATUS_DEFEATED, STATUS_OPEN, current_raid, load_raid, player_raid_key
from common import (
    ApiError,
    RAID_STATE_SK,
    api_handler,
    aws_client,
    ddb_n,
    ddb_s,
    json_response,
    parse_json_body,
    raid_pk,
    raid_server_sk,
    sanitize_display_name,
    utc_now_iso,
)
from match_result import PLAYER_ID_PATTERN

MAX_PLAYERS = 16
MAX_DAMAGE = 10**9
MAX_SEQ = 2**53
RAID_ID_PATTERN = re.compile(r"^[A-Za-z0-9_\-]{1,64}$")
# GameLift 게임 세션 ARN 또는 로컬 서버가 만든 ID
SERVER_ID_PATTERN = re.compile(r"^[A-Za-z0-9:/_.\-]{1,256}$")


def _require_int(value, name, minimum, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise ApiError(400, "invalid_report", "%s must be an integer in [%d, %d]." % (name, minimum, maximum))
    return value


def validate_report(body):
    """보고 형식을 검증하고 정규화한 사본을 돌려준다. raidId가 비어 있으면 지금 회차에 보고한다."""
    raid_id = body.get("raidId", "")
    if not isinstance(raid_id, str) or (raid_id and not RAID_ID_PATTERN.match(raid_id)):
        raise ApiError(400, "invalid_report", "raidId is malformed.")

    server_id = body.get("serverId")
    if not isinstance(server_id, str) or not SERVER_ID_PATTERN.match(server_id):
        raise ApiError(400, "invalid_report", "serverId is missing or malformed.")

    players = body.get("players", [])
    if not isinstance(players, list) or len(players) > MAX_PLAYERS:
        raise ApiError(400, "invalid_report", "players must contain at most %d entries." % MAX_PLAYERS)

    normalized_players = []
    seen_player_ids = set()
    for player in players:
        if not isinstance(player, dict):
            raise ApiError(400, "invalid_report", "Each player must be an object.")
        player_id = player.get("playerId") or ""
        if player_id:
            if not isinstance(player_id, str) or not PLAYER_ID_PATTERN.match(player_id):
                raise ApiError(400, "invalid_report", "playerId is malformed.")
            # 한 트랜잭션에서 같은 항목을 두 번 바꿀 수 없다.
            if player_id in seen_player_ids:
                raise ApiError(400, "invalid_report", "playerId appears more than once.")
            seen_player_ids.add(player_id)
        normalized_players.append(
            {
                "playerId": player_id,
                "displayName": sanitize_display_name(player.get("displayName")),
                "damage": _require_int(player.get("damage"), "damage", 0, MAX_DAMAGE),
            }
        )

    return {
        "raidId": raid_id,
        "serverId": server_id,
        "seq": _require_int(body.get("seq"), "seq", 1, MAX_SEQ),
        "players": normalized_players,
    }


def build_transaction(report, table_name, recorded_at):
    """보고 순번 갱신 + 전역 체력 차감 + 백엔드 ID가 있는 플레이어마다 누적 딜 추가를 한 트랜잭션 항목으로 만든다."""
    raid_id = report["raidId"]
    total_damage = sum(player["damage"] for player in report["players"])
    items = [
        {
            "Update": {
                "TableName": table_name,
                "Key": {"PK": ddb_s(raid_pk(raid_id)), "SK": ddb_s(raid_server_sk(report["serverId"]))},
                "UpdateExpression": "SET LastSeq = :seq, UpdatedAt = :now",
                "ConditionExpression": "attribute_not_exists(LastSeq) OR LastSeq < :seq",
                "ExpressionAttributeValues": {":seq": ddb_n(report["seq"]), ":now": ddb_s(recorded_at)},
            }
        },
        {
            "Update": {
                "TableName": table_name,
                "Key": {"PK": ddb_s(raid_pk(raid_id)), "SK": ddb_s(RAID_STATE_SK)},
                "UpdateExpression": "SET LastDamageAt = :now ADD Hp :damage",
                "ConditionExpression": "attribute_exists(PK)",
                "ExpressionAttributeValues": {":damage": ddb_n(-total_damage), ":now": ddb_s(recorded_at)},
            }
        },
    ]

    for player in report["players"]:
        if not player["playerId"] or player["damage"] <= 0:
            continue
        items.append(
            {
                "Update": {
                    "TableName": table_name,
                    "Key": player_raid_key(player["playerId"], raid_id),
                    # RankKey와 Damage가 모두 있는 행만 순위 인덱스에 들어간다.
                    # 표시 이름은 참가할 때 로그인 이름으로 이미 적었다. 서버의 경기 표시 이름은 그 이름이 없을 때만 쓴다.
                    "UpdateExpression": (
                        "SET RankKey = :rank, RaidId = :raid, DisplayName = if_not_exists(DisplayName, :name), "
                        "EntityType = :type, LastDamageAt = :now ADD Damage :damage"
                    ),
                    "ExpressionAttributeValues": {
                        ":rank": ddb_s(raid_pk(raid_id)),
                        ":raid": ddb_s(raid_id),
                        ":name": ddb_s(player["displayName"]),
                        ":type": ddb_s("RaidPlayer"),
                        ":now": ddb_s(recorded_at),
                        ":damage": ddb_n(player["damage"]),
                    },
                }
            }
        )
    return items


def _cancellation_reasons(error):
    response = getattr(error, "response", {}) or {}
    if response.get("Error", {}).get("Code") != "TransactionCanceledException":
        return None
    return [reason.get("Code") for reason in response.get("CancellationReasons") or []]


def record_damage(dynamodb, table_name, report, recorded_at):
    """딜을 반영하면 True, 이미 받은 순번이면 False. 딜이 없는 보고(체력 조회)는 쓰지 않는다."""
    if not any(player["damage"] > 0 for player in report["players"]):
        return False

    try:
        dynamodb.transact_write_items(TransactItems=build_transaction(report, table_name, recorded_at))
    except Exception as error:  # noqa: BLE001 - botocore ClientError의 취소 사유만 구분한다.
        reasons = _cancellation_reasons(error)
        if reasons and reasons[0] == "ConditionalCheckFailed":
            return False
        if reasons and len(reasons) > 1 and reasons[1] == "ConditionalCheckFailed":
            raise ApiError(404, "raid_not_found", "Boss raid %s does not exist." % report["raidId"]) from error
        raise
    return True


def settle_defeat(dynamodb, table_name, raid, defeated_at):
    """체력이 0 이하가 된 열린 회차를 격파로 바꾼다. 여러 서버가 동시에 불러도 한 번만 바뀐다."""
    if raid["defeated"] or raid["hp"] > 0:
        return raid
    try:
        dynamodb.update_item(
            TableName=table_name,
            Key={"PK": ddb_s(raid_pk(raid["raidId"])), "SK": ddb_s(RAID_STATE_SK)},
            UpdateExpression="SET #status = :defeated, DefeatedAt = :now",
            ConditionExpression="#status = :open",
            ExpressionAttributeNames={"#status": "Status"},
            ExpressionAttributeValues={
                ":defeated": ddb_s(STATUS_DEFEATED),
                ":open": ddb_s(STATUS_OPEN),
                ":now": ddb_s(defeated_at),
            },
        )
        print("raid-damage raidId=%s defeated" % raid["raidId"])
    except dynamodb.exceptions.ConditionalCheckFailedException:
        pass
    return dict(raid, defeated=True)


@api_handler
def handler(event, context):
    report = validate_report(parse_json_body(event))
    dynamodb = aws_client("dynamodb")
    table_name = os.environ["TABLE_NAME"]

    if not report["raidId"]:
        raid, _ = current_raid(dynamodb, table_name)
        if raid is None:
            raise ApiError(404, "raid_not_found", "No boss raid has been opened.")
        report["raidId"] = raid["raidId"]

    recorded_at = utc_now_iso()
    recorded = record_damage(dynamodb, table_name, report, recorded_at)
    raid = load_raid(dynamodb, table_name, report["raidId"], consistent=True)
    if raid is None:
        raise ApiError(404, "raid_not_found", "Boss raid %s does not exist." % report["raidId"])
    raid = settle_defeat(dynamodb, table_name, raid, recorded_at)

    if recorded:
        print("raid-damage raidId=%s serverId=%s seq=%d damage=%d hp=%d" % (
            report["raidId"], report["serverId"], report["seq"],
            sum(player["damage"] for player in report["players"]), raid["hp"]))
    return json_response(200, dict(raid, recorded=recorded))
