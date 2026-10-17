"""백엔드 순수 로직 단위 테스트. AWS나 boto3 없이 실행한다.

    python -m unittest discover -s Backend/tests -v
"""

import io
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

import common  # noqa: E402
from auth import verify_steam_ticket  # noqa: E402
import boss_raid  # noqa: E402
import raid_damage  # noqa: E402
from common import ApiError  # noqa: E402
from match_join import join_boss_raid, join_or_create, read_mode  # noqa: E402
from match_result import build_transaction, is_duplicate_report, validate_report  # noqa: E402
from player import build_profile  # noqa: E402

SECRET = b"test-secret"
ISSUER = "labproject-test"


class TokenTests(unittest.TestCase):
    def test_round_trip(self):
        token = common.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000)
        claims = common.verify_token(SECRET, token, ISSUER, now=1001)
        self.assertEqual(claims["sub"], "steam:1")
        self.assertEqual(claims["name"], "Alice")

    def test_rejects_tampered_signature_and_claims(self):
        token = common.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000)
        header, claims, signature = token.split(".")
        forged_claims = common._b64url_encode(json.dumps({"iss": ISSUER, "sub": "steam:2", "exp": 99999}).encode())
        for forged in (header + "." + forged_claims + "." + signature, token[:-2] + "AA"):
            with self.assertRaises(ApiError) as context:
                common.verify_token(SECRET, forged, ISSUER, now=1001)
            self.assertEqual(context.exception.status, 401)

    def test_rejects_expired_wrong_issuer_and_wrong_secret(self):
        token = common.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000, ttl=10)
        for secret, issuer, now in ((SECRET, ISSUER, 1010), (SECRET, "other", 1001), (b"other", ISSUER, 1001)):
            with self.assertRaises(ApiError):
                common.verify_token(secret, token, issuer, now=now)

    def test_require_player_needs_bearer_header(self):
        with self.assertRaises(ApiError) as context:
            common.require_player({"headers": {}}, secret=SECRET)
        self.assertEqual(context.exception.code, "missing_token")


class FakeHttpResponse(io.BytesIO):
    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False


def steam_opener(payload, captured=None):
    def opener(url, timeout):
        if captured is not None:
            captured.append(url)
        return FakeHttpResponse(json.dumps(payload).encode("utf-8"))

    return opener


class SteamTicketTests(unittest.TestCase):
    def test_accepts_ok_ticket_and_sends_identity(self):
        urls = []
        payload = {"response": {"params": {"result": "OK", "steamid": "76561198000000000"}}}
        steam_id = verify_steam_ticket("ABCDEF0123456789", "key", "4972140", "labproject-backend", steam_opener(payload, urls))
        self.assertEqual(steam_id, "76561198000000000")
        self.assertIn("identity=labproject-backend", urls[0])
        self.assertIn("appid=4972140", urls[0])

    def test_rejects_error_banned_and_malformed_tickets(self):
        cases = (
            ("ABCDEF0123456789", {"response": {"error": {"errorcode": 101, "errordesc": "Invalid ticket"}}}, 401),
            ("ABCDEF0123456789", {"response": {"params": {"result": "OK", "steamid": "1", "publisherbanned": True}}}, 403),
            ("not-hex-ticket!!", {}, 400),
        )
        for ticket, payload, status in cases:
            with self.assertRaises(ApiError) as context:
                verify_steam_ticket(ticket, "key", "4972140", "id", steam_opener(payload))
            self.assertEqual(context.exception.status, status)

    def test_rejected_key_is_distinguished_from_outage(self):
        import urllib.error

        def forbidden(url, timeout):
            raise urllib.error.HTTPError(url, 403, "Forbidden", {}, io.BytesIO(b"Access is denied."))

        with self.assertRaises(ApiError) as context:
            verify_steam_ticket("ABCDEF0123456789", "key", "4972140", "id", forbidden)
        self.assertEqual(context.exception.code, "steam_key_rejected")


