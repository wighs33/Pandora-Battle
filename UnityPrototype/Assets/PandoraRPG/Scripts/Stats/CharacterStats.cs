using System;
using UnityEngine;

namespace PandoraRPG.Stats
{
    /// <summary>
    /// Owns one character's runtime resources and exposes its authored combat stats.
    /// Damage formulas themselves belong to CombatMath.
    /// </summary>
    public sealed class CharacterStats : MonoBehaviour
    {
        public event Action ResourcesChanged;
        public event Action StatsChanged;
        public event Action Died;

        [Header("Level")]
        [SerializeField, Min(1)] private int level = 1;
        [SerializeField, Min(0f)] private float experience;
        [SerializeField, Min(1f)] private float experienceToNextLevel = 100f;

        [Header("Stats")]
        [SerializeField] private CharacterStatBlock values = new();

        private float currentHealth;
        private float currentShield;
        private float currentMana;
        private float currentStamina;

        public int Level => level;
        public float Experience => experience;
        public float ExperienceToNextLevel => experienceToNextLevel;

        public CharacterStatBlock Values => values;

        public float CurrentHealth => currentHealth;
        public float CurrentShield => currentShield;
        public float CurrentMana => currentMana;
        public float CurrentStamina => currentStamina;

        public bool IsDead => currentHealth <= 0f;

        private void Awake()
        {
            RestoreAllResources();
        }

        public void RestoreAllResources()
        {
            currentHealth = values.Resources.Health;
            currentShield = values.Resources.Shield;
            currentMana = values.Resources.Mana;
            currentStamina = values.Resources.Stamina;

            ResourcesChanged?.Invoke();
        }

        /// <summary>
        /// Applies already-calculated final damage.
        /// Shield absorbs damage before health.
        /// </summary>
        public float TakeDamage(float finalDamage)
        {
            if (finalDamage <= 0f || IsDead)
            {
                return 0f;
            }

            float remainingDamage = finalDamage;

            float shieldDamage = Mathf.Min(currentShield, remainingDamage);
            currentShield -= shieldDamage;
            remainingDamage -= shieldDamage;

            float healthDamage = Mathf.Min(currentHealth, remainingDamage);
            currentHealth -= healthDamage;

            ResourcesChanged?.Invoke();

            if (currentHealth <= 0f)
            {
                currentHealth = 0f;
                Died?.Invoke();
            }

            return healthDamage;
        }

        public float Heal(float amount)
        {
            if (amount <= 0f || IsDead)
            {
                return 0f;
            }

            float previousHealth = currentHealth;
            currentHealth = Mathf.Min(currentHealth + amount, values.Resources.Health);

            ResourcesChanged?.Invoke();
            return currentHealth - previousHealth;
        }

        public bool TrySpendMana(float amount)
        {
            return TrySpendResource(ref currentMana, amount);
        }

        public bool TrySpendStamina(float amount)
        {
            return TrySpendResource(ref currentStamina, amount);
        }

        public void RestoreMana(float amount)
        {
            currentMana = RestoreResource(currentMana, amount, values.Resources.Mana);
            ResourcesChanged?.Invoke();
        }

        public void RestoreStamina(float amount)
        {
            currentStamina = RestoreResource(currentStamina, amount, values.Resources.Stamina);
            ResourcesChanged?.Invoke();
        }

        public void NotifyStatsChanged()
        {
            values.Clamp();
            StatsChanged?.Invoke();
        }

        private bool TrySpendResource(ref float currentValue, float amount)
        {
            float safeAmount = Mathf.Max(amount, 0f);
            if (currentValue < safeAmount)
            {
                return false;
            }

            currentValue -= safeAmount;
            ResourcesChanged?.Invoke();
            return true;
        }

        private static float RestoreResource(float currentValue, float amount, float maximum)
        {
            return Mathf.Min(currentValue + Mathf.Max(amount, 0f), maximum);
        }

        private void OnValidate()
        {
            level = Mathf.Max(level, 1);
            experience = Mathf.Max(experience, 0f);
            experienceToNextLevel = Mathf.Max(experienceToNextLevel, 1f);
            values ??= new CharacterStatBlock();
            values.Clamp();
        }
    }
}
