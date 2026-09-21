# Pandora Battle - Unity Single Player Prototype

This folder is the Unity-side single-player RPG port of Pandora Battle.

The main goal is not a line-by-line Unreal port. The goal is to preserve Pandora Battle's recognizable combat rules while making the Unity code easier to understand, test, and extend.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the readability rules used during the port.

## Kept from Pandora Battle

- Pandora loadout and three skill slots
- Weapon-based basic attacks
- Strength / Intelligence / Critical combat stats
- Armor / Recovery / status resistances
- Health / Shield / Mana / Stamina
- Pandora power stats for Left / Up / Right loadout slots
- Data-driven weapons, skills, and Pandoras
- Hierarchical gameplay tags

## Removed from the single-player prototype

- Steam sessions
- Iris / Push Model replication
- Server RPCs and authority checks
- Prediction / reconciliation
- Multiplayer lobby and match flow

## Unreal -> Unity mapping

| Unreal | Unity prototype |
| --- | --- |
| Gameplay Tag | GameplayTagSet |
| BasicAttributeSet | CharacterStats + CharacterStatBlock |
| Gameplay Effect damage math | CombatMath |
| ItemDefinition weapon data | WeaponDefinition |
| SkillDefinition | SkillDefinition |
| PandoraDefinition | PandoraDefinition |
| PandoraComponent / SkillBinder | PandoraLoadout |

## First playable milestone

1. Third-person movement and camera
2. Equip one melee weapon
3. Basic attack with an animation-event hit window
4. One enemy with HP and chase/attack AI
5. Equip three Pandoras in Left / Up / Right slots
6. Use the selected Pandora's three skills
7. Gain EXP and level up
8. Minimal HUD for HP / MP / Stamina / skills

The combat/data core comes first so movement, AI, UI, and animation code all depend on one readable set of rules.
