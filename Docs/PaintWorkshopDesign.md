# Paint Workshop UI

`/Game/UI/Widget/WBP_PaintCanvas`의 기준 디자인 공간은 **1280 × 800**이다.
UMG 기본 RoundedBox/Image, 색 점, 짧은 붓 자국을 조합한 어두운 작업실 디자인이며 장식 텍스처를 사용하지 않는다.

```text
Info_PaintRoot                 기존 CanvasPanel 이름/타입 유지
└─ ScaleBox_Root               전체 영역에 stretch anchors, Scale To Fit / Both
   └─ SizeBox_DesignSpace       1280 × 800
      └─ Overlay_Root
         ├─ Workshop_Backboard
         ├─ Workshop_PaintMarks
         └─ Workshop_MainContent
            ├─ Workshop_Header
            ├─ HorizontalBox_Workspace
            │  ├─ Workshop_CanvasArea
            │  └─ Workshop_PreviewColumn
            │     ├─ Workshop_Portrait_Card
            │     └─ Workshop_Speech_Card
            └─ Workshop_BottomHint
```

## Designer 크기 조절

부모에 배치한 `WBP_PaintCanvas`의 **Canvas Slot → Size X / Size Y**를 변경한다.
`Auto Size`는 끄고 `Render Transform → Scale`은 `(1, 1)`로 유지한다.
전체 프레임, 글자, 장식, 이미지가 같은 비율로 확대/축소되며, 외부 영역의 비율이 다르면 여백이 생긴다.
`SizeBox_DesignSpace`의 기준 크기는 개별 배치 인스턴스의 크기를 바꾸기 위해 수정하지 않는다.

세 런타임 Image는 각각 정사각형 SizeBox와 ScaleBox 안에 있다.
그림판의 실제 Image geometry를 사용하는 기존 포인터 좌표 계산과 얼굴 512 × 512 캡처, 정사각형 페인트 렌더 타깃을 유지한다.

## 계약과 연결

- `Info_PaintRoot`, `Img_Canvas`, `Img_FacePreview`, `Img_SpeechPreview` 유지.
- `Info_PaintTitle`, `Info_CanvasTitle`, `Info_FaceTitle`, `Info_SpeechTitle`, `Info_PaintHint` 및 기존 `Paint.*` 번역 바인딩 유지.
- 런타임 머티리얼/렌더 타깃 연결과 Blueprint EventGraph 변경 없음.
- 색 점은 장식이며 색 선택 버튼이 아니다.
- 닫기/말풍선/얼굴 적용 버튼은 기존 `WBP_Info` 소유다. 안내 문구와 겹치지 않도록 해당 세 버튼의 Y 위치만 960으로 조정했다.

## 확인한 범위

- `WBP_PaintCanvas`, `WBP_Info` Blueprint 컴파일 및 에셋 검증: 오류/경고 없음.
- PIE에서 외부 크기 1440 × 810, 1080 × 600, 800 × 810의 균일 스케일링 확인.
- 실제 그리기 입력과 세 이미지 갱신, 말풍선 출력, 얼굴 데칼 생성 확인.
- 새 PIE 세션에서 페인트 열기 → 닫기 → 다시 열기 확인.
- C++/런타임 로직 변경 없음. 전체 빌드 및 멀티플레이 회귀 검증은 수행하지 않음.
