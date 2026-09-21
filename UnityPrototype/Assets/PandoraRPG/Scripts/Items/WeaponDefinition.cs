using UnityEngine;

namespace PandoraRPG.Items
{
    [CreateAssetMenu(menuName = "Pandora RPG/Weapon Definition", fileName = "Weapon_")]
    public sealed class WeaponDefinition : ScriptableObject
    {
        [SerializeField] private string id;
        [SerializeField] private string displayName;
        [SerializeField] private string weaponTag = "Item.Weapon.Sword";
        [SerializeField, TextArea] private string description;

        [Header("Combat")]
        [SerializeField, Min(0f)] private float baseDamage = 10f;
        [SerializeField, Min(0f)] private float staminaCost = 10f;
        [SerializeField, Min(0.01f)] private float attackInterval = 0.6f;
        [SerializeField, Min(0f)] private float equippedMovementSpeedMultiplier = 1f;

        [Header("Presentation")]
        [SerializeField] private GameObject weaponPrefab;
        [SerializeField] private RuntimeAnimatorController animatorController;
        [SerializeField] private Sprite icon;

        public string Id => id;
        public string DisplayName => string.IsNullOrWhiteSpace(displayName) ? name : displayName;
        public string WeaponTag => weaponTag;
        public string Description => description;
        public float BaseDamage => baseDamage;
        public float StaminaCost => staminaCost;
        public float AttackInterval => attackInterval;
        public float EquippedMovementSpeedMultiplier => equippedMovementSpeedMultiplier;
        public GameObject WeaponPrefab => weaponPrefab;
        public RuntimeAnimatorController AnimatorController => animatorController;
        public Sprite Icon => icon;
    }
}
