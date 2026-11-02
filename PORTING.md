# CCC LE port notes

Target runtime: Skyrim LE 1.9.32.0.8 with SKSE 1.7.3 (32-bit).

## First milestone

1. Load only against the supported LE runtime and receive SKSE's `DataLoaded` message. **Implemented.**
2. Track dialogue-menu open/close events and resolve the active actor through the LE topic manager handle. **Implemented.**
3. Blend camera yaw, pitch, and field of view toward an NPC-facing single, then restore the captured camera values on dialogue exit. **Prototype implemented; needs in-game validation.**
4. Add speaker-aware player/NPC reverse shots after the initial camera driver is verified in-game.

## Port boundary

Keep the shot-selection and geometry policy independent of game types. Reimplement the runtime boundary using the LE SKSE headers, LE structure layouts, and verified LE hooks. The SE/AE `CommonLibSSE-NG` types and Address Library relocations cannot be used in this Win32 DLL.

Letterbox rendering, subtitle placement, face animation, and third-party integrations are later milestones. The LE renderer is Direct3D 9, so CCC's Direct3D 11 letterbox implementation needs a separate renderer.

## Build setup

For local builds, this folder expects the supplied SKSE 1.7.3 source archive to be extracted to `../skse173/skse_1_07_03`. Configure with a Win32 MSVC generator. GitHub Actions checks out the archived SKSE source at a pinned commit, configures Win32, and uploads the resulting DLL as a workflow artifact.

The GitHub Actions build certifies compilation only. Camera behavior still needs validation in a Skyrim LE runtime, especially its blend timing and interaction with vanilla camera updates.