def valid_report(**overrides):
    report = {
        "matchId": "arn:aws:gamelift:ap-northeast-2::gamesession/fleet-1/gsess-1",
        "mapKey": "Colosseum",
        "endReason": "completed",
        "winnerTeam": 0,
        "players": [
            {"playerId": "steam:1", "displayName": "A", "team": 0, "kills": 3, "deaths": 1, "result": "win"},
            {"playerId": "dev:bob", "displayName": "B", "team": 1, "kills": 1, "deaths": 3, "result": "lose"},
            {"playerId": "", "displayName": "Guest", "team": 1, "kills": 0, "deaths": 0, "result": "lose"},
        ],
    }
    report.update(overrides)
    return report


class MatchReportTests(unittest.TestCase):
    def test_valid_report_is_normalized(self):
        report = validate_report(valid_report())
        self.assertEqual(len(report["players"]), 3)
        self.assertEqual(report["players"][2]["playerId"], "")

    def test_rejects_invalid_reports(self):
        bad_players = [dict(valid_report()["players"][0]), dict(valid_report()["players"][0])]
        cases = (
            valid_report(matchId=""),
            valid_report(endReason="unknown"),
            valid_report(players=[]),
            valid_report(players=bad_players),
            valid_report(players=[dict(valid_report()["players"][0], result="mvp")]),
            valid_report(players=[dict(valid_report()["players"][0], kills=True)]),
            valid_report(players=[dict(valid_report()["players"][0], playerId="steam:abc")]),
        )
        for body in cases:
            with self.assertRaises(ApiError):
                validate_report(body)

    def test_transaction_writes_match_once_and_updates_known_players(self):
        report = validate_report(valid_report())
        items = build_transaction(report, "table", "2026-09-30T00:00:00Z", "arn:aws:iam::1:role/server")
        # 경기 결과 1 + 백엔드 ID가 있는 플레이어 2명 x (전적 갱신 + 경기 기록)
        self.assertEqual(len(items), 5)
        self.assertEqual(items[0]["Put"]["ConditionExpression"], "attribute_not_exists(PK)")
        self.assertEqual(len(items[0]["Put"]["Item"]["Players"]["L"]), 3)
        winner_update = items[1]["Update"]
        self.assertEqual(winner_update["Key"]["PK"], {"S": "PLAYER#steam:1"})
        self.assertEqual(winner_update["ExpressionAttributeNames"]["#result"], "Wins")
        self.assertEqual(winner_update["ExpressionAttributeValues"][":kills"], {"N": "3"})
        self.assertEqual(items[2]["Put"]["Item"]["SK"]["S"].split("#")[0], "MATCH")
        self.assertEqual(items[3]["Update"]["ExpressionAttributeNames"]["#result"], "Losses")

    def test_duplicate_detection_uses_first_cancellation_reason(self):
        class FakeError(Exception):
            def __init__(self, reasons):
                self.response = {"Error": {"Code": "TransactionCanceledException"}, "CancellationReasons": reasons}

        self.assertTrue(is_duplicate_report(FakeError([{"Code": "ConditionalCheckFailed"}, {"Code": "None"}])))
        self.assertFalse(is_duplicate_report(FakeError([{"Code": "None"}, {"Code": "ValidationError"}])))


