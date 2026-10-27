# GAS / Pandora 입력·출처 정리 — 2026-09-19

실제 입력·출처 코드를 수정했다. 스킬별 Spec과 기존 실행 정책은 유지하고, 입력 Pending 두 개와 불필요한 성공 응답 대기를 제거했다. 신규 프로덕션 Manager, Registry, 복제 Context는 만들지 않았다.

1. **확인한 최신 상태와 변경 파일**

   기준 엔진은 로컬 UE 5.8.2, CL 56702186이다. `AttackAbility`, `RangedAttackAbility`, `GrappleAbility`, `SkillAbility`, `HitReactAbility`는 LocalPredicted, 부모 PdGameplayAbility는 ServerOnly다. C++ 정책 설정은 변경하지 않았다. Rider 자식 클래스 인덱스와 에디터 Python으로 Blueprint 12개의 실제 CDO를 읽었다. GA_Attack의 명시적 LocalPredicted 덮어쓰기를 확인했으며 ServerInitiated는 없다.

   GameFeatureData를 로드해 읽은 명시적 입력 태그는 GA_Grapple의 Input.Ability.Movement.Grapple이다. 스킬 태그는 Pandora Binder가 부여한다. 일반 공격은 ControllerInputComponent → CombatComponent → TryActivateAbilitiesByTag 및 별도 콤보 RPC 경로다. 장착/해제는 ServerOnly를 상속하지만 해제를 기다리는 입력 능력이 아니다. UsesInputRelease는 Press 데이터 스킬과 Grapple에서 참이다. 해당 입력 능력들은 InstancedPerActor다. HitReact의 InstancedPerExecution은 입력 대상이 아니다. 공격의 후속 콤보 재시작 및 자동사격 타이머는 이번 Pending과 다른 역할이므로 유지했다.

   프로덕션 변경 파일:

   - `Component/AbilitySystem/AbilityGrantAndInputManager.{h,cpp}`: Pending 제거, Release 즉시 처리, 동일 시전 검사, 직접 입력 RPC 연결.
   - `Component/AbilitySystem/PdAbilitySystemComponent.{h,cpp}`: 불필요한 override와 대기 제거, 입력 정리 및 이전 실패 응답 보호.
   - `Mode/PdPlayerController.{h,cpp}`: Pending Release만 처리하던 PostProcessInput override 제거. 엔진 기본 후처리는 상속된다.
   - `Pandora/PandoraSkillSource.{h,cpp}`, `Pandora/PandoraSkillBinder.cpp`, `Component/Pandora/PandoraComponent.h`, `Component/Pandora/PandoraComponentSkillSources.cpp`, `AbilitySystem/Ability/PdGameplayAbility.cpp`: 중복 Source 레벨 제거, 관련 생성 인자 정리, Source 수신 시 UI 직접 알림.
   - `AbilitySystem/Ability/EquipAbility.cpp`, `HitReactAbility.cpp`: 이전 ServerInitiated 정책을 설명하던 주석만 수정.

   검증 파일: `Tests/GasInputTestAbility.h`, `Tests/GasInputTests.cpp`, `Tests/GasInputNetworkTest.{h,cpp}`, `Scripts/audit_gas_pandora.py`, `Scripts/test_gas_input_network.py`. 테스트 클래스는 실제 게임 맵·기본 설정에 연결하지 않았다.

2. **PendingRemoteActivations**

   멤버, Contains/Add/Remove/Reset, 전용 Manager NotifyAbilityActivated 및 ASC override를 제거했다. 비활성 대상은 곧바로 TryActivateAbility한다. 실패 정리와 해제를 쓰지 않는 능력의 InputPressed 정리는 유지했다. ServerInitiated 대응 대기를 새로 만들지 않았다.

3. **PendingHoldReleases**

   멤버와 프레임 후처리 함수를 Manager/ASC/Controller에서 제거했다. Released는 ASC를 받아 누름 기록을 소비하고 즉시 전달한다. 정지·UI 차단을 이유로 Release 기록 소비를 막지 않는다. Owner/Avatar/로컬 제어가 유효하지 않으면 기록과 InputPressed는 정리하고 콜백은 전달하지 않는다.

4. **유지한 입력 기록과 콜백 안전성**

   HoldAbilityHandlesByInputTag는 누를 당시 대상을 기억한다. Release에서 현재 태그로 다시 검색하지 않는다. ClearAbilityInput/ClearAllAbilityInputs 및 종료·실패·회수·사망 호출을 유지했다. 실패·종료 시 Spec.InputPressed도 정리하며, 늦은 ClientActivateAbilityFailed는 해당 인스턴스의 예측 키가 일치할 때만 입력 기록을 지운다.

   Release 함수 안에서 모든 기존 InputPressed를 먼저 내리고, Handle/인스턴스/예측 키를 스냅샷으로 잡는다. 임시 OnAbilityEnded 구독으로 이번 호출 중 종료된 대상을 무효화한다. 예: A의 Release → B 종료 → B 재활성화 → B 처리 순서에서 같은 Handle·동일 인스턴스·유효하지 않은 예측 키여도 이전 Release를 전달하지 않는다. 이 상태는 호출이 끝나면 사라진다.

   SendInputToActiveAbility도 LocalInputConfirm → AbilitySpecInputReleased → InvokeReplicatedEvent 사이에 재조회한다. LocalInputConfirm 또는 InputReleased가 종료·재활성화할 수 있어, 인스턴스·예측 키 비교와 호출 중 종료 기록이 필요하다. AbilityListLock은 콜백 중 회수에 의한 Spec 무효화를 막는다. 확인 중 Owner/Avatar가 사라졌는지도 다시 검사한다.

