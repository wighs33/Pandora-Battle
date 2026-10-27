# PlayerHUD 하단 디자인 조절

`WBP_PlayerHUD` Designer에서 아래 **ScaleBox의 Canvas Panel Slot → Size X / Size Y**를 변경한다.

| 선택할 위젯 | 기준 크기 | 포함 영역 |
|---|---|---|
| `CombatHudScale` | 560 × 160 | 전투 기술 2슬롯 + 판도라 3슬롯, 제목과 장식 |
| `QuickHudScale` | 310 × 200 | 퀵슬롯 4 × 2, 제목과 벨트 장식 |

각 그룹은 `ScaleBox → SizeBox(기준 크기) → Overlay` 구조다.
Overlay 안에서 `Ornaments`와 `Content`가 동일한 좌표 공간을 공유한다.
`WBP_ActionSlot`, `WBP_SkillBar`, `WBP_QuickSlot`은 해당 Content의 자식이다.
기존의 독립적인 `CombatFrame` / `QuickFrame`은 제거했다.

- ScaleBox는 `Scale To Fit`, `Both`로 설정했다. 가로세로 비율이 다른 크기를 지정하면 여백을 두고 원래 비율을 유지한다.
- 예: 전투 그룹 700 × 200은 125%, 420 × 120은 75%다.
- 퀵슬롯 그룹 387.5 × 250은 125%, 232.5 × 150은 75%다.
- 내부 Design SizeBox나 개별 슬롯 대신 최상위 ScaleBox의 Canvas Slot 크기를 바꾼다.
- 위치와 화면 기준점은 같은 Canvas Slot의 Position / Anchors / Alignment로 조절한다.

전투는 꺾인 금속선과 붉은 강조색, 판도라는 연결된 원형 룬과 보라색,
퀵슬롯은 두 줄의 벨트와 카드형 슬롯으로 구분한다. 장식은 UMG 기본 브러시만 사용한다.
일부 장식의 각도만 회전하며, RenderTransform Scale은 사용하지 않는다.

아이콘·입력키·수량·활성 상태·쿨다운 바인딩과 게임 로직은 유지한다.
쿨다운의 기존 0→1 충전 값은 아이콘 아래 얇은 진행선으로 표시한다.
ActionSlot과 QuickSlot의 Designer 기본 엔트리 클래스는 현재 DataAsset과 동일하다.
실행 중에는 기존 코드대로 설정 DataAsset이 우선한다.

입력키 표시 설정:
- `/Game/Input/DA_Input` → **Input Action Displays** → 해당 Input Action의 **Key Text**를 수정한다.
- 예: `IA_SelectPandora = 좌SHIFT`, `IA_Grapple = F`, `IA_Skill1~3 = Q/E/R`, 퀵슬롯과 제스처 = `1~8`.
- 이 문구는 HUD 표시용이다. 실제 입력 바인딩 변경은 Input Mapping Context에서 별도로 한다.
- 하단 HUD는 `KeyText` TextBlock으로 표시한다. 기존 `Icon` 값은 이미지 기반 위젯의 호환성을 위해 남아 있으며, 텍스트 배지에는 사용하지 않는다.
- 전투/판도라 배지는 높이 23, 최소 폭 24이며 문구 길이에 따라 폭이 늘어난다. 어두운 배경, 얇은 회색 테두리, 밝은 글자로 구성한다.
- 키 배경의 RenderTransform Scale은 1로 유지한다.
- 퀵슬롯 엔트리는 67×67, 내부 프레임은 61×61 정사각형이다. 아이템 이미지를 53×53으로 크게 표시한다.
- 퀵슬롯 하단에는 키(왼쪽)와 수량(오른쪽)을 9pt로 수평 배치한다. 키 영역은 17×15이며, 수량 영역과 3px 간격을 둔다.
- 퀵슬롯 4×2 콘텐츠는 기존 310×200 기준 공간 안에 배치한다. 외부 ScaleBox의 사용자 지정 크기는 유지한다.

전투/판도라 슬롯 정렬:
- `WBP_ActionSlotEntry`, `WBP_SkillSlot`, `WBP_EmptySkillSlot`의 기준 크기는 88×110이다.
- 전투와 판도라의 실제 아이콘은 54×54로 맞추고, 키 배지는 같은 높이에 배치한다.
- 판도라 3슬롯은 각 슬롯의 좌우 5px 여백을 포함해 `WBP_SkillBar`에 294×120 공간을 배정한다.
- 스킬 크기를 변경할 때는 바깥 원뿐 아니라 안쪽 원, 아이콘, 활성/비활성 프레임, 쿨다운, 키 위치와 부모 Bar 공간을 함께 맞춰야 한다. 전체 HUD 크기 변경은 `CombatHudScale`에서 한다.
