"""Lambda 백엔드의 DynamoDB 테이블을 MySQL로 옮긴다. 비어 있는 DB에 한 번만 실행한다.

    python -m app.import_dynamodb --table labproject-dev

API 주소를 Lambda에서 이 서버로 넘기기 직전에 API 서버 계정(LABPROJECT_DATABASE_URL)으로 실행한다.
모든 쓰기가 한 트랜잭션이라 중간에 실패하면 아무것도 남지 않는다.

DynamoDB 항목 → 테이블
- PLAYER#<로그인 ID> / PROFILE    → players. 전적 카운터는 옮기지 않고, 옮긴 경기 기록을 집계한 값과 같은지 확인한다.
- MATCH#<경기 ID> / RESULT        → matches, match_players. PLAYER#/MATCH# 항목은 같은 경기의 플레이어별 사본이라 읽지 않는다.
- RAID#<회차> / STATE             → boss_raids. CONFIG/BOSSRAID가 가리키는 회차만 Open 값을 entry_open으로 받는다.
- PLAYER#<로그인 ID> / RAID#<회차> → boss_raid_players. 이미 받은 보상은 골드 기록에도 남긴다(기록 시각은 옮긴 시각).
- RAID#<회차> / SERVER#<서버 ID>   → boss_raid_server_reports
"""

import argparse
import datetime

import boto3
from boto3.dynamodb.types import TypeDeserializer
from sqlalchemy import text

from .config import required_env
from .db import create_db_engine, in_transaction
from .players import Player, credit_gold, load_profile, split_account_id

# Lambda 백엔드(Backend/src/common.py utc_now_iso)가 쓴 UTC 시각 형식
TIMESTAMP_FORMAT = "%Y-%m-%dT%H:%M:%SZ"
PLAYER_PREFIX = "PLAYER#"
RAID_PREFIX = "RAID#"
SERVER_PREFIX = "SERVER#"
# PROFILE의 카운터 이름 → load_profile의 stats 이름
STAT_COUNTERS = {"Matches": "matches", "Wins": "wins", "Losses": "losses", "Draws": "draws", "Kills": "kills",
                 "Deaths": "deaths"}


def scan_items(table_name):
    deserializer = TypeDeserializer()
    pages = boto3.client("dynamodb").get_paginator("scan").paginate(TableName=table_name, ConsistentRead=True)
    for page in pages:
        for item in page["Items"]:
            yield {name: deserializer.deserialize(value) for name, value in item.items()}


def parse_time(value):
    return datetime.datetime.strptime(value, TIMESTAMP_FORMAT) if value else None


def classify(items):
    """항목을 종류별로 나눈다. 모르는 항목이 있으면 버리지 않고 멈춘다."""
    groups = {"profiles": [], "matches": [], "raids": [], "raid_players": [], "raid_servers": [], "match_copies": 0}
    config = {}
    for item in items:
        pk, sk = item["PK"], item["SK"]
        if pk.startswith(PLAYER_PREFIX) and sk == "PROFILE":
            groups["profiles"].append(item)
        elif pk.startswith(PLAYER_PREFIX) and sk.startswith("MATCH#"):
            groups["match_copies"] += 1
        elif pk.startswith(PLAYER_PREFIX) and sk.startswith(RAID_PREFIX):
            groups["raid_players"].append(item)
        elif pk.startswith("MATCH#") and sk == "RESULT":
            groups["matches"].append(item)
        elif pk.startswith(RAID_PREFIX) and sk == "STATE":
            groups["raids"].append(item)
        elif pk.startswith(RAID_PREFIX) and sk.startswith(SERVER_PREFIX):
            groups["raid_servers"].append(item)
        elif pk == "CONFIG" and sk == "BOSSRAID":
            config = item
        else:
            raise ValueError("Unknown DynamoDB item %s / %s" % (pk, sk))
    return groups, config


def import_players(conn, profiles):
    player_ids = {}
    for profile in sorted(profiles, key=lambda item: item["CreatedAt"]):
        account_id = profile["PK"][len(PLAYER_PREFIX):]
        provider, provider_user_id = split_account_id(account_id)
        player_ids[account_id] = conn.execute(
            text(
                """
                INSERT INTO players (provider, provider_user_id, display_name, created_at, last_login_at)
                VALUES (:provider, :user_id, :name, :created_at, :last_login_at)
                """
            ),
            {
                "provider": provider,
                "user_id": provider_user_id,
                "name": profile["DisplayName"],
                "created_at": parse_time(profile["CreatedAt"]),
                "last_login_at": parse_time(profile.get("LastLoginAt")),
            },
        ).lastrowid
    return player_ids


def import_matches(conn, matches, player_ids):
    # match_no 순서가 최근 경기 순서이므로 기록 시각 순으로 넣는다.
    for match in sorted(matches, key=lambda item: item["RecordedAt"]):
        match_no = conn.execute(
            text(
                """
                INSERT INTO matches (match_id, map_key, end_reason, winner_team, recorded_at)
                VALUES (:match_id, :map_key, :end_reason, :winner_team, :recorded_at)
                """
            ),
            {
                "match_id": match["MatchId"],
                "map_key": match["MapKey"],
                "end_reason": match["EndReason"],
                "winner_team": int(match["WinnerTeam"]),
                "recorded_at": parse_time(match["RecordedAt"]),
            },
        ).lastrowid
        conn.execute(
            text(
                """
                INSERT INTO match_players (match_no, slot, player_id, display_name, team, kills, deaths, result)
                VALUES (:match_no, :slot, :player_id, :display_name, :team, :kills, :deaths, :result)
                """
            ),
            [
                {
                    "match_no": match_no,
                    "slot": slot,
                    # 로그인 ID가 없던 참가자는 게스트로 남는다.
                    "player_id": player_ids.get(player["PlayerId"]),
                    "display_name": player["DisplayName"],
                    "team": int(player["Team"]),
                    "kills": int(player["Kills"]),
                    "deaths": int(player["Deaths"]),
                    "result": player["Result"],
                }
                for slot, player in enumerate(match["Players"])
            ],
        )


