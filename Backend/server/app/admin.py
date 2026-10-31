"""운영 명령: 보스 레이드 회차를 열고 닫는다. Lambda 백엔드의 Backend/bossraid.ps1과 같은 일을 MySQL에 한다.

    python -m app.admin raid status
    python -m app.admin raid open --hp 3000000 --reward-gold 1000 [--daily-attempts 3] [--attempt-seconds 300]
    python -m app.admin raid close      새 참가만 막는다. 들어간 플레이어는 도전 시간이 끝날 때까지 계속한다.
    python -m app.admin raid reopen     닫은 회차를 다시 연다(격파 전만).
"""

import argparse

from . import boss_raid
from .config import required_env
from .db import create_db_engine, in_transaction


def print_status(conn):
    raid = boss_raid.current_raid(conn)
    if raid is None:
        print("No boss raid has been opened yet.")
        return
    summary = "defeated" if raid.defeated_at else ("open" if raid.entry_open else "closed")
    print("Boss raid %s is %s." % (raid.raid_id, summary))
    print("  Health   {:,} / {:,} ({:.1f}%)".format(raid.hp, raid.max_hp, 100.0 * raid.hp / raid.max_hp))
    print("  Attempts {} per day, {} seconds each, reward {:,} gold".format(
        raid.daily_attempts, raid.attempt_seconds, raid.reward_gold))


def main():
    parser = argparse.ArgumentParser(description="LabProject operations.")
    parser.add_argument("target", choices=["raid"])
    parser.add_argument("action", choices=["status", "open", "close", "reopen"])
    parser.add_argument("--hp", type=int, help="새 회차의 체력(open)")
    parser.add_argument("--reward-gold", type=int, help="격파하면 딜을 넣은 플레이어 모두가 받는 골드(open)")
    parser.add_argument("--daily-attempts", type=int, default=3)
    parser.add_argument("--attempt-seconds", type=int, default=300)
    args = parser.parse_args()

    engine = create_db_engine(required_env("DATABASE_URL"))
    if args.action == "open":
        if args.hp is None or args.reward_gold is None:
            parser.error("open needs --hp and --reward-gold.")
        if args.hp < 1 or args.reward_gold < 0 or args.daily_attempts < 1 or args.attempt_seconds < 1:
            parser.error("--hp, --daily-attempts and --attempt-seconds must be at least 1, --reward-gold at least 0.")
        in_transaction(engine, lambda conn: boss_raid.open_raid(
            conn, args.hp, args.reward_gold, args.daily_attempts, args.attempt_seconds))
    elif args.action in ("close", "reopen"):
        try:
            in_transaction(engine, lambda conn: boss_raid.set_entry_open(conn, args.action == "reopen"))
        except ValueError as error:
            parser.exit(1, "%s\n" % error)
    in_transaction(engine, print_status)
    engine.dispose()


if __name__ == "__main__":
    main()