class FakeGameLift:
    class exceptions:  # noqa: N801 - boto3 클라이언트의 exceptions 속성과 같은 모양
        class GameSessionFullException(Exception):
            pass

        class InvalidGameSessionStatusException(Exception):
            pass

        class FleetCapacityExceededException(Exception):
            pass

    def __init__(self, searchable, full_sessions=(), statuses=("ACTIVE",), active_unindexed=(), activating=()):
        self.searchable = searchable
        self.full_sessions = set(full_sessions)
        self.statuses = list(statuses)
        self.active_unindexed = list(active_unindexed)
        self.activating = list(activating)
        self.created = []
        self.player_sessions = []

    def search_game_sessions(self, **request):
        self.search_request = request
        return {"GameSessions": [{"GameSessionId": session_id} for session_id in self.searchable]}

    def create_player_session(self, GameSessionId, PlayerId, PlayerData):
        if GameSessionId in self.full_sessions:
            raise self.exceptions.GameSessionFullException()
        self.player_sessions.append((GameSessionId, PlayerId, json.loads(PlayerData)))
        return {"PlayerSession": {"PlayerSessionId": "psess-" + GameSessionId, "IpAddress": "127.0.0.1", "Port": 7777}}

    def create_game_session(self, **request):
        self.created.append(request)
        return {"GameSession": {"GameSessionId": "gsess-new"}}

    def describe_game_sessions(self, GameSessionId=None, **request):
        if GameSessionId is None and request.get("StatusFilter") == "ACTIVATING":
            return {"GameSessions": list(self.activating)}
        if GameSessionId is None:
            # 검색 색인에는 아직 없지만 이미 ACTIVE인 세션(가득 찬 세션, 참가 차단 세션 포함)
            return {"GameSessions": list(self.active_unindexed)}
        status = self.statuses.pop(0) if len(self.statuses) > 1 else self.statuses[0]
        return {"GameSessions": [{"GameSessionId": GameSessionId, "Status": status}]}


class MatchJoinTests(unittest.TestCase):
    def join(self, gamelift, location="", mode="match"):
        return join_or_create(
            gamelift, "fleet-1", location, "steam:1", "Alice", 4, mode=mode,
            sleep=lambda seconds: None, clock=lambda: 0.0
        )

    def test_mode_defaults_to_match_and_rejects_unknown_values(self):
        self.assertEqual(read_mode({}), "match")
        self.assertEqual(read_mode({"mode": "bossraid"}), "bossraid")
        with self.assertRaises(ApiError) as context:
            read_mode({"mode": "battle-royale"})
        self.assertEqual(context.exception.code, "invalid_mode")

    def test_boss_raid_search_and_new_session_carry_the_mode(self):
        gamelift = FakeGameLift([])
        self.join(gamelift, mode="bossraid")
        self.assertIn("gameSessionProperties.mode = 'bossraid'", gamelift.search_request["FilterExpression"])
        self.assertEqual(gamelift.created[0]["GameProperties"], [{"Key": "mode", "Value": "bossraid"}])
        self.assertEqual(gamelift.created[0]["Name"], "labproject-bossraid")

    def test_unindexed_fallback_keeps_modes_apart(self):
        boss_raid_world = {"GameSessionId": "gsess-bossraid", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 6,
                     "GameProperties": [{"Key": "mode", "Value": "bossraid"}]}
        legacy_match = {"GameSessionId": "gsess-legacy", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 4}

        gamelift = FakeGameLift([], active_unindexed=[boss_raid_world, legacy_match])
        self.assertEqual(self.join(gamelift)["playerSessionId"], "psess-gsess-legacy")

        gamelift = FakeGameLift([], active_unindexed=[legacy_match, boss_raid_world])
        self.assertEqual(self.join(gamelift, mode="bossraid")["playerSessionId"], "psess-gsess-bossraid")

    def test_joins_existing_session_with_free_slot(self):
        gamelift = FakeGameLift(["gsess-full", "gsess-open"], full_sessions=["gsess-full"])
        info = self.join(gamelift)
        self.assertEqual(info["playerSessionId"], "psess-gsess-open")
        self.assertEqual(gamelift.created, [])
        self.assertEqual(gamelift.player_sessions[0][2], {"displayName": "Alice"})

    def test_creates_session_in_anywhere_location_and_waits_for_active(self):
        gamelift = FakeGameLift([], statuses=("ACTIVATING", "ACTIVATING", "ACTIVE"))
        info = self.join(gamelift, location="custom-labproject-dev")
        self.assertEqual(info, {"ipAddress": "127.0.0.1", "dnsName": "", "port": 7777, "playerSessionId": "psess-gsess-new"})
        self.assertEqual(gamelift.created[0]["Location"], "custom-labproject-dev")
        self.assertEqual(gamelift.search_request["Location"], "custom-labproject-dev")

    def test_joins_new_session_missing_from_search_index(self):
        unindexed = [
            {"GameSessionId": "gsess-full", "CurrentPlayerSessionCount": 4, "MaximumPlayerSessionCount": 4},
            {"GameSessionId": "gsess-locked", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 4,
             "PlayerSessionCreationPolicy": "DENY_ALL"},
            {"GameSessionId": "gsess-fresh", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 4,
             "PlayerSessionCreationPolicy": "ACCEPT_ALL"},
        ]
        gamelift = FakeGameLift([], active_unindexed=unindexed)
        info = self.join(gamelift, location="custom-labproject-dev")
        self.assertEqual(info["playerSessionId"], "psess-gsess-fresh")
        self.assertEqual(gamelift.created, [])

    def test_waits_for_activating_session_of_same_mode_instead_of_creating(self):
        activating = [
            {"GameSessionId": "gsess-match-starting", "GameProperties": [{"Key": "mode", "Value": "match"}]},
            {"GameSessionId": "gsess-raid-starting", "GameProperties": [{"Key": "mode", "Value": "bossraid"}]},
        ]
        gamelift = FakeGameLift([], statuses=("ACTIVATING", "ACTIVE"), activating=activating)
        info = self.join(gamelift, mode="bossraid")
        self.assertEqual(info["playerSessionId"], "psess-gsess-raid-starting")
        self.assertEqual(gamelift.created, [])

    def test_failed_session_is_reported(self):
        with self.assertRaises(ApiError) as context:
            self.join(FakeGameLift([], statuses=("ERROR",)))
        self.assertEqual(context.exception.code, "session_failed")


