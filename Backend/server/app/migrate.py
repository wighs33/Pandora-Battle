"""스키마 마이그레이션: migrations/NNN_이름.sql을 번호 순서대로 한 번씩 적용한다.

    python -m app.migrate                       # LABPROJECT_OWNER_DATABASE_URL의 DB
    python -m app.migrate --url-env TEST_OWNER_DATABASE_URL

스키마를 바꿀 권한이 있는 owner 계정으로 실행한다(API 서버 계정은 행 읽기·쓰기만 한다).
적용한 번호와 파일 체크섬은 schema_migrations에 남는다. 이미 적용한 파일을 고치면 체크섬이 달라져 멈추므로,
바꿀 내용은 새 번호의 파일로 추가한다.
MySQL은 DDL마다 암묵적으로 커밋하므로 파일 중간에서 실패하면 앞 문장은 남는다. 그때는 남은 것을 손으로 정리하고 다시 실행한다.
"""

import argparse
import hashlib
import pathlib
import re

from sqlalchemy import text

from .config import required_env
from .db import create_db_engine

MIGRATIONS_DIR = pathlib.Path(__file__).resolve().parent.parent / "migrations"
FILE_PATTERN = re.compile(r"^(\d{3})_[a-z0-9_]+\.sql$")


def split_statements(sql):
    """한 줄 주석(--)을 빼고, 줄 끝의 ;로 문장을 나눈다. 마이그레이션 파일은 이 규칙에 맞춰 쓴다."""
    statements, current = [], []
    for line in sql.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("--"):
            continue
        current.append(line)
        if stripped.endswith(";"):
            statements.append("\n".join(current).rstrip()[:-1])
            current = []
    if current:
        raise ValueError("The last statement does not end with ';'.")
    return statements


def migration_files(directory=MIGRATIONS_DIR):
    files = []
    for path in sorted(directory.iterdir()):
        match = FILE_PATTERN.match(path.name)
        if match:
            files.append((int(match.group(1)), path))
    versions = [version for version, _ in files]
    if len(versions) != len(set(versions)):
        raise ValueError("Two migration files share a version number.")
    return files


def migrate(engine, directory=MIGRATIONS_DIR, log=print):
    with engine.begin() as conn:
        conn.execute(
            text(
                """
                CREATE TABLE IF NOT EXISTS schema_migrations (
                    version    INT NOT NULL,
                    name       VARCHAR(255) NOT NULL,
                    checksum   CHAR(64) CHARACTER SET ascii NOT NULL,
                    applied_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
                    PRIMARY KEY (version)
                )
                """
            )
        )
        rows = conn.execute(text("SELECT version, checksum FROM schema_migrations"))
        applied = {row.version: row.checksum for row in rows}

    for version, path in migration_files(directory):
        content = path.read_bytes()
        checksum = hashlib.sha256(content).hexdigest()
        if version in applied:
            if applied[version] != checksum:
                raise RuntimeError("%s changed after it was applied. Add a new migration instead." % path.name)
            continue
        with engine.begin() as conn:
            for statement in split_statements(content.decode("utf-8")):
                # 문장에 바인드 변수가 없으므로 드라이버에 그대로 넘긴다(text()는 값 안의 ':이름'을 변수로 읽는다).
                conn.exec_driver_sql(statement)
            conn.execute(
                text("INSERT INTO schema_migrations (version, name, checksum) VALUES (:version, :name, :checksum)"),
                {"version": version, "name": path.name, "checksum": checksum},
            )
        log("applied %s" % path.name)


def main():
    parser = argparse.ArgumentParser(description="Apply pending schema migrations.")
    parser.add_argument("--url-env", default="OWNER_DATABASE_URL", help="LABPROJECT_ 뒤에 붙는 연결 URL 환경 변수 이름")
    args = parser.parse_args()
    engine = create_db_engine(required_env(args.url_env))
    migrate(engine)
    engine.dispose()


if __name__ == "__main__":
    main()
