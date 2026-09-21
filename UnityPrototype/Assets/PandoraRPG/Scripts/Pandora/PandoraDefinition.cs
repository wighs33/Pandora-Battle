using System.Collections.Generic;
using PandoraRPG.Abilities;
using PandoraRPG.Core;
using UnityEngine;

namespace PandoraRPG.Pandora
{
    [CreateAssetMenu(menuName = "Pandora RPG/Pandora Definition", fileName = "Pandora_")]
    public sealed class PandoraDefinition : ScriptableObject
    {
        public const int MaxLevel = 3;

        [Header("Identity")]
        [SerializeField] private string id;
        [SerializeField] private string pandoraTag = "Pandora.Offensive";
        [SerializeField] private string displayName;
        [SerializeField, TextArea] private string description;
        [SerializeField] private Sprite icon;

        [Header("Weapon Compatibility")]
        [SerializeField] private List<string> activatableWeaponTags = new();

        [Header("Progression")]
        [SerializeField] private int[] pointsRequiredPerLevel = { 1, 1, 1 };

        [Header("Skills")]
        [SerializeField] private SkillDefinition[] skills = new SkillDefinition[MaxLevel];

        public string Id => id;
        public string PandoraTag => pandoraTag;
        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? name : displayName;
        public string Description => description;
        public Sprite Icon => icon;

        public int GetRequiredPointsForLevel(int level)
        {
            int index = Mathf.Clamp(level - 1, 0, MaxLevel - 1);
            return pointsRequiredPerLevel != null && index < pointsRequiredPerLevel.Length
                ? Mathf.Max(pointsRequiredPerLevel[index], 1)
                : 1;
        }

        public SkillDefinition GetSkill(int slotIndex)
        {
            return skills != null && slotIndex >= 0 && slotIndex < skills.Length ? skills[slotIndex] : null;
        }

        public bool IsSkillSlotUnlocked(int slotIndex, int pandoraLevel)
        {
            return slotIndex >= 0
                && slotIndex < MaxLevel
                && pandoraLevel >= GetRequiredLevelForSkillSlot(slotIndex)
                && GetSkill(slotIndex) != null;
        }

        public bool IsCompatibleWithWeaponTag(string weaponTag)
        {
            if (activatableWeaponTags == null || activatableWeaponTags.Count == 0)
            {
                return true;
            }

            foreach (string allowedTag in activatableWeaponTags)
            {
                if (GameplayTagSet.Matches(weaponTag, allowedTag))
                {
                    return true;
                }
            }

            return false;
        }

        public static int GetRequiredLevelForSkillSlot(int slotIndex)
        {
            return Mathf.Clamp(slotIndex + 1, 1, MaxLevel);
        }
    }
}
