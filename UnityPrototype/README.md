# Pandora Battle - Unity Single Player Prototype

This folder is the Unity-side port of Pandora Battle.

## Goal

Keep the identity and combat rules of the Unreal project while removing multiplayer-only complexity.

Kept:
- Pandora loadout and 3 skill slots
- Weapon-based basic attacks
- Strength / Intelligence / Critical combat stats
- Armor / Recovery / status resistances
- Health / Shield / Mana / Stamina
- Pandora Force stats for Left / Up / Right loadout slots
- Data-driven weapons, skills, and Pandoras
- Hierarchical gameplay tags

Removed from the prototype:
- Steam sessions
- Iris / Push Model replication
- Server RPCs and authority checks
- Prediction / reconciliation
- Multiplayer lobby and match flow

## Unreal -> Unity mapping

| Unreal | Unity prototype |
| --- | --- |
| Gameplay Tag | GameplayTagSet |
| BasicAttributeSet | CharacterStats |
| Gameplay Effect damage math | CombatMath |
| ItemDefinition weapon data | WeaponDefinition |
| SkillDefinition | SkillDefinition |
| PandoraDefinition | PandoraDefinition |
| PandoraComponent / SkillBinder | PandoraLoadout |

## First playable milestone

1. Third-person movement and camera
2. Equip one melee weapon
3. Basic attack with animation-event hit window
4. One enemy with HP and chase/attack AI
5. Equip three Pandoras in Left / Up / Right slots
6. Use the selected Pandora's three skills
7. Gain EXP and level up
8. Minimal HUD for HP / MP / Stamina / skills

The current commit intentionally starts from the combat/data core so movement, AI, UI, and animation code can depend on stable rules instead of duplicating them.
