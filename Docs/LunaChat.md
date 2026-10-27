# Luna 대화 (로컬 LLM)

타이틀의 안내자 Luna에게 질문을 입력하면, 같은 PC에서 도는 로컬 LLM(Gemma 3 4B)이 답을 만들어 말풍선에 보여 줍니다.
- 게임과 함께 배포한 모델 서버(llama.cpp의 `llama-server` + GGUF)를 게임이 직접 띄워 씁니다. 번들이 없으면 같은 PC의 Ollama에 묻습니다(개발용).
- 답은 토큰이 도착하는 대로 말풍선에 이어 쓰고, 그동안 Luna의 입 모양 morph target이 움직입니다.
- 게임 서버나 외부 API를 쓰지 않습니다.

## 흐름

```mermaid
sequenceDiagram
    participant P as 플레이어
    participant W as UTitleWidget
    participant H as ATitleHUD
    participant L as ULunaChatSubsystem
    participant O as 모델 서버 (llama-server 또는 Ollama)
    Note over L,O: 타이틀이 열릴 때 PrepareModel()이 llama-server를 미리 띄우고 /health로 준비를 확인
    P->>W: 입력창에 질문 + Enter
    W->>H: OnLunaQuestionSubmitted
    H->>H: 안내 문구 순환 정지, "생각 중" 말풍선(입 닫힘)
    H->>L: Ask(질문)
    L->>O: ① 절 고르기 (stream 끔, JSON 스키마로 {"sections":[번호]} 고정)
    O-->>L: 질문에 맞는 가이드 절 번호 (최대 2개)
    L->>O: ② 답 요청 (stream, 시스템 프롬프트 + 고른 절 + 최근 대화)
    O-->>L: 응답 조각 — NDJSON 또는 SSE (HTTP 스레드)
    L-->>H: OnReplyUpdated(지금까지의 답) (게임 스레드)
    H->>W: 말풍선 갱신, 입 움직임
    O-->>L: done
    L-->>H: OnReplyFinished
    H->>H: 읽는 시간만큼 표시 후, 15초 뒤 안내 문구 재개
```

| 코드 | 역할 |
| --- | --- |
| [`LunaChatSubsystem`](../Source/LabProject/Luna/LunaChatSubsystem.h) | 절 고르기와 답 요청, 스트리밍 수신, 대화 기록(최근 4쌍), 취소 |
| [`LunaLocalServer`](../Source/LabProject/Luna/LunaLocalServer.h) | 함께 배포한 `llama-server`를 자식 프로세스로 띄우고 `/health`로 준비 확인, 게임과 함께 종료 |
| [`LunaChatProtocol`](../Source/LabProject/Luna/LunaChatProtocol.h) | NDJSON·SSE 줄 해석, 가이드 절 나누기와 고른 절 번호 해석, `<think>` 구간 제거, 말풍선용 정리(자동화 테스트) |
| [`LunaChatSettings`](../Source/LabProject/Luna/LunaChatSettings.h) | 번들 폴더·실행 파일·모델·문맥 길이, Ollama 주소·모델, 온도, 최대 토큰, 시간 제한 (Project Settings → Game → Luna Chat) |
| [`TitleWidget`](../Source/LabProject/UI/Title/TitleWidget.cpp) | 입력창 생성(말풍선처럼 C++ 위젯 트리로), Enter 처리 |
| [`TitleHUD`](../Source/LabProject/Lobby/Contents/TitleHUD.cpp) | 말풍선 타이밍, 입 모양, 안내 문구와 대화의 전환 |

## 설계 포인트

- **근거가 있는 답:** 시스템 프롬프트의 게임 지식은 따로 쓰지 않고, 게임에 이미 있는 글만 넣습니다.
  - 가이드 메뉴 본문을 절로 나눠 두고, 질문마다 먼저 짧은 요청으로 맞는 절(최대 2개)을 고르게 합니다. 가이드 전체를 넣으면 질문마다 수천 토큰을 다시 처리하고, 작은 모델이 여러 절의 내용을 섞었기 때문입니다.
  - 절 고르기 응답은 JSON 스키마로 `{"sections":[번호]}` 형식을 고정합니다(Ollama는 `format`, llama-server는 OpenAI 호환 `response_format`). 절 이름에는 메뉴 언어 이름 옆에 영어 이름을 함께 적어, 한국어 질문에서도 절을 잘 고르게 했습니다.
  - 고른 절과 `DT_MenuText`의 Luna 안내 문구를 현재 메뉴 언어로 읽어 넣으므로, 가이드나 안내 문구를 고치면 Luna의 답도 따라 바뀝니다.
  - 근거에 없는 내용은 모른다고 말하고 가이드 메뉴를 권하게 했습니다.
