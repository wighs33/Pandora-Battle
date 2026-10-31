"""실제 MySQL(labproject_test)에 붙는 통합 테스트.

모듈을 시작할 때 테이블을 모두 지우고 마이그레이션부터 다시 적용하고, 테스트마다 데이터 행을 비운다(상점 상품 시드는 남긴다).
LABPROJECT_TEST_DATABASE_URL(앱 계정)과 LABPROJECT_TEST_OWNER_DATABASE_URL(스키마 계정)이 있어야 돈다. local.ps1 test가 넣어 준다.
동시성 테스트는 여러 스레드가 함께 출발해 같은 행을 바꾸게 하고, 잠금 덕분에 결과가 한 번에 하나씩 처리한 것과 같은지 본다.
"""

import datetime
import itertools
import logging
import os
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor

from fastapi.testclient import TestClient
from sqlalchemy import exc, text

from app import boss_raid, matches, shop
from app.config import Settings
from app.db import create_db_engine, in_transaction, mysql_error_code
from app.errors import ApiError
from app.main import create_app
from app.migrate import migrate
from app.models import MatchReport, RaidDamageReport
from tests.fakes import FakeGameLift

APP_URL = os.environ.get("LABPROJECT_TEST_DATABASE_URL", "")
OWNER_URL = os.environ.get("LABPROJECT_TEST_OWNER_DATABASE_URL", "")
SERVER_KEY = "test-server-key"
SERVER_HEADERS = {"X-Server-Key": SERVER_KEY}
# 시드 마이그레이션(002)에 들어 있는 상품
SKIN = "SkinDefinition:DA_CowboyHat"
PANDORA = "PandoraDefinition:DA_Pandora_Fire"
# 외래 키 때문에 자식 표부터 지운다.
DATA_TABLES = (
    "gold_ledger",
    "player_pandoras",
    "player_skins",
    "match_players",
    "matches",
    "boss_raid_server_reports",
    "boss_raid_players",
    "boss_raids",
    "players",
)
ER_TABLEACCESS_DENIED = 1142

owner_engine = None
app = None
match_numbers = itertools.count(1)


def setUpModule():
    global owner_engine, app
    if not APP_URL or not OWNER_URL:
        raise unittest.SkipTest("LABPROJECT_TEST_DATABASE_URL and LABPROJECT_TEST_OWNER_DATABASE_URL are not set.")
    owner_engine = create_db_engine(OWNER_URL)
    with owner_engine.begin() as conn:
        conn.exec_driver_sql("SET FOREIGN_KEY_CHECKS = 0")
        tables = conn.exec_driver_sql(
            "SELECT table_name FROM information_schema.tables WHERE table_schema = DATABASE()"
        ).scalars().all()
        for table in tables:
            conn.exec_driver_sql("DROP TABLE `%s`" % table)
        conn.exec_driver_sql("SET FOREIGN_KEY_CHECKS = 1")
    migrate(owner_engine, log=lambda message: None)

    settings = Settings(
        database_url=APP_URL,
        token_secret=b"test-secret",
        server_key=SERVER_KEY,
        stage="test",
        allow_dev_login=True,
        gamelift_fleet_id="fleet-1",
    )
    app = create_app(settings, gamelift_client=FakeGameLift())
    # 요청마다 남는 INFO 로그가 테스트 결과를 가리지 않게 경고부터만 보인다.
    logging.getLogger("app").setLevel(logging.WARNING)


def tearDownModule():
    app.state.engine.dispose()
    owner_engine.dispose()


def player(account_id, result="win", **fields):
    display_name = account_id.split(":")[-1] or "Guest"
    return dict({"playerId": account_id, "displayName": display_name, "result": result}, **fields)


def run_concurrently(count, work):
    """count개 스레드가 함께 출발해 work(번호)를 실행한다. 결과나 예외를 번호 순서대로 돌려준다."""
    barrier = threading.Barrier(count)

    def run(index):
        barrier.wait()
        try:
            return work(index)
        except Exception as error:  # noqa: BLE001 - 예외도 결과로 모아 비교한다.
            return error

    with ThreadPoolExecutor(count) as pool:
        return list(pool.map(run, range(count)))


