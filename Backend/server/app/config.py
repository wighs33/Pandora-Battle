"""서버 설정. 모두 LABPROJECT_로 시작하는 환경 변수에서 읽는다(로컬에서는 local.ps1이 .env를 읽어 넘긴다)."""

import os
from dataclasses import dataclass

ENV_PREFIX = "LABPROJECT_"


def required_env(name, environ=os.environ):
    value = environ.get(ENV_PREFIX + name, "")
    if not value:
        raise RuntimeError("Environment variable %s%s is required." % (ENV_PREFIX, name))
    return value


@dataclass(frozen=True)
class Settings:
    # SQLAlchemy 연결 URL(mysql+pymysql://user:password@host:port/db)
    database_url: str
    token_secret: bytes
    # 전용 서버가 /server/* 경로를 부를 때 X-Server-Key 헤더로 보내는 값
    server_key: str
    stage: str = "dev"
    # 한 PC에서 클라이언트 여러 개를 테스트할 때만 켠다.
    allow_dev_login: bool = False
    # 비어 있으면 Steam 로그인은 503 steam_login_not_configured
    steam_web_api_key: str = ""
    steam_app_id: str = "4972140"
    steam_ticket_identity: str = "labproject-backend"
    # 비어 있으면 /match/join은 503 fleet_not_configured
    gamelift_fleet_id: str = ""
    gamelift_location: str = ""
    max_players_per_session: int = 4
    max_players_per_boss_raid_session: int = 6

    @property
    def token_issuer(self):
        return "labproject-" + self.stage

    @classmethod
    def from_env(cls, environ=os.environ):
        """필수 값 셋을 읽고, 나머지는 환경 변수가 있을 때만 기본값을 덮는다(필드 이름을 대문자로 쓴 이름)."""
        optional_fields = (
            ("stage", str),
            ("allow_dev_login", lambda value: value == "true"),
            ("steam_web_api_key", str),
            ("steam_app_id", str),
            ("steam_ticket_identity", str),
            ("gamelift_fleet_id", str),
            ("gamelift_location", str),
            ("max_players_per_session", int),
            ("max_players_per_boss_raid_session", int),
        )
        overrides = {}
        for field, convert in optional_fields:
            value = environ.get(ENV_PREFIX + field.upper(), "")
            if value:
                overrides[field] = convert(value)
        return cls(
            database_url=required_env("DATABASE_URL", environ),
            token_secret=required_env("TOKEN_SECRET", environ).encode("utf-8"),
            server_key=required_env("SERVER_KEY", environ),
            **overrides,
        )
