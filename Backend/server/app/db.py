"""MySQL 연결 풀과 트랜잭션."""

import logging

from sqlalchemy import create_engine, exc

logger = logging.getLogger(__name__)

# MySQL 오류 번호
ER_DUP_ENTRY = 1062
ER_LOCK_DEADLOCK = 1213

DEADLOCK_ATTEMPTS = 3


def create_db_engine(url):
    return create_engine(
        url,
        # MySQL은 오래 쉰 연결을 끊는다(wait_timeout). 풀에서 꺼낼 때 살아 있는지 확인한다.
        pool_pre_ping=True,
        # NOW(3)와 DATETIME 기본값이 서버 설정과 상관없이 UTC가 되도록 연결마다 시간대를 맞춘다.
        connect_args={"init_command": "SET time_zone = '+00:00'"},
    )


def mysql_error_code(error):
    """SQLAlchemy가 감싼 PyMySQL 오류에서 MySQL 오류 번호를 꺼낸다."""
    original = getattr(error, "orig", None)
    args = getattr(original, "args", ())
    return args[0] if args and isinstance(args[0], int) else None


def is_duplicate_key(error):
    return isinstance(error, exc.IntegrityError) and mysql_error_code(error) == ER_DUP_ENTRY


def in_transaction(engine, work):
    """work(conn)을 한 트랜잭션으로 실행하고 결과를 돌려준다. 예외가 나면 전부 되돌린다.

    InnoDB는 교착 상태를 발견하면 한쪽 트랜잭션을 통째로 되돌린다. 되돌려진 쪽은 아무것도 쓰지 않았으므로 처음부터 다시 실행한다.
    """
    for attempt in range(1, DEADLOCK_ATTEMPTS + 1):
        try:
            with engine.begin() as conn:
                return work(conn)
        except exc.OperationalError as error:
            if mysql_error_code(error) != ER_LOCK_DEADLOCK or attempt == DEADLOCK_ATTEMPTS:
                raise
            # 잠금 순서를 지키면 생기지 않아야 하므로, 생기면 어느 경로인지 찾을 수 있게 남긴다.
            logger.warning(
                "Deadlock in %s, retrying (%d/%d)", getattr(work, "__qualname__", work), attempt, DEADLOCK_ATTEMPTS
            )
