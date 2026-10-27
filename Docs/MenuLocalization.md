# 공통 게임 설정과 메뉴 번역

`WBP_Title`, `WBP_Lobby`, `WBP_RoomList`, `WBP_Shop`, `WBP_Guide`, `WBP_Record`의 **게임 설정** 버튼은 같은 `WBP_GameSettings`를 엽니다. 언어와 전체 음량을 설정할 수 있습니다. 로비의 별도 Match settings 버튼은 요청에 따라 제거했습니다.

지원 언어는 기존 가이드와 같은 12개입니다: 한국어, 영어, 일본어, 중국어 간체/번체, 스페인어(스페인), 러시아어, 포르투갈어(브라질), 독일어, 프랑스어, 폴란드어, 터키어. 언어 목록에는 각 언어의 자국어 이름이 표시됩니다.

## 수정할 곳

| 역할 | 위치 |
| --- | --- |
| 버튼, 제목, 목차, 툴팁, 팀 색상, 맵 이름, 경고와 설정 문구 | `/Game/UI/Localization/DT_MenuText` |
| 번역표 CSV | `Content/UI/Localization/Source/DT_MenuText.csv` |
| 가이드 8개 페이지의 본문과 이미지 | 기존 `/Game/Data/DA_Guide` |
| 상점의 판도라 33종·스킨 14종 이름과 설명 | `DT_MenuText`의 `Product.<애셋 전체 경로>.Name` / `.Description` 행 |
| 판도라·스킨 아이콘, 가격과 상품 동작 | 기존 상품 데이터 애셋 |
| 티어 기준과 티어 이미지 | 기존 `/Game/Data/DA_Record` |
| 위젯 이름 → 번역 키 연결 | 각 WBP의 Class Defaults → UI / Localization → Menu Text Bindings, Menu Tooltip Bindings |
| 공통 설정 팝업의 디자인 | `/Game/UI/Widget/Settings/WBP_GameSettings` |
| 기본 언어, 번역표, CJK/서양어 폰트, 설정 위젯 클래스 | Project Settings → Game → Menu Localization (`Config/DefaultGame.ini`) |

메뉴 번역은 데이터테이블에서 직접 수정하거나 CSV를 편집해서 가져올 수 있습니다. **한 행은 문구 키, 12개 열은 언어별 번역**입니다. 번역에 사용되는 키는 바꾸지 않는 것이 좋습니다. 새 문구를 추가하면 번역표에 행을 추가하고 해당 위젯의 바인딩에 연결합니다. 플레이어가 입력한 닉네임과 게임 브랜드는 번역 대상이 아닙니다. 가이드 이미지 속에 포함된 글자는 기존 이미지 콘텐츠를 유지합니다.

현재 공통 번역표는 192개 키 × 12개 언어입니다. 상품 애셋을 이동하거나 이름을 바꾸면 해당 상품의 번역 키도 갱신합니다. 번역 행이 없는 새 상품은 원본 데이터 애셋의 이름·설명을 표시합니다. CSV 행 키에는 공백을 사용하지 않습니다. UE 임포터가 공백을 제거하므로 맵 표시 키는 `Map.WindNest`로 통일하고, 런타임 조회에서 `Map.Wind Nest`를 정규화합니다. 세션/RPC에 사용하는 맵 식별자는 그대로 유지합니다.

CSV를 수정한 뒤에는 PIE를 종료하고 UE의 Output Log에서 다음을 실행합니다. 프로젝트 경로가 다르면 경로를 바꿉니다. 임포트 스크립트는 CSV와 애셋의 키가 일치하는지, 저장에 성공했는지 검사합니다.

```text
py "C:/Users/whgus/Desktop/Study/LabProject/Scripts/import_menu_translations.py"
```

`Scripts/menu_translations.mjs`는 이번 초기 번역을 생성한 원본입니다. 이 파일을 고쳤다면 `node Scripts/menu_translations.mjs`로 CSV를 다시 생성한 뒤 위 임포트 명령을 실행합니다. 데이터테이블이나 CSV에서 따로 수정한 내용을 유지하려면 생성 스크립트를 다시 실행하지 않거나, 수정 사항을 스크립트에도 반영해야 합니다.

## 코드의 역할

