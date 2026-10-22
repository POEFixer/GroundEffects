# Changelog

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
