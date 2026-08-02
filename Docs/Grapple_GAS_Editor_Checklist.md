# Grapple GAS 에디터 체크리스트

이 문서는 C++ 구현 이후 Unreal Editor에서 직접 확인할 항목만 정리한다.
에디터 에셋은 자동 수정하지 않았다.

## 필수

1. 현재 실행 중인 Unreal Editor를 완전히 종료한다.
2. `LabProjectEditor Win64 Development`를 빌드한 뒤 에디터를 다시 연다.
   새 네이티브 `UGrappleAbility`, `ATargetActor_GrappleTrace`와 리플렉션 필드가
   추가되었으므로 이 단계에서는 Live Coding을 사용하지 않는다.
3. `/Game/Data/DA_CharacterAction`을 열고 다음 값을 확인한다.
   - `Grapple Hook > Cooldown Duration`: 원하는 쿨다운 시간. `0`이면 쿨다운이 적용되지 않는다.
   - `Grapple Hook > Input Action`: `/Game/Input/Action/IA_Grapple`
   - `Grapple Hook > Aim Camera`: 조준 중 사용할 FOV, 붐 오프셋, 카메라 회전, 보간 속도
4. `/Game/Input/DA_Input`을 열고 다음 참조를 확인한다.
   - `Grapple Input Action`: `/Game/Input/Action/IA_Grapple`
   - `Character Action Definition`: `/Game/Data/DA_CharacterAction`
5. `/GF_Pandoras/GF_Pandoras`를 열고 플레이어 대상 `Add Abilities`를 찾는다.
   - 기존 `Add Abilities`의 `Target Class`가 `/Game/Actor/Stickman/BP_Player`라면 그 액션을 사용한다.
   - 없다면 `Actions`에 `Add Abilities`를 하나 추가하고 `Target Class`를
     `/Game/Actor/Stickman/BP_Player`로 설정한다.
   - 네트워크 옵션에서 `Server`를 활성화한다. GameplayAbility 부여는 서버가 담당한다.
   - `Abilities`에 다음 엔트리를 추가한다.
     - `Ability`: `GrappleAbility` (`/Script/LabProject.GrappleAbility`)
     - `Level`: `1`
     - `Input Tag`: `Input.Ability.Movement.Grapple`
6. `BP_Player`의 상속된 `GrappleComponent`에서 필요하면 다음 물리 값을 조정한다.
   - `Trace Start Offset`
   - `Trace Distance`
   - `Trace Radius`
   - `Trace Channel`
   - `Move Duration`
   - `Move Start Delay`
   - `Hook Attach Delay`

## 추가할 필요가 없는 에셋

- `GA_Grapple` 블루프린트 어빌리티를 새로 만들 필요가 없다.

`UGrappleAbility`는 네이티브 클래스이므로 `GF_Pandoras`의 `Add Abilities`에서
직접 선택할 수 있다. Grapple은 해당 GameFeature가 활성화될 때만 부여되고,
비활성화될 때 함께 제거된다.

## 플레이 테스트

다음 경우를 PIE 리슨 서버 2인과 패키징된 Steam 2인 환경에서 각각 확인한다.

- F를 누르는 동안 조준 카메라와 크로스헤어가 표시된다.
- 유효한 지점을 보고 F를 놓으면 서버 재트레이스 후 이동한다.
- 유효한 지점이 없으면 이동과 쿨다운이 발생하지 않는다.
- 성공 후 액션 슬롯에 `Cooldown.Grapple` 쿨다운이 표시된다.
- 쿨다운 중에는 다시 활성화되지 않는다.
- 피격, 빙결, 사망 또는 Dash로 취소되면 케이블과 이동이 즉시 정리된다.
- 리슨 서버 플레이어와 원격 클라이언트 양쪽에서 같은 결과가 나온다.
- 재부활 후에도 Grapple 어빌리티가 한 개만 부여되어 정상 동작한다.
