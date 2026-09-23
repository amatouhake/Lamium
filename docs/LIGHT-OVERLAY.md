# Light level overlay

Experimental, disabled and unbound by default. Features and Hotkeys expose a
toggle; the feature also selects stored sky light instead of stored block light.
Settings apply immediately and use the existing persistence and binding system.

The initial implementation scans a 9 by 9 by 5 cell volume around the player's
floored position (405 candidates). It draws decimal 0 through 15 as flat line
digits above air cells whose block below reports solid through the SDK. Multiple
floors in the volume can each have a marker. Digits have north at their top.
Only loaded chunks and cells within the dimension height range are queried.
Unavailable/invalid samples produce no marker, never an invented zero.

These are stored client light values, not effective daylight, a hostile-mob
spawn verdict, or server-only information. Non-air surfaces, partial blocks and
unusual solid/collision shapes are not comprehensively supported. This first
pass scans each rendered frame and keeps no world references or cached samples;
runtime performance must be measured before increasing its fixed small range.

Pure tests cover negative coordinates, stacked floors, unavailable and invalid
samples, coordinate overflow, and decimal geometry for every valid value.
The shared settings tests exercise labels and disk round trips for both options.
Native value correctness, solid-surface filtering, depth/readability, light
updates and dimension changes still require Minecraft validation. Do not classify
this feature as runtime-validated based on build/tests alone.
