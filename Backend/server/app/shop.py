"""골드로 상품 사기. 가격은 shop_products에서 읽고, 골드 차감·골드 기록·보유 목록 추가를 한 트랜잭션으로 묶는다."""

from sqlalchemy import exc, text

from .db import is_duplicate_key
from .errors import ApiError
from .players import current_gold, spend_gold

# 상점에서 산 판도라의 시작 레벨(상점 위젯 PurchasedPandoraStartingLevel의 기본값과 같다)
PURCHASED_PANDORA_LEVEL = 1

_OWNERSHIP = {
    "pandora": (
        "SELECT 1 FROM player_pandoras WHERE player_id = :player_id AND pandora_id = :product_id",
        "INSERT INTO player_pandoras (player_id, pandora_id, level) VALUES (:player_id, :product_id, :level)",
    ),
    "skin": (
        "SELECT 1 FROM player_skins WHERE player_id = :player_id AND skin_id = :product_id",
        "INSERT INTO player_skins (player_id, skin_id) VALUES (:player_id, :product_id)",
    ),
}


def _already_owned():
    return ApiError(409, "already_owned", "The player already owns this product.")


def purchase(conn, player_id, product_id):
    product = conn.execute(
        text("SELECT product_type, gold_price FROM shop_products WHERE product_id = :product_id AND on_sale"),
        {"product_id": product_id},
    ).one_or_none()
    if product is None:
        raise ApiError(404, "product_not_found", "This product is not on sale.")

    owned_query, grant_statement = _OWNERSHIP[product.product_type]
    params = {"player_id": player_id, "product_id": product_id, "level": PURCHASED_PANDORA_LEVEL}
    # 이미 가진 상품이면 골드가 모자란지보다 먼저 알려 준다.
    if conn.execute(text(owned_query), params).first():
        raise _already_owned()

    try:
        # 골드 차감이 플레이어 행을 먼저 잠근다(players.py 설명).
        if not spend_gold(conn, player_id, product.gold_price, "purchase", product_id):
            raise ApiError(409, "not_enough_gold", "Not enough gold.")
        conn.execute(text(grant_statement), params)
    except exc.IntegrityError as error:
        # 같은 상품을 동시에 산 요청이 먼저 커밋했다(구매 기록이나 보유 목록의 키가 겹친다). 이 트랜잭션은 되돌아간다.
        if is_duplicate_key(error):
            raise _already_owned() from error
        raise
    return {"productId": product_id, "gold": current_gold(conn, player_id)}