class ConditionalCheckFailed(Exception):
    pass


class FakeDynamoDb:
    """테스트에 쓰는 식만 해석하는 메모리 테이블. 항목은 (PK, SK)로 찾는다."""

    class exceptions:  # noqa: N801 - boto3 클라이언트의 exceptions 속성과 같은 모양
        ConditionalCheckFailedException = ConditionalCheckFailed

    def __init__(self, items=()):
        self.items = {(item["PK"]["S"], item["SK"]["S"]): dict(item) for item in items}

    def get_item(self, TableName, Key, ConsistentRead=False):
        item = self.items.get((Key["PK"]["S"], Key["SK"]["S"]))
        return {"Item": dict(item)} if item else {}

    @staticmethod
    def _value(attribute):
        return int(attribute["N"]) if "N" in attribute else attribute["S"]

    def _check(self, item, condition, values, names):
        def clause(text):
            text = text.strip()
            for function, expected in (("attribute_not_exists(", False), ("attribute_exists(", True)):
                if text.startswith(function):
                    name = text[len(function):-1]
                    return (names.get(name, name) in item) == expected
            for operator in ("<>", "<", ">", "="):
                if " %s " % operator in text:
                    left, right = (part.strip() for part in text.split(" %s " % operator))
                    left = names.get(left, left)
                    if left not in item:
                        return False
                    a, b = self._value(item[left]), self._value(values[right])
                    return {"<>": a != b, "<": a < b, ">": a > b, "=": a == b}[operator]
            raise AssertionError("unsupported condition " + text)

        return any(all(clause(part) for part in group.split(" AND ")) for group in condition.split(" OR "))

    def update_item(self, TableName, Key, UpdateExpression, ExpressionAttributeValues,
                    ConditionExpression=None, ExpressionAttributeNames=None, ReturnValues=None):
        names = ExpressionAttributeNames or {}
        key = (Key["PK"]["S"], Key["SK"]["S"])
        item = dict(self.items.get(key, {"PK": Key["PK"], "SK": Key["SK"]}))
        if ConditionExpression and not self._check(item, ConditionExpression, ExpressionAttributeValues, names):
            raise ConditionalCheckFailed()
        expression = " " + UpdateExpression
        set_part, _, add_part = expression.partition(" ADD ")
        for assignment in filter(None, set_part.replace(" SET ", "", 1).split(", ")):
            name, value = (part.strip() for part in assignment.split(" = "))
            item[names.get(name, name)] = ExpressionAttributeValues[value]
        for addition in filter(None, add_part.split(", ")):
            name, value = addition.split()
            current = int(item.get(name, {"N": "0"})["N"])
            item[name] = {"N": str(current + int(ExpressionAttributeValues[value]["N"]))}
        self.items[key] = item
        return {"Attributes": dict(item)} if ReturnValues else {}

    def query(self, TableName, KeyConditionExpression, ExpressionAttributeValues, IndexName=None,
              ScanIndexForward=True, Limit=None, Select=None):
        values = ExpressionAttributeValues
        if IndexName == "RaidRanking":
            rows = [item for item in self.items.values() if item.get("RankKey") == values[":rank"] and "Damage" in item]
            if ":damage" in values:
                rows = [item for item in rows if int(item["Damage"]["N"]) > int(values[":damage"]["N"])]
            rows.sort(key=lambda item: int(item["Damage"]["N"]), reverse=not ScanIndexForward)
        else:
            rows = [item for (pk, sk), item in sorted(self.items.items())
                    if pk == values[":pk"]["S"] and sk.startswith(values[":raid"]["S"])]
        rows = rows[:Limit] if Limit else rows
        return {"Count": len(rows)} if Select == "COUNT" else {"Items": [dict(row) for row in rows]}


