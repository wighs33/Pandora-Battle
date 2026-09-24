using System;
using PandoraRPG.Stats;
using UnityEngine;

namespace PandoraRPG.Pandora
{
    public enum PandoraSlot
    {
        Left,
        Up,
        Right
    }

    [Serializable]
    public sealed class PandoraSlotState
    {
        public PandoraDefinition Definition;
        [Range(1, PandoraDefinition.MaxLevel)] public int Level = 1;

        public bool IsEmpty => Definition == null;
    }

    /// <summary>
    /// Stores the three equipped Pandoras and the currently selected slot.
    /// Skill execution belongs to a separate controller.
    /// </summary>
    public sealed class PandoraLoadout
    {
        private readonly PandoraSlotState left = new();
        private readonly PandoraSlotState up = new();
        private readonly PandoraSlotState right = new();

        public PandoraSlot SelectedSlot { get; private set; } = PandoraSlot.Left;

        public PandoraSlotState GetSlot(PandoraSlot slot)
        {
            return slot switch
            {
                PandoraSlot.Left => left,
                PandoraSlot.Up => up,
                PandoraSlot.Right => right,
                _ => throw new ArgumentOutOfRangeException(nameof(slot), slot, null)
            };
        }

        public PandoraSlotState GetSelectedSlot()
        {
            return GetSlot(SelectedSlot);
        }

        public void Equip(PandoraSlot slot, PandoraDefinition definition, int level)
        {
            PandoraSlotState state = GetSlot(slot);
            state.Definition = definition;
            state.Level = Mathf.Clamp(level, 1, PandoraDefinition.MaxLevel);
        }

        public bool TrySelect(PandoraSlot slot)
        {
            if (GetSlot(slot).IsEmpty)
            {
                return false;
            }

            SelectedSlot = slot;
            return true;
        }

        public float GetSelectedPowerBonus(CharacterStats stats)
        {
            if (stats == null)
            {
                return 0f;
            }

            PandoraPowerStats power = stats.PandoraPower;

            return SelectedSlot switch
            {
                PandoraSlot.Left => power.LeftSlot,
                PandoraSlot.Up => power.UpSlot,
                PandoraSlot.Right => power.RightSlot,
                _ => 0f
            };
        }
    }
}
