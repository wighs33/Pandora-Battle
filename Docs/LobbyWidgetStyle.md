# WBP_Lobby style

Applied on 2026-09-16 through the local Unreal Engine 5.8.2 MCP server.

## Result

- `WBP_Lobby` and `WBP_LobbyUser` match the title screen's plum, cream, and gold palette.
- Participant rows retain editable nicknames, team selection, owner marks, friend actions, and host kick actions. The original runtime-tinted `ColorBorder` is now a narrow accent strip.
- The existing `UserList` remains inside its scroll box. Four rows fit at once; six rows remain accessible by scrolling.
- The existing map thumbnails keep their original proportions. Map arrows, player count, settings, return to lobby, start, audio, and leave controls retain their native behavior.
- `Btn_GameSettings` opens the personal settings popup; map selection uses the previous/next arrows.
- A 1920 × 1080 design canvas scales uniformly. A full-screen border fills any letterboxing so the 3D world does not show through at other aspect ratios.
- `TeamBalanceWarningRoot` isolates the warning from the design canvas. This is required because the existing warning code also changes its text widget's parent's visibility.
- The countdown remains in the primary-action area under the existing native visibility rules.

## Assets

- `Content/UI/Widget/Lobby/WBP_Lobby.uasset`
- `Content/UI/Widget/Lobby/WBP_LobbyUser.uasset`
- `Content/UI/Asset/LobbyStyle/`: 15 UI textures with editable SVG and PNG sources under `SourceArt/`.
- Reuses the existing lobby background, map thumbnails, Noto Sans CJK KR fonts, and title primary-button artwork.
- `Scripts/generate_lobby_chrome.cjs`: deterministic SVG and PNG generation with Node.js + sharp.
- `Scripts/restyle_lobby_widgets.mjs`: MCP authoring script; preserves bound widget objects and compiles both Widget Blueprints before saving.
- `output/lobby-style/WBP_Lobby-preview.png`: actual single-player PIE screenshot, not the concept illustration.

No C++ gameplay code was changed.

## Verification

- Both Widget Blueprints compiled and saved successfully; required bound controls were checked.
- Actual PIE images inspected at a wide window size and at 640 × 480 client resolution.
- Team selection changed from Red to Blue through the UI.
- Next map changed WindNest to Village and the capacity display changed from 1 / 6 to 1 / 3.
- The game settings button opened the existing settings popup.
- Six transient layout-only rows were displayed, and scrolling to the end exposed rows five and six. These fixtures were discarded when PIE ended and are not in the final screenshot or saved assets.
- Node.js syntax checks and scoped whitespace checks passed.
- Steam invitation/friend overlays, multiplayer kicking, and travel into a match were not exercised. Invite visibility follows the existing online-subsystem availability rule.
- The editor log contains a pre-existing LevelDefinition initialization warning/error when directly starting the lobby map; subsequent map selection and configuration worked. No changes were made to the level or game mode configuration.

Original Widget Blueprint binaries are preserved in `Saved/Codex/LobbyStyle/before/`. Stop the editor before restoring them. Property snapshots, authoring logs, diagnostics, and additional verification screenshots are in `Saved/Codex/LobbyStyle/`.