def owner_scalar(sql, **params):
    with owner_engine.connect() as conn:
        return conn.execute(text(sql), params).scalar_one()


class ApiTestCase(unittest.TestCase):
    def setUp(self):
        with owner_engine.begin() as conn:
            for table in DATA_TABLES:
                conn.exec_driver_sql("DELETE FROM %s" % table)
        app.state.gamelift = FakeGameLift(["gsess-1"])
        self.client = TestClient(app)
        self.engine = app.state.engine

    def tearDown(self):
        # 골드를 바꾼 경로가 무엇이든, 플레이어마다 골드 기록의 합계가 잔액과 같아야 한다.
        with owner_engine.connect() as conn:
            mismatched = conn.execute(
                text(
                    """
                    SELECT p.player_id, p.gold, COALESCE(SUM(g.amount), 0) AS ledger
                    FROM players AS p
                    LEFT JOIN gold_ledger AS g ON g.player_id = p.player_id
                    GROUP BY p.player_id, p.gold
                    HAVING p.gold <> ledger
                    """
                )
            ).all()
        self.assertEqual(mismatched, [])

    # 도우미 ----------------------------------------------------------------------------------------------------------

    def login(self, dev_id, name=None):
        response = self.client.post("/auth/dev", json={"devId": dev_id, "displayName": name or dev_id.title()})
        self.assertEqual(response.status_code, 200, response.text)
        return {"Authorization": "Bearer " + response.json()["token"]}

    def profile(self, headers):
        response = self.client.get("/player/me", headers=headers)
        self.assertEqual(response.status_code, 200, response.text)
        return response.json()

    def player_id(self, account_id):
        return owner_scalar(
            "SELECT player_id FROM players WHERE provider = 'dev' AND provider_user_id = :user_id",
            user_id=account_id.split(":")[1],
        )

    def report_match(self, match_id, players, **fields):
        body = dict({"matchId": match_id, "mapKey": "Colosseum", "endReason": "completed", "players": players}, **fields)
        return self.client.post("/server/match-result", json=body, headers=SERVER_HEADERS)

    def give_gold(self, account_id, amount):
        """골드는 보상으로만 생긴다. 그 금액을 보상으로 주는 경기를 하나 보고한다."""
        response = self.report_match("gift-%d" % next(match_numbers), [player(account_id, goldReward=amount)])
        self.assertTrue(response.json()["recorded"], response.text)

    def price(self, product_id):
        return owner_scalar("SELECT gold_price FROM shop_products WHERE product_id = :id", id=product_id)

    def purchase(self, headers, product_id):
        return self.client.post("/shop/purchase", json={"productId": product_id}, headers=headers)

    def in_transaction(self, work):
        return in_transaction(self.engine, work)

    def open_raid(self, hp=1000, reward_gold=500, daily_attempts=2, now=None):
        return self.in_transaction(lambda conn: boss_raid.open_raid(conn, hp, reward_gold, daily_attempts, 300, now))

    def report_damage(self, seq, players, server_id="server-1", raid_id=""):
        body = {"raidId": raid_id, "serverId": server_id, "seq": seq, "players": players}
        return self.client.post("/server/raid-damage", json=body, headers=SERVER_HEADERS)

    def raid_standing(self, headers):
        response = self.client.get("/bossraid/me", headers=headers)
        self.assertEqual(response.status_code, 200, response.text)
        return response.json()


class SchemaTests(ApiTestCase):
    def test_migrations_are_recorded_and_not_applied_twice(self):
        self.assertEqual(owner_scalar("SELECT GROUP_CONCAT(version ORDER BY version) FROM schema_migrations"), "1,2")
        applied = []
        migrate(owner_engine, log=applied.append)
        self.assertEqual(applied, [])

    def test_app_account_can_only_read_and_write_rows(self):
        for statement in ("DELETE FROM players", "DROP TABLE gold_ledger"):
            with self.assertRaises(exc.DatabaseError) as context:
                with self.engine.begin() as conn:
                    conn.exec_driver_sql(statement)
            self.assertEqual(mysql_error_code(context.exception), ER_TABLEACCESS_DENIED)


