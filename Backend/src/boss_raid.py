"""보스 레이드(월드 보스) 회차, 하루 도전 횟수, 순위, 보상.

운영자가 Backend/bossraid.ps1로 회차를 열면 모든 게임 서버가 하나의 전역 체력을 함께 깎는다(raid_damage.py).
플레이어는 하루 정해진 횟수만큼 도전하고(POST /match/join이 들어갈 때마다 한 번씩 쓴다), 회차 동안 넣은 딜을 누적해 순위를 매긴다.
보스가 쓰러지면 그 회차에 딜을 넣은 플레이어 모두가 보상 골드를 받는다.

- GET  /bossraid/status  로그인 없이: 개최 여부와 전역 체력. 타이틀 화면이 로그인 전에 물어 버튼을 켜고 끈다.
- GET  /bossraid/me      로그인: 오늘 남은 도전 횟수, 내 누적 딜과 순위, 상위 순위.
- POST /bossraid/claim   로그인: 격파된 회차의 받지 않은 보상을 한 번만 내준다.

개최 여부와 지금 회차는 설정 항목(CONFIG/BOSSRAID) 하나에 두므로, 열고 닫을 때 게임 패치나 백엔드 배포가 필요 없다.
"""

import os
import time

from common import (
    ApiError,
    BOSS_RAID_SK,
    CONFIG_PK,
    RAID_KEY_PREFIX,
    RAID_RANKING_INDEX,
    RAID_STATE_SK,
    api_handler,
    aws_client,
    ddb_n,
    ddb_s,
    json_response,
    player_pk,
    raid_pk,
    require_player,
    utc_now_iso,
)

STATUS_OPEN = "open"
STATUS_DEFEATED = "defeated"
RANKING_SIZE = 10
# 하루 도전 횟수는 한국 시간 자정에 다시 채워진다.
ATTEMPT_DAY_UTC_OFFSET_SECONDS = 9 * 60 * 60


def _s(item, name, default=""):
    return item.get(name, {}).get("S", default)


def _n(item, name, default=0):
    return int(item.get(name, {}).get("N", default))


def attempt_day(now=None):
    """도전 횟수를 세는 날짜(한국 시간 기준 YYYY-MM-DD)."""
    current = time.time() if now is None else now
    return time.strftime("%Y-%m-%d", time.gmtime(current + ATTEMPT_DAY_UTC_OFFSET_SECONDS))


def player_raid_key(player_id, raid_id):
    return {"PK": ddb_s(player_pk(player_id)), "SK": ddb_s(raid_pk(raid_id))}


def _query_pages(dynamodb, request):
    """Query 결과를 페이지마다 돌려준다. 한 번에 1MB까지만 읽으므로 끝까지 읽을 때 쓴다."""
    while True:
        page = dynamodb.query(**request)
        yield page
        if "LastEvaluatedKey" not in page:
            return
        request = dict(request, ExclusiveStartKey=page["LastEvaluatedKey"])


# 회차 -----------------------------------------------------------------------------------------------------------------

def raid_from_item(item):
    """STATE 항목을 응답 모양으로 바꾼다. 여러 서버의 마지막 일격이 겹치면 체력이 0 아래로 내려가므로 0으로 자른다."""
    return {
        "raidId": _s(item, "RaidId"),
        "hp": max(0, _n(item, "Hp")),
        "maxHp": _n(item, "MaxHp"),
        "defeated": _s(item, "Status") == STATUS_DEFEATED,
        "dailyAttempts": _n(item, "DailyAttempts"),
        "attemptSeconds": _n(item, "AttemptSeconds"),
        "rewardGold": _n(item, "RewardGold"),
    }


def load_raid(dynamodb, table_name, raid_id, consistent=False):
    item = dynamodb.get_item(
        TableName=table_name,
        Key={"PK": ddb_s(raid_pk(raid_id)), "SK": ddb_s(RAID_STATE_SK)},
        ConsistentRead=consistent,
    ).get("Item")
    return raid_from_item(item) if item else None


