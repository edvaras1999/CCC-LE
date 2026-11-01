# CCC LE port notes

Target runtime: Skyrim LE 1.9.32.0.8 with SKSE 1.7.3 (32-bit).

## First milestone

1. Load only against the supported LE runtime and receive SKSE's `DataLoaded` message.
2. Track the dialogue partner from the LE topic manager and dialogue line events.
3. Drive one camera shot at a time, blend between player and NPC shots, and restore the vanilla camera on dialogue exit.

## Port boundary

Keep the shot-selection and geometry policy independent of game types. Reimplement the runtime boundary using the LE SKSE headers, LE structure layouts, and verified LE hooks. The SE/AE `CommonLibSSE-NG` types and Address Library relocations cannot be used in this Win32 DLL.

Letterbox rendering, subtitle placement, face animation, and third-party integrations are later milestones. The LE renderer is Direct3D 9, so CCC's Direct3D 11 letterbox implementation needs a separate renderer.

## Build setup

For local builds, this folder expects the supplied SKSE 1.7.3 source archive to be extracted to `../skse173/skse_1_07_03`. Configure with a Win32 MSVC generator. GitHub Actions checks out the archived SKSE source at a pinned commit, configures Win32, and uploads the resulting DLL as a workflow artifact.

The initial plugin entry point is only a runtime and lifecycle scaffold; it does not yet alter camera behavior. A green Actions build will certify compilation only; in-game camera behavior still needs a Skyrim LE runtime for validation.
