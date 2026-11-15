# Contributing to Lamium

Lamium is a hobby project, developed at the maintainer's pace and for their
own use first. Reports and pull requests are welcome, but there is no promise
of a reply, a fix or a merge. The maintainer checks every change in
Minecraft before it ships.

Please write in English, and keep it short: a few clear lines are read
sooner than a long text. If English is hard for you, a machine translation is
fine; put your original text below it.

Be polite. The maintainer may close or lock issues and pull requests.

## Issues

Lamium supports the Minecraft, LeviLamina and Windows versions listed in the
[README](README.md#status); reports from other versions may not be followed
up. Pick the form that fits when you open an issue, or "Question or other"
when none does. One topic per issue helps. If a report mixes several, the maintainer will
split it rather than close it.

- **Bug:** what happened and what you expected, the steps to reproduce it,
  and the Minecraft, LeviLamina and Lamium versions. In Lamium Settings
  (`L`), click the version next to the title to copy all three in one line.
  How you launch the game and which other mods are installed help too.
  `mods/Lamium/logs/lamium.log` often helps; look through it before pasting,
  since an error line may contain a file path with your Windows user name.
- **Feature or improvement:** what you want. Optionally, what Lamium does
  today and anything you know about Bedrock's limits. A short request is
  fine.
- **Translation:** the language, where the text appears, the current text
  and, if you have one, a better wording. See [Translating](docs/TRANSLATING.md).
- **Anything else:** questions, which platforms are supported, or reports
  that Lamium works on a particular server. Those reports are useful: much
  of [validation status](docs/VALIDATION.md) is still unchecked.

Requests that will be done stay open, get a comment when they enter the
[backlog](docs/BACKLOG.md), and close with the release that ships them.
Requests that will not be done are closed as "not planned" with a reason.

## Security

Report a security problem privately through GitHub's "Report a
vulnerability" button on the Security tab, not in a public issue. A crash
from a malformed file can be reported as an ordinary bug.

## Pull requests

| Change | How |
| --- | --- |
| Translation or documentation fixes | Welcome, no need to ask first |
| Small bug fixes | Welcome, no need to ask first |
| Larger features | A pull request is fine, but opening an issue first saves wasted work. The maintainer may rework a feature to fit the rest of Lamium, or decline it |
| Dependencies or build settings | Not accepted unless discussed first: they change the distribution contract ([Distribution](docs/DISTRIBUTION.md)) |

Keep one change per pull request. CI builds every pull request and runs
`LamiumTests`, `LamiumNativeTests` and the package checks. Run
`xmake build LamiumTests && xmake run LamiumTests` locally if you can; build
steps are in the [README](README.md#build).

Code follows the rules in
[AGENTS.md](AGENTS.md#architecture-and-rules): pure logic in headers with
tests, game glue in `.cpp`, no game pointers kept across frames, and every
feature restores vanilla behavior when it is turned off. Its "Code style"
section applies too; the rest of that file (deploying, committing, when to
stop) is the maintainer's own workflow. Product and UI rules are in
[Design](docs/DESIGN.md).

Every new user-visible string needs a row in each language, or the build
fails. Write the English text; if you cannot write the Japanese or Chinese
text, put the English text in those rows too and say so in the pull request.
The maintainer will translate it.

AI-assisted code is fine and does not need to be disclosed. Whoever submits
a change is responsible for it.

### Where the code comes from

Lamium is licensed under `LGPL-3.0-only`, and contributions are accepted
under the same license. There is no CLA or DCO. Instead, the pull request
description asks you to state that:

- you wrote the change, or it comes from a source you name;
- you can offer it under `LGPL-3.0-only`;
- it does not copy code from the reference-only projects in
  [Provenance](docs/PROVENANCE.md);
- if you also work on another mod, which one and how it relates.

Translations do not need this statement.
