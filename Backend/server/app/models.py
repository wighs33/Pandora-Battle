"""요청 본문 형식. 필드 이름은 JSON에서 camelCase(playerId)이고, 형식이 틀리면 400 invalid_request로 거절한다."""

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator
from pydantic.alias_generators import to_camel

DISPLAY_NAME_MAX_LENGTH = 32

# 로그인 ID: Steam은 SteamID64, 개발용은 이름. 빈 문자열이나 null은 백엔드에 로그인하지 않은 참가자다.
ACCOUNT_ID_PATTERN = r"^(steam:[0-9]{1,20}|dev:[A-Za-z0-9_-]{1,32})$"
OPTIONAL_ACCOUNT_ID_PATTERN = r"^$|" + ACCOUNT_ID_PATTERN
PRODUCT_ID_PATTERN = r"^(PandoraDefinition|SkinDefinition):[A-Za-z0-9_]{1,96}$"
# GameLift 게임 세션 ARN 또는 로컬 서버가 만든 ID
MATCH_ID_PATTERN = r"^[A-Za-z0-9:/_.\-]{1,256}$"
SERVER_ID_PATTERN = MATCH_ID_PATTERN
MAP_KEY_PATTERN = r"^[A-Za-z0-9_.\-]{0,64}$"
RAID_ID_PATTERN = r"^$|^[A-Za-z0-9_\-]{1,64}$"

MAX_PLAYERS = 16
MAX_COUNT = 100_000
# 서버가 보고하는 한 경기 보상의 상한. 잘못된 보고가 골드를 크게 늘리지 못하게 막는다.
MAX_MATCH_GOLD_REWARD = 100_000
MAX_DAMAGE = 10**9
MAX_SEQ = 2**53


def sanitize_display_name(value, fallback="Player"):
    """표시 이름은 화면 출력에만 쓰므로 제어 문자를 지우고 길이만 제한한다."""
    if not isinstance(value, str):
        return fallback
    cleaned = "".join(ch for ch in value if ch.isprintable()).strip()
    return cleaned[:DISPLAY_NAME_MAX_LENGTH] or fallback


class ApiModel(BaseModel):
    # strict: "3"이나 true를 숫자로 바꿔 받지 않는다.
    model_config = ConfigDict(strict=True, alias_generator=to_camel, populate_by_name=True)


def _require_unique_accounts(players):
    account_ids = [player.player_id for player in players if player.player_id]
    if len(account_ids) != len(set(account_ids)):
        raise ValueError("playerId appears more than once.")


class SteamLoginRequest(ApiModel):
    ticket: str = Field(pattern=r"^[0-9A-Fa-f]{16,4096}$")
    display_name: Any = None


class DevLoginRequest(ApiModel):
    dev_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,32}$")
    display_name: Any = None


class MatchJoinRequest(ApiModel):
    mode: Literal["match", "bossraid"] = "match"


class PurchaseRequest(ApiModel):
    product_id: str = Field(pattern=PRODUCT_ID_PATTERN)


class MatchReportPlayer(ApiModel):
    player_id: str | None = Field(None, pattern=OPTIONAL_ACCOUNT_ID_PATTERN)
    display_name: str = "Player"
    team: int = Field(-1, ge=-1, le=64)
    kills: int = Field(0, ge=0, le=MAX_COUNT)
    deaths: int = Field(0, ge=0, le=MAX_COUNT)
    result: Literal["win", "lose", "draw"]
    # 전용 서버가 정한 이 경기의 보상 골드. 로그인한 플레이어에게만 준다.
    gold_reward: int = Field(0, ge=0, le=MAX_MATCH_GOLD_REWARD)

    @field_validator("display_name", mode="before")
    @classmethod
    def _sanitize_name(cls, value):
        return sanitize_display_name(value)


class MatchReport(ApiModel):
    match_id: str = Field(pattern=MATCH_ID_PATTERN)
    map_key: str = Field("", pattern=MAP_KEY_PATTERN)
    end_reason: Literal["completed", "player_exit"]
    winner_team: int = Field(-1, ge=-1, le=64)
    players: list[MatchReportPlayer] = Field(min_length=1, max_length=MAX_PLAYERS)

    @model_validator(mode="after")
    def _unique_players(self):
        _require_unique_accounts(self.players)
        return self


class RaidDamagePlayer(ApiModel):
    player_id: str | None = Field(None, pattern=OPTIONAL_ACCOUNT_ID_PATTERN)
    damage: int = Field(ge=0, le=MAX_DAMAGE)


class RaidDamageReport(ApiModel):
    """raidId가 비어 있으면 지금 회차에 보고한다. 딜이 없는 보고는 지금 체력만 묻는다."""

    raid_id: str = Field("", pattern=RAID_ID_PATTERN)
    server_id: str = Field(pattern=SERVER_ID_PATTERN)
    seq: int = Field(ge=1, le=MAX_SEQ)
    players: list[RaidDamagePlayer] = Field(default_factory=list, max_length=MAX_PLAYERS)

    @model_validator(mode="after")
    def _unique_players(self):
        _require_unique_accounts(self.players)
        return self
