# Cinematic Conversation Camera LE

An in-progress backport of Cinematic Conversation Camera for Skyrim Legendary Edition.

## Target

- Skyrim LE runtime 1.9.32.0.8 (32-bit)
- SKSE 1.7.3

## Status

The current build checks the LE runtime, watches the dialogue menu, resolves the current dialogue actor, and blends the camera toward an NPC-facing shot with a tighter field of view. It restores the saved camera rotation and field of view when the menu closes.

This is the first camera milestone. Speaker-aware reverse shots, scene coverage, and the remaining CCC features will follow in stages. The current camera behavior still needs in-game validation.

## Build

GitHub Actions builds a Win32 DLL on Windows and uploads it as the `CinematicConversationCamera-LE` workflow artifact. The workflow checks out a pinned SKSE source archive for its headers; it does not include SKSE binaries in the artifact.

For a local build, use Visual Studio 2022 with the C++ desktop workload and CMake, then configure for Win32 and set `SKSE_LE_ROOT` to the `src` folder from the SKSE 1.7.3 source archive.

## License

The CCC source is GPL-3.0-or-later, with the permissions listed in `EXCEPTIONS.md` from the upstream repository. This backport will retain the applicable notices as its implementation grows.