def config_item(raid_id="r1", is_open=True):
    return {"PK": {"S": "CONFIG"}, "SK": {"S": "BOSSRAID"}, "Open": {"BOOL": is_open}, "RaidId": {"S": raid_id}}


def raid_item(raid_id="r1", hp=1000, status="open", daily_attempts=3, reward_gold=500):
    return {
        "PK": {"S": "RAID#" + raid_id}, "SK": {"S": "STATE"}, "RaidId": {"S": raid_id},
        "Hp": {"N": str(hp)}, "MaxHp": {"N": "1000"}, "Status": {"S": status},
        "DailyAttempts": {"N": str(daily_attempts)}, "AttemptSeconds": {"N": "300"},
        "RewardGold": {"N": str(reward_gold)},
    }


def player_raid_item(player_id, raid_id="r1", damage=None, name="P", **extra):
    item = {"PK": {"S": "PLAYER#" + player_id}, "SK": {"S": "RAID#" + raid_id}, "RaidId": {"S": raid_id},
            "DisplayName": {"S": name}}
    if damage is not None:
        item.update(RankKey={"S": "RAID#" + raid_id}, Damage={"N": str(damage)})
    item.update(extra)
    return item


class BossRaidStatusTests(unittest.TestCase):
    def test_accepts_players_only_while_open_and_not_defeated(self):
        cases = (
            ((), False),
            ((config_item(is_open=False), raid_item()), False),
            ((config_item(), raid_item(status="defeated", hp=0)), False),
            ((config_item(), raid_item()), True),
        )
        for items, expected in cases:
            self.assertEqual(boss_raid.current_raid(FakeDynamoDb(items), "t")[1], expected)
        self.assertIsNone(boss_raid.current_raid(FakeDynamoDb(), "t")[0])

    def test_health_below_zero_is_shown_as_zero(self):
        raid = boss_raid.raid_from_item(raid_item(hp=-25, status="defeated"))
        self.assertEqual((raid["hp"], raid["maxHp"], raid["defeated"]), (0, 1000, True))

    def test_attempt_day_changes_at_korean_midnight(self):
        # 2026-10-12T14:59:59Z는 한국 시간 10월 12일 23:59:59다.
        self.assertEqual(boss_raid.attempt_day(now=1791817199), "2026-10-12")
        self.assertEqual(boss_raid.attempt_day(now=1791817200), "2026-10-13")


