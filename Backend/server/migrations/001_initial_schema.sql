-- 게임 백엔드 첫 스키마. 데이터베이스 기본 문자 집합은 utf8mb4다(local.ps1이 만든다).
-- 시각은 모두 UTC로 저장한다. API 서버는 연결마다 time_zone을 '+00:00'으로 맞춘다.
-- 외부 ID(로그인 ID, 게임 정의 ID, 경기·서버 ID)는 대소문자를 구분해야 하므로 ascii_bin으로 비교한다.

-- 플레이어 계정. 로그인 수단(Steam, 개발용)의 사용자 ID마다 한 행이고, 골드는 서버만 바꾼다.
CREATE TABLE players (
    player_id        BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    provider         ENUM('steam', 'dev') NOT NULL,
    provider_user_id VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    display_name     VARCHAR(32) NOT NULL,
    gold             INT NOT NULL DEFAULT 0,
    created_at       DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    last_login_at    DATETIME(3) NULL,
    PRIMARY KEY (player_id),
    UNIQUE KEY uq_players_login (provider, provider_user_id),
    CONSTRAINT ck_players_gold CHECK (gold >= 0)
);

-- 상점 상품. product_id는 게임 정의의 PrimaryAssetId("PandoraDefinition:DA_Pandora_Fire")다.
-- 구매 가격은 클라이언트가 보낸 값이 아니라 이 표에서 읽는다.
CREATE TABLE shop_products (
    product_id   VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    product_type ENUM('pandora', 'skin') NOT NULL,
    gold_price   INT NOT NULL,
    on_sale      BOOLEAN NOT NULL,
    PRIMARY KEY (product_id),
    CONSTRAINT ck_shop_products_price CHECK (gold_price >= 0)
);

-- 보유 목록. 기본 키가 (플레이어, 정의)라서 같은 것을 두 번 가질 수 없다.
CREATE TABLE player_pandoras (
    player_id   BIGINT UNSIGNED NOT NULL,
    pandora_id  VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    level       TINYINT UNSIGNED NOT NULL,
    acquired_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (player_id, pandora_id),
    CONSTRAINT fk_player_pandoras_player FOREIGN KEY (player_id) REFERENCES players (player_id),
    CONSTRAINT ck_player_pandoras_level CHECK (level >= 1)
);

CREATE TABLE player_skins (
    player_id   BIGINT UNSIGNED NOT NULL,
    skin_id     VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    acquired_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (player_id, skin_id),
    CONSTRAINT fk_player_skins_player FOREIGN KEY (player_id) REFERENCES players (player_id)
);

