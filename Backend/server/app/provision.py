"""RDS에 DB와 두 계정을 만든다. 다시 실행하면 비밀번호와 권한을 지금 설정으로 맞춘다.

    python -m app.provision

RDS 마스터 계정(LABPROJECT_MASTER_DATABASE_URL)으로 실행한다. 계정 이름·비밀번호와 DB 이름은 API 서버가 쓰는
연결 URL(LABPROJECT_OWNER_DATABASE_URL, LABPROJECT_DATABASE_URL)에서 읽으므로 설정이 한 곳에만 있다.
- 스키마 담당(owner): 이 DB의 모든 권한. 마이그레이션에 쓴다.
- API 서버(app): 행 읽기·쓰기(SELECT, INSERT, UPDATE)만. 행 삭제와 스키마 변경은 할 수 없다.
RDS에서는 API 서버가 다른 호스트라 계정을 모든 호스트('%')로 만들고, 접속 범위는 DB 보안 그룹이 API 서버로 좁힌다.
로컬 MySQL은 local.ps1 setup이 같은 계정을 localhost 전용으로 만든다.
"""

import re

from sqlalchemy import text
from sqlalchemy.engine import make_url

from .config import required_env
from .db import create_db_engine

# 계정·DB 이름은 바인드 변수로 넘길 수 없어 SQL에 그대로 넣으므로 이 글자만 받는다.
IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z0-9_]{1,32}$")
ANY_HOST = "%"


def _identifier(name):
    if not IDENTIFIER_PATTERN.match(name or ""):
        raise ValueError("Unexpected database or user name: %r" % name)
    return name


def provision(master_engine, owner_url, app_url):
    owner, app = make_url(owner_url), make_url(app_url)
    if owner.database != app.database:
        raise ValueError("The owner and app URLs must use the same database.")
    database = _identifier(owner.database)
    accounts = ((owner, "ALL PRIVILEGES"), (app, "SELECT, INSERT, UPDATE"))
    with master_engine.begin() as conn:
        conn.execute(
            text("CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci" % database)
        )
        for account, privileges in accounts:
            params = {"user": _identifier(account.username), "host": ANY_HOST, "password": account.password}
            conn.execute(text("CREATE USER IF NOT EXISTS :user@:host IDENTIFIED BY :password"), params)
            conn.execute(text("ALTER USER :user@:host IDENTIFIED BY :password"), params)
            conn.execute(text("GRANT %s ON `%s`.* TO :user@:host" % (privileges, database)), params)
    return database, [account.username for account, _ in accounts]


def main():
    engine = create_db_engine(required_env("MASTER_DATABASE_URL"))
    database, users = provision(engine, required_env("OWNER_DATABASE_URL"), required_env("DATABASE_URL"))
    engine.dispose()
    print("Database %s is ready for %s." % (database, ", ".join(users)))


if __name__ == "__main__":
    main()
