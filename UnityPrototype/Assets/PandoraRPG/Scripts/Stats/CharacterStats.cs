using System;
using UnityEngine;

namespace PandoraRPG.Stats
{
    public sealed class CharacterStats : MonoBehaviour
    {
        public event Action ResourcesChanged;
        public event Action StatsChanged;
        public event Action Died;

        [Header("Leveling")]
        [SerializeField, Min(1)] private int level = 1;
        [SerializeField, Min(0f)] private float experience;
        [SerializeField, Min(1f)] private float maxExperience = 100f;

        [Header("Offense")]
        [SerializeField, Range(0f, 100f)] private float strength = 10f;
        [SerializeField, Range(0f, 100f)] private float intelligence = 10f;
        [SerializeField, Range(0f, 100f)] private float critical = 10f;

        [Header("Defense")]
        [SerializeField, Range(0f, 100f)] private float armor = 10f;
        [SerializeField, Range(0f, 100f)] private float recovery = 10f;

        [Header("Resistance")]
        [SerializeField, Range(0f, 100f)] private float frostbiteResistance = 10f;
        [SerializeField, Range(0f, 100f)] private float burnResistance = 10f;
        [SerializeField, Range(0f, 100f)] private float electricShockResistance = 10f;

        [Header("Pandora Force")]
        [SerializeField, Range(0f, 100f)] private float firstPandora = 10f;
        [SerializeField, Range(0f, 100f)] private float secondPandora = 10f;
        [SerializeField, Range(0f, 100f)] private float thirdPandora = 10f;

        [Header("Agility")]
        [SerializeField, Range(0f, 100f)] private float attackSpeed = 10f;
        [SerializeField, Range(0f, 100f)] private float movementSpeed = 10f;
        [SerializeField, Range(0f, 100f)] private float arcane = 10f;

        [Header("Resources")]
        [SerializeField, Min(1f)] private float maxHealth = 100f;
        [SerializeField, Min(0f)] private float maxShield;
        [SerializeField, Min(0f)] private float maxMana = 100f;
        [SerializeField, Min(0f)] private float maxStamina = 100f;

        private float health;
        private float shield;
        private float mana;
        private float stamina;

        public int Level => level;
        public float Experience => experience;
        public float MaxExperience => maxExperience;

        public float Strength => strength;
        public float Intelligence => intelligence;
        public float Critical => critical;
        public float Armor => armor;
        public float Recovery => recovery;

        public float FrostbiteResistance => frostbiteResistance;
        public float BurnResistance => burnResistance;
        public float ElectricShockResistance => electricShockResistance;

        public float FirstPandora => firstPandora;
        public float SecondPandora => secondPandora;
        public float ThirdPandora => thirdPandora;

        public float AttackSpeed => attackSpeed;
        public float MovementSpeed => movementSpeed;
        public float Arcane => arcane;

        public float Health => health;
        public float Shield => shield;
        public float Mana => mana;
        public float Stamina => stamina;

        public float MaxHealth => maxHealth;
        public float MaxShield => maxShield;
        public float MaxMana => maxMana;
        public float MaxStamina => maxStamina;
        public bool IsDead => health <= 0f;

        private void Awake()
        {
            ResetResources();
        }

        public void ResetResources()
        {
            health = maxHealth;
            shield = maxShield;
            mana = maxMana;
            stamina = maxStamina;
            ResourcesChanged?.Invoke();
        }

        public float ApplyDamage(float damage)
        {
            float remaining = Mathf.Max(damage, 0f);
            if (remaining <= 0f || IsDead)
            {
                return 0f;
            }

            float shieldDamage = Mathf.Min(shield, remaining);
            shield -= shieldDamage;
            remaining -= shieldDamage;

            float healthDamage = Mathf.Min(health, remaining);
            health -= healthDamage;

            ResourcesChanged?.Invoke();

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

            float before = health;
            health = Mathf.Min(health + amount, maxHealth);
            ResourcesChanged?.Invoke();
            return health - before;
        }

        public bool TrySpendMana(float amount)
        {
            amount = Mathf.Max(amount, 0f);
            if (mana < amount)
            {
                return false;
            }

            mana -= amount;
            ResourcesChanged?.Invoke();
            return true;
        }

        public bool TrySpendStamina(float amount)
        {
            amount = Mathf.Max(amount, 0f);
            if (stamina < amount)
            {
                return false;
            }

            stamina -= amount;
            ResourcesChanged?.Invoke();
            return true;
        }

        public void RestoreMana(float amount)
        {
            mana = Mathf.Min(mana + Mathf.Max(amount, 0f), maxMana);
            ResourcesChanged?.Invoke();
        }

        public void RestoreStamina(float amount)
        {
            stamina = Mathf.Min(stamina + Mathf.Max(amount, 0f), maxStamina);
            ResourcesChanged?.Invoke();
        }

        public void SetCoreCombatStats(
            float newStrength,
            float newIntelligence,
            float newCritical,
            float newArmor,
            float newRecovery,
            float newArcane)
        {
            strength = ClampStat(newStrength);
            intelligence = ClampStat(newIntelligence);
            critical = ClampStat(newCritical);
            armor = ClampStat(newArmor);
            recovery = ClampStat(newRecovery);
            arcane = ClampStat(newArcane);
            StatsChanged?.Invoke();
        }

        private static float ClampStat(float value)
        {
            return Mathf.Clamp(value, 0f, 100f);
        }

        private void OnValidate()
        {
            level = Mathf.Max(level, 1);
            experience = Mathf.Max(experience, 0f);
            maxExperience = Mathf.Max(maxExperience, 1f);

            strength = ClampStat(strength);
            intelligence = ClampStat(intelligence);
            critical = ClampStat(critical);
            armor = ClampStat(armor);
            recovery = ClampStat(recovery);
            frostbiteResistance = ClampStat(frostbiteResistance);
            burnResistance = ClampStat(burnResistance);
            electricShockResistance = ClampStat(electricShockResistance);
            firstPandora = ClampStat(firstPandora);
            secondPandora = ClampStat(secondPandora);
            thirdPandora = ClampStat(thirdPandora);
            attackSpeed = ClampStat(attackSpeed);
            movementSpeed = ClampStat(movementSpeed);
            arcane = ClampStat(arcane);

            maxHealth = Mathf.Max(maxHealth, 1f);
            maxShield = Mathf.Max(maxShield, 0f);
            maxMana = Mathf.Max(maxMana, 0f);
            maxStamina = Mathf.Max(maxStamina, 0f);
        }
    }
}
