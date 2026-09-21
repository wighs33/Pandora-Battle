using System;
using System.Collections.Generic;

namespace PandoraRPG.Core
{
    /// <summary>
    /// Lightweight hierarchical tag container inspired by Unreal Gameplay Tags.
    /// "State.Dead" matches "State", while exact matching requires the full tag.
    /// </summary>
    public sealed class GameplayTagSet
    {
        private readonly HashSet<string> tags = new(StringComparer.Ordinal);

        public int Count => tags.Count;

        public bool Add(string tag)
        {
            return !string.IsNullOrWhiteSpace(tag) && tags.Add(Normalize(tag));
        }

        public bool Remove(string tag)
        {
            return !string.IsNullOrWhiteSpace(tag) && tags.Remove(Normalize(tag));
        }

        public void Clear()
        {
            tags.Clear();
        }

        public bool Has(string query, bool exact = false)
        {
            if (string.IsNullOrWhiteSpace(query))
            {
                return false;
            }

            string normalizedQuery = Normalize(query);
            foreach (string tag in tags)
            {
                if (exact ? string.Equals(tag, normalizedQuery, StringComparison.Ordinal) : Matches(tag, normalizedQuery))
                {
                    return true;
                }
            }

            return false;
        }

        public static bool Matches(string tag, string query)
        {
            if (string.IsNullOrWhiteSpace(tag) || string.IsNullOrWhiteSpace(query))
            {
                return false;
            }

            tag = Normalize(tag);
            query = Normalize(query);

            return string.Equals(tag, query, StringComparison.Ordinal)
                || tag.StartsWith(query + ".", StringComparison.Ordinal);
        }

        private static string Normalize(string tag)
        {
            return tag.Trim().Trim('.');
        }
    }
}
