# Client SDK recipe

`levilamina-client-sdk.lua` prepares the SDK for Windows x64, LeviLamina Client
26.51.3. Both the source archive and the official release archive have pinned
SHA-256 checksums. The release DLL is used only to generate an import library;
it is not copied into Lamium's package or installed into a game instance.

The normal upstream package recipe builds the entire runtime from source. With
this tag and MSVC 14.44 headers, that build fails in `ExecuteCommandEvent.cpp`
because the defaulted `MinecraftCommands` destructor deletes an incomplete
`CommandRegistry`. A pre-existing SDK cache can hide this failure.

The local recipe copies the public API and Minecraft headers, overlaying the
client headers in the same order as the upstream build. It generates a module
definition from the official DLL's named exports and uses Microsoft's librarian
to create `LeviLamina.lib`. The game still needs the separately installed runtime.

Dependencies are explicitly listed, with Bedrock runtime data
`v26.51.1-client.4` matching this release's source build. Package repository
revisions and transitive versions remain in the project's dependency lock.
Review the source headers, runtime exports, dependencies and checksums together
when upgrading the SDK. Never substitute an unrelated local runtime DLL.

This recipe is Lamium's build integration, not an upstream SDK release. Header
licenses remain those of LeviLamina and its dependencies; see the root notices.
