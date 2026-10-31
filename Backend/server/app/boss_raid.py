"""보스 레이드(월드 보스): 회차, 하루 도전 횟수, 전역 체력, 순위, 보상.

운영자가 회차를 열면(python -m app.admin raid open) 모든 게임 서버가 hp 하나를 함께 깎는다.
플레이어는 하루 정해진 횟수만큼 도전하고(/match/join이 들어갈 때마다 한 번씩 쓴다), 회차 동안 넣은 딜로 순위를 매긴다.
보스가 쓰러지면 그 회차에 딜을 넣은 플레이어 모두가 보상 골드를 한 번 받는다.
"""

import datetime
import time

from sqlalchemy import text

from .errors import ApiError
from .players import credit_gold, find_player_ids

RANKING_SIZE = 10
# 하루 도전 횟수는 한국 시간 자정에 다시 채워진다.
ATTEMPT_DAY_UTC_OFFSET = datetime.timedelta(hours=9)

_RAID_COLUMNS = "raid_id, max_hp, hp, daily_attempts, attempt_seconds, reward_gold, entry_open, defeated_at"


def attempt_day(now=None):
    """도전 횟수를 세는 날짜(한국 시간 기준)."""
    current = datetime.datetime.fromtimestamp(time.time() if now is None else now, datetime.timezone.utc)
    return (current + ATTEMPT_DAY_UTC_OFFSET).date()


# 회차 -----------------------------------------------------------------------------------------------------------------

def raid_view(raid):
    return {
        "raidId": raid.raid_id,
        "hp": raid.hp,
        "maxHp": raid.max_hp,
        "defeated": raid.defeated_at is not None,
        "dailyAttempts": raid.daily_attempts,
        "attemptSeconds": raid.attempt_seconds,
        "rewardGold": raid.reward_gold,
    }


def is_accepting(raid):
    """닫혀 있거나 이미 격파된 회차는 새 참가를 받지 않는다. 회차 정보는 그 뒤에도 순위와 보상 안내에 쓴다."""
    return bool(raid.entry_open) and raid.defeated_at is None


def current_raid(conn):
    """가장 최근에 연 회차. 연 적이 없으면 None."""
    return conn.execute(
        text("SELECT %s FROM boss_raids ORDER BY opened_at DESC, raid_id DESC LIMIT 1" % _RAID_COLUMNS)
    ).one_or_none()


def load_raid(conn, raid_id, lock=False):
    return conn.execute(
        text("SELECT %s FROM boss_raids WHERE raid_id = :raid_id%s" % (_RAID_COLUMNS, " FOR UPDATE" if lock else "")),
        {"raid_id": raid_id},
    ).one_or_none()


def open_raid(conn, max_hp, reward_gold, daily_attempts, attempt_seconds, now=None):
    """체력이 가득 찬 새 회차를 열고 지금 회차로 만든다. 회차 ID는 연 시각(UTC)이다."""
    opened = datetime.datetime.fromtimestamp(time.time() if now is None else now, datetime.timezone.utc)
    raid_id = opened.strftime("%Y%m%d-%H%M%S")
    conn.execute(
        text(
            """
            INSERT INTO boss_raids
                (raid_id, max_hp, hp, daily_attempts, attempt_seconds, reward_gold, entry_open, opened_at)
            VALUES (:raid_id, :max_hp, :max_hp, :daily_attempts, :attempt_seconds, :reward_gold, TRUE, :opened_at)
            """
        ),
        {
            "raid_id": raid_id,
            "max_hp": max_hp,
            "daily_attempts": daily_attempts,
            "attempt_seconds": attempt_seconds,
            "reward_gold": reward_gold,
            "opened_at": opened.replace(tzinfo=None),
        },
    )
    # 참가를 받을 수 있는 회차는 지금 회차 하나뿐이다.
    conn.execute(
        text("UPDATE boss_raids SET entry_open = FALSE WHERE raid_id <> :raid_id AND entry_open"), {"raid_id": raid_id}
    )
    return raid_id


def set_entry_open(conn, entry_open):
    """지금 회차의 새 참가를 열거나 닫는다. 이미 들어간 플레이어는 도전 시간이 끝날 때까지 계속한다."""
    raid = current_raid(conn)
    if raid is None:
        raise ValueError("No raid has been opened yet.")
    if entry_open and raid.defeated_at is not None:
        raise ValueError("Raid %s was already defeated. Open a new one." % raid.raid_id)
    conn.execute(
        text("UPDATE boss_raids SET entry_open = :entry_open WHERE raid_id = :raid_id"),
        {"entry_open": entry_open, "raid_id": raid.raid_id},
    )
    return raid.raid_id


