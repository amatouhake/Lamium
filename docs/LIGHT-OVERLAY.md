# Light level overlay

Experimental, disabled and unbound by default. Features and Hotkeys expose a
toggle; the feature also selects stored sky light instead of stored block light.
Settings apply immediately and use the existing persistence and binding system.

The initial implementation scans a 9 by 9 by 5 cell volume around the player's
floored feet position (405 candidates). It draws decimal 0 through 15 as flat line
digits above air cells whose block below reports solid through the SDK. Multiple
floors in the volume can each have a marker. Digits have north at their top.
Only loaded chunks and cells within the dimension height range are queried.
Unavailable/invalid samples produce no marker, never an invented zero.

These are stored client light values, not effective daylight, a hostile-mob
spawn verdict, or server-only information. Non-air surfaces, partial blocks and
unusual solid/collision shapes are not comprehensively supported. This first
pass scans each rendered frame and keeps no world references or cached samples;
runtime performance must be measured before increasing its fixed small range.
Digit geometry appends directly into one reserved render batch, avoiding a
temporary vector and copy for every marker. This does not reduce native world
queries or cache light values; frame-time impact has not been measured.

Pure tests cover negative coordinates, stacked floors, unavailable and invalid
samples, coordinate overflow, and decimal geometry for every valid value.
The shared settings tests exercise labels and disk round trips for both options.
Native value correctness, solid-surface filtering, depth/readability, light
updates and dimension changes still require Minecraft validation. Do not classify
this feature as runtime-validated based on build/tests alone.

## Initial runtime smoke (2026-09-23)

The normal (non-trace, non-probe) build of `a4213a0` displayed floor digits
in a local creative world in rear third-person view. Switching from stored block
light to stored sky light changed visible digits from 0 to 15; the overlay also
remained visible after closing settings. Switching back and disabling the feature
removed the digits while the existing Shape overlay remained visible. Both light
options were restored to Off, with the binding still Unbound, before normal exit.

This verifies rendering and immediate settings integration in that scene only.
There was no independent comparison of the sampled values, light-source edit,
dimension transition, or performance measurement. Those validation gates remain
open. See [VALIDATION.md](VALIDATION.md) for the installed build identity.