class BossRaidAttemptTests(unittest.TestCase):
    def setUp(self):
        self.db = FakeDynamoDb([config_item(), raid_item(daily_attempts=2)])
        self.raid = boss_raid.load_raid(self.db, "t", "r1")

    def consume(self, today="2026-10-13"):
        return boss_raid.consume_attempt(self.db, "t", self.raid, "steam:1", "Alice", today)

    def test_daily_limit_resets_on_the_next_day(self):
        self.assertEqual(self.consume(), 1)
        self.assertEqual(self.consume(), 0)
        with self.assertRaises(ApiError) as context:
            self.consume()
        self.assertEqual(context.exception.code, "no_attempts_left")
        self.assertEqual(self.consume(today="2026-10-14"), 1)

    def test_failed_join_returns_the_attempt(self):
        def failing_join(claims, mode, fleet_id):
            raise ApiError(504, "session_timeout", "Game session did not become active in time.")

        claims = {"sub": "steam:1", "name": "Alice"}
        with self.assertRaises(ApiError):
            join_boss_raid(claims, "fleet-1", self.db, "t", join=failing_join)
        row = self.db.get_item("t", boss_raid.player_raid_key("steam:1", "r1"))["Item"]
        self.assertEqual(boss_raid.attempts_left(row, 2, boss_raid.attempt_day()), 2)

        result = join_boss_raid(claims, "fleet-1", self.db, "t", join=lambda c, m, f: {"port": 7777})
        self.assertEqual(result, {"port": 7777, "attemptsLeft": 1})

    def test_closed_raid_is_rejected_without_using_an_attempt(self):
        db = FakeDynamoDb([config_item(is_open=False), raid_item()])
        with self.assertRaises(ApiError) as context:
            join_boss_raid({"sub": "steam:1"}, "fleet-1", db, "t", join=lambda c, m, f: {})
        self.assertEqual(context.exception.code, "bossraid_closed")
        self.assertNotIn(("PLAYER#steam:1", "RAID#r1"), db.items)


class BossRaidRankingTests(unittest.TestCase):
    def test_ties_share_a_rank_and_players_without_damage_are_unranked(self):
        db = FakeDynamoDb([
            raid_item(),
            player_raid_item("steam:1", damage=300, name="A"),
            player_raid_item("steam:2", damage=500, name="B"),
            player_raid_item("steam:3", damage=300, name="C"),
            player_raid_item("steam:4", name="D", AttemptDay={"S": "2026-10-13"}, AttemptsToday={"N": "1"}),
            player_raid_item("steam:5", raid_id="old", damage=999, name="E"),
        ])
        raid = boss_raid.load_raid(db, "t", "r1")
        standing = boss_raid.player_standing(db, "t", raid, "steam:3", "2026-10-13")
        self.assertEqual([(entry["rank"], entry["name"]) for entry in standing["top"]], [(1, "B"), (2, "A"), (2, "C")])
        self.assertEqual((standing["damage"], standing["rank"], standing["attemptsLeft"]), (300, 2, 3))

        unranked = boss_raid.player_standing(db, "t", raid, "steam:4", "2026-10-13")
        self.assertEqual((unranked["rank"], unranked["attemptsLeft"]), (0, 2))


class BossRaidRewardTests(unittest.TestCase):
    def test_reward_is_given_once_for_each_defeated_raid_with_damage(self):
        db = FakeDynamoDb([
            raid_item("won", hp=0, status="defeated", reward_gold=500),
            raid_item("running", hp=10),
            raid_item("watched", hp=0, status="defeated", reward_gold=700),
            player_raid_item("steam:1", raid_id="won", damage=40),
            player_raid_item("steam:1", raid_id="running", damage=40),
            player_raid_item("steam:1", raid_id="watched", AttemptDay={"S": "2026-10-13"}),
        ])
        self.assertEqual(boss_raid.claim_rewards(db, "t", "steam:1", "2026-10-13T00:00:00Z"), {"gold": 500, "raids": ["won"]})
        self.assertEqual(boss_raid.claim_rewards(db, "t", "steam:1", "2026-10-13T00:01:00Z"), {"gold": 0, "raids": []})


def damage_report(**overrides):
    report = {
        "raidId": "r1",
        "serverId": "arn:aws:gamelift:ap-northeast-2::gamesession/fleet-1/gsess-1",
        "seq": 3,
        "players": [
            {"playerId": "steam:1", "displayName": "A", "damage": 120},
            {"playerId": "", "displayName": "Guest", "damage": 30},
            {"playerId": "dev:bob", "displayName": "B", "damage": 0},
        ],
    }
    report.update(overrides)
    return report


