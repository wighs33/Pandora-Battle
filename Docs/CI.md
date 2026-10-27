# 빌드·테스트·배포 자동화 (GitHub Actions)

이 PC를 GitHub Actions 자체 호스팅 러너로 쓰고, 푸시마다 테스트하고 매일 밤 패키징합니다.

| 워크플로 | 언제 | 하는 일 |
| --- | --- | --- |
| [`ci.yml`](../.github/workflows/ci.yml) | `master`에 푸시할 때, 손으로 실행할 때 | 에셋 동기화 → 빌드 번호 → 에디터 빌드와 Unit·Integration 테스트 → 백엔드 테스트 → 리포트 업로드 |
| [`nightly.yml`](../.github/workflows/nightly.yml) | 매일 03:00(KST), 손으로 실행할 때 | 에셋 동기화 → 빌드 번호 → 클라이언트 Shipping 패키지 → 전용 서버 빌드·쿡·스테이징 → (선택) Anywhere 서버 재시작 |

## 구성

- **에셋**: 공개 저장소에는 로직 애셋만 있습니다(`.gitignore`의 Content 정책). 러너는 체크아웃한 뒤 [`Scripts/SyncBuildMachineContent.ps1`](../Scripts/SyncBuildMachineContent.ps1)로 이 PC의 전체 프로젝트(`PD_ASSET_SOURCE`)에서 git이 추적하지 않는 Content·Plugins 파일만 복사합니다.
  - git이 추적하는 파일은 덮어쓰지 않으므로, 체크아웃한 커밋의 버전이 그대로 쓰입니다.
  - 처음 한 번은 약 9 GB(1.6만 개)를 복사하고, 이후에는 바뀐 파일만 복사합니다.
- **빌드 번호**: [`Scripts/StampBuildVersion.ps1`](../Scripts/StampBuildVersion.ps1)이 커밋 수와 커밋 해시를 `Source/LabProject/Common/BuildVersionStamp.h`(git 제외)에 씁니다. 같은 커밋은 푸시 빌드든 야간 빌드든 같은 번호입니다.
  - 타이틀 화면 오른쪽 아래와 시작 로그에 `v1.0.0.0 · #123 · ab4a0f2`처럼 나옵니다. 앞의 버전은 프로젝트 설정의 `ProjectVersion`입니다.
  - 스탬프 없이 빌드한 개발 빌드는 `v1.0.0.0 · dev`로 나옵니다.
- **패키지**: [`Scripts/PackageClient.ps1`](../Scripts/PackageClient.ps1)이 런처 엔진 UAT로 Win64 Shipping을 만듭니다. 용량이 커서 GitHub에 올리지 않고 `PD_BUILD_ARCHIVE\<빌드 번호>`에 남기며, UAT 로그만 산출물로 올립니다.
- **서버**: [`Scripts/StageServer.ps1`](../Scripts/StageServer.ps1)이 [GameLift 호스팅과 백엔드](GameLift_Backend.md#서버-빌드쿡스테이징)의 세 단계(소스 엔진 빌드 → 런처 엔진 쿡 → 소스 UAT 스테이징)를 합니다.
  - 스테이징 폴더의 서버가 실행 중이면 덮어쓰지 않고 멈춥니다. 접속한 플레이어가 있을 수 있기 때문입니다.
  - 손으로 실행할 때 `restart_anywhere`를 켜면 서버를 끄고 스테이징한 뒤 Anywhere 서버를 새 빌드로 다시 띄웁니다. 빈 서버일 때만 켭니다(강제 종료하면 활성 세션이 몇 분 남습니다).

## 러너 준비 (한 번)

1. **설치 위치**: 짧은 경로에 둡니다(예: `D:\ar`). 작업 폴더가 `D:\ar\_work\Pandora-Battle\Pandora-Battle`이 되는데, 경로가 길면 쿡이 Windows 경로 길이 제한에 걸립니다.
2. **내려받기와 등록**: [GitHub 러너 릴리스](https://github.com/actions/runner/releases)에서 `actions-runner-win-x64-<버전>.zip`을 받아 풉니다. 등록 토큰은 저장소 관리자 권한으로 받습니다.
   ```powershell
   $Token = gh api -X POST repos/wighs33/Pandora-Battle/actions/runners/registration-token --jq .token
   .\config.cmd --url https://github.com/wighs33/Pandora-Battle --token $Token --name labproject-pc --labels unreal --work _work --unattended
   ```
3. **환경 변수**: 러너 폴더의 `.env`에 씁니다.
   ```text
   UE_ROOT=C:\UE\UE_5.8
   PD_SOURCE_ENGINE=D:\UE_5.8_Src
   PD_ASSET_SOURCE=C:\Users\<사용자>\Desktop\Study\LabProject
   PD_BUILD_ARCHIVE=D:\CI\Builds
   ```
4. **실행**: 사용자 계정으로 `run.cmd`를 띄워 둡니다. 서비스 계정(NETWORK SERVICE)으로 돌리면 AWS CLI 프로필(`gamelift-dev`)과 사용자 DDC를 쓰지 못합니다.

## 보안

저장소가 공개이므로 다음을 지킵니다.

- 워크플로에 `pull_request` 트리거를 두지 않습니다. 외부 기여자의 코드가 이 PC에서 실행되지 않게 하기 위해서입니다.
- 저장소 설정 → Actions → General → "Fork pull request workflows from outside collaborators"를 **Require approval for all outside collaborators**로 둡니다. 누군가 PR에서 워크플로 파일을 고쳐 트리거를 더해도 승인 없이는 돌지 않습니다.
- AWS 자격 증명은 저장소 비밀에 넣지 않습니다. 러너가 사용자 계정의 AWS CLI 프로필을 그대로 씁니다.

## 알아 둘 점

- 러너는 한 대라 CI와 야간 빌드가 같은 동시 실행 그룹(`unreal-build-machine`)으로 줄을 섭니다.
- 작업 폴더는 실행이 끝나도 지우지 않습니다(`clean: false`). 동기화한 에셋과 빌드 산출물을 다음 실행에서 다시 쓰기 위해서입니다.
- 작업 중인 에디터와 CI가 동시에 돌면 CPU를 나눠 씁니다. 테스트는 시간 제한 없이 조건으로 기다리므로 결과는 같지만 느려집니다.