- **함께 배포한 모델 서버:** 설치 폴더의 `Luna\`에 `llama-server.exe`와 GGUF 모델이 있으면 게임이 직접 띄웁니다.
  - 같은 PC의 빈 포트(127.0.0.1)에만 열고, Windows Job Object(`KILL_ON_JOB_CLOSE`)로 묶어 게임이 비정상 종료돼도 서버가 남지 않게 합니다.
  - 모델을 올리는 동안 `/health`가 503을 주므로, 200이 올 때까지 기다린 뒤 질문을 보냅니다. 준비 전에 온 질문은 기다렸다가 보냅니다.
  - 타이틀이 열릴 때 미리 띄우고, 타이틀을 떠나면 서버를 끝내 메모리를 돌려줍니다.
  - 문맥 길이를 4096으로 줄여, 모델 최대 길이로 잡힐 때 수 GB 더 쓰던 메모리를 아낍니다.
- **답하는 언어:** 플레이어가 어떤 언어로 물어도 메뉴 언어(12개 중 하나)로 답합니다. 언어가 바뀌면 이전 대화를 비웁니다.
- **스레드:** 스트림 콜백은 HTTP 스레드에서 옵니다.
  - 이 콜백에서는 잠금이 걸린 버퍼에 바이트만 쌓고, 해석과 UI 알림은 게임 스레드에서 합니다.
  - 완료 콜백이 마지막 조각보다 먼저 올 수 있어, 완료 시점에 남은 바이트를 먼저 처리합니다.
- **조각 경계:** 네트워크 조각은 줄이나 한글(UTF-8 3바이트) 중간에서 끊길 수 있습니다.
  - 바이트를 모아 두었다가 `\n`까지 온 줄만 해석합니다. `\n`은 UTF-8 다중 바이트 문자 안에 나오지 않습니다.
  - 테스트는 1바이트씩 넣어 모든 경계를 확인합니다.
- **취소:** 답하는 중에 새 질문이 오거나 타이틀을 떠나면 요청을 취소합니다. 요청 번호를 먼저 바꿔서 늦게 도착한 조각은 버립니다.
- **입력창 스타일:** UE 5.8의 `UEditableTextBox::SetWidgetStyle`은 Slate 위젯이 만들어진 뒤에는 인자의 주소를 Slate에 넘깁니다.
  - 그래서 Slate 위젯이 생기기 전(`NativeOnInitialized`)에 한 번만 설정합니다.
  - 플레이어가 어떤 언어로 쳐도 깨지지 않게 CJK 폰트를 씁니다.
- **말풍선 위치:** 꼬리는 말풍선 폭의 75% 지점에 두고, 그 꼬리 끝을 Luna 머리 위에 맞춥니다(`LunaSpeechTailFraction`).
  - 몸통이 주로 왼쪽으로 펼쳐집니다. 그래서 가장 긴 답(줄바꿈 폭 420 + 여백)도 오른쪽 위 게임 설정 버튼에 가리지 않습니다.
- **Luna 머티리얼:** 눈 깜빡임(`Key_1`)과 입(`Key_2`)은 모프 타깃이고, 얼굴이 있는 몸 머티리얼 영역에 걸려 있습니다.
  - 모프 타깃이 켜질 때 머티리얼에 "Used with Morph Targets"가 없으면, 엔진이 그 영역을 기본 머티리얼(회색)로 바꿉니다. 모프를 꺼도 되돌리지 않습니다.
  - 에디터 PIE에서는 이 플래그가 자동으로 켜지지만, 독립 실행(`-game`)과 패키징 빌드에서는 켜지지 않습니다. 그래서 피부가 가끔 회색으로 보였습니다.
  - 가져올 때 만들어진 부모(엔진 플러그인의 `FBXLegacyPhongSurfaceMaterial`)는 고칠 수 없습니다. 그래서 프로젝트에 복사본 `M_LunaSurface`를 만들어 플래그를 켜고, Luna 머티리얼 4개의 부모로 바꿨습니다.
  - Luna FBX를 다시 가져오면 부모가 원래대로 돌아가니, 그때는 `M_LunaSurface`로 다시 바꿉니다.

## 설치

### 배포 빌드: 함께 배포한 모델

- 게임은 설치 폴더(개발 실행에서는 프로젝트 폴더)의 `Luna\`에서 `llama-server.exe`와 `models\gemma-3-4b-it-Q4_K_M.gguf`를 찾습니다.
- 수 GB라 git에는 넣지 않고(`.gitignore`의 `/Luna/`), Steam에서는 무료 DLC 데포로 받습니다.
  - [`Scripts/StageLunaDepot.ps1`](../Scripts/StageLunaDepot.ps1)이 llama-server 실행에 필요한 파일과 모델만 골라 ContentBuilder에 스테이징하고 데포 스크립트를 만듭니다.
  - 업로드 폴더 안에 `Luna\`를 두므로, SteamPipeGUI로 올려도 설치 폴더의 `Luna\`에 들어갑니다.

### 개발용: Ollama

번들이 없으면 Ollama를 씁니다.

1. Ollama 설치: `winget install --id Ollama.Ollama --exact`
   - 설치하면 백그라운드에서 `127.0.0.1:11434` 서버가 실행됩니다.
2. 모델 받기: `ollama pull gemma3:4b`
   - 약 3.3GB입니다. 12개 메뉴 언어를 모두 다루는 다국어 모델이라 골랐습니다.
   - 이 PC의 네트워크에서는 Ollama 모델 저장소의 일부 IP(Cloudflare R2 `172.64.66.1`)가 응답하지 않습니다. 그래서 같은 가중치를 Hugging Face에서 받아 이름을 맞췄습니다.
     ```powershell
     ollama pull hf.co/ggml-org/gemma-3-4b-it-GGUF:Q4_K_M
     ollama cp hf.co/ggml-org/gemma-3-4b-it-GGUF:Q4_K_M gemma3:4b
     ```
3. 다른 모델을 쓰려면 Project Settings → Game → Luna Chat의 `Model`을 바꿉니다.
   - ini에서 주소를 바꿀 때는 `OllamaUrl="http://..."`처럼 따옴표로 감쌉니다. 따옴표 없는 `//`는 주석으로 잘립니다.

