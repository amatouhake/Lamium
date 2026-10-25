# Translating Lamium

Lamium's UI ships in English (default), Japanese, Simplified Chinese
(`zh_CN`) and Traditional Chinese (`zh_TW`). The text follows the game
language; any other language shows English. The Traditional Chinese text is
its own translation from English, not converted from Simplified Chinese.

## Where the text lives

- `src/ui/Translations.h`: every key with its English and Japanese text.
- `src/ui/TranslationsZhCN.h`: Simplified Chinese, and
  `src/ui/TranslationsZhTW.h`: Traditional Chinese. Each has one row per key
  in the same order as `Translations.h`. The build fails if a key is missing
  or out of order.

## Correcting the Chinese text

The first Simplified Chinese text was drafted by a coding agent and has not
been reviewed by a native speaker yet. Corrections are welcome, especially
Minecraft and mod terminology. Send them as a pull request that edits
`TranslationsZhCN.h`:

- Change only the text, never the key or the row order.
- Keep every placeholder (`{}`, `{:.1f}`, ...) in its original order.
- Settings labels use `Name: {}` with an ASCII colon and space; the settings
  screen splits the name from the value there. Hotkey names keep the
  `Lamium: ` prefix.
- Prefer the terms the game itself uses in `zh_CN` (for example 物品栏, 潜影盒,
  收纳袋, 鞘翅, 区块), and in `zh_TW` (物品欄, 界伏蚌盒, 組合包, 鞘翅, 區塊).
  Feature names in `zh_TW` follow what Taiwanese players call them (for
  example 靈魂出竅 for FreeCamera, 環顧四周 for Freelook).
- Traditional Chinese corrections edit `TranslationsZhTW.h` the same way.

If you would rather not open a pull request, open an issue with the
language, where the text appears, the current text and your wording
([Contributing](../CONTRIBUTING.md)).

Run `xmake build LamiumTests && xmake run LamiumTests` if you can; the tests
check placeholders and that every locale is complete. A screenshot from the
game helps when a line is too long for its column.

## Adding a key

Add the key with English and Japanese to `Translations.h`, then add the
Chinese rows at the same position in `TranslationsZhCN.h` and
`TranslationsZhTW.h`. If you cannot write a Chinese row, put the English text
there and say so in the pull request.
