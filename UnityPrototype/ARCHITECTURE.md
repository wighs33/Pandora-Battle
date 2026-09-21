# Pandora RPG Unity Architecture

## Primary goal

The code should be understandable to a developer who did not build the project.

When readability and abstraction conflict, prefer the simpler code until duplication or change pressure proves that another abstraction is needed.

## Rules

### 1. Separate data, runtime state, and calculations

- ScriptableObject definitions contain authoring data.
- MonoBehaviour components own runtime state and Unity lifecycle.
- Pure calculations live in small stateless classes such as `CombatMath`.

A definition should not search the scene or mutate a character.
A calculation helper should not know about GameObjects.
A runtime component should not become a database of unrelated content.

### 2. Prefer domain names over generic names

Good:
- `PandoraLoadout`
- `CharacterStats`
- `WeaponDefinition`
- `SkillDefinition`

Avoid unless the responsibility is genuinely broad:
- `GameManager`
- `AbilityManager`
- `DataHandler`
- `CommonUtility`

### 3. One obvious path for each action

For example, damage should have one readable flow:

```text
Attack/Skill
  -> CombatMath calculates outgoing damage
  -> target CharacterStats.TakeDamage()
  -> resource change/death events
```

Do not hide the same rule in UI, animation, enemy, and player classes.

### 4. Do not port multiplayer architecture into a single-player game

The Unity prototype intentionally does not copy:
- authority checks
- RPC wrappers
- replication callbacks
- prediction/pending input state
- server/client presentation branches

If multiplayer is added later, networking should wrap the gameplay model rather than define it.

### 5. Keep data containers boring

Serialized stat groups are simple data classes on purpose.
They are grouped by meaning so the Inspector and code read the same way.

### 6. Avoid speculative abstractions

Do not introduce an interface, base class, service locator, event bus, or generic action graph until at least two real systems need the shared behavior.

### 7. Comments explain why, not what

Bad:
```csharp
// Subtract mana.
mana -= amount;
```

Good:
```csharp
// Skills pay their cost only after activation validation succeeds.
```

## Unreal -> Unity simplification

| Unreal project | Unity prototype | Reason |
| --- | --- | --- |
| `BasicAttributeSet` | `CharacterStats` + `CharacterStatBlock` | Keep runtime resources separate from grouped stat data |
| Gameplay Effect math | `CombatMath` | Pure rules are easy to read and test |
| `PandoraComponent` + `PandoraSkillBinder` | `PandoraLoadout` | No replication or ability granting is needed |
| Gameplay Tags | `GameplayTagSet` | Keep hierarchical identifiers without GAS |
| Primary Data Assets | ScriptableObjects | Native Unity authoring model |

## Folder intent

```text
Scripts/
├── Abilities/   skill definitions and later skill execution
├── Combat/      combat rules and hit/damage flow
├── Core/        small engine-independent project primitives
├── Items/       item and weapon definitions/runtime equipment
├── Pandora/     Pandora definitions and loadout
└── Stats/       character stat data and runtime resources
```
