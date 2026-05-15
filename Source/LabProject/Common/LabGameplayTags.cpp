#include "Common/LabGameplayTags.h"

namespace LabGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Attack, "Action.Attack", "Default melee attack ability tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_RangedAttack, "Action.RangedAttack", "Default ranged attack ability tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_HitReact, "Action.HitReact", "Default hit react ability tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Equip, "Action.Equip", "Default equip ability tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Unequip, "Action.Unequip", "Default unequip ability tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller damage magnitude tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StatUp, "Data.StatUp", "SetByCaller stat operation tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon, "Item.Weapon", "Weapon item filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment, "Item.Equipment", "Equipment item filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Consumable, "Item.Consumable", "Consumable item filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Valuable, "Item.Valuable", "Valuable item filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Hat, "Item.Equipment.Hat", "Hat equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Top, "Item.Equipment.Top", "Top equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Bottom, "Item.Equipment.Bottom", "Bottom equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Shoes, "Item.Equipment.Shoes", "Shoes equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Earring, "Item.Equipment.Earring", "Earring equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Necklace, "Item.Equipment.Necklace", "Necklace equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Ring, "Item.Equipment.Ring", "Ring equipment slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Equipment_Rune, "Item.Equipment.Rune", "Rune equipment slot tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pandora_Offensive, "Pandora.Offensive", "Offensive pandora filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pandora_Defensive, "Pandora.Defensive", "Defensive pandora filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pandora_Support, "Pandora.Support", "Support pandora filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pandora_Special, "Pandora.Special", "Special pandora filter tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Pandora, "Skin.Pandora", "Pandora skin filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics, "Skin.Cosmetics", "Cosmetics skin filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Gesture, "Skin.Gesture", "Gesture skin filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Riding, "Skin.Riding", "Riding skin filter tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Hat, "Skin.Cosmetics.Hat", "Hat skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Top, "Skin.Cosmetics.Top", "Top skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Bottom, "Skin.Cosmetics.Bottom", "Bottom skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Shoes, "Skin.Cosmetics.Shoes", "Shoes skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Hair, "Skin.Cosmetics.Hair", "Hair skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Face, "Skin.Cosmetics.Face", "Face skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Back, "Skin.Cosmetics.Back", "Back skin slot tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skin_Cosmetics_Aura, "Skin.Cosmetics.Aura", "Aura skin slot tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Profile, "UI.Profile", "Profile UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Status, "UI.Status", "Status UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Equipment, "UI.Equipment", "Equipment UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Inventory, "UI.Inventory", "Inventory UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_SkinEquipment, "UI.SkinEquipment", "Skin equipment UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_SkinInventory, "UI.SkinInventory", "Skin inventory UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_PandoraEquipment, "UI.PandoraEquipment", "Pandora equipment UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_PandoraInventory, "UI.PandoraInventory", "Pandora inventory UI page tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Widget_Info, "UI.Widget.Info", "Default info widget class tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Widget_SelectPandora, "UI.Widget.SelectPandora", "Default select pandora widget class tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Widget_AimCrosshair, "UI.Widget.AimCrosshair", "Default aim crosshair widget class tag.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Offense_Strength, "Status.Offense.Strength", "Strength stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Offense_Intelligence, "Status.Offense.Intelligence", "Intelligence stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Offense_Arcane, "Status.Offense.Arcane", "Arcane stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Defense_Toughness, "Status.Defense.Toughness", "Toughness stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Defense_Recovery, "Status.Defense.Recovery", "Recovery stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Defense_MagicResistance, "Status.Defense.MagicResistance", "Magic resistance stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resistance_Immunity, "Status.Resistance.Immunity", "Immunity stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resistance_Fortitude, "Status.Resistance.Fortitude", "Fortitude stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resistance_Sanity, "Status.Resistance.Sanity", "Sanity stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_PandoraForce_FirstPandora, "Status.PandoraForce.FirstPandora", "First pandora force stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_PandoraForce_SecondPandora, "Status.PandoraForce.SecondPandora", "Second pandora force stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_PandoraForce_ThirdPandora, "Status.PandoraForce.ThirdPandora", "Third pandora force stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resource_MaxHealth, "Status.Resource.MaxHealth", "Maximum health stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resource_MaxMana, "Status.Resource.MaxMana", "Maximum mana stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Resource_MaxStamina, "Status.Resource.MaxStamina", "Maximum stamina stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Agility_AttackSpeed, "Status.Agility.AttackSpeed", "Attack speed stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Agility_MovementSpeed, "Status.Agility.MovementSpeed", "Movement speed stat tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Agility_CriticalChance, "Status.Agility.CriticalChance", "Critical chance stat tag.");
}