def current_raid(dynamodb, table_name):
    """운영자가 연 지금 회차와 참가를 받는지 여부. 회차가 없으면 (None, False)다.

    닫혀 있거나 이미 격파된 회차는 새 참가를 받지 않는다. 회차 정보는 닫힌 뒤에도 순위와 보상 안내에 쓴다.
    """
    config = dynamodb.get_item(
        TableName=table_name,
        Key={"PK": ddb_s(CONFIG_PK), "SK": ddb_s(BOSS_RAID_SK)},
    ).get("Item", {})
    raid_id = _s(config, "RaidId")
    raid = load_raid(dynamodb, table_name, raid_id) if raid_id else None
    accepting = raid is not None and config.get("Open", {}).get("BOOL", False) and not raid["defeated"]
    return raid, accepting


# 하루 도전 횟수 ------------------------------------------------------------------------------------------------------

def attempts_left(row, daily_attempts, today):
    used = _n(row, "AttemptsToday") if _s(row, "AttemptDay") == today else 0
    return max(0, daily_attempts - used)


def consume_attempt(dynamodb, table_name, raid, player_id, display_name, today):
    """오늘 도전 1회를 쓰고 남은 횟수를 돌려준다. 다 썼으면 403 no_attempts_left.

    두 조건부 갱신 중 하나만 성공한다. 오늘 처음이면 횟수를 1로 새로 쓰고, 아니면 한도 안에서만 1을 더한다.
    """
    key = player_raid_key(player_id, raid["raidId"])
    conditional_failed = dynamodb.exceptions.ConditionalCheckFailedException
    try:
        dynamodb.update_item(
            TableName=table_name,
            Key=key,
            UpdateExpression=(
                "SET AttemptDay = :today, AttemptsToday = :one, "
                "RaidId = :raid, DisplayName = :name, EntityType = :type"
            ),
            ConditionExpression="attribute_not_exists(AttemptDay) OR AttemptDay <> :today",
            ExpressionAttributeValues={
                ":today": ddb_s(today),
                ":one": ddb_n(1),
                ":raid": ddb_s(raid["raidId"]),
                ":name": ddb_s(display_name),
                ":type": ddb_s("RaidPlayer"),
            },
        )
        return raid["dailyAttempts"] - 1
    except conditional_failed:
        pass

    try:
        response = dynamodb.update_item(
            TableName=table_name,
            Key=key,
            UpdateExpression="SET DisplayName = :name ADD AttemptsToday :one",
            ConditionExpression="AttemptDay = :today AND AttemptsToday < :limit",
            ExpressionAttributeValues={
                ":today": ddb_s(today),
                ":one": ddb_n(1),
                ":limit": ddb_n(raid["dailyAttempts"]),
                ":name": ddb_s(display_name),
            },
            ReturnValues="UPDATED_NEW",
        )
    except conditional_failed as error:
        raise ApiError(403, "no_attempts_left", "No boss raid attempts left today.") from error
    return raid["dailyAttempts"] - _n(response["Attributes"], "AttemptsToday")


def refund_attempt(dynamodb, table_name, raid_id, player_id, today):
    """자리를 잡지 못해 들어가지 못한 도전을 돌려준다. 그사이 날짜가 바뀌었으면 돌려줄 것이 없다."""
    try:
        dynamodb.update_item(
            TableName=table_name,
            Key=player_raid_key(player_id, raid_id),
            UpdateExpression="ADD AttemptsToday :minus_one",
            ConditionExpression="AttemptDay = :today AND AttemptsToday > :zero",
            ExpressionAttributeValues={":today": ddb_s(today), ":minus_one": ddb_n(-1), ":zero": ddb_n(0)},
        )
    except dynamodb.exceptions.ConditionalCheckFailedException:
        pass


# 순위 ----------------------------------------------------------------------------------------------------------------

def ranked_entries(items):
    """딜 내림차순 항목에 순위를 붙인다. 딜이 같으면 같은 순위다."""
    entries = []
    for index, item in enumerate(items):
        damage = _n(item, "Damage")
        rank = entries[-1]["rank"] if entries and entries[-1]["damage"] == damage else index + 1
        entries.append({"rank": rank, "name": _s(item, "DisplayName", "Player"), "damage": damage})
    return entries


