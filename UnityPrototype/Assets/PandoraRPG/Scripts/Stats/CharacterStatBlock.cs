using System;
using UnityEngine;

namespace PandoraRPG.Stats
{
    // These classes intentionally stay as simple Inspector data holders.
    // Runtime behavior belongs to CharacterStats or CharacterResources.

    [Serializable]
    public sealed class OffenseStats
    {
        [Range(0f, 100f)] public float Strength = 10f;
        [Range(0f, 100f)] public float Intelligence = 10f;
        [Range(0f, 100f)] public float Critical = 10f;

        public void Clamp()
        {
            Strength = StatClamp.Percent(Strength);
            Intelligence = StatClamp.Percent(Intelligence);
            Critical = StatClamp.Percent(Critical);
        }
    }

    [Serializable]
    public sealed class DefenseStats
    {
        [Range(0f, 100f)] public float Armor = 10f;
        [Range(0f, 100f)] public float Recovery = 10f;

        public void Clamp()
        {
            Armor = StatClamp.Percent(Armor);
            Recovery = StatClamp.Percent(Recovery);
        }
    }

    [Serializable]
    public sealed class ResistanceStats
    {
        [Range(0f, 100f)] public float Frostbite = 10f;
        [Range(0f, 100f)] public float Burn = 10f;
        [Range(0f, 100f)] public float ElectricShock = 10f;

        public void Clamp()
        {
            Frostbite = StatClamp.Percent(Frostbite);
            Burn = StatClamp.Percent(Burn);
            ElectricShock = StatClamp.Percent(ElectricShock);
        }
    }

    [Serializable]
    public sealed class PandoraPowerStats
    {
        [Range(0f, 100f)] public float LeftSlot = 10f;
        [Range(0f, 100f)] public float UpSlot = 10f;
        [Range(0f, 100f)] public float RightSlot = 10f;

        public void Clamp()
        {
            LeftSlot = StatClamp.Percent(LeftSlot);
            UpSlot = StatClamp.Percent(UpSlot);
            RightSlot = StatClamp.Percent(RightSlot);
        }
    }

    [Serializable]
    public sealed class AgilityStats
    {
        [Range(0f, 100f)] public float AttackSpeed = 10f;
        [Range(0f, 100f)] public float MovementSpeed = 10f;
        [Range(0f, 100f)] public float Arcane = 10f;

        public void Clamp()
        {
            AttackSpeed = StatClamp.Percent(AttackSpeed);
            MovementSpeed = StatClamp.Percent(MovementSpeed);
            Arcane = StatClamp.Percent(Arcane);
        }
    }

    [Serializable]
    public sealed class ResourceLimits
    {
        [Min(1f)] public float MaxHealth = 100f;
        [Min(0f)] public float MaxShield;
        [Min(0f)] public float MaxMana = 100f;
        [Min(0f)] public float MaxStamina = 100f;

        public void Clamp()
        {
            MaxHealth = Mathf.Max(MaxHealth, 1f);
            MaxShield = Mathf.Max(MaxShield, 0f);
            MaxMana = Mathf.Max(MaxMana, 0f);
            MaxStamina = Mathf.Max(MaxStamina, 0f);
        }
    }

    [Serializable]
    public sealed class CharacterStatBlock
    {
        public OffenseStats Offense = new();
        public DefenseStats Defense = new();
        public ResistanceStats Resistance = new();
        public PandoraPowerStats PandoraPower = new();
        public AgilityStats Agility = new();
        public ResourceLimits Resources = new();

        public void Clamp()
        {
            Offense ??= new OffenseStats();
            Defense ??= new DefenseStats();
            Resistance ??= new ResistanceStats();
            PandoraPower ??= new PandoraPowerStats();
            Agility ??= new AgilityStats();
            Resources ??= new ResourceLimits();

            Offense.Clamp();
            Defense.Clamp();
            Resistance.Clamp();
            PandoraPower.Clamp();
            Agility.Clamp();
            Resources.Clamp();
        }
    }

    internal static class StatClamp
    {
        public static float Percent(float value)
        {
            return Mathf.Clamp(value, 0f, 100f);
        }
    }
}