5. **UPandoraSkillSource는 축소 후 유지**

   런타임 객체를 DataAsset으로 합치는 대안을 검토했지만 현재 콘텐츠가 실제로 SkillDefinition을 공유한다. Angel/Light의 HolyBall, Machine/Nature의 DefenceField, Darkness/Steel의 BlackHole이 그 예다. DataAsset을 SourceObject로 직접 공유하면 출처별 쿨다운 조회와 EffectSource 기반 회수 범위가 합쳐진다. 이를 복구할 Registry나 새 복제 테이블을 만드는 대신 기존 개별 출처 식별을 유지했다.

   Source의 PandoraLevel은 Binder가 Spec.Level에도 넣는 슬롯의 요구 레벨을 복사했고, 유일한 C++ 읽기도 Initialize에 되돌려 넣는 용도였다. 저장 필드·복제·getter·생성 인자를 제거했다. 실제 투자 레벨과 의미가 같다고 가정해 합친 것이 아니다.

6. **최종 데이터 위치**

   | 정보 | 위치와 의미 |
   |---|---|
   | 부여 항목의 판도라·스킬 인덱스 | Source.PandoraDefinition / SkillIndex. 부여 정체성(A) |
   | SkillDefinition | PandoraDefinition.Skills의 기존 강한 에셋 참조. Source는 인덱스로 조회하며 복사하지 않음(A/E) |
   | 부여 능력 레벨 | Spec.Level. 현재 Binder는 슬롯 요구 레벨로 설정(A) |
   | 판도라 투자/해금 레벨 | 기존 PandoraTreeComponent. Spec.Level과 별개(B/D) |
   | 현재 선택·배치 | 기존 PandoraComponent(B) |
   | 시전에 사용할 방향 | 기존 Source.LoadoutDirection. 서버 PreActivate에서 확정하고 복제(C) |
   | 사용 횟수 | 기존 SkillAbility.UsesSinceCooldown. 시전 사이 유지(D) |
   | 타이머·진행 중 액션 | 기존 SkillAbility 인스턴스(C) |
   | 효과 출처 식별 | 개별 SourceObject 포인터(A) |
   | 출처 수명·서브오브젝트 복제 | 기존 PandoraComponent의 소유 배열/등록·해제. 실제 Spec 회수 콜백까지 보존 |

   Spec.SourceObject → UPandoraSkillSource → USkillDefinition 연결은 유지한다. 기존 DataAsset 직접 연결 경로도 유지한다. 활성화 전에는 Handle/ActorInfo의 Spec으로 조건·비용·쿨다운을 조회하며, CDO에 시전 데이터를 쓰지 않는다. 현재 선택 변경은 기존 비선택 시전의 Source를 바꾸지 않는다. 로딩·쿠킹·에셋 참조 유지 경로를 변경하지 않았고 새 복제 데이터 연결도 도입하지 않았다.

7. **ActivationsWaitingForSource 제거 근거**

   UE의 AbilitySystemComponent_Abilities.cpp에서 ClientActivateAbilitySucceedWithEventData를 확인했다. LocalPredicted 분기는 이미 존재하는 예측 인스턴스를 예측 키로 찾아 ConfirmActivateSucceed만 호출한다. 이 분기에서는 Source를 읽어서 새 실행을 시작하지 않는다. 현재 Pandora 부여는 고정 USkillAbility/LocalPredicted이고, 서버 통지로 Source 의존 실행을 시작할 ServerInitiated 경로가 없다.

   배열, 재개 함수, IsAbilitySourceReady, ClearPendingActivation, 관련 성공·종료·취소 RPC override, 회수/사망 정리, ASC NotifyAbilitySourceReady를 제거했다. GAS 엔진의 Spec 미도착 Pending은 그대로다. **활성화 전 IsSourceReady 검사는 유지했다.** Source 도착 시 PandoraComponent가 스킬바 알림을 보내고, ASC OnRep_ActivateAbilities의 UI 알림도 유지한다. 데이터를 못 읽어도 실행을 버리고 끝내는 새 경로를 넣지 않았다.

