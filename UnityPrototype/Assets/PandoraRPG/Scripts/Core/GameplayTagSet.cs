using System;
using System.Collections.Generic;

namespace PandoraRPG.Core
{
    /// <summary>
    /// Small hierarchical tag set inspired by Unreal Gameplay Tags.
    /// Example: "State.Dead" is considered a child of "State".
    /// </summary>
    public sealed class GameplayTagSet
    {
        private readonly HashSet<string> tags = new(StringComparer.Ordinal);

        public int Count => tags.Count;

        public bool Add(string tag)
        {
            return IsValid(tag) && tags.Add(Normalize(tag));
        }

        public bool Remove(string tag)
        {
            return IsValid(tag) && tags.Remove(Normalize(tag));
        }

        public void Clear()
        {
            tags.Clear();
        }

        public bool Contains(string query, bool exactMatch = false)
        {
            if (!IsValid(query))
            {
                return false;
            }

            string normalizedQuery = Normalize(query);
            foreach (string tag in tags)
            {
                bool matches = exactMatch
                    ? string.Equals(tag, normalizedQuery, StringComparison.Ordinal)
                    : MatchesOrIsChildOf(tag, normalizedQuery);

                if (matches)
                {
                    return true;
                }
            }

            return false;
        }

        public static bool MatchesOrIsChildOf(string tag, string parentTag)
        {
            if (!IsValid(tag) || !IsValid(parentTag))
            {
                return false;
            }

            string normalizedTag = Normalize(tag);
            string normalizedParent = Normalize(parentTag);

            return string.Equals(normalizedTag, normalizedParent, StringComparison.Ordinal)
                || normalizedTag.StartsWith(normalizedParent + ".", StringComparison.Ordinal);
        }

        private static bool IsValid(string tag)
        {
            return !string.IsNullOrWhiteSpace(tag);
        }

        private static string Normalize(string tag)
        {
            return tag.Trim().Trim('.');
        }
    }
}