class AccountTests(ApiTestCase):
    def test_dev_login_creates_one_account_and_updates_its_name(self):
        self.login("alice", "Alice")
        headers = self.login("alice", "Alice 2")
        profile = self.profile(headers)
        self.assertEqual((profile["playerId"], profile["displayName"], profile["gold"]), ("dev:alice", "Alice 2", 0))
        self.assertEqual(owner_scalar("SELECT COUNT(*) FROM players"), 1)

    def test_login_ids_are_case_sensitive(self):
        self.login("alice")
        self.login("Alice")
        self.assertEqual(owner_scalar("SELECT COUNT(*) FROM players"), 2)

    def test_requests_need_a_valid_token(self):
        response = self.client.get("/player/me")
        self.assertEqual((response.status_code, response.json()["error"]), (401, "missing_token"))
        response = self.client.get("/player/me", headers={"Authorization": "Bearer a.b.c"})
        self.assertEqual((response.status_code, response.json()["error"]), (401, "invalid_token"))

    def test_errors_share_one_format(self):
        response = self.client.post("/auth/dev", json={"devId": "no spaces!"})
        self.assertEqual((response.status_code, response.json()["error"]), (400, "invalid_request"))
        self.assertIn("devId", response.json()["message"])
        response = self.client.get("/missing")
        self.assertEqual((response.status_code, response.json()["error"]), (404, "not_found"))


class MatchResultTests(ApiTestCase):
    def test_server_key_is_required(self):
        body = {"matchId": "match-1", "endReason": "completed", "players": [player("dev:alice")]}
        for headers in ({}, {"X-Server-Key": "wrong"}):
            response = self.client.post("/server/match-result", json=body, headers=headers)
            self.assertEqual((response.status_code, response.json()["error"]), (401, "invalid_server_key"))

    def test_records_stats_history_and_rewards(self):
        alice = self.login("alice")
        bob = self.login("bob")
        response = self.report_match("match-1", [
            player("dev:alice", "win", team=0, kills=3, deaths=1, goldReward=50),
            player("dev:bob", "lose", team=1, kills=1, deaths=3, goldReward=20),
            player("", "lose", team=1),
        ])
        self.assertEqual(response.json(), {"recorded": True, "duplicate": False})

        profile = self.profile(alice)
        self.assertEqual(profile["stats"], {"matches": 1, "wins": 1, "losses": 0, "draws": 0, "kills": 3, "deaths": 1})
        self.assertEqual(profile["gold"], 50)
        recent = profile["recentMatches"][0]
        self.assertEqual(
            (recent["matchId"], recent["result"], recent["mapKey"], recent["kills"]), ("match-1", "win", "Colosseum", 3)
        )
        self.assertEqual(self.profile(bob)["gold"], 20)
        self.assertEqual(owner_scalar("SELECT COUNT(*) FROM match_players WHERE player_id IS NULL"), 1)

    def test_duplicate_report_changes_nothing(self):
        alice = self.login("alice")
        players = [player("dev:alice", "win", kills=2, goldReward=50)]
        self.report_match("match-1", players)
        response = self.report_match("match-1", players)
        self.assertEqual(response.json(), {"recorded": False, "duplicate": True})
        profile = self.profile(alice)
        self.assertEqual((profile["stats"]["matches"], profile["stats"]["kills"], profile["gold"]), (1, 2, 50))

    def test_account_that_never_logged_in_is_kept_as_guest(self):
        with self.assertLogs("app.matches", "WARNING") as logs:
            response = self.report_match("match-1", [player("dev:ghost", goldReward=50)])
        self.assertTrue(response.json()["recorded"])
        self.assertIn("unknown account dev:ghost", logs.output[0])
        self.assertEqual(owner_scalar("SELECT COUNT(*) FROM players"), 0)
        self.assertEqual(owner_scalar("SELECT COUNT(*) FROM match_players WHERE player_id IS NULL"), 1)

    def test_recent_matches_are_newest_first_and_stats_add_up(self):
        alice = self.login("alice")
        for match_id, result in (("match-1", "win"), ("match-2", "lose"), ("match-3", "draw")):
            self.report_match(match_id, [player("dev:alice", result, kills=1)])
        profile = self.profile(alice)
        self.assertEqual([match["matchId"] for match in profile["recentMatches"]], ["match-3", "match-2", "match-1"])
        self.assertEqual(profile["stats"], {"matches": 3, "wins": 1, "losses": 1, "draws": 1, "kills": 3, "deaths": 0})

    def test_concurrent_reports_sharing_players_all_count(self):
        alice = self.login("alice")
        bob = self.login("bob")

        def report(index):
            pair = [player("dev:alice", goldReward=10), player("dev:bob", "lose", goldReward=10)]
            # 보고마다 플레이어 순서를 바꿔도 서버는 player_id 순서로 잠근다.
            players = pair if index % 2 == 0 else pair[::-1]
            body = {"matchId": "match-%d" % index, "endReason": "completed", "players": players}
            return self.in_transaction(lambda conn: matches.record_match(conn, MatchReport.model_validate(body)))

        self.assertEqual(run_concurrently(6, report), [True] * 6)
        self.assertEqual((self.profile(alice)["gold"], self.profile(bob)["gold"]), (60, 60))
        self.assertEqual(self.profile(alice)["stats"]["matches"], 6)


