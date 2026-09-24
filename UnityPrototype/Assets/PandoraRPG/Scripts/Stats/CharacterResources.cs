using System;
using UnityEngine;

namespace PandoraRPG.Stats
{
    /// <summary>
    /// Owns the resources that change while a character is playing.
    /// It does not calculate attack damage or character stats.
    /// </summary>
    [RequireComponent(typeof(CharacterStats))]
    public sealed class CharacterResources : MonoBehaviour
    {
        public event Action Changed;
        public event Action Died;

        [SerializeField] private CharacterStats stats;

        private float health;
        private float shield;
        private float mana;
        private float stamina;

        public float Health => health;
        public float Shield => shield;
        public float Mana => mana;
        public float Stamina => stamina;

        public bool IsDead => health <= 0f;

        private void Awake()
        {
            if (stats == null)
            {
                stats = GetComponent<CharacterStats>();
            }

            RestoreAll();
        }

        public void RestoreAll()
        {
            ResourceLimits limits = stats.Resources;

            health = limits.MaxHealth;
            shield = limits.MaxShield;
            mana = limits.MaxMana;
            stamina = limits.MaxStamina;

            Changed?.Invoke();
        }

        /// <summary>
        /// Applies final damage after all combat calculations.
        /// Shield is consumed before health.
        /// Returns the amount of health damage actually taken.
        /// </summary>
        public float TakeDamage(float finalDamage)
        {
            if (finalDamage <= 0f || IsDead)
            {
                return 0f;
            }

            float remainingDamage = finalDamage;

            float shieldDamage = Mathf.Min(shield, remainingDamage);
            shield -= shieldDamage;
            remainingDamage -= shieldDamage;

            float healthDamage = Mathf.Min(health, remainingDamage);
            health -= healthDamage;

            Changed?.Invoke();

            if (health <= 0f)
            {
                health = 0f;
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

            float previousHealth = health;
            health = Mathf.Min(health + amount, stats.Resources.MaxHealth);

            Changed?.Invoke();
            return health - previousHealth;
        }

        public bool TrySpendMana(float amount)
        {
            return TrySpend(ref mana, amount);
        }

        public bool TrySpendStamina(float amount)
        {
            return TrySpend(ref stamina, amount);
        }

        public void RestoreMana(float amount)
        {
            mana = Restore(mana, amount, stats.Resources.MaxMana);
            Changed?.Invoke();
        }

        public void RestoreStamina(float amount)
        {
            stamina = Restore(stamina, amount, stats.Resources.MaxStamina);
            Changed?.Invoke();
        }

        private bool TrySpend(ref float currentValue, float amount)
        {
            float cost = Mathf.Max(amount, 0f);
            if (currentValue < cost)
            {
                return false;
            }

            currentValue -= cost;
            Changed?.Invoke();
            return true;
        }

        private static float Restore(float currentValue, float amount, float maximum)
        {
            return Mathf.Min(currentValue + Mathf.Max(amount, 0f), maximum);
        }

        private void Reset()
        {
            stats = GetComponent<CharacterStats>();
        }
    }
}
