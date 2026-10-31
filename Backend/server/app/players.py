"""플레이어 계정, 프로필, 골드.

로그인 ID("steam:<SteamID64>", "dev:<이름>")는 바깥(토큰, GameLift, 전용 서버 보고)에서 쓰는 ID이고,
DB 안에서는 players.player_id를 외래 키로 쓴다. 로그인 ID는 (provider, provider_user_id) 유일 키로 찾는다.

재화를 바꾸는 트랜잭션은 플레이어 행부터 잠근다(UPDATE players). 보유 목록이나 기록 행을 먼저 넣으면 그 외래 키 검사가
플레이어 행에 공유 잠금을 걸어, 같은 플레이어를 바꾸는 두 트랜잭션이 서로의 공유 잠금을 기다리며 교착된다.
여러 플레이어를 바꿀 때는 player_id 오름차순으로 잠근다.
"""

from collections import defaultdict
from dataclasses import dataclass

from sqlalchemy import bindparam, text

RECENT_MATCH_LIMIT = 20


@dataclass(frozen=True)
class Player:
    player_id: int
    account_id: str
    display_name: str


def split_account_id(account_id):
    provider, _, provider_user_id = account_id.partition(":")
    return provider, provider_user_id


def make_account_id(provider, provider_user_id):
    return provider + ":" + provider_user_id


def upsert_login(conn, provider, provider_user_id, display_name):
    """로그인할 때마다 표시 이름과 로그인 시각을 갱신하고, 처음이면 계정을 만든다. player_id를 돌려준다."""
    params = {"provider": provider, "user_id": provider_user_id, "name": display_name}
    conn.execute(
        text(
            """
            INSERT INTO players (provider, provider_user_id, display_name, last_login_at)
            VALUES (:provider, :user_id, :name, NOW(3)) AS new
            ON DUPLICATE KEY UPDATE display_name = new.display_name, last_login_at = new.last_login_at
            """
        ),
        params,
    )
    return conn.execute(
        text("SELECT player_id FROM players WHERE provider = :provider AND provider_user_id = :user_id"), params
    ).scalar_one()


def find_player(conn, account_id):
    provider, provider_user_id = split_account_id(account_id)
    row = conn.execute(
        text(
            "SELECT player_id, display_name FROM players WHERE provider = :provider AND provider_user_id = :user_id"
        ),
        {"provider": provider, "user_id": provider_user_id},
    ).one_or_none()
    return Player(row.player_id, account_id, row.display_name) if row else None


_FIND_PLAYER_IDS = text(
    "SELECT player_id, provider_user_id FROM players WHERE provider = :provider AND provider_user_id IN :user_ids"
).bindparams(bindparam("user_ids", expanding=True))


def find_player_ids(conn, account_ids):
    """로그인 ID 목록을 {로그인 ID: player_id}로 바꾼다. 계정이 없는 ID는 결과에 없다."""
    user_ids_by_provider = defaultdict(list)
    for account_id in account_ids:
        provider, provider_user_id = split_account_id(account_id)
        user_ids_by_provider[provider].append(provider_user_id)

    found = {}
    for provider, user_ids in user_ids_by_provider.items():
        for row in conn.execute(_FIND_PLAYER_IDS, {"provider": provider, "user_ids": user_ids}):
            found[make_account_id(provider, row.provider_user_id)] = row.player_id
    return found


def _add_ledger_entry(conn, player_id, amount, reason, reference_id):
    # 같은 (사유, 참조 ID)로 두 번 넣으면 유일 키가 막고, 트랜잭션 전체가 되돌아간다.
    conn.execute(
        text(
            """
            INSERT INTO gold_ledger (player_id, amount, reason, reference_id)
            VALUES (:player_id, :amount, :reason, :reference_id)
            """
        ),
        {"player_id": player_id, "amount": amount, "reason": reason, "reference_id": reference_id},
    )


def credit_gold(conn, player_id, amount, reason, reference_id):
    """골드를 더하고 같은 금액의 기록을 남긴다."""
    conn.execute(
        text("UPDATE players SET gold = gold + :amount WHERE player_id = :player_id"),
        {"amount": amount, "player_id": player_id},
    )
    _add_ledger_entry(conn, player_id, amount, reason, reference_id)


def spend_gold(conn, player_id, amount, reason, reference_id):
    """골드가 충분하면 빼고 기록을 남긴 뒤 True. 모자라면 아무것도 바꾸지 않고 False."""
    spent = conn.execute(
        text("UPDATE players SET gold = gold - :amount WHERE player_id = :player_id AND gold >= :amount"),
        {"amount": amount, "player_id": player_id},
    ).rowcount
    if spent == 0:
        return False
    _add_ledger_entry(conn, player_id, -amount, reason, reference_id)
    return True


def current_gold(conn, player_id):
    return conn.execute(
        text("SELECT gold FROM players WHERE player_id = :player_id"), {"player_id": player_id}
    ).scalar_one()


def load_profile(conn, player):
    """프로필, 골드, 보유 목록, 누적 전적과 최근 경기. 누적 전적은 저장하지 않고 경기 참가 기록을 집계한다."""
    params = {"player_id": player.player_id}
    profile = conn.execute(
        text("SELECT display_name, gold FROM players WHERE player_id = :player_id"), params
    ).one()
    stats = conn.execute(
        text(
            """
            SELECT COUNT(*) AS matches,
                   COALESCE(SUM(result = 'win'), 0) AS wins,
                   COALESCE(SUM(result = 'lose'), 0) AS losses,
                   COALESCE(SUM(result = 'draw'), 0) AS draws,
                   COALESCE(SUM(kills), 0) AS kills,
                   COALESCE(SUM(deaths), 0) AS deaths
            FROM match_players
            WHERE player_id = :player_id
            """
        ),
        params,
    ).one()
    recent_matches = conn.execute(
        text(
            """
            SELECT m.match_id, m.recorded_at, m.map_key, mp.result, mp.team, mp.kills, mp.deaths
            FROM match_players AS mp
            JOIN matches AS m ON m.match_no = mp.match_no
            WHERE mp.player_id = :player_id
            ORDER BY mp.match_no DESC
            LIMIT :limit
            """
        ),
        dict(params, limit=RECENT_MATCH_LIMIT),
    )
    pandoras = conn.execute(
        text("SELECT pandora_id, level FROM player_pandoras WHERE player_id = :player_id ORDER BY pandora_id"), params
    )
    skins = conn.execute(
        text("SELECT skin_id FROM player_skins WHERE player_id = :player_id ORDER BY skin_id"), params
    )
    return {
        "playerId": player.account_id,
        "displayName": profile.display_name,
        "gold": profile.gold,
        # SUM은 DECIMAL로 돌아오므로 정수로 바꾼다.
        "stats": {name: int(value) for name, value in stats._mapping.items()},
        "recentMatches": [
            {
                "matchId": row.match_id,
                "recordedAt": row.recorded_at.strftime("%Y-%m-%dT%H:%M:%SZ"),
                "mapKey": row.map_key,
                "result": row.result,
                "team": row.team,
                "kills": row.kills,
                "deaths": row.deaths,
            }
            for row in recent_matches
        ],
        "pandoras": [{"pandoraId": row.pandora_id, "level": row.level} for row in pandoras],
        "skins": [row.skin_id for row in skins],
    }
