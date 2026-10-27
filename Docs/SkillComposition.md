# 스킬 실행 구조

모든 판도라 스킬은 `USkillAbility`로 실행한다. `USkillDefinition.Action`이 동작과 순서를 정의하며 Ability 클래스 선택 필드는 없다. 스킬마다 별도의 AbilitySpec과 PandoraSkillSource로 입력, 레벨, 쿨다운과 출처를 구분한다.

## 책임과 설정

- SkillDefinition: UI, 비용, 시간, 활성화 정책, 공통 피해·상태 효과·연출·이동 설정과 액션 트리.
- SkillAbility: GAS 활성화, 한 번의 비용 확정, 입력 해제와 전체 시전의 종료·취소.
- SkillAction: 개별 동작. 에셋에 인라인으로 저장한 트리를 시전마다 복제해 실행한다.
- Skill/Actions: 조준, 투사체 시전, 오라, 검기, 미사일, 소환, 배치 액터와 보호막 동작.

투사체·범위·소환 등의 설정은 각 액션의 Settings에만 저장한다. 오라 회복은 해당 액션의 Healing에 저장한다. 타이머, 태스크, 실행 중 대상과 생성 액터는 실행 복사본이 관리한다.

## 조합과 컨텍스트

Sequence는 앞 액션이 완료되면 다음 액션을 시작한다. Parallel은 모든 자식이 완료되면 끝나며, 실패하면 진행 중인 다른 자식도 정리한다.

예: Sequence 아래에 Dash와 Fire Projectile을 배치하고, 투사체 OnImpact 아래에 Parallel로 Area Damage와 Spawn Actor를 연결한다.

FSkillActionContext로 대상과 위치를 전달한다. 돌진 완료, 게임플레이 이벤트와 투사체 적중은 다음 단계의 컨텍스트를 갱신한다. Parallel의 각 자식은 독립된 컨텍스트를 사용한다. 같은 애니메이션 슬롯이나 조준 카메라를 점유하는 동작은 Sequence로 배치한다.

Projectile Cast는 조준 확정·차징·소켓 연속 발사를 포함한 시전이다. Fire Projectile은 현재 컨텍스트로 발사하고 적중 후 액션을 연결하는 단순 동작이다. 몽타주 이벤트를 직접 조합하려면 이벤트 대기 Sequence를 Parallel의 첫 자식에, Play Montage를 다음 자식에 배치한다.

## 수명과 네트워크

Instant는 액션 트리가 완료되면 종료한다. Press는 입력을 놓으면 종료하며 Activation.MinimumHoldSeconds로 최소 유지 시간을 설정한다.

Duration 스킬은 USkillAbility의 활성화 시점부터 Time.Duration을 계산한다. 조준·준비·몽타주·소환물 상승·지연 피해도 이 시간에 포함되며, 늦게 시작한 액션이 종료 시점을 늘리지 않는다. 액션 트리가 먼저 완료되어도 스킬은 설정 시간까지 유지하고, 만료되면 실행 중인 액션과 유지 효과를 정리한 뒤 정상 종료한다. 액션에 남은 시간이 필요하면 GetRemainingDuration()을 조회한다. Wait와 Repeat는 자체 대기·반복 설정을 사용하지만 Duration의 공통 종료 시점을 넘겨 실행되지 않는다. Instant는 액션 트리 완료, Press는 입력 해제 정책을 유지한다.

비용과 쿨다운은 시전 시작에 한 번 확정한다. 액션이 다시 CommitSkill을 요청해도 중복 차감하지 않는다. 연속 사용 횟수는 Activation.UsesPerCooldown과 레벨 배율로 설정한다.

조준·몽타주·이동은 소유 클라이언트에서도 실행하며, 피해와 권한이 필요한 액터 생성은 서버가 처리한다. 클라이언트에서 액션이 먼저 완료되어도 최종 완료는 서버를 따른다. 스킬별 식별 태그는 AbilitySpec에 저장하고 활성 능력 조회와 취소에도 포함한다.

## 이전과 검증

기존 스킬 18개를 액션으로 전환하고 보호막 Blueprint의 기본값을 데이터 에셋에 반영한다. 참조가 사라진 전용 스킬 Ability와 Blueprint는 제거한다. 사망·피격·장비 전환 등 별도 시스템 Ability는 유지한다.

LabProject.Skill.Actions 테스트는 순서·병렬 실행, 실패 전파, 타이머 정리, 상태 분리, GAS 활성화·취소, 투사체 적중 컨텍스트와 같은 클래스의 스킬별 독립 실행을 검사한다. LabProject.AbilitySystem.Source는 판도라 선택 변경 후에도 부여된 스킬 출처가 유지되는지 검사한다.