- `UMenuLocalizationSubsystem`: GameInstance 단위의 현재 언어, 로컬 저장, 번역 조회와 변경 알림을 담당합니다. UI별 언어 상태를 따로 두지 않습니다.
- `ULocalizedMenuWidget`: 각 에셋의 바인딩을 읽어 텍스트/툴팁을 갱신하고 폰트를 적용합니다. 화면 생성 시 적용하고, 언어 변경 이벤트에 구독/해제합니다.
- `UGameSettingsWidget`: 언어/음량 입력과 팝업 닫기만 담당합니다. 모달 입력은 기존 `UUiSubsystem`, 음량은 기존 `UAudioSettingsSubsystem`을 사용합니다.
- `UGuideWidget`: 선택한 공통 언어에 맞는 `DA_Guide.LocalizedContent`를 읽습니다. 가이드 자체의 콤보박스와 문자열 기반 언어 파서는 제거했습니다. 현재 페이지는 유지합니다.
- 로비의 동적 경고, 맵 이름, 시작 문구와 참가자 팀 선택 표시도 공통 번역을 조회합니다. 팀 콤보의 내부 `Red/Blue/...`와 맵 키는 번역하지 않아 서버 요청의 식별자가 달라지지 않습니다.
- 상점 카드와 미리보기는 `GetProductText`로 상품 이름·설명을 조회합니다. 가격, 보유 상태, 구매 결과 메시지도 언어 변경 시 갱신하며 선택한 상품과 상품 데이터는 유지합니다.
- 방 항목, 방 만들기/연결 팝업, 전적 행도 `ULocalizedMenuWidget`을 사용합니다. 전적은 행을 다시 만들지 않고 승패·제목을 갱신하므로 수치와 스크롤 상태를 유지합니다.

`EGuideLanguage`는 `Settings/UiLanguage.h`로 옮겼지만 리플렉션 이름과 숫자 값을 유지했습니다. 기존 `DA_Guide`의 96개 본문 번역을 다시 입력하거나 에셋을 마이그레이션할 필요가 없습니다. `DA_Guide`의 기존 언어 목록/개별 폰트 필드는 에셋 호환성을 위해 남아 있으며, 화면의 언어 선택과 폰트는 공통 설정이 담당합니다.

선택한 언어가 비어 있는 번역은 영어, 한국어 순으로 대체합니다. 언어는 `Saved/SaveGames/MenuLanguageSettings.sav`에 저장하며 맵 이동과 재실행 후에도 유지합니다. 기본 언어는 한국어입니다. 서버의 언어나 상대 플레이어의 설정은 바꾸지 않습니다. 엔진 전체 culture를 바꾸는 대신 요청된 메뉴의 번역 데이터를 직접 선택하는 구조입니다.

긴 메뉴 문구에는 ScaleBox를 적용했습니다. CJK 언어는 프로젝트의 Noto Sans CJK, 나머지는 Roboto를 사용하여 폴란드어·터키어·키릴 문자도 표시합니다. 자유 입력 닉네임의 폰트는 기존 에셋 설정을 유지합니다. UE 5.8의 `UEditableTextBox::SetWidgetStyle`은 전달된 스타일 주소를 Slate에 보관하므로 임시 스타일 구조체를 전달하지 않습니다.

## 검증

UE 5.8.2의 `LabProjectEditor Win64 Development` 빌드와 다음 자동 테스트를 실행했습니다.

```text
Automation RunTests LabProject.UI.Localization
```

- `CatalogCoverage`: 메뉴 번역표의 모든 언어 셀 확인
- `Fallback`: 누락/공백 번역의 대체 언어 및 잘못된 저장 언어 검증
- `GuideCompatibility`: 기존 8개 페이지 × 12개 언어 데이터 보존

`Scripts/verify_menu_localization.py`는 PIE에서 열린 화면을 대상으로 12개 언어를 실제로 전환하고, 바인딩/툴팁/가이드 본문/설정 선택 상태/팀 옵션 보존을 검사합니다. 실제 UI에서 언어 선택, 자동 저장, 에디터 및 PIE 재실행 후 복원, 가이드와 로비의 설정 버튼, Escape 닫기, 번역된 팀 선택과 맵 전환도 확인했습니다. 결과는 `Saved/Codex/Localization`, 스크린샷은 `output/localization`에 있습니다.

새 번역표와 설정 화면은 항상 쿠킹할 디렉터리에 등록하고 12개 언어의 국제화 설정을 추가했습니다. 전체 프로젝트 패키징/배포 빌드는 이번 검증 범위에 포함하지 않았습니다.

네 메뉴의 스타일·원본 이미지 유지와 추가 실행 검증은 [MenuWidgetStyle.md](MenuWidgetStyle.md)를 참고하세요.
