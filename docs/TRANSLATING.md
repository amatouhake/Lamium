# Translating Lamium

Lamium's UI ships in English (default), Japanese, Simplified Chinese
(`zh_CN`) and Spanish (`es_ES` / `es_MX`). The text follows the game language; any other language shows
English. Traditional Chinese is not offered until it is translated and
reviewed on its own (it is not generated from Simplified Chinese).

## Where the text lives

- `src/ui/Translations.h`: every key with its English and Japanese text.
- `src/ui/TranslationsZhCN.h`: Simplified Chinese, one row per key in the
  same order as `Translations.h`.
- `src/ui/TranslationsEs.h`: Spanish, one row per key in the same order as
  `Translations.h`. The build fails if a key is missing or out of order.

## Correcting localized text

Corrections from native speakers are welcome, especially Minecraft and mod terminology. Send them as a pull request that edits `TranslationsZhCN.h` or `TranslationsEs.h`:

- Change only the text, never the key or the row order.
- Keep every placeholder (`{}`, `{:.1f}`, ...) in its original order.
- Settings labels use `Name: {}` with an ASCII colon and space; the settings
  screen splits the name from the value there. Hotkey names keep the
  `Lamium: ` prefix.
- Prefer the terms the game itself uses in each locale.

If you would rather not open a pull request, open an issue with the
language, where the text appears, the current text and your wording
([Contributing](../CONTRIBUTING.md)).

Run `xmake build LamiumTests && xmake run LamiumTests` if you can; the tests
check placeholders and that every locale is complete. A screenshot from the
game helps when a line is too long for its column.

## Adding a key

Add the key with English and Japanese to `Translations.h`, then add the
corresponding rows at the same position in `TranslationsZhCN.h` and `TranslationsEs.h`.

