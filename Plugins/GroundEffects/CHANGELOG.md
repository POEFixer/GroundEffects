# Changelog

## 1.0.2

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.0.1

- The "?" next to a section title now sits in its own gap instead of on the section's line.
- Diagnostics now looks the same as in the other POEFixer plugins.
- The bottom of the settings tab names the plugin's original author, as in the other plugins maintained by POEFixer.

## 1.0.0

- First release maintained by POEFixer.
- Circles now stay on the ground while the camera moves instead of trailing behind it, and a circle at the
  screen edge keeps its visible part instead of changing shape.
- Much lighter on performance: each effect is read once, only possible ground effects are looked at, and the
  search is spread over frames so the game never stutters.
- New settings tab in the POEFixer style: grouped cards, explanations on every option, reorderable rules,
  restore default rules, overall opacity, and a Diagnostics section.
- Names are outlined so they stay readable on bright ground; circles get a thin dark edge.
- Discover effects lists each kind of effect once and can add a rule for it in one click.
- Settings are saved safely and automatically; your old settings are taken over.