8. **클라이언트·서버 입력 흐름**

   클라이언트 Press → 누름 당시 Handle 기록 → Spec.InputPressed=true → 로컬 예측 활성화/GAS 서버 요청 → Release에서 기록 소비 → 타기팅 확정 → 직접 복제 설정이 켜져 있으면 ServerSetInputReleased → 로컬 AbilitySpecInputReleased → 같은 시전이면 Generic Event → 기존 종료 규칙.

   서버에서는 GAS 활성화 요청을 검증·실행하고, 직접 입력 RPC가 AbilitySpecInputReleased를 호출한다. 호스트는 로컬 콜백으로 직접 처리한다. SkillAbility의 bReplicateInputDirectly=true를 유지하면서 누락되었던 ServerSetInputPressed/Released 호출을 엔진 AbilityLocalInput*와 동일한 위치에 연결했다.

   AbilitySpecInput*와 InvokeReplicatedEvent 자체는 RPC가 아니다. Grapple은 직접 복제를 켜지 않았고 기존 WaitInputRelease 태스크가 ServerSetReplicatedEvent로 전달한다. 직접 입력 RPC는 Generic Event를 발생시키지 않는다. 기존 입력 Task 경로를 추가로 복제하거나 대체하지 않았다.

9. **효과 제거 범위**

   RemoveGrantedContent의 Query.EffectSource = Spec.SourceObject와 호출 대상 ASC는 그대로다. 동일 SkillDefinition을 공유하는 A/B를 부여해 A 회수 시 같은 ASC의 A 효과만 사라지고 B 효과와 다른 ASC의 A 효과가 유지되는 자동 테스트를 추가했다. 재부여 시에도 새로운 출처 객체를 사용한다.

   Ability가 만드는 피해/상태 효과와 MakeOutgoingGameplayEffectSpec의 기존 Source 연결, 투사체로 넘기는 EffectSpec은 변경하지 않았다. 일부 SelfBuff/Aura 경로는 SkillDefinition, EffectArea/OmenOrb는 자기 액터를 EffectSource로 쓰며 자기 효과 핸들/액터 수명으로 정리한다. 이들을 Source Query의 제거 대상으로 새로 포함시키지 않았다.

10. **검증 결과와 범위**

   LabProjectEditor Win64 Development 단계별 빌드 성공. 최종 자동 테스트 결과와 개별 항목은 `Saved/Automation/GasRefactor/index.json`, `Saved/Logs/GasRefactorTests.log`에 있다.

   - 입력 테스트: 빠른 탭, Release 직후 Press, 기록 1회 소비, 바인딩 교체, 종료 후 재시전, 실패·즉시 종료·회수·사망·Avatar 소실, 여러 대상 콜백, 무효 예측 키, LocalInputConfirm 종료·재시전, 늦은 실패 응답.
   - Pandora 테스트: 공유 에셋의 출처/효과 분리, 재부여, Source/Spec 수신 UI 알림, 활성화 전 준비 검사, 실제 SkillAbility의 MinimumHoldSeconds 및 대기 액션을 포함한 Duration, 비용·횟수·쿨다운·취소.
   - 별도 프로세스 리슨 호스트/원격 클라이언트: 기본 조건 및 PktLag=150ms/PktLoss=10%에서 빠른 Release → 재Press 3회. 클라이언트 EndAbility 복제를 의도적으로 끈 테스트 능력에서 서버 InputReleased 3회를 로그로 직접 확인했다. 이 테스트는 실제 PdASC/입력 Manager와 엔진 RPC를 사용한다. 원래 게임의 모든 스킬을 검증한 것은 아니다.
   - 에셋 감사: 실제 Blueprint CDO 12개, PandoraDefinition 33개, GF_Pandoras의 부여 항목 로드. 결과는 `Saved/Automation/GasPandoraAudit.json`. 프로젝트 Content/Plugins의 uasset/umap 직렬화 이름 검색에서 삭제한 GetPandoraLevel/ProcessPendingInputReleases 및 Source 클래스 참조는 발견되지 않았다. PandoraLevel 문자열의 UI 검색 결과는 First/Second/ThirdPandoraLevel 위젯 이름으로, 삭제 필드와 관계없다. 에셋을 저장하거나 변경하지 않았다.

   미실행: 실제 전투 맵에서 공격 몽타주·그래플 타기팅·모든 Pandora 액션의 수동 플레이, Steam 연결, 실제 Seamless Travel/부활 화면 전환, 패키징/전체 쿠킹, 네트워크상 Pandora Source와 선택 데이터의 모든 도착 순서 조합. Avatar 소실·사망과 로컬 Source 수신은 자동 테스트로만 검증했다. 리슨 테스트 로그에는 이번 변경과 관계없는 Toolset Python 플러그인 초기화 오류도 있으므로 전체 엔진 로그가 무오류라고 주장하지 않는다.

재실행 예:

```powershell
python Scripts/test_gas_input_network.py
python Scripts/test_gas_input_network.py --lag 150 --loss 10
# Unreal Editor Automation에서 LabProject.GAS 그룹 실행
# 에셋 감사: UnrealEditor-Cmd <project> -run=pythonscript -script=<Scripts/audit_gas_pandora.py> -unattended -nullrhi
```