class RaidDamageTests(unittest.TestCase):
    def test_rejects_invalid_reports(self):
        player = damage_report()["players"][0]
        cases = (
            damage_report(serverId=""),
            damage_report(seq=0),
            damage_report(raidId="bad id"),
            damage_report(players=[dict(player, damage=-1)]),
            damage_report(players=[dict(player, damage=1.5)]),
            damage_report(players=[player, dict(player)]),
        )
        for body in cases:
            with self.assertRaises(ApiError):
                raid_damage.validate_report(body)
        self.assertEqual(raid_damage.validate_report(damage_report(raidId="", players=[]))["players"], [])

    def test_transaction_lowers_shared_health_and_credits_known_players(self):
        report = raid_damage.validate_report(damage_report())
        items = raid_damage.build_transaction(report, "t", "2026-10-13T00:00:00Z")
        # 보고 순번 + 전역 체력 + 백엔드 ID가 있고 딜을 넣은 플레이어 1명
        self.assertEqual(len(items), 3)
        self.assertEqual(items[0]["Update"]["ConditionExpression"], "attribute_not_exists(LastSeq) OR LastSeq < :seq")
        self.assertEqual(items[1]["Update"]["ExpressionAttributeValues"][":damage"], {"N": "-150"})
        self.assertEqual(items[2]["Update"]["Key"]["PK"], {"S": "PLAYER#steam:1"})
        self.assertEqual(items[2]["Update"]["ExpressionAttributeValues"][":rank"], {"S": "RAID#r1"})

    def test_repeated_sequence_is_not_applied_twice(self):
        class Cancelled(Exception):
            def __init__(self, reasons):
                super().__init__("cancelled")
                self.response = {"Error": {"Code": "TransactionCanceledException"},
                                 "CancellationReasons": [{"Code": code} for code in reasons]}

        class Table:
            def __init__(self, reasons):
                self.reasons = reasons

            def transact_write_items(self, TransactItems):
                raise Cancelled(self.reasons)

        report = raid_damage.validate_report(damage_report())
        self.assertFalse(raid_damage.record_damage(Table(["ConditionalCheckFailed", "None", "None"]), "t", report, "now"))
        with self.assertRaises(ApiError) as context:
            raid_damage.record_damage(Table(["None", "ConditionalCheckFailed", "None"]), "t", report, "now")
        self.assertEqual(context.exception.code, "raid_not_found")
        # 딜이 없는 보고는 체력만 묻고 쓰지 않는다.
        self.assertFalse(raid_damage.record_damage(Table([]), "t", dict(report, players=[]), "now"))

    def test_zero_health_marks_the_raid_defeated_once(self):
        db = FakeDynamoDb([raid_item(hp=-20)])
        raid = boss_raid.load_raid(db, "t", "r1")
        self.assertTrue(raid_damage.settle_defeat(db, "t", raid, "now")["defeated"])
        self.assertTrue(raid_damage.settle_defeat(db, "t", raid, "later")["defeated"])
        self.assertEqual(db.items[("RAID#r1", "STATE")]["Status"], {"S": "defeated"})
        self.assertEqual(db.items[("RAID#r1", "STATE")]["DefeatedAt"], {"S": "now"})


class PlayerProfileTests(unittest.TestCase):
    def test_profile_and_recent_matches(self):
        items = [
            {"SK": {"S": "PROFILE"}, "DisplayName": {"S": "Alice"}, "Matches": {"N": "2"}, "Wins": {"N": "1"}},
            {"SK": {"S": "MATCH#2026-09-30T00:00:00Z#m2"}, "MatchId": {"S": "m2"}, "Result": {"S": "win"}, "Kills": {"N": "3"}},
        ]
        profile = build_profile("steam:1", items)
        self.assertEqual(profile["displayName"], "Alice")
        self.assertEqual(profile["stats"]["matches"], 2)
        self.assertEqual(profile["stats"]["losses"], 0)
        self.assertEqual(profile["recentMatches"][0]["kills"], 3)


if __name__ == "__main__":
    unittest.main()
