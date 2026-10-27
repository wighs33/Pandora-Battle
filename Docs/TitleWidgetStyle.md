# WBP_Title — fantasy title style

Applied to `/Game/UI/Widget/Title/WBP_Title` on 2026-09-16 through the local Unreal Engine 5.8.2 MCP server.

- Warm castle background and logo, plum panels, cream text, gold primary action.
- Quick match is the primary action; room list and training are secondary actions.
- Shop, guide, and records share a compact row. Volume/mute and exit use a separate utility bar.
- Existing bound button and audio-slider objects, names, C++ parent class, and event connections are retained.
- A 1920 × 1080 design canvas is wrapped in `SizeBox` and `ScaleBox` (`ScaleToFit`) so the composition keeps its aspect ratio. Other aspect ratios are letterboxed.
- Button hover increases the artwork brightness; press darkens it and offsets button content by three design units.
- Functional labels use the project's existing Noto Sans CJK KR Bold/Medium fonts.

## Assets and sources

- Widget: `Content/UI/Widget/Title/WBP_Title.uasset`
- Dedicated artwork: `Content/UI/Asset/TitleStyle/`
- Background PNG and editable button/icon SVGs: `Content/UI/Asset/TitleStyle/SourceArt/`
- Actual PIE screenshot: `output/title-style/WBP_Title-preview.png`
- Vector source generator: `Scripts/generate_title_chrome.cjs` (Node.js + sharp)
- MCP authoring functions: `Scripts/restyle_title_widget.mjs`

The new background was produced with the built-in image generation tool from the approved concept. The UI chrome/icons are deterministic vector artwork rasterized with sharp. `SourceArt/background-prompt.txt` contains the exact image-edit prompt. All labels and interactive controls are real UMG widgets; the background contains only scenery and the English game logo.

## Validation

- Widget Blueprint compilation and explicit asset saves succeeded.
- All nine existing bound controls are present, including `AudioVolumeSlider_`.
- Actual PIE rendering inspected in a wide window and a 640 × 480 client area: Korean text is legible, the menu stays inside the viewport, and the smaller aspect ratio is letterboxed.
- Shop, Guide, and Records were opened through the redesigned buttons in PIE.
- Hover interaction was exercised; the styles include distinct normal/hovered/pressed/disabled brushes.
- No Blueprint/runtime errors were reported in the verification editor log.
- Online quick matching and joining a network room were not exercised.

The pre-edit widget binary and property snapshots are preserved locally under `Saved/Codex/TitleStyle/before/`. Stop the editor before restoring the binary backup. Verification logs and additional screenshots are in `Saved/Codex/TitleStyle/`.
