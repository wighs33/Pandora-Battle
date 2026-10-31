"""LabProject 게임 API 서버.

    uvicorn app.main:create_app --factory --host 127.0.0.1 --port 8000

Lambda 백엔드(Backend/src)와 같은 경로·응답 형식을 쓰고, DynamoDB 대신 MySQL에 기록한다.
- 클라이언트 경로는 로그인 토큰(Authorization: Bearer)으로, 전용 서버 경로(/server/*)는 서버 키(X-Server-Key)로 확인한다.
- 골드와 보유 목록은 서버가 정한다. 구매와 보상은 골드, 골드 기록, 보유 목록을 한 트랜잭션에서 함께 바꾼다.

라우트 함수는 일반 def로 둔다. FastAPI가 스레드 풀에서 실행하므로, DB를 기다리거나 GameLift 세션이 열리기를 기다리는 동안
다른 요청을 막지 않는다.
"""

import hmac
import logging
import re
from typing import Annotated

from fastapi import APIRouter, Depends, FastAPI, Header, Request
from sqlalchemy import Engine

from . import boss_raid, gamelift, matches, players, shop, steam
from .config import Settings
from .db import create_db_engine, in_transaction
from .errors import ApiError, install_error_handlers
from .models import (
    DevLoginRequest,
    MatchJoinRequest,
    MatchReport,
    PurchaseRequest,
    RaidDamageReport,
    SteamLoginRequest,
    sanitize_display_name,
)
from .tokens import issue_token, verify_token

BEARER_PATTERN = re.compile(r"^Bearer\s+(\S+)$")

logger = logging.getLogger("app")
router = APIRouter()


# 의존성 -------------------------------------------------------------------------------------------------------------

def get_settings(request: Request) -> Settings:
    return request.app.state.settings


def get_engine(request: Request) -> Engine:
    return request.app.state.engine


SettingsDep = Annotated[Settings, Depends(get_settings)]
EngineDep = Annotated[Engine, Depends(get_engine)]


def current_player(settings: SettingsDep, engine: EngineDep, authorization: Annotated[str, Header()] = ""):
    match = BEARER_PATTERN.match(authorization)
    if not match:
        raise ApiError(401, "missing_token", "Authorization header with a Bearer token is required.")
    claims = verify_token(settings.token_secret, match.group(1), settings.token_issuer)
    player = in_transaction(engine, lambda conn: players.find_player(conn, claims["sub"]))
    if player is None:
        # 서명은 맞지만 계정이 없다(DB를 비운 뒤의 옛 토큰). 다시 로그인하게 한다.
        raise ApiError(401, "invalid_token", "Session token is invalid or expired.")
    return player


PlayerDep = Annotated[players.Player, Depends(current_player)]


def require_server(settings: SettingsDep, x_server_key: Annotated[str, Header()] = ""):
    if not hmac.compare_digest(x_server_key.encode("utf-8"), settings.server_key.encode("utf-8")):
        raise ApiError(401, "invalid_server_key", "A valid X-Server-Key header is required.")


# 로그인 -------------------------------------------------------------------------------------------------------------

def _login(settings, engine, provider, provider_user_id, display_name):
    in_transaction(engine, lambda conn: players.upsert_login(conn, provider, provider_user_id, display_name))
    account_id = players.make_account_id(provider, provider_user_id)
    token = issue_token(settings.token_secret, account_id, display_name, settings.token_issuer)
    return {"token": token, "playerId": account_id, "displayName": display_name}


@router.post("/auth/steam")
def steam_login(body: SteamLoginRequest, settings: SettingsDep, engine: EngineDep):
    if not settings.steam_web_api_key:
        raise ApiError(503, "steam_login_not_configured", "Steam Web API key is not configured.")
    steam_id = steam.verify_steam_ticket(
        body.ticket, settings.steam_web_api_key, settings.steam_app_id, settings.steam_ticket_identity
    )
    return _login(settings, engine, "steam", steam_id, sanitize_display_name(body.display_name))


@router.post("/auth/dev")
def dev_login(body: DevLoginRequest, settings: SettingsDep, engine: EngineDep):
    if not settings.allow_dev_login:
        raise ApiError(404, "not_found", "Not found.")
    return _login(settings, engine, "dev", body.dev_id, sanitize_display_name(body.display_name, fallback=body.dev_id))


# 플레이어·상점 -------------------------------------------------------------------------------------------------------

@router.get("/player/me")
def player_me(player: PlayerDep, engine: EngineDep):
    return in_transaction(engine, lambda conn: players.load_profile(conn, player))


@router.post("/shop/purchase")
def shop_purchase(body: PurchaseRequest, player: PlayerDep, engine: EngineDep):
    return in_transaction(engine, lambda conn: shop.purchase(conn, player.player_id, body.product_id))


# 게임 세션 ----------------------------------------------------------------------------------------------------------