class ShopTests(ApiTestCase):
    def test_purchase_spends_gold_and_grants_the_product(self):
        alice = self.login("alice")
        start = self.price(SKIN) + self.price(PANDORA) + 50
        self.give_gold("dev:alice", start)

        response = self.purchase(alice, SKIN)
        self.assertEqual(response.json(), {"productId": SKIN, "gold": start - self.price(SKIN)})
        self.assertEqual(self.purchase(alice, PANDORA).status_code, 200)

        profile = self.profile(alice)
        self.assertEqual(profile["gold"], 50)
        self.assertEqual(profile["skins"], [SKIN])
        self.assertEqual(profile["pandoras"], [{"pandoraId": PANDORA, "level": 1}])

    def test_rejected_purchases_change_nothing(self):
        alice = self.login("alice")
        self.give_gold("dev:alice", self.price(SKIN) - 1)
        response = self.purchase(alice, SKIN)
        self.assertEqual((response.status_code, response.json()["error"]), (409, "not_enough_gold"))
        self.assertEqual(self.profile(alice)["skins"], [])

        self.give_gold("dev:alice", 1)
        self.assertEqual(self.purchase(alice, SKIN).status_code, 200)
        response = self.purchase(alice, SKIN)
        self.assertEqual((response.status_code, response.json()["error"]), (409, "already_owned"))
        self.assertEqual(self.profile(alice)["gold"], 0)

        response = self.purchase(alice, "SkinDefinition:DA_Missing")
        self.assertEqual((response.status_code, response.json()["error"]), (404, "product_not_found"))
        response = self.purchase(alice, "Skin:DA_CowboyHat")
        self.assertEqual((response.status_code, response.json()["error"]), (400, "invalid_request"))

    def test_concurrent_purchases_cannot_overspend(self):
        alice = self.login("alice")
        price = self.price(SKIN)
        with owner_engine.connect() as conn:
            products = conn.execute(
                text(
                    """
                    SELECT product_id FROM shop_products
                    WHERE gold_price = :price AND on_sale
                    ORDER BY product_id
                    LIMIT 8
                    """
                ),
                {"price": price},
            ).scalars().all()
        self.give_gold("dev:alice", price)
        player_id = self.player_id("dev:alice")

        def buy(index):
            return self.in_transaction(lambda conn: shop.purchase(conn, player_id, products[index]))

        results = run_concurrently(len(products), buy)
        self.assertEqual(sum(isinstance(result, dict) for result in results), 1)
        self.assertEqual({result.code for result in results if isinstance(result, ApiError)}, {"not_enough_gold"})
        profile = self.profile(alice)
        self.assertEqual((profile["gold"], len(profile["skins"]) + len(profile["pandoras"])), (0, 1))

    def test_concurrent_purchases_of_one_product_charge_once(self):
        alice = self.login("alice")
        self.give_gold("dev:alice", 3 * self.price(SKIN))
        player_id = self.player_id("dev:alice")

        def buy(index):
            return self.in_transaction(lambda conn: shop.purchase(conn, player_id, SKIN))

        results = run_concurrently(8, buy)
        self.assertEqual(sum(isinstance(result, dict) for result in results), 1)
        self.assertEqual({result.code for result in results if isinstance(result, ApiError)}, {"already_owned"})
        self.assertEqual(self.profile(alice)["gold"], 2 * self.price(SKIN))


