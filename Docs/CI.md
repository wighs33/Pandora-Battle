# 빌드·테스트 자동화

푸시와 PR마다 GitHub 호스팅 러너가 백엔드 단위 테스트를 돌립니다. Unreal 빌드·자동화 테스트·패키징은 전체 에셋이 있는 개발 PC에서 `Scripts/`의 스크립트로 직접 돌립니다.

## GitHub Actions

| 워크플로 | 언제 | 하는 일 |
| --- | --- | --- |
| [`ci.yml`](../.github/workflows/ci.yml) | `master`에 푸시할 때, PR을 열거나 갱신할 때 | `ubuntu-latest`에서 `python -m unittest discover -s Backend/tests` |

- 백엔드 테스트는 표준 라이브러리만 쓰므로 설치 단계가 없습니다.
- 권한은 `contents: read`만 줍니다. 비밀 값을 쓰지 않으므로 외부 PR에서 돌아도 안전합니다.

### 자체 호스팅 러너를 뺀 이유

처음에는 이 PC를 자체 호스팅 러너로 써서 푸시마다 에디터 빌드·Unreal 테스트를, 매일 밤 Shipping 패키지와 전용 서버 스테이징을 돌렸습니다.

- 공개 저장소에는 로직 애셋만 있어, 러너가 이 PC의 전체 프로젝트에서 Content를 복사해 와야 했습니다(약 9 GB).
- 러너 PC가 꺼져 있으면 작업이 줄만 서고 끝나지 않았습니다. 실제로 푸시·야간 작업이 한 번도 끝까지 돌지 못했습니다.

그래서 언제나 돌 수 있는 백엔드 테스트만 CI에 두고, 나머지는 같은 스크립트를 개발 PC에서 직접 돌리는 도구로 남겼습니다(`da2f77b`).

## 개발 PC에서 돌리는 스크립트

| 스크립트 | 하는 일 |
| --- | --- |
| [`RunAutomationTests.ps1`](../Scripts/RunAutomationTests.ps1) | 에디터 빌드 후 자동화 테스트를 단계별로 돌리고 리포트(`index.json`)와 로그를 남깁니다. 실패하거나 테스트가 하나도 돌지 않으면 종료 코드 1 |
| [`StampBuildVersion.ps1`](../Scripts/StampBuildVersion.ps1) | 커밋 수와 해시를 `Source/LabProject/Common/BuildVersionStamp.h`(git 제외)에 씁니다. 시작 로그에 `v1.0.0.0 · #123 · ab4a0f2`처럼 나옵니다 |
| [`PackageClient.ps1`](../Scripts/PackageClient.ps1) | 런처 엔진 UAT로 Win64 Shipping 배포 패키지를 만들고 보관합니다 |
| [`StageServer.ps1`](../Scripts/StageServer.ps1) | 전용 서버를 빌드·쿡·스테이징하고, 원하면 GameLift Anywhere 서버를 새 빌드로 다시 띄웁니다([GameLift 호스팅과 백엔드](GameLift_Backend.md)) |
| [`RunPerfBenchmark.ps1`](../Scripts/RunPerfBenchmark.ps1) | 보스 레이드 대규모 소환의 서버·클라이언트 부하를 재고 `results.json`(원하면 Insights 트레이스)을 남깁니다([보스 레이드 성능](BossRaid_Performance.md)) |
| [`StageLunaDepot.ps1`](../Scripts/StageLunaDepot.ps1) | Luna 로컬 모델을 Steam 무료 DLC 데포로 올릴 수 있게 스테이징합니다([Luna 대화](LunaChat.md)) |

예시:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\RunAutomationTests.ps1 -EngineRoot C:\UE\UE_5.8
powershell -ExecutionPolicy Bypass -File Scripts\RunAutomationTests.ps1 -EngineRoot C:\UE\UE_5.8 -Suite Unit -SkipBuild
```