def _join_boss_raid(request, settings, engine, player):
    """도전 횟수를 먼저 쓰고(커밋) 자리를 잡는다. 자리를 잡지 못하면 쓴 횟수를 돌려준다.

    GameLift 세션이 열리기를 기다리는 동안(최대 수십 초) DB 트랜잭션과 잠금을 잡고 있지 않도록 둘을 나눈다.
    """
    today = boss_raid.attempt_day()

    def consume(conn):
        raid = boss_raid.current_raid(conn)
        if raid is None or not boss_raid.is_accepting(raid):
            raise ApiError(403, "bossraid_closed", "Boss raid is not open right now.")
        return raid.raid_id, boss_raid.consume_attempt(conn, raid, player.player_id, today)

    raid_id, attempts_left = in_transaction(engine, consume)
    try:
        result = gamelift.join_session(
            request.app.state.gamelift, settings, player.account_id, player.display_name, gamelift.BOSS_RAID_MODE
        )
    except Exception:
        in_transaction(engine, lambda conn: boss_raid.refund_attempt(conn, raid_id, player.player_id, today))
        raise
    return dict(result, attemptsLeft=attempts_left)


@router.post("/match/join")
def match_join(body: MatchJoinRequest, request: Request, player: PlayerDep, settings: SettingsDep, engine: EngineDep):
    if body.mode == gamelift.BOSS_RAID_MODE:
        return _join_boss_raid(request, settings, engine, player)
    return gamelift.join_session(
        request.app.state.gamelift, settings, player.account_id, player.display_name, body.mode
    )


# 보스 레이드 --------------------------------------------------------------------------------------------------------

@router.get("/bossraid/status")
def bossraid_status(engine: EngineDep):
    """로그인 없이 묻는다. 타이틀 화면이 로그인 전에 레이드 버튼을 켜고 끄는 데 쓴다."""
    raid = in_transaction(engine, boss_raid.current_raid)
    if raid is None:
        return {"open": False, "raid": None}
    return {"open": boss_raid.is_accepting(raid), "raid": boss_raid.raid_view(raid)}


@router.get("/bossraid/me")
def bossraid_me(player: PlayerDep, engine: EngineDep):
    def read(conn):
        raid = boss_raid.current_raid(conn)
        if raid is None:
            return {"open": False, "raid": None}
        standing = boss_raid.standing(conn, raid, player.player_id, boss_raid.attempt_day())
        return dict(standing, open=boss_raid.is_accepting(raid))

    return in_transaction(engine, read)


@router.post("/bossraid/claim")
def bossraid_claim(player: PlayerDep, engine: EngineDep):
    def claim(conn):
        reward = boss_raid.claim_rewards(conn, player.player_id)
        return dict(reward, balance=players.current_gold(conn, player.player_id))

    return in_transaction(engine, claim)


# 전용 서버 보고 -----------------------------------------------------------------------------------------------------

@router.post("/server/match-result", dependencies=[Depends(require_server)])
def server_match_result(report: MatchReport, engine: EngineDep):
    recorded = in_transaction(engine, lambda conn: matches.record_match(conn, report))
    logger.info("match-result matchId=%s recorded=%s", report.match_id, recorded)
    return {"recorded": recorded, "duplicate": not recorded}


@router.post("/server/raid-damage", dependencies=[Depends(require_server)])
def server_raid_damage(report: RaidDamageReport, engine: EngineDep):
    raid, recorded = in_transaction(engine, lambda conn: boss_raid.record_damage(conn, report))
    if recorded:
        logger.info(
            "raid-damage raidId=%s serverId=%s seq=%d damage=%d hp=%d",
            raid.raid_id, report.server_id, report.seq, sum(player.damage for player in report.players), raid.hp,
        )
    return dict(boss_raid.raid_view(raid), recorded=recorded)


# 앱 -----------------------------------------------------------------------------------------------------------------

def _configure_logging():
    """이 서버의 로그(app, app.*)만 INFO부터 uvicorn 로그와 같은 모양으로 남긴다. 라이브러리 로그 수준은 건드리지 않는다."""
    app_logger = logging.getLogger("app")
    if not app_logger.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(logging.Formatter("%(levelname)s:     %(name)s %(message)s"))
        app_logger.addHandler(handler)
        app_logger.setLevel(logging.INFO)


def _create_gamelift_client(settings):
    if not settings.gamelift_fleet_id:
        return None
    # 플릿을 쓸 때만 boto3를 불러온다(로컬 개발과 테스트에는 필요 없고, 불러오는 데 시간이 걸린다).
    import boto3

    return boto3.client("gamelift")


def create_app(settings=None, gamelift_client=None):
    """uvicorn --factory가 인자 없이 부르면 환경 변수로 설정한다. 테스트는 설정과 가짜 GameLift를 넘긴다."""
    settings = settings or Settings.from_env()
    _configure_logging()

    # /docs에서 경로와 요청 형식을 보고 직접 호출해 볼 수 있다.
    app = FastAPI(title="LabProject API")
    app.state.settings = settings
    app.state.engine = create_db_engine(settings.database_url)
    app.state.gamelift = gamelift_client or _create_gamelift_client(settings)
    install_error_handlers(app)
    app.include_router(router)
    return app