class BossRaidTests(ApiTestCase):
    def join_raid(self, headers):
        return self.client.post("/match/join", json={"mode": "bossraid"}, headers=headers)

    def test_status_follows_open_close_and_defeat(self):
        self.assertEqual(self.client.get("/bossraid/status").json(), {"open": False, "raid": None})
        raid_id = self.open_raid(hp=100)
        status = self.client.get("/bossraid/status").json()
        self.assertEqual((status["open"], status["raid"]["raidId"], status["raid"]["hp"]), (True, raid_id, 100))

        self.in_transaction(lambda conn: boss_raid.set_entry_open(conn, False))
        self.assertFalse(self.client.get("/bossraid/status").json()["open"])
        self.in_transaction(lambda conn: boss_raid.set_entry_open(conn, True))
        self.assertTrue(self.client.get("/bossraid/status").json()["open"])

        self.report_damage(1, [{"playerId": "", "damage": 100}])
        status = self.client.get("/bossraid/status").json()
        self.assertEqual((status["open"], status["raid"]["defeated"], status["raid"]["hp"]), (False, True, 0))
        with self.assertRaises(ValueError):
            self.in_transaction(lambda conn: boss_raid.set_entry_open(conn, True))

    def test_new_raid_replaces_the_current_one(self):
        now = datetime.datetime(2026, 10, 31, tzinfo=datetime.timezone.utc).timestamp()
        first = self.open_raid(now=now)
        second = self.open_raid(now=now + 60)
        self.assertEqual(self.client.get("/bossraid/status").json()["raid"]["raidId"], second)
        self.assertFalse(owner_scalar("SELECT entry_open FROM boss_raids WHERE raid_id = :id", id=first))

    def test_join_uses_daily_attempts_and_refunds_failed_joins(self):
        alice = self.login("alice")
        self.open_raid(daily_attempts=2)

        app.state.gamelift = FakeGameLift(statuses=("ERROR",))
        response = self.join_raid(alice)
        self.assertEqual((response.status_code, response.json()["error"]), (503, "session_failed"))
        self.assertEqual(self.raid_standing(alice)["attemptsLeft"], 2)

        gamelift = app.state.gamelift = FakeGameLift(["gsess-raid"])
        first = self.join_raid(alice).json()
        self.assertEqual((first["playerSessionId"], first["attemptsLeft"]), ("psess-gsess-raid", 1))
        self.assertEqual(gamelift.player_sessions[0][1], "dev:alice")
        self.assertEqual(self.join_raid(alice).json()["attemptsLeft"], 0)
        response = self.join_raid(alice)
        self.assertEqual((response.status_code, response.json()["error"]), (403, "no_attempts_left"))
        self.assertEqual(self.raid_standing(alice)["attemptsLeft"], 0)

    def test_closed_or_missing_raid_rejects_joins(self):
        alice = self.login("alice")
        response = self.join_raid(alice)
        self.assertEqual((response.status_code, response.json()["error"]), (403, "bossraid_closed"))
        self.open_raid()
        self.in_transaction(lambda conn: boss_raid.set_entry_open(conn, False))
        response = self.join_raid(alice)
        self.assertEqual((response.status_code, response.json()["error"]), (403, "bossraid_closed"))

    def test_attempts_refill_on_a_new_day(self):
        self.login("alice")
        self.open_raid(daily_attempts=1)
        player_id = self.player_id("dev:alice")
        day = datetime.date(2026, 10, 30)

        def consume(today):
            def work(conn):
                return boss_raid.consume_attempt(conn, boss_raid.current_raid(conn), player_id, today)

            return self.in_transaction(work)

        self.assertEqual(consume(day), 0)
        with self.assertRaises(ApiError) as context:
            consume(day)
        self.assertEqual(context.exception.code, "no_attempts_left")
        self.assertEqual(consume(day + datetime.timedelta(days=1)), 0)

    def test_damage_reports_are_applied_once_and_ranked(self):
        alice, bob, carol, dave = (self.login(name) for name in ("alice", "bob", "carol", "dave"))
        self.open_raid(hp=10_000)

        response = self.report_damage(1, [
            {"playerId": "dev:alice", "damage": 300},
            {"playerId": "dev:bob", "damage": 500},
            {"playerId": "", "damage": 100},
        ]).json()
        self.assertEqual((response["recorded"], response["hp"]), (True, 9100))
        response = self.report_damage(1, [{"playerId": "dev:alice", "damage": 300}]).json()
        self.assertEqual((response["recorded"], response["hp"]), (False, 9100))
        self.report_damage(2, [{"playerId": "dev:alice", "damage": 300}])
        # 다른 서버는 순번을 따로 센다.
        self.report_damage(1, [{"playerId": "dev:carol", "damage": 500}], server_id="server-2")

        standing = self.raid_standing(alice)
        self.assertEqual((standing["damage"], standing["rank"], standing["raid"]["hp"]), (600, 1, 8300))
        self.assertEqual(standing["top"], [
            {"rank": 1, "name": "Alice", "damage": 600},
            {"rank": 2, "name": "Bob", "damage": 500},
            {"rank": 2, "name": "Carol", "damage": 500},
        ])
        self.assertEqual(self.raid_standing(carol)["rank"], 2)
        self.assertEqual((self.raid_standing(dave)["damage"], self.raid_standing(dave)["rank"]), (0, 0))

    def test_empty_report_reads_hp_and_unknown_raid_is_rejected(self):
        response = self.report_damage(1, [])
        self.assertEqual((response.status_code, response.json()["error"]), (404, "raid_not_found"))
        raid_id = self.open_raid(hp=1000)
        response = self.report_damage(1, []).json()
        self.assertEqual((response["raidId"], response["hp"], response["recorded"]), (raid_id, 1000, False))
        response = self.report_damage(1, [{"playerId": "", "damage": 1}], raid_id="missing")
        self.assertEqual((response.status_code, response.json()["error"]), (404, "raid_not_found"))

    def test_concurrent_final_blows_stop_at_zero_and_keep_all_damage(self):
        names = ["p%d" % index for index in range(5)]
        for name in names:
            self.login(name)
        self.open_raid(hp=1000)

        def report(index):
            players = [{"playerId": "dev:" + names[index % 5], "damage": 200}]
            body = RaidDamageReport.model_validate({"serverId": "server-%d" % index, "seq": 1, "players": players})
            raid, recorded = self.in_transaction(lambda conn: boss_raid.record_damage(conn, body))
            return recorded

        self.assertEqual(run_concurrently(10, report), [True] * 10)
        status = self.client.get("/bossraid/status").json()["raid"]
        self.assertEqual((status["hp"], status["defeated"]), (0, True))
        self.assertEqual(owner_scalar("SELECT SUM(damage) FROM boss_raid_players"), 2000)

    def test_rewards_are_paid_once_after_defeat(self):
        alice = self.login("alice")
        bob = self.login("bob")
        raid_id = self.open_raid(hp=100, reward_gold=500)

        self.report_damage(1, [{"playerId": "dev:alice", "damage": 40}])
        before_defeat = self.client.post("/bossraid/claim", headers=alice).json()
        self.assertEqual(before_defeat, {"gold": 0, "raids": [], "balance": 0})
        self.report_damage(2, [{"playerId": "dev:alice", "damage": 60}])

        claim = self.client.post("/bossraid/claim", headers=alice).json()
        self.assertEqual(claim, {"gold": 500, "raids": [raid_id], "balance": 500})
        self.assertEqual(self.client.post("/bossraid/claim", headers=alice).json()["gold"], 0)
        self.assertEqual(self.client.post("/bossraid/claim", headers=bob).json()["gold"], 0)
        self.assertEqual(self.profile(alice)["gold"], 500)

    def test_concurrent_claims_pay_once(self):
        alice = self.login("alice")
        self.open_raid(hp=100, reward_gold=500)
        self.report_damage(1, [{"playerId": "dev:alice", "damage": 100}])
        player_id = self.player_id("dev:alice")

        def claim(index):
            return self.in_transaction(lambda conn: boss_raid.claim_rewards(conn, player_id))

        results = run_concurrently(8, claim)
        self.assertEqual(sorted(result["gold"] for result in results), [0] * 7 + [500])
        self.assertEqual(self.profile(alice)["gold"], 500)


if __name__ == "__main__":
    unittest.main()
