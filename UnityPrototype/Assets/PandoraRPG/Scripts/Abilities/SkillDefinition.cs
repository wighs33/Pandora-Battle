using UnityEngine;

namespace PandoraRPG.Abilities
{
    public enum SkillCastType
    {
        Instant,
        Press,
        Duration
    }

    public enum SkillActionType
    {
        Melee,
        Projectile,
        TargetedArea,
        Dash,
        Buff,
        Summon
    }

    public enum StatusEffectType
    {
        None,
        Burn,
        Frostbite,
        ElectricShock
    }

    [CreateAssetMenu(menuName = "Pandora RPG/Skill Definition", fileName = "Skill_")]
    public sealed class SkillDefinition : ScriptableObject
    {
        [Header("Identity")]
        [SerializeField] private string id;
        [SerializeField] private string displayName;
        [SerializeField, TextArea] private string description;
        [SerializeField] private Sprite icon;

        [Header("Execution")]
        [SerializeField] private SkillCastType castType = SkillCastType.Instant;
        [SerializeField] private SkillActionType actionType = SkillActionType.Projectile;
        [SerializeField] private bool cancelOnHit;

        [Header("Cost / Time")]
        [SerializeField, Min(0f)] private float manaCost;
        [SerializeField, Min(0f)] private float cooldownSeconds = 1f;
        [SerializeField, Min(0f)] private float durationSeconds;

        [Header("Damage")]
        [SerializeField, Min(0f)] private float baseDamage = 10f;
        [SerializeField] private StatusEffectType statusEffect;
        [SerializeField, Min(1)] private int statusStacks = 1;

        [Header("Presentation")]
        [SerializeField] private AnimationClip animationClip;
        [SerializeField] private GameObject effectPrefab;

        public string Id => id;
        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? name : displayName;
        public string Description => description;
        public Sprite Icon => icon;
        public SkillCastType CastType => castType;
        public SkillActionType ActionType => actionType;
        public bool CancelOnHit => cancelOnHit;
        public float ManaCost => manaCost;
        public float CooldownSeconds => cooldownSeconds;
        public float DurationSeconds => durationSeconds;
        public float BaseDamage => baseDamage;
        public StatusEffectType StatusEffect => statusEffect;
        public int StatusStacks => Mathf.Max(statusStacks, 1);
        public AnimationClip AnimationClip => animationClip;
        public GameObject EffectPrefab => effectPrefab;
    }
}