모델 서버를 띄우지 못했거나 Ollama가 꺼져 있으면 Luna가 "지금은 대답하기 어려워요"라고 말합니다. 원인은 `LogLunaChat` 경고에 남습니다.

## 확인 결과 (RX 7600 8GB)

- **속도(Ollama로 측정):** 모델이 GPU에 전부 올라가며(2.7GB), 초당 약 78토큰입니다.
  - 첫 질문은 모델을 올리느라 약 5초가 걸리고, 이후는 1초 안팎입니다.
  - `keep_alive` 15분 동안 메모리에 남습니다.
- **게임 안 확인:** 입력창에 영어로 "What is the training room for?"를 입력하고 Enter를 눌렀습니다.
  - 입력창이 비워지고, Luna가 메뉴 언어인 한국어로 훈련장 안내 문구에 근거한 답을 말풍선에 보여 주었습니다.
- **자동화 테스트:** `LabProject.Unit.Protocol.LunaStream`(Ollama NDJSON), `LunaOpenAiStream`(llama-server SSE), `LunaGuideSections`(절 나누기·고른 번호 해석) 3개가 통과합니다. 스트림 테스트는 1바이트씩 넣어 모든 조각 경계를 확인합니다.

콘솔 명령 `pd.Luna.Ask <질문>`으로 입력창 없이 질문할 수 있습니다. 답은 말풍선과 `LogLunaChat` 로그에 함께 나옵니다.
