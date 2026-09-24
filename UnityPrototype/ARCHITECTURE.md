# Pandora RPG Unity Architecture

## Primary rule

A class should answer one clear question.

- **Definition**: What is this thing configured to be?
- **Stats**: What numbers does this character have?
- **Resources**: What are this character's current HP / MP / Shield / Stamina values?
- **Math**: How is a result calculated?
- **Loadout**: What is currently equipped or selected?
- **Controller**: What should happen in response to player or AI input?

Avoid classes that own several of these responsibilities at once.

## Data flow

```text
ScriptableObject Definitions
        |
        v
Runtime State / Components
        |
        v
Pure Calculation
        |
        v
Gameplay Result
        |
        v
Animation / VFX / UI
```

Presentation code should observe gameplay state. It should not contain combat rules.

## Folder responsibilities

```text
Core/
  Small project-wide utilities only.

Stats/
  CharacterStats       Configured and derived character numbers.
  CharacterResources   Current HP, Shield, Mana, Stamina.
  CharacterStatBlock   Grouped stat data shown in the Inspector.

Combat/
  CombatMath           Pure combat formulas.
  Future attack execution and hit result types.

Items/
  WeaponDefinition     Weapon configuration data.

Abilities/
  SkillDefinition      Skill configuration data.

Pandora/
  PandoraDefinition    Pandora configuration data.
  PandoraLoadout       Equipped slots and current selection.
```

## Naming rules

Use names that describe intent instead of implementation.

Good:
- `TakeDamage`
- `TrySpendMana`
- `CanUseWithWeapon`
- `SelectedSlot`
- `MaxHealth`

Avoid vague names:
- `Process`
- `HandleData`
- `DoAction`
- `Value1`
- `Temp`

Boolean methods should read like questions:
- `IsDead`
- `IsSkillUnlocked`
- `CanUseWithWeapon`
- `TrySelect`

## Dependency rules

1. Definitions do not depend on runtime player objects.
2. Math classes do not read scene objects.
3. UI never calculates damage.
4. CharacterResources does not know how damage was calculated.
5. CharacterStats does not own current HP / MP.
6. PandoraLoadout does not grant abilities itself. A future controller/service will perform execution.
7. Network-specific concepts are not introduced unless multiplayer becomes an actual requirement.

## Porting rule from Pandora Battle

Do not translate Unreal classes one-to-one.

Keep the gameplay rule, then rebuild the responsibility in the simplest Unity form.

Examples:

- `UBasicAttributeSet`
  - Unreal: stats, resources, replication, damage scratch attributes, callbacks.
  - Unity: `CharacterStats` + `CharacterResources` + `CombatMath`.

- `UPandoraComponent`
  - Unreal: inventory, loadout, selection, replication, async loading, ability grants.
  - Unity: start with `PandoraLoadout`; add separate inventory and skill execution classes only when the game needs them.

This keeps the single-player project understandable without carrying multiplayer architecture into it.
