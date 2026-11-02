# Cinematic Conversation Camera LE

An in-progress backport of Cinematic Conversation Camera for Skyrim Legendary Edition.

## Target

- Skyrim LE runtime 1.9.32.0.8 (32-bit)
- SKSE 1.7.3

## Status

The current build checks the LE runtime, watches the dialogue menu, resolves and retains the dialogue NPC, and uses Skyrim's camera-target function to switch between NPC-facing and player-facing views when the NPC's speech state changes in third person. First person keeps the NPC lock. The field of view transitions smoothly, and the previous camera target, rotation, and field of view are restored when dialogue closes. This speaker-switching step still needs in-game validation.

This is the first ACC-style camera milestone. Distance-based framing, configurable POV/speed, head tracking, and later CCC scene coverage will follow in stages.

## Build

GitHub Actions builds a Win32 DLL on Windows and uploads it as the `CinematicConversationCamera-LE` workflow artifact. The workflow checks out a pinned SKSE source archive for its headers; it does not include SKSE binaries in the artifact.

For a local build, use Visual Studio 2022 with the C++ desktop workload and CMake, then configure for Win32 and set `SKSE_LE_ROOT` to the `src` folder from the SKSE 1.7.3 source archive.

## Log

The plugin writes `CinematicConversationCamera.log` to `Documents/My Games/Skyrim/SKSE`. It records SKSE loading, dialogue hook setup, dialogue menu open/close events, and whether the dialogue actor was resolved. It does not write a line every frame.

## License

The CCC source is GPL-3.0-or-later, with the permissions listed in `EXCEPTIONS.md` from the upstream repository. Alternate Conversation Camera LE by Felisky and the upstream ACC source by NasGorTelorCeplok were used as behavioral and technical references. This backport will retain applicable notices as its implementation grows.