def import_raids(conn, groups, config, player_ids):
    current_raid_id = config.get("RaidId", "")
    for raid in sorted(groups["raids"], key=lambda item: item["CreatedAt"]):
        defeated_at = parse_time(raid.get("DefeatedAt"))
        conn.execute(
            text(
                """
                INSERT INTO boss_raids
                    (raid_id, max_hp, hp, daily_attempts, attempt_seconds, reward_gold, entry_open, opened_at, defeated_at)
                VALUES (:raid_id, :max_hp, :hp, :daily_attempts, :attempt_seconds, :reward_gold, :entry_open, :opened_at,
                        :defeated_at)
                """
            ),
            {
                "raid_id": raid["RaidId"],
                "max_hp": int(raid["MaxHp"]),
                # 여러 서버의 마지막 일격이 겹치면 DynamoDB의 체력은 0 아래로 내려가 있다.
                "hp": max(0, int(raid["Hp"])),
                "daily_attempts": int(raid["DailyAttempts"]),
                "attempt_seconds": int(raid["AttemptSeconds"]),
                "reward_gold": int(raid["RewardGold"]),
                "entry_open": raid["RaidId"] == current_raid_id and config.get("Open", False) and defeated_at is None,
                "opened_at": parse_time(raid["CreatedAt"]),
                "defeated_at": defeated_at,
            },
        )

    # 보상을 받은 행은 플레이어 행을 잠그므로, 플레이어 순서대로 넣는다(players.py의 잠금 순서).
    rows = sorted(groups["raid_players"], key=lambda item: (player_ids[item["PK"][len(PLAYER_PREFIX):]], item["RaidId"]))
    for row in rows:
        player_id = player_ids[row["PK"][len(PLAYER_PREFIX):]]
        claimed_at = parse_time(row.get("RewardClaimedAt"))
        attempt_day = row.get("AttemptDay")
        conn.execute(
            text(
                """
                INSERT INTO boss_raid_players (raid_id, player_id, damage, attempt_day, attempts_today, reward_claimed_at)
                VALUES (:raid_id, :player_id, :damage, :attempt_day, :attempts_today, :claimed_at)
                """
            ),
            {
                "raid_id": row["RaidId"],
                "player_id": player_id,
                "damage": int(row.get("Damage", 0)),
                "attempt_day": datetime.date.fromisoformat(attempt_day) if attempt_day else None,
                "attempts_today": int(row.get("AttemptsToday", 0)),
                "claimed_at": claimed_at,
            },
        )
        if claimed_at is not None:
            credit_gold(conn, player_id, int(row["RewardGold"]), "raid_reward", row["RaidId"])

    for server in groups["raid_servers"]:
        conn.execute(
            text("INSERT INTO boss_raid_server_reports (raid_id, server_id, last_seq) VALUES (:raid_id, :server_id, :seq)"),
            {
                "raid_id": server["PK"][len(RAID_PREFIX):],
                "server_id": server["SK"][len(SERVER_PREFIX):],
                "seq": int(server["LastSeq"]),
            },
        )


def check_stats(conn, profiles, player_ids):
    """DynamoDB가 경기마다 더해 온 전적 카운터와, 옮긴 경기 기록을 집계한 전적이 같아야 한다."""
    mismatches = []
    for profile in profiles:
        account_id = profile["PK"][len(PLAYER_PREFIX):]
        stats = load_profile(conn, Player(player_ids[account_id], account_id, profile["DisplayName"]))["stats"]
        expected = {name: int(profile.get(counter, 0)) for counter, name in STAT_COUNTERS.items()}
        if stats != expected:
            mismatches.append("%s: DynamoDB %s, MySQL %s" % (account_id, expected, stats))
    if mismatches:
        raise RuntimeError("Stats differ after the import:\n" + "\n".join(mismatches))


def import_items(conn, items):
    groups, config = classify(items)
    if conn.execute(text("SELECT COUNT(*) FROM players")).scalar_one():
        raise RuntimeError("The players table is not empty. Import only into a fresh database.")
    player_ids = import_players(conn, groups["profiles"])
    import_matches(conn, groups["matches"], player_ids)
    import_raids(conn, groups, config, player_ids)
    check_stats(conn, groups["profiles"], player_ids)
    return {
        "players": len(groups["profiles"]),
        "matches": len(groups["matches"]),
        "match copies skipped": groups["match_copies"],
        "raids": len(groups["raids"]),
        "raid players": len(groups["raid_players"]),
        "raid servers": len(groups["raid_servers"]),
    }


def main():
    parser = argparse.ArgumentParser(description="Copy the Lambda backend's DynamoDB table into an empty MySQL database.")
    parser.add_argument("--table", required=True, help="DynamoDB 테이블 이름(예: labproject-dev)")
    args = parser.parse_args()

    items = list(scan_items(args.table))
    engine = create_db_engine(required_env("DATABASE_URL"))
    summary = in_transaction(engine, lambda conn: import_items(conn, items))
    engine.dispose()
    print("Imported %d items from %s:" % (len(items), args.table))
    for name, count in summary.items():
        print("  %-20s %d" % (name, count))


if __name__ == "__main__":
    main()
