using System;
using UnityEngine;

namespace PandoraRPG.Stats
{
    /// <summary>
    /// Stores a character's progression and combat stats.
    /// Current HP, Mana, Shield, and Stamina belong to CharacterResources.
    /// </summary>
    public sealed class CharacterStats : MonoBehaviour
    {
        public event Action StatsChanged;

        [Header("Progression")]
        [SerializeField, Min(1)] private int level = 1;
        [SerializeField, Min(0f)] private float experience;
        [SerializeField, Min(1f)] private float experienceToNextLevel = 100f;

        [Header("Combat Stats")]
        [SerializeField] private CharacterStatBlock statBlock = new();

        public int Level => level;
        public float Experience => experience;
        public float ExperienceToNextLevel => experienceToNextLevel;

        public OffenseStats Offense => statBlock.Offense;
        public DefenseStats Defense => statBlock.Defense;
        public ResistanceStats Resistance => statBlock.Resistance;
        public PandoraPowerStats PandoraPower => statBlock.PandoraPower;
        public AgilityStats Agility => statBlock.Agility;
        public ResourceLimits Resources => statBlock.Resources;

        /// <summary>
        /// Call this after changing runtime stats through a progression system.
        /// </summary>
        public void NotifyStatsChanged()
        {
            statBlock.Clamp();
            StatsChanged?.Invoke();
        }

        private void OnValidate()
        {
            level = Mathf.Max(level, 1);
            experience = Mathf.Max(experience, 0f);
            experienceToNextLevel = Mathf.Max(experienceToNextLevel, 1f);

            statBlock ??= new CharacterStatBlock();
            statBlock.Clamp();
        }
    }
}
