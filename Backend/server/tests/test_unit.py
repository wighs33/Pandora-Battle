"""DB 없이 도는 단위 테스트: 토큰, Steam 티켓, GameLift 자리 잡기, 요청 형식, 마이그레이션 파일."""

import datetime
import io
import json
import pathlib
import tempfile
import unittest
import urllib.error

from pydantic import ValidationError

from app import tokens
from app.boss_raid import attempt_day
from app.errors import ApiError
from app.gamelift import join_or_create
from app.migrate import migration_files, split_statements
from app.models import MatchReport, RaidDamageReport
from app.steam import verify_steam_ticket
from tests.fakes import FakeGameLift

SECRET = b"test-secret"
ISSUER = "labproject-test"


class TokenTests(unittest.TestCase):
    def test_round_trip(self):
        token = tokens.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000)
        claims = tokens.verify_token(SECRET, token, ISSUER, now=1001)
        self.assertEqual(claims["sub"], "steam:1")
        self.assertEqual(claims["name"], "Alice")

    def test_rejects_tampered_signature_and_claims(self):
        token = tokens.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000)
        header, _, signature = token.split(".")
        forged_claims = tokens._b64url_encode(json.dumps({"iss": ISSUER, "sub": "steam:2", "exp": 99999}).encode())
        for forged in (header + "." + forged_claims + "." + signature, token[:-2] + "AA"):
            with self.assertRaises(ApiError) as context:
                tokens.verify_token(SECRET, forged, ISSUER, now=1001)
            self.assertEqual(context.exception.status, 401)

    def test_rejects_expired_wrong_issuer_and_wrong_secret(self):
        token = tokens.issue_token(SECRET, "steam:1", "Alice", ISSUER, now=1000, ttl=10)
        for secret, issuer, now in ((SECRET, ISSUER, 1010), (SECRET, "other", 1001), (b"other", ISSUER, 1001)):
            with self.assertRaises(ApiError):
                tokens.verify_token(secret, token, issuer, now=now)


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
        opener = steam_opener(payload, urls)
        steam_id = verify_steam_ticket("ABCDEF0123456789", "key", "4972140", "labproject-backend", opener)
        self.assertEqual(steam_id, "76561198000000000")
        self.assertIn("identity=labproject-backend", urls[0])
        self.assertIn("appid=4972140", urls[0])

    def test_rejects_error_and_banned_tickets(self):
        cases = (
            ({"response": {"error": {"errorcode": 101, "errordesc": "Invalid ticket"}}}, 401),
            ({"response": {"params": {"result": "OK", "steamid": "1", "publisherbanned": True}}}, 403),
        )
        for payload, status in cases:
            with self.assertRaises(ApiError) as context:
                verify_steam_ticket("ABCDEF0123456789", "key", "4972140", "id", steam_opener(payload))
            self.assertEqual(context.exception.status, status)

    def test_rejected_key_is_distinguished_from_outage(self):
        def forbidden(url, timeout):
            raise urllib.error.HTTPError(url, 403, "Forbidden", {}, io.BytesIO(b"Access is denied."))

        with self.assertRaises(ApiError) as context, self.assertLogs("app.steam", "WARNING") as logs:
            verify_steam_ticket("ABCDEF0123456789", "key", "4972140", "id", forbidden)
        self.assertEqual(context.exception.code, "steam_key_rejected")
        # 로그에는 Steam의 응답만 남고, API 키가 든 요청 URL은 남지 않는다.
        self.assertIn("HTTP 403: Access is denied.", logs.output[0])
        self.assertNotIn("key=", logs.output[0])


