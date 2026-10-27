# 방 목록·상점·가이드·전적 스타일

2026-09-16, UE 5.8.2에서 적용했습니다. 타이틀·로비와 같은 보라색 패널, 크림색 글자, 금색 테두리와 주요 버튼, 성 배경을 사용합니다. 네 화면은 1920×1080 기준 레이아웃을 ScaleBox로 화면에 맞춥니다.

## 수정한 위젯

| 화면 | 구성 |
| --- | --- |
| `WBP_RoomList` | 2열 방 카드, 새로고침, 빈 목록 안내, 방 만들기 |
| `WBP_Shop` | 3열 상품 카드, 카테고리 탭, 원본 아이콘과 설명 미리보기, 보유 골드 |
| `WBP_Guide` | 8개 목차, 페이지 제목·번호, 스크롤 본문, 원본 보조 이미지 |
| `WBP_Record` | 현재 티어·누적 승리·진행도, 최근 경기 표, 승패 색상 |

관련 `WBP_RoomItem`, `WBP_ShopEntry`, `WBP_ShopPreviewPanel`, `WBP_RecordEntry`, `WBP_CreateRoomPopup`, `WBP_ConnectingPopup`도 같은 스타일로 정리했습니다. 방 이름 입력 등 기존 동작을 담당하는 바인딩은 유지했습니다.

## 이미지와 데이터 소유권

- 티어 그림은 `DA_Record.TierEntries`에서 읽습니다.
- 가이드 본문 96개 번역과 보조 이미지는 `DA_Guide`에서 읽습니다. 이미지의 가로세로 비율을 유지하며 표시하고 그림 속 문구는 원본 그대로입니다.
- 판도라·스킨 그림은 상품 데이터 애셋이 소유합니다. 이번 작업은 아이콘을 교체하지 않습니다. 추후 교체할 때는 상품 데이터 애셋의 아이콘 참조를 수정합니다.
- 화면에서 번역하는 문구는 공통 `DT_MenuText`에 모으고, 화면 배치와 색상은 각 WBP에서 관리합니다. 상세 편집 방법은 [MenuLocalization.md](MenuLocalization.md)에 있습니다.

작업 전후 `DA_Guide`, `DA_Record`, 판도라 33종, 스킨 14종의 SHA-256을 비교했습니다. 49개 모두 일치합니다. 결과는 `Saved/Codex/MenuStyle/content-preservation.json`에 있습니다.

## 재적용과 검증

`Scripts/restyle_menu_screens.mjs`는 에디터 MCP를 통해 네 화면과 하위 위젯을 편집·컴파일·저장하는 작성 스크립트입니다. `restyleMenuScreens({UE, fs, projectDir, only})`로 실행하며 `UE`는 MCP `call_tool`의 반환값을 풀어주는 어댑터입니다. 게임 실행에 이 스크립트나 MCP는 필요하지 않습니다.

`Scripts/menu_screen_translations.mjs`, `menu_product_translations.mjs`, `menu_product_descriptions.mjs`를 `menu_translations.mjs`가 합쳐 CSV를 생성합니다. 상품 키와 영문 원본은 `menu_product_sources.json`에 있습니다. 직접 수정한 CSV를 유지하려면 생성 스크립트에도 같은 수정 사항을 반영해야 합니다.

검증 결과:

- `LabProjectEditor Win64 Development` 빌드 성공.
- `LabProject.UI.Localization`의 CatalogCoverage, Fallback, GuideCompatibility 테스트 성공.
- PIE에서 `Scripts/verify_menu_screens.py`로 네 화면과 방 만들기 팝업의 12개 언어 전환 검사 성공.
- 실제 상품 선택 유지, 상품 카드·미리보기의 이름/설명/원본 아이콘 연결 확인.
- 가이드 8페이지 × 12개 언어 본문과 원본 이미지 일치 확인. 기존 검증 스크립트로 언어 변경 시 현재 페이지 유지도 확인.
- 전적 5개 행의 수치 유지와 승패 번역 확인.
- 실제 게임 설정 UI에서 영어 선택 후 상점 반영 확인. 원래 한국어 설정으로 복원.
- 방 만들기 팝업의 입력 안내·취소 확인. 구매, 실제 방 생성/참가, 전체 프로젝트 패키징은 수행하지 않았습니다.

실행 결과는 `Saved/Codex/MenuStyle/runtime-*.json`, 로그는 `Saved/Logs/MenuStyleFinal.log`에 있습니다. 최종 화면은 아래 파일입니다.

- `output/menu-style/WBP_RoomList-ko.png`
- `output/menu-style/WBP_Shop-ko.png`
- `output/menu-style/WBP_Shop-en.png`
- `output/menu-style/WBP_Guide-ko.png`
- `output/menu-style/WBP_Record-ko.png`
- `output/menu-style/WBP_CreateRoom-ko.png`
