"""GET /bossraid/status: 보스 레이드가 지금 열려 있는지 알려 준다.

보스 레이드는 상시 열지 않고 운영자가 여는 기간에만 참가를 받는다. 열림 여부는 테이블의 설정 항목 하나에 두고
Backend/bossraid.ps1로 바꾸므로, 열고 닫을 때 게임 패치나 백엔드 배포가 필요 없다. 항목이 없으면 닫힌 것으로 본다.
타이틀 화면이 로그인 전에 물어보므로 인증 없이 받는다. POST /match/join도 같은 값을 확인해 닫힌 동안에는 자리를 잡아 주지 않는다.
"""

import os

from common import BOSS_RAID_SK, CONFIG_PK, api_handler, aws_client, ddb_s, json_response


def is_boss_raid_open(dynamodb, table_name):
    item = dynamodb.get_item(
        TableName=table_name,
        Key={"PK": ddb_s(CONFIG_PK), "SK": ddb_s(BOSS_RAID_SK)},
    ).get("Item", {})
    return item.get("Open", {}).get("BOOL", False)


@api_handler
def handler(event, context):
    return json_response(200, {"open": is_boss_raid_open(aws_client("dynamodb"), os.environ["TABLE_NAME"])})