def count_higher_damage(dynamodb, table_name, raid_id, damage):
    request = {
        "TableName": table_name,
        "IndexName": RAID_RANKING_INDEX,
        "KeyConditionExpression": "RankKey = :rank AND Damage > :damage",
        "ExpressionAttributeValues": {":rank": ddb_s(raid_pk(raid_id)), ":damage": ddb_n(damage)},
        "Select": "COUNT",
    }
    return sum(page.get("Count", 0) for page in _query_pages(dynamodb, request))


def player_standing(dynamodb, table_name, raid, player_id, today):
    raid_id = raid["raidId"]
    row = dynamodb.get_item(TableName=table_name, Key=player_raid_key(player_id, raid_id)).get("Item", {})
    damage = _n(row, "Damage")
    top = dynamodb.query(
        TableName=table_name,
        IndexName=RAID_RANKING_INDEX,
        KeyConditionExpression="RankKey = :rank",
        ExpressionAttributeValues={":rank": ddb_s(raid_pk(raid_id))},
        ScanIndexForward=False,
        Limit=RANKING_SIZE,
    ).get("Items", [])
    return {
        "raid": raid,
        "attemptsLeft": attempts_left(row, raid["dailyAttempts"], today),
        "damage": damage,
        # 딜을 넣기 전에는 순위가 없다(0).
        "rank": count_higher_damage(dynamodb, table_name, raid_id, damage) + 1 if damage > 0 else 0,
        "top": ranked_entries(top),
    }


# 보상 ----------------------------------------------------------------------------------------------------------------

def claim_rewards(dynamodb, table_name, player_id, claimed_at):
    """딜을 넣은 회차 중 격파됐고 아직 받지 않은 회차의 보상 골드를 더해 돌려준다. 회차마다 한 번만 준다."""
    request = {
        "TableName": table_name,
        "KeyConditionExpression": "PK = :pk AND begins_with(SK, :raid)",
        "ExpressionAttributeValues": {":pk": ddb_s(player_pk(player_id)), ":raid": ddb_s(RAID_KEY_PREFIX)},
    }
    gold = 0
    claimed_raids = []
    for page in _query_pages(dynamodb, request):
        for row in page.get("Items", []):
            if _n(row, "Damage") <= 0 or "RewardClaimedAt" in row:
                continue
            raid = load_raid(dynamodb, table_name, _s(row, "RaidId"))
            if not raid or not raid["defeated"]:
                continue
            try:
                dynamodb.update_item(
                    TableName=table_name,
                    Key={"PK": row["PK"], "SK": row["SK"]},
                    UpdateExpression="SET RewardClaimedAt = :now, RewardGold = :gold",
                    ConditionExpression="attribute_not_exists(RewardClaimedAt)",
                    ExpressionAttributeValues={":now": ddb_s(claimed_at), ":gold": ddb_n(raid["rewardGold"])},
                )
            except dynamodb.exceptions.ConditionalCheckFailedException:
                # 같은 플레이어의 다른 요청이 먼저 받았다.
                continue
            gold += raid["rewardGold"]
            claimed_raids.append(raid["raidId"])
    return {"gold": gold, "raids": claimed_raids}


# 핸들러 --------------------------------------------------------------------------------------------------------------

def status(dynamodb, table_name):
    raid, accepting = current_raid(dynamodb, table_name)
    return {"open": accepting, "raid": raid}


def me(event, dynamodb, table_name):
    claims = require_player(event)
    raid, accepting = current_raid(dynamodb, table_name)
    if raid is None:
        return {"open": False, "raid": None}
    standing = player_standing(dynamodb, table_name, raid, claims["sub"], attempt_day())
    return dict(standing, open=accepting)


@api_handler
def handler(event, context):
    dynamodb = aws_client("dynamodb")
    table_name = os.environ["TABLE_NAME"]
    route = event.get("routeKey")
    if route == "GET /bossraid/status":
        return json_response(200, status(dynamodb, table_name))
    if route == "GET /bossraid/me":
        return json_response(200, me(event, dynamodb, table_name))
    if route == "POST /bossraid/claim":
        claims = require_player(event)
        return json_response(200, claim_rewards(dynamodb, table_name, claims["sub"], utc_now_iso()))
    raise ApiError(404, "not_found", "Not found.")