class MatchJoinTests(unittest.TestCase):
    def join(self, gamelift, location="", mode="match", timeout=24.0):
        return join_or_create(
            gamelift, "fleet-1", location, "steam:1", "Alice", 4, mode=mode,
            sleep=lambda seconds: None, clock=lambda: 0.0, timeout=timeout,
        )

    def test_boss_raid_search_and_new_session_carry_the_mode(self):
        gamelift = FakeGameLift()
        self.join(gamelift, mode="bossraid")
        self.assertIn("gameSessionProperties.mode = 'bossraid'", gamelift.search_request["FilterExpression"])
        self.assertEqual(gamelift.created[0]["GameProperties"], [{"Key": "mode", "Value": "bossraid"}])

    def test_joins_existing_session_with_free_slot(self):
        gamelift = FakeGameLift(["gsess-full", "gsess-open"], full_sessions=["gsess-full"])
        info = self.join(gamelift)
        self.assertEqual(info["playerSessionId"], "psess-gsess-open")
        self.assertEqual(gamelift.created, [])
        self.assertEqual(gamelift.player_sessions[0][1:], ("steam:1", {"displayName": "Alice"}))

    def test_creates_session_in_anywhere_location_and_waits_for_active(self):
        gamelift = FakeGameLift(statuses=("ACTIVATING", "ACTIVATING", "ACTIVE"))
        info = self.join(gamelift, location="custom-labproject-dev")
        expected = {"ipAddress": "127.0.0.1", "dnsName": "", "port": 7777, "playerSessionId": "psess-gsess-new"}
        self.assertEqual(info, expected)
        self.assertEqual(gamelift.created[0]["Location"], "custom-labproject-dev")

    def test_joins_new_session_missing_from_search_index(self):
        unindexed = [
            {"GameSessionId": "gsess-full", "CurrentPlayerSessionCount": 4, "MaximumPlayerSessionCount": 4},
            {"GameSessionId": "gsess-locked", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 4,
             "PlayerSessionCreationPolicy": "DENY_ALL"},
            {"GameSessionId": "gsess-fresh", "CurrentPlayerSessionCount": 1, "MaximumPlayerSessionCount": 4},
        ]
        info = self.join(FakeGameLift(active_unindexed=unindexed))
        self.assertEqual(info["playerSessionId"], "psess-gsess-fresh")

    def test_waits_for_activating_session_of_same_mode_instead_of_creating(self):
        activating = [
            {"GameSessionId": "gsess-match-starting", "GameProperties": [{"Key": "mode", "Value": "match"}]},
            {"GameSessionId": "gsess-raid-starting", "GameProperties": [{"Key": "mode", "Value": "bossraid"}]},
        ]
        gamelift = FakeGameLift(statuses=("ACTIVATING", "ACTIVE"), activating=activating)
        self.assertEqual(self.join(gamelift, mode="bossraid")["playerSessionId"], "psess-gsess-raid-starting")
        self.assertEqual(gamelift.created, [])

    def test_failed_and_slow_sessions_are_reported(self):
        with self.assertRaises(ApiError) as context:
            self.join(FakeGameLift(statuses=("ERROR",)))
        self.assertEqual(context.exception.code, "session_failed")
        # 시계가 멈춰 있으므로 기다릴 시간을 0으로 주면 첫 확인에서 시간이 다 된다.
        with self.assertRaises(ApiError) as context:
            self.join(FakeGameLift(statuses=("ACTIVATING",)), timeout=0.0)
        self.assertEqual(context.exception.code, "session_timeout")


def match_report(**overrides):
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


class RequestModelTests(unittest.TestCase):
    def test_valid_match_report_is_normalized(self):
        report = MatchReport.model_validate(match_report())
        self.assertEqual(report.players[0].player_id, "steam:1")
        self.assertEqual(report.players[2].player_id, "")
        self.assertEqual(report.players[0].gold_reward, 0)

    def test_display_name_is_cleaned_and_limited(self):
        player = dict(match_report()["players"][0], displayName="  Al\u0007ice" + "x" * 40)
        report = MatchReport.model_validate(match_report(players=[player]))
        self.assertEqual(report.players[0].display_name, ("Alice" + "x" * 40)[:32])

    def test_rejects_invalid_match_reports(self):
        first = match_report()["players"][0]
        cases = (
            match_report(matchId=""),
            match_report(endReason="unknown"),
            match_report(players=[]),
            match_report(players=[dict(first), dict(first)]),
            match_report(players=[dict(first, result="mvp")]),
            match_report(players=[dict(first, kills=True)]),
            match_report(players=[dict(first, kills="3")]),
            match_report(players=[dict(first, playerId="steam:abc")]),
            match_report(players=[dict(first, goldReward=-1)]),
        )
        for body in cases:
            with self.assertRaises(ValidationError):
                MatchReport.model_validate(body)

    def test_raid_damage_report_rules(self):
        report = RaidDamageReport.model_validate({"serverId": "local-1", "seq": 1})
        self.assertEqual((report.raid_id, report.players), ("", []))
        for body in (
            {"serverId": "local-1", "seq": 0},
            {"serverId": "", "seq": 1},
            {"serverId": "local-1", "seq": 1, "players": [{"playerId": "dev:a", "damage": -1}]},
            {"serverId": "local-1", "seq": 1, "players": [{"playerId": "dev:a", "damage": 1}] * 2},
        ):
            with self.assertRaises(ValidationError):
                RaidDamageReport.model_validate(body)


class AttemptDayTests(unittest.TestCase):
    def test_day_changes_at_korean_midnight(self):
        korean_midnight = datetime.datetime(2026, 10, 30, 15, 0, tzinfo=datetime.timezone.utc).timestamp()
        self.assertEqual(attempt_day(korean_midnight - 1), datetime.date(2026, 10, 30))
        self.assertEqual(attempt_day(korean_midnight), datetime.date(2026, 10, 31))


class MigrationFileTests(unittest.TestCase):
    def test_splits_statements_and_skips_comments(self):
        sql = "-- comment\nCREATE TABLE a (\n    id INT -- inline\n);\n\nINSERT INTO a VALUES (1);\n"
        expected = ["CREATE TABLE a (\n    id INT -- inline\n)", "INSERT INTO a VALUES (1)"]
        self.assertEqual(split_statements(sql), expected)
        with self.assertRaises(ValueError):
            split_statements("SELECT 1")

    def test_files_are_ordered_and_versions_unique(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for name in ("002_b.sql", "001_a.sql", "notes.txt"):
                (root / name).write_text("SELECT 1;")
            self.assertEqual([version for version, _ in migration_files(root)], [1, 2])
            (root / "002_c.sql").write_text("SELECT 1;")
            with self.assertRaises(ValueError):
                migration_files(root)

    def test_project_migrations_parse(self):
        files = migration_files()
        self.assertEqual([version for version, _ in files][:2], [1, 2])
        for _, path in files:
            self.assertTrue(split_statements(path.read_text(encoding="utf-8")))


if __name__ == "__main__":
    unittest.main()
