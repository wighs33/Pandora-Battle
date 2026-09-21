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
        [SerializeField] private string categoryTag = "Pandora.Offensive";
        [SerializeField] private string displayName;
        [SerializeField, TextArea] private string description;
        [SerializeField] private Sprite icon;

        [Header("Weapon Compatibility")]
        [SerializeField] private List<string> compatibleWeaponTags = new();

        [Header("Progression")]
        [SerializeField] private int[] pointsRequiredPerLevel = { 1, 1, 1 };

        [Header("Skills")]
        [SerializeField] private SkillDefinition[] skills = new SkillDefinition[MaxLevel];

        public string Id => id;
        public string CategoryTag => categoryTag;
        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? name : displayName;
        public string Description => description;
        public Sprite Icon => icon;

        public int GetRequiredPointsForLevel(int level)
        {
            int index = Mathf.Clamp(level - 1, 0, MaxLevel - 1);
            if (pointsRequiredPerLevel == null || index >= pointsRequiredPerLevel.Length)
            {
                return 1;
            }

            return Mathf.Max(pointsRequiredPerLevel[index], 1);
        }

        public SkillDefinition GetSkill(int slotIndex)
        {
            if (skills == null || slotIndex < 0 || slotIndex >= skills.Length)
            {
                return null;
            }

            return skills[slotIndex];
        }

        public bool IsSkillUnlocked(int slotIndex, int pandoraLevel)
        {
            return GetSkill(slotIndex) != null
                && pandoraLevel >= GetRequiredLevelForSkill(slotIndex);
        }

        public bool CanUseWithWeapon(string weaponTag)
        {
            if (compatibleWeaponTags == null || compatibleWeaponTags.Count == 0)
            {
                return true;
            }

            foreach (string compatibleTag in compatibleWeaponTags)
            {
                if (GameplayTagSet.MatchesOrIsChildOf(weaponTag, compatibleTag))
                {
                    return true;
                }
            }

            return false;
        }

        public static int GetRequiredLevelForSkill(int slotIndex)
        {
            return Mathf.Clamp(slotIndex + 1, 1, MaxLevel);
        }
    }
}
