using UnityEngine;

namespace PandoraRPG.Combat
{
    /// <summary>
    /// Combat formulas ported from Pandora Battle's current Unreal implementation.
    /// </summary>
    public static class CombatMath
    {
        public static float CalculateStrengthAdjustedWeaponDamage(float weaponDamage, float strength)
        {
            float safeDamage = Mathf.Max(weaponDamage, 0f);
            float safeStrength = Mathf.Max(strength, 0f);
            return safeDamage * (1f + safeStrength * 0.01f);
        }

        public static float CalculateSkillDamage(float baseDamage, float intelligence, float pandoraSlotBonus)
        {
            float safeDamage = Mathf.Max(baseDamage, 0f);
            float bonusPercent = Mathf.Max(intelligence, 0f) + Mathf.Max(pandoraSlotBonus, 0f);
            return safeDamage * (1f + bonusPercent * 0.01f);
        }

        public static float CalculateCriticalDamageMultiplier(float critical)
        {
            return 2f + Mathf.Max(critical, 0f) * 0.01f;
        }

        public static float ApplyCritical(float damage, float critical)
        {
            return Mathf.Max(damage, 0f) * CalculateCriticalDamageMultiplier(critical);
        }

        public static float CalculateArmorReduction(float targetArmor, float targetFinalStrengthDamage)
        {
            return Mathf.Max(targetFinalStrengthDamage, 0f)
                * Mathf.Clamp(targetArmor, 0f, 100f)
                * 0.01f;
        }

        public static float MitigateByArmor(float incomingDamage, float targetArmor, float targetFinalStrengthDamage)
        {
            float reduction = CalculateArmorReduction(targetArmor, targetFinalStrengthDamage);
            return Mathf.Max(Mathf.Max(incomingDamage, 0f) - reduction, 0f);
        }

        public static float MitigateStatusDamage(float incomingDamage, float statusResistance)
        {
            float multiplier = 1f - Mathf.Clamp(statusResistance, 0f, 100f) * 0.01f;
            return Mathf.Max(incomingDamage, 0f) * multiplier;
        }

        public static float CalculateCooldown(float baseCooldownSeconds, float arcane)
        {
            float reductionPercent = Mathf.Clamp(arcane, 0f, 100f);
            return Mathf.Max(baseCooldownSeconds, 0f) * (1f - reductionPercent * 0.01f);
        }
    }
}
