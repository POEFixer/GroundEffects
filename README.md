# Ground Effects

Current version: 1.0.4

Requires POE2Fixer 364 or newer.

A POEFixer plugin that marks dangerous ground effects in Path of Exile 2 with coloured circles on the ground:
shocked, chilled and ignited ground, caustic clouds, and anything else you add a rule for. Each circle has the
size of the effect itself, follows the camera smoothly and stays on the ground while you move.

## How to use

1. Install **Ground Effects** from the **Plugin Manager**, the first tab under POEFixer's **Plugins** tab. It is
   enabled right away, and the Plugin Manager keeps it up to date. The **Enabled** box at the top of the
   plugin's own tab turns it off and on.
2. Enter a map. Hostile ground effects that match a rule get a circle in the rule's colour.
3. Change colours, add your own rules or turn the plugin's options on and off in its tab. Changes apply at once
   and are saved automatically.

To mark an effect that has no rule yet, open **Diagnostics**, turn on **Discover effects**, walk near the effect
and press **+** on its row: a rule for its type id is added at the bottom of the rule list.

## Settings

### Display
- **Fill circles**: paint the inside of each circle with the rule colour (off: outline only).
- **Show names**: write the rule name (or the effect's type id) beside each circle.
- **Outline width**: thickness of the circle outline. A thin dark edge under it keeps it visible on bright ground.
- **Opacity**: fades all circles and names together; each rule's own transparency applies on top.
- **Smoothness**: points per circle (more is rounder and costs a little more time per frame).
- **Height offset**: moves all circles up or down; leave it at 0 unless circles float or sink.

### When to show
- **Only hostile effects**: skip effects created by you and your allies.
- **Hide in towns and hideouts**.
- **Only while the game window is active**: hide the circles while another window is in front of the game.
- **Hide while the POEFixer menu is open**.

### Effect rules
Each rule has an on/off switch, a colour (its transparency is the fill strength), a name and a match text. The
match text is searched, not case-sensitive, in the effect's type id, visuals and metadata path, for example
`ShockedGround`, `ChilledGround`, `IgnitedGround`, `CausticCloud`. Rules are tried from the top and the first
enabled match wins; use the arrows to reorder them. **Restore default rules** brings back the four defaults.

### Performance
- **Look for new effects every**: how often new effects are picked up (circles already shown move every frame).
- **Scan time per frame**: the most time one frame may spend looking; a longer search continues next frame.
- **Most circles at once**: upper limit of circles drawn together.

### Diagnostics
Live numbers (circles, scan cost), **Fast path filter** (read only entities whose path looks like a ground
effect; turn off only if an effect never shows up in the discovery list), **Discover effects** with an optional
log file `config/GroundEffectsDiscovery.log`. Discovery is never saved and is off again next time.

## Known limits

- Only real ground effects are marked. Monster explosions and other one-off server effects have no ground-effect
  data the plugin can read, so they cannot be shown.
- An effect is picked up at the next scan after it appears (150 ms by default).
- Settings live in `config/GroundEffects.json` next to the plugin. An older `config/GroundEffects.ini` is read
  once and moved to the new file automatically.

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `GroundEffects.zip` from [Releases](https://github.com/POEFixer/GroundEffects/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\GroundEffects\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/GroundEffects/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against
- `tools/plugin_i18n/gen.py` - the translation tool: after editing `Plugins/GroundEffects/lang/en.json` run `python tools/plugin_i18n/gen.py Plugins/GroundEffects` (it rewrites the embedded English header); `--gate` checks every language file and the keys the sources use

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `GroundEffects.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\GroundEffects\GroundEffects.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `GroundEffects.dll` into `Plugins\GroundEffects\` next to the POE2Fixer executable, together with `Plugins\GroundEffects\lang` from this repository (same folder names).

## Credits

Originally created by 1k4ru5g3 (https://github.com/1k4ru5g3/GroundEffects). Maintained by POEFixer since
October 2026.
