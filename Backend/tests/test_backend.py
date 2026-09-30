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
from common import ApiError  # noqa: E402
from match_join import join_or_create  # noqa: E402
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

    def __init__(self, searchable, full_sessions=(), statuses=("ACTIVE",)):
        self.searchable = searchable
        self.full_sessions = set(full_sessions)
        self.statuses = list(statuses)
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

    def describe_game_sessions(self, GameSessionId):
        status = self.statuses.pop(0) if len(self.statuses) > 1 else self.statuses[0]
        return {"GameSessions": [{"GameSessionId": GameSessionId, "Status": status}]}


class MatchJoinTests(unittest.TestCase):
    def join(self, gamelift, location=""):
        return join_or_create(
            gamelift, "fleet-1", location, "steam:1", "Alice", 4, sleep=lambda seconds: None, clock=lambda: 0.0
        )

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

    def test_failed_session_is_reported(self):
        with self.assertRaises(ApiError) as context:
            self.join(FakeGameLift([], statuses=("ERROR",)))
        self.assertEqual(context.exception.code, "session_failed")


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
