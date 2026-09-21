using System;
using PandoraRPG.Stats;

namespace PandoraRPG.Pandora
{
    public enum PandoraLoadoutDirection
    {
        Left,
        Up,
        Right
    }

    [Serializable]
    public sealed class PandoraLoadoutSlot
    {
        public PandoraDefinition definition;
        public int level = 1;
    }

    /// <summary>
    /// Runtime loadout. Only the selected slot receives skill input,
    /// mirroring Pandora Battle's selected-Pandora input binding policy.
    /// </summary>
    public sealed class PandoraLoadout
    {
        private readonly PandoraLoadoutSlot left = new();
        private readonly PandoraLoadoutSlot up = new();
        private readonly PandoraLoadoutSlot right = new();

        public PandoraLoadoutDirection SelectedDirection { get; private set; } = PandoraLoadoutDirection.Left;

        public PandoraLoadoutSlot GetSlot(PandoraLoadoutDirection direction)
        {
            return direction switch
            {
                PandoraLoadoutDirection.Left => left,
                PandoraLoadoutDirection.Up => up,
                PandoraLoadoutDirection.Right => right,
                _ => left
            };
        }

        public PandoraLoadoutSlot GetSelectedSlot()
        {
            return GetSlot(SelectedDirection);
        }

        public void SetSlot(PandoraLoadoutDirection direction, PandoraDefinition definition, int level)
        {
            PandoraLoadoutSlot slot = GetSlot(direction);
            slot.definition = definition;
            slot.level = Math.Clamp(level, 1, PandoraDefinition.MaxLevel);
        }

        public bool Select(PandoraLoadoutDirection direction)
        {
            if (GetSlot(direction).definition == null)
            {
                return false;
            }

            SelectedDirection = direction;
            return true;
        }

        public float GetSelectedDamageBonusPercent(CharacterStats stats)
        {
            if (stats == null)
            {
                return 0f;
            }

            return SelectedDirection switch
            {
                PandoraLoadoutDirection.Left => stats.FirstPandora,
                PandoraLoadoutDirection.Up => stats.SecondPandora,
                PandoraLoadoutDirection.Right => stats.ThirdPandora,
                _ => 0f
            };
        }
    }
}