# 하루 도전 횟수 ------------------------------------------------------------------------------------------------------

def attempts_left(raid, row, today):
    used = row.attempts_today if row is not None and row.attempt_day == today else 0
    return max(0, raid.daily_attempts - used)


def consume_attempt(conn, raid, player_id, today):
    """오늘 도전 1회를 쓰고 남은 횟수를 돌려준다. 다 썼으면 403 no_attempts_left(트랜잭션이 되돌아가 횟수도 그대로다).

    INSERT ... ON DUPLICATE KEY UPDATE 한 문장이 행을 만들거나 잠근 채 고치므로, 같은 플레이어의 동시 요청은 차례로 처리된다.
    """
    conn.execute(
        text(
            """
            INSERT INTO boss_raid_players (raid_id, player_id, attempt_day, attempts_today)
            VALUES (:raid_id, :player_id, :today, 1) AS new
            ON DUPLICATE KEY UPDATE
                -- MySQL은 SET을 왼쪽부터 적용한다. attempt_day를 바꾸기 전에 횟수를 계산해야 날이 바뀐 것을 안다.
                attempts_today = IF(boss_raid_players.attempt_day <=> new.attempt_day,
                                    boss_raid_players.attempts_today + 1, 1),
                attempt_day = new.attempt_day
            """
        ),
        {"raid_id": raid.raid_id, "player_id": player_id, "today": today},
    )
    used = conn.execute(
        text("SELECT attempts_today FROM boss_raid_players WHERE raid_id = :raid_id AND player_id = :player_id"),
        {"raid_id": raid.raid_id, "player_id": player_id},
    ).scalar_one()
    if used > raid.daily_attempts:
        raise ApiError(403, "no_attempts_left", "No boss raid attempts left today.")
    return raid.daily_attempts - used


def refund_attempt(conn, raid_id, player_id, today):
    """자리를 잡지 못해 들어가지 못한 도전을 돌려준다. 그사이 날짜가 바뀌었으면 돌려줄 것이 없다."""
    conn.execute(
        text(
            """
            UPDATE boss_raid_players SET attempts_today = attempts_today - 1
            WHERE raid_id = :raid_id AND player_id = :player_id AND attempt_day = :today AND attempts_today > 0
            """
        ),
        {"raid_id": raid_id, "player_id": player_id, "today": today},
    )


# 순위 ----------------------------------------------------------------------------------------------------------------

def standing(conn, raid, player_id, today):
    """오늘 남은 도전 횟수, 내 누적 딜과 순위, 상위 순위. 딜이 같으면 같은 순위다."""
    params = {"raid_id": raid.raid_id, "player_id": player_id}
    row = conn.execute(
        text(
            """
            SELECT damage, attempt_day, attempts_today FROM boss_raid_players
            WHERE raid_id = :raid_id AND player_id = :player_id
            """
        ),
        params,
    ).one_or_none()
    damage = row.damage if row is not None else 0
    # 딜을 넣기 전에는 순위가 없다(0).
    rank = 0
    if damage > 0:
        rank = 1 + conn.execute(
            text("SELECT COUNT(*) FROM boss_raid_players WHERE raid_id = :raid_id AND damage > :damage"),
            {"raid_id": raid.raid_id, "damage": damage},
        ).scalar_one()
    top = conn.execute(
        text(
            """
            SELECT RANK() OVER (ORDER BY r.damage DESC) AS ranking, p.display_name, r.damage
            FROM boss_raid_players AS r
            JOIN players AS p ON p.player_id = r.player_id
            WHERE r.raid_id = :raid_id AND r.damage > 0
            ORDER BY r.damage DESC, r.player_id
            LIMIT :size
            """
        ),
        {"raid_id": raid.raid_id, "size": RANKING_SIZE},
    )
    return {
        "raid": raid_view(raid),
        "attemptsLeft": attempts_left(raid, row, today),
        "damage": damage,
        "rank": rank,
        "top": [{"rank": entry.ranking, "name": entry.display_name, "damage": entry.damage} for entry in top],
    }


# 딜 보고 -------------------------------------------------------------------------------------------------------------

