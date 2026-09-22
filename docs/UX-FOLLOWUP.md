# Settings and controls follow-up

The current build is a development checkpoint, not a finished settings experience.
Further UI work is deferred until hands-on feedback has been collected.

- The F8 panel has an opaque background, preventing inspection of the world
  while changing visual settings. Evaluate a translucent background and how
  settings can be previewed in context.
- Lamium's panel has no key-binding editor. Default keys are defined in code;
  the implementation registers them with Minecraft's Keyboard & Mouse settings,
  and remapping there was previously tested. Users still report being unable to
  change bindings in game. Reproduce that experience and improve discovery and
  editing within the unified settings UI; do not treat the existing registration
  as resolution of the usability problem.
- The top-left gameplay hints have no user-facing visibility toggle.
- A flat ten-row panel is only a prototype. Revisit navigation, grouping, search
  and descriptions before adding enough features to make it hard to browse.
- Reconsider explicit Save and close. Evaluate immediate application and
  automatic persistence, including clear handling of failed saves, instead of
  assuming the current draft/Save/Cancel interaction is the final design.

These are open design tasks, not promises of a particular implementation.
Runtime validation gaps remain separately documented in [VALIDATION.md](VALIDATION.md).
