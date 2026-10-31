"""전용 서버의 경기 결과 보고. 경기, 참가자, 보상 골드를 한 트랜잭션으로 기록한다."""

import logging

from sqlalchemy import exc, text

from .db import is_duplicate_key
from .players import credit_gold, find_player_ids

logger = logging.getLogger(__name__)


def record_match(conn, report):
    """처음 받은 경기면 기록하고 True, 이미 기록한 경기 ID면 아무것도 바꾸지 않고 False."""
    try:
        match_no = conn.execute(
            text(
                """
                INSERT INTO matches (match_id, map_key, end_reason, winner_team)
                VALUES (:match_id, :map_key, :end_reason, :winner_team)
                """
            ),
            {
                "match_id": report.match_id,
                "map_key": report.map_key,
                "end_reason": report.end_reason,
                "winner_team": report.winner_team,
            },
        ).lastrowid
    except exc.IntegrityError as error:
        # 같은 경기를 다시 보고했다. 동시에 온 보고는 먼저 넣은 쪽이 커밋할 때까지 기다렸다가 여기로 온다.
        if is_duplicate_key(error):
            return False
        raise

    reported_accounts = [player.player_id for player in report.players if player.player_id]
    player_ids = find_player_ids(conn, reported_accounts)
    for account_id in sorted(set(reported_accounts) - set(player_ids)):
        # 백엔드 로그인으로만 계정이 생기므로 정상 흐름에서는 없다. 전적은 게스트로 남긴다.
        logger.warning("match %s reported unknown account %s", report.match_id, account_id)

    # 보상을 먼저 준다. 플레이어 행을 player_id 순서로 잠근 뒤에 참가자 행을 넣어야 교착되지 않는다(players.py 설명).
    rewards = sorted(
        (player_ids[player.player_id], player.gold_reward)
        for player in report.players
        if player.player_id in player_ids and player.gold_reward > 0
    )
    for player_id, gold in rewards:
        credit_gold(conn, player_id, gold, "match_reward", report.match_id)

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
                "player_id": player_ids.get(player.player_id),
                "display_name": player.display_name,
                "team": player.team,
                "kills": player.kills,
                "deaths": player.deaths,
                "result": player.result,
            }
            for slot, player in enumerate(report.players)
        ],
    )
    return True