-- 골드 증감 기록. players.gold를 바꾸는 트랜잭션은 같은 금액의 행을 함께 넣으므로 플레이어별 amount 합계가 gold와 같다.
-- (플레이어, 사유, 참조 ID)가 유일해서 같은 경기·레이드 보상이나 같은 상품 구매가 두 번 기록되지 않는다.
CREATE TABLE gold_ledger (
    entry_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    player_id    BIGINT UNSIGNED NOT NULL,
    amount       INT NOT NULL,
    reason       ENUM('match_reward', 'raid_reward', 'purchase') NOT NULL,
    reference_id VARCHAR(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    created_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (entry_id),
    UNIQUE KEY uq_gold_ledger_once (player_id, reason, reference_id),
    CONSTRAINT fk_gold_ledger_player FOREIGN KEY (player_id) REFERENCES players (player_id)
);

-- 전용 서버가 보고한 경기. match_id는 서버가 정한 경기 ID(GameLift 게임 세션 ARN 등)이고,
-- 같은 값으로 다시 보고하면 유일 키에 막혀 전적과 보상이 두 번 들어가지 않는다.
CREATE TABLE matches (
    match_no    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    match_id    VARCHAR(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    map_key     VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    end_reason  ENUM('completed', 'player_exit') NOT NULL,
    winner_team SMALLINT NOT NULL,
    recorded_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    PRIMARY KEY (match_no),
    UNIQUE KEY uq_matches_match_id (match_id)
);

-- 경기 참가자. 누적 전적은 따로 저장하지 않고 이 표를 집계해 계산한다.
-- player_id가 NULL이면 백엔드에 로그인하지 않은 참가자(게스트)다.
CREATE TABLE match_players (
    match_no     BIGINT UNSIGNED NOT NULL,
    slot         TINYINT UNSIGNED NOT NULL,
    player_id    BIGINT UNSIGNED NULL,
    display_name VARCHAR(32) NOT NULL,
    team         SMALLINT NOT NULL,
    kills        INT UNSIGNED NOT NULL,
    deaths       INT UNSIGNED NOT NULL,
    result       ENUM('win', 'lose', 'draw') NOT NULL,
    PRIMARY KEY (match_no, slot),
    UNIQUE KEY uq_match_players_player (match_no, player_id),
    -- 플레이어의 최근 경기를 최신순으로 읽고, 누적 전적을 집계한다.
    KEY ix_match_players_recent (player_id, match_no),
    CONSTRAINT fk_match_players_match FOREIGN KEY (match_no) REFERENCES matches (match_no),
    CONSTRAINT fk_match_players_player FOREIGN KEY (player_id) REFERENCES players (player_id)
);

-- 보스 레이드 회차. 운영자가 열 때마다 한 행이고, 가장 최근에 연 회차가 지금 회차다.
-- 모든 게임 서버가 hp 하나를 함께 깎는다. 격파 여부는 defeated_at으로, 새 참가를 받는지는 entry_open으로 나타낸다.
CREATE TABLE boss_raids (
    raid_id         VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    max_hp          BIGINT NOT NULL,
    hp              BIGINT NOT NULL,
    daily_attempts  TINYINT UNSIGNED NOT NULL,
    attempt_seconds SMALLINT UNSIGNED NOT NULL,
    reward_gold     INT NOT NULL,
    entry_open      BOOLEAN NOT NULL,
    opened_at       DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
    defeated_at     DATETIME(3) NULL,
    PRIMARY KEY (raid_id),
    KEY ix_boss_raids_opened (opened_at),
    CONSTRAINT ck_boss_raids_hp CHECK (hp BETWEEN 0 AND max_hp),
    CONSTRAINT ck_boss_raids_settings CHECK (max_hp > 0 AND daily_attempts > 0 AND attempt_seconds > 0 AND reward_gold >= 0)
);

-- 회차별 플레이어 기록: 누적 딜, 오늘 쓴 도전 횟수(한국 시간 날짜 기준), 보상 수령 시각.
CREATE TABLE boss_raid_players (
    raid_id           VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    player_id         BIGINT UNSIGNED NOT NULL,
    damage            BIGINT NOT NULL DEFAULT 0,
    attempt_day       DATE NULL,
    attempts_today    TINYINT UNSIGNED NOT NULL DEFAULT 0,
    reward_claimed_at DATETIME(3) NULL,
    PRIMARY KEY (raid_id, player_id),
    -- 회차 안에서 딜 순서로 순위를 읽는다.
    KEY ix_boss_raid_players_ranking (raid_id, damage),
    -- 플레이어가 딜을 넣은 회차를 찾아 보상을 준다.
    KEY ix_boss_raid_players_player (player_id),
    CONSTRAINT fk_boss_raid_players_raid FOREIGN KEY (raid_id) REFERENCES boss_raids (raid_id),
    CONSTRAINT fk_boss_raid_players_player FOREIGN KEY (player_id) REFERENCES players (player_id),
    CONSTRAINT ck_boss_raid_players_damage CHECK (damage >= 0)
);

-- 게임 서버별 마지막 딜 보고 순번. 같은 보고를 다시 보내도 한 번만 반영한다.
CREATE TABLE boss_raid_server_reports (
    raid_id   VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    server_id VARCHAR(256) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    last_seq  BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (raid_id, server_id),
    CONSTRAINT fk_boss_raid_server_reports_raid FOREIGN KEY (raid_id) REFERENCES boss_raids (raid_id)
);