def record_damage(conn, report):
    """서버가 보고한 딜로 전역 체력을 깎고 플레이어별 누적 딜을 더한다. (반영 후 회차, 반영 여부)를 돌려준다.

    회차 행을 잠그고 시작하므로 같은 회차의 보고는 한 번에 하나씩 처리된다. 그래서 체력을 읽고 계산해 쓰는 사이에
    다른 서버의 딜이 끼어들지 않고, 마지막 일격이 여러 서버에서 동시에 와도 체력은 0에서 멈추며 격파는 한 번만 기록된다.
    - 서버마다 보고 순번(seq)을 늘려 보낸다. 이미 받은 순번은 다시 반영하지 않는다(재전송 대비).
    - 격파된 뒤에 도착한 딜도 누적 딜에는 더한다. 서버가 격파를 알기 전에 실제로 넣은 딜이기 때문이다.
    - 딜이 없는 보고는 지금 체력만 묻는 것이므로 아무것도 쓰지 않는다.
    """
    raid_id = report.raid_id
    if not raid_id:
        latest = current_raid(conn)
        if latest is None:
            raise ApiError(404, "raid_not_found", "No boss raid has been opened.")
        raid_id = latest.raid_id

    total_damage = sum(player.damage for player in report.players)
    raid = load_raid(conn, raid_id, lock=total_damage > 0)
    if raid is None:
        raise ApiError(404, "raid_not_found", "Boss raid %s does not exist." % raid_id)
    if total_damage == 0:
        return raid, False

    key = {"raid_id": raid_id, "server_id": report.server_id}
    last_seq = conn.execute(
        text(
            """
            SELECT last_seq FROM boss_raid_server_reports
            WHERE raid_id = :raid_id AND server_id = :server_id
            FOR UPDATE
            """
        ),
        key,
    ).scalar_one_or_none()
    if last_seq is not None and last_seq >= report.seq:
        return raid, False
    conn.execute(
        text(
            """
            INSERT INTO boss_raid_server_reports (raid_id, server_id, last_seq)
            VALUES (:raid_id, :server_id, :seq) AS new
            ON DUPLICATE KEY UPDATE last_seq = new.last_seq
            """
        ),
        dict(key, seq=report.seq),
    )

    hp = max(0, raid.hp - total_damage)
    conn.execute(
        text(
            """
            UPDATE boss_raids
            SET hp = :hp, defeated_at = CASE WHEN :hp = 0 AND defeated_at IS NULL THEN NOW(3) ELSE defeated_at END
            WHERE raid_id = :raid_id
            """
        ),
        {"hp": hp, "raid_id": raid_id},
    )

    player_ids = find_player_ids(conn, [player.player_id for player in report.players if player.player_id])
    damage_by_player = sorted(
        (player_ids[player.player_id], player.damage)
        for player in report.players
        if player.player_id in player_ids and player.damage > 0
    )
    for player_id, damage in damage_by_player:
        conn.execute(
            text(
                """
                INSERT INTO boss_raid_players (raid_id, player_id, damage) VALUES (:raid_id, :player_id, :damage) AS new
                ON DUPLICATE KEY UPDATE damage = boss_raid_players.damage + new.damage
                """
            ),
            {"raid_id": raid_id, "player_id": player_id, "damage": damage},
        )
    return load_raid(conn, raid_id), True


# 보상 ----------------------------------------------------------------------------------------------------------------

def claim_rewards(conn, player_id):
    """딜을 넣은 회차 중 격파됐고 아직 받지 않은 회차의 보상 골드를 준다. 회차마다 한 번만 준다."""
    # 플레이어 행부터 잠근다(players.py 설명). 같은 플레이어의 동시 요청은 여기서 차례를 기다린다.
    conn.execute(
        text("SELECT player_id FROM players WHERE player_id = :player_id FOR UPDATE"), {"player_id": player_id}
    )
    # 잠금 읽기라서 먼저 끝난 요청이 남긴 수령 시각을 본다. 같은 보상을 두 번 주지 않는다.
    unclaimed = conn.execute(
        text(
            """
            SELECT r.raid_id, r.reward_gold
            FROM boss_raid_players AS p
            JOIN boss_raids AS r ON r.raid_id = p.raid_id
            WHERE p.player_id = :player_id AND p.damage > 0 AND p.reward_claimed_at IS NULL
              AND r.defeated_at IS NOT NULL
            ORDER BY r.raid_id
            FOR UPDATE OF p
            """
        ),
        {"player_id": player_id},
    ).all()
    for raid in unclaimed:
        conn.execute(
            text(
                """
                UPDATE boss_raid_players SET reward_claimed_at = NOW(3)
                WHERE raid_id = :raid_id AND player_id = :player_id
                """
            ),
            {"raid_id": raid.raid_id, "player_id": player_id},
        )
        credit_gold(conn, player_id, raid.reward_gold, "raid_reward", raid.raid_id)
    return {"gold": sum(raid.reward_gold for raid in unclaimed), "raids": [raid.raid_id for raid in unclaimed]}
