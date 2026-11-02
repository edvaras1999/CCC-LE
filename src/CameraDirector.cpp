#include "common/IPrefix.h"

#include <string>

#include "CameraDirector.h"
#include "Logger.h"

#include "skse/GameCamera.h"
#include "skse/GameData.h"
#include "skse/GameEvents.h"
#include "skse/GameMenus.h"
#include "skse/GameReferences.h"
#include "skse/GameThreads.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace CameraDirector
{
	namespace
	{
		constexpr const char* kDialogueMenuName = "Dialogue Menu";
		constexpr UInt32 kMenuTopicManagerAddress = 0x013105D8;
		constexpr UInt32 kInvalidRefHandleAddress = 0x01310630;
		constexpr UInt32 kPlayerPointerAddress = 0x01B2E8E4;
		constexpr UInt32 kLookupRefByHandleAddress = 0x004A9180;
		constexpr UInt32 kSetCameraTargetAddress = 0x0083D960;
		constexpr UInt32 kDialogueMenuVTableAddress = 0x010E4C9C;
		constexpr UInt32 kDialogueMenuNextFrameSlot = 5;
		constexpr UInt32 kCameraTargetHandleOffset = 0x2C;
		constexpr UInt32 kPlayerRefHandle = 0x00100000;
		constexpr float kTargetFov = 55.0f;
		constexpr float kAimHeight = 82.0f;
		constexpr float kBlendRate = 7.0f;
		constexpr DWORD kSpeakerSwitchDelayMs = 167;
		constexpr float kPi = 3.14159265358979323846f;

		bool g_dialogueOpen = false;
		bool g_restoreCamera = false;
		bool g_haveSnapshot = false;
		bool g_loggedActorResolved = false;
		bool g_loggedCameraActive = false;
		bool g_loggedFirstPersonNotice = false;
		bool g_speakerStateInitialized = false;
		bool g_lastNpcSpeaking = false;
		bool g_cameraTargetChanged = false;
		float g_savedWorldFov = 65.0f;
		float g_savedFirstPersonFov = 65.0f;
		float g_savedRotZ = 0.0f;
		float g_savedRotX = 0.0f;
		DWORD g_lastTick = 0;
		DWORD g_speakerStateSince = 0;
		UInt32 g_dialogueTargetHandle = 0;
		UInt32 g_savedCameraTargetHandle = 0;
		using DialogueMenuNextFrameFn = void(__thiscall*)(void*, UInt32, UInt32);
		DialogueMenuNextFrameFn g_originalDialogueMenuNextFrame = nullptr;

		[[nodiscard]] float WrapAngle(float a_angle)
		{
			while (a_angle > kPi) {
				a_angle -= 2.0f * kPi;
			}
			while (a_angle < -kPi) {
				a_angle += 2.0f * kPi;
			}
			return a_angle;
		}

		[[nodiscard]] float BlendAngle(float a_current, float a_target, float a_amount)
		{
			return a_current + WrapAngle(a_target - a_current) * a_amount;
		}

		[[nodiscard]] UInt32 CurrentDialogueTargetHandle()
		{
			auto* topics = *reinterpret_cast<MenuTopicManager**>(kMenuTopicManagerAddress);
			if (!topics) {
				return 0;
			}

			return topics->talkingHandle;
		}

		[[nodiscard]] PlayerCharacter* Player();

		[[nodiscard]] Actor* ActorFromHandle(UInt32 a_handle)
		{
			UInt32 handle = a_handle;
			const UInt32 invalidHandle = *reinterpret_cast<const UInt32*>(kInvalidRefHandleAddress);
			if (handle == 0 || handle == invalidHandle) {
				return nullptr;
			}

			using LookupRefByHandle = bool(__cdecl*)(UInt32*, TESObjectREFR**);
			const auto lookup = reinterpret_cast<LookupRefByHandle>(kLookupRefByHandleAddress);
			TESObjectREFR* reference = nullptr;
			if (!lookup(&handle, &reference) || !reference || reference->formType != kFormType_Character) {
				return nullptr;
			}
			return static_cast<Actor*>(reference);
		}

		[[nodiscard]] Actor* DialogueActor()
		{
			if (g_dialogueTargetHandle == 0) {
				const UInt32 handle = CurrentDialogueTargetHandle();
				auto* candidate = ActorFromHandle(handle);
				auto* player = Player();
				if (candidate && player && candidate != static_cast<Actor*>(player)) {
					g_dialogueTargetHandle = handle;
				}
			}
			return ActorFromHandle(g_dialogueTargetHandle);
		}

		[[nodiscard]] PlayerCharacter* Player()
		{
			auto** player = reinterpret_cast<PlayerCharacter**>(kPlayerPointerAddress);
			return player ? *player : nullptr;
		}

		[[nodiscard]] UInt32 CameraTargetHandle(PlayerCamera* a_camera)
		{
			return *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(a_camera) + kCameraTargetHandleOffset);
		}

		void SetCameraTarget(PlayerCamera* a_camera, Actor* a_target)
		{
			using SetCameraTargetFn = void(__thiscall*)(PlayerCamera*, Actor*);
			const auto setCameraTarget = reinterpret_cast<SetCameraTargetFn>(kSetCameraTargetAddress);
			setCameraTarget(a_camera, a_target);
			g_cameraTargetChanged = true;
		}

		[[nodiscard]] bool IsActorTalking(const Actor* a_actor)
		{
			return (a_actor->flags1 & 0x180) != 0x180 || a_actor->unk108[0] > 0;
		}

		void RestoreCameraTarget(PlayerCamera* a_camera)
		{
			auto* player = Player();
			if (!player) {
				return;
			}

			if (g_savedCameraTargetHandle == kPlayerRefHandle) {
				SetCameraTarget(a_camera, static_cast<Actor*>(player));
				return;
			}

			if (auto* oldTarget = ActorFromHandle(g_savedCameraTargetHandle)) {
				SetCameraTarget(a_camera, oldTarget);
			} else {
				SetCameraTarget(a_camera, static_cast<Actor*>(player));
			}
		}

		void SaveCameraState()
		{
			auto* camera = PlayerCamera::GetSingleton();
			if (!camera) {
				return;
			}

			g_savedWorldFov = camera->worldFOV;
			g_savedFirstPersonFov = camera->firstPersonFOV;
			g_savedRotZ = camera->rotZ;
			g_savedRotX = camera->rotX;
			g_savedCameraTargetHandle = CameraTargetHandle(camera);
			g_haveSnapshot = true;
		}

		void RestoreCameraState()
		{
			if (!g_haveSnapshot) {
				return;
			}
			if (auto* camera = PlayerCamera::GetSingleton()) {
				camera->worldFOV = g_savedWorldFov;
				camera->firstPersonFOV = g_savedFirstPersonFov;
				camera->rotZ = g_savedRotZ;
				camera->rotX = g_savedRotX;
			}
			g_haveSnapshot = false;
			g_restoreCamera = false;
		}

		class MenuListener final : public BSTEventSink<MenuOpenCloseEvent>
		{
		public:
			EventResult ReceiveEvent(MenuOpenCloseEvent* a_event, EventDispatcher<MenuOpenCloseEvent>*) override
			{
				if (!a_event || !a_event->menuName.data ||
					std::strcmp(a_event->menuName.data, kDialogueMenuName) != 0) {
					return kEvent_Continue;
				}

				if (a_event->opening) {
					SaveCameraState();
					g_dialogueOpen = true;
					g_restoreCamera = false;
					g_loggedActorResolved = false;
					g_loggedCameraActive = false;
					g_loggedFirstPersonNotice = false;
					g_speakerStateInitialized = false;
					g_cameraTargetChanged = false;
					g_dialogueTargetHandle = 0;
					Logger::Write("Dialogue Menu opened; camera state captured.");
				} else {
					g_dialogueOpen = false;
					if (!g_loggedActorResolved) {
						Logger::Write("Dialogue ended before a dialogue actor could be resolved.");
					}
					if (g_cameraTargetChanged) {
						if (auto* camera = PlayerCamera::GetSingleton()) {
							RestoreCameraTarget(camera);
						}
					}
					RestoreCameraState();
					g_dialogueTargetHandle = 0;
					g_speakerStateInitialized = false;
					Logger::Write("Dialogue Menu closed; camera state restored.");
				}
				return kEvent_Continue;
			}
		};

		MenuListener g_menuListener;

		void UpdateCamera()
		{
			auto* camera = PlayerCamera::GetSingleton();
			if (!camera) {
				return;
			}

			const DWORD now = GetTickCount();
			float delta = g_lastTick == 0 ? (1.0f / 60.0f) : static_cast<float>(now - g_lastTick) / 1000.0f;
			g_lastTick = now;
			delta = std::clamp(delta, 0.0f, 0.05f);
			const float blend = std::clamp(delta * kBlendRate, 0.0f, 1.0f);

			if (g_dialogueOpen) {
				const auto state = camera->cameraState ? camera->cameraState->stateId : 0xFFFFFFFFu;
				if (state != PlayerCamera::kCameraState_FirstPerson &&
					state != PlayerCamera::kCameraState_ThirdPerson2) {
					return;
				}

				auto* target = DialogueActor();
				auto* player = Player();
				if (!target || !player || target == static_cast<Actor*>(player)) {
					return;
				}
				if (!g_loggedActorResolved) {
					Logger::Write("Dialogue actor resolved; camera aim updates are active.");
					g_loggedActorResolved = true;
				}

				if (!g_loggedCameraActive) {
					Logger::Write("ACC-style face focus active; speaker-based target switching is enabled in third person.");
					g_loggedCameraActive = true;
				}

				const bool thirdPerson = state == PlayerCamera::kCameraState_ThirdPerson2;
				if (thirdPerson) {
					const bool npcSpeaking = IsActorTalking(target);
					if (!g_speakerStateInitialized || npcSpeaking != g_lastNpcSpeaking) {
						g_speakerStateInitialized = true;
						g_lastNpcSpeaking = npcSpeaking;
						g_speakerStateSince = now;
						if (npcSpeaking) {
							if (CameraTargetHandle(camera) != kPlayerRefHandle) {
								SetCameraTarget(camera, static_cast<Actor*>(player));
								Logger::Write("NPC speaking; camera target switched to player for the NPC-facing view.");
							}
						}
					}

					if (!npcSpeaking && now - g_speakerStateSince >= kSpeakerSwitchDelayMs &&
						CameraTargetHandle(camera) == kPlayerRefHandle) {
						SetCameraTarget(camera, target);
						Logger::Write("NPC line ended; camera target switched to the NPC for the player-facing view.");
					}
				} else {
					if (!g_loggedFirstPersonNotice) {
						Logger::Write("First-person dialogue keeps the NPC lock; speaker POV switching requires third person.");
						g_loggedFirstPersonNotice = true;
					}

					const float dx = target->pos.x - camera->pos.x;
					const float dy = target->pos.y - camera->pos.y;
					const float dz = target->pos.z + kAimHeight - camera->pos.z;
					const float horizontal = std::sqrt(dx * dx + dy * dy);
					if (horizontal >= 1.0f) {
						const float desiredZ = std::atan2(dx, dy);
						const float desiredX = std::atan2(dz, horizontal);
						camera->rotZ = BlendAngle(camera->rotZ, desiredZ, blend);
						camera->rotX = BlendAngle(camera->rotX, desiredX, blend);
					}
				}

				camera->worldFOV += (kTargetFov - camera->worldFOV) * blend;
				camera->firstPersonFOV += (kTargetFov - camera->firstPersonFOV) * blend;
				return;
			}

			if (g_restoreCamera && g_haveSnapshot) {
				camera->rotZ = BlendAngle(camera->rotZ, g_savedRotZ, blend);
				camera->rotX = BlendAngle(camera->rotX, g_savedRotX, blend);
				camera->worldFOV += (g_savedWorldFov - camera->worldFOV) * blend;
				camera->firstPersonFOV += (g_savedFirstPersonFov - camera->firstPersonFOV) * blend;

				const float fovError = std::fabs(camera->worldFOV - g_savedWorldFov) +
					std::fabs(camera->firstPersonFOV - g_savedFirstPersonFov);
				const float angleError = std::fabs(WrapAngle(camera->rotZ - g_savedRotZ)) +
					std::fabs(WrapAngle(camera->rotX - g_savedRotX));
				if (fovError < 0.1f && angleError < 0.01f) {
					g_restoreCamera = false;
					g_haveSnapshot = false;
				}
			}
		}

		void __fastcall DialogueMenuNextFrameHook(void* a_menu, void*, UInt32 a_arg0, UInt32 a_arg1)
		{
			g_originalDialogueMenuNextFrame(a_menu, a_arg0, a_arg1);
			UpdateCamera();
		}

		bool InstallDialogueMenuFrameHook()
		{
			static_assert(sizeof(void*) == sizeof(UInt32), "Skyrim LE is a 32-bit executable");
			const UInt32 slotAddress = kDialogueMenuVTableAddress + kDialogueMenuNextFrameSlot * sizeof(void*);
			auto** slot = reinterpret_cast<void**>(slotAddress);
			if (*slot == reinterpret_cast<void*>(&DialogueMenuNextFrameHook)) {
				return g_originalDialogueMenuNextFrame != nullptr;
			}
			if (!*slot) {
				Logger::Write("Dialogue menu vtable slot 5 is empty; hook was not installed.");
				return false;
			}

			DWORD oldProtection = 0;
			if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtection)) {
				Logger::Write("VirtualProtect failed for the dialogue menu vtable; hook was not installed.");
				return false;
			}
			g_originalDialogueMenuNextFrame = reinterpret_cast<DialogueMenuNextFrameFn>(*slot);
			*slot = reinterpret_cast<void*>(&DialogueMenuNextFrameHook);
			DWORD ignored = 0;
			VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
			return g_originalDialogueMenuNextFrame != nullptr;
		}

	}

	bool Initialize()
	{
		auto* menus = MenuManager::GetSingleton();
		if (!menus || !menus->MenuOpenCloseEventDispatcher()) {
			Logger::Write("Menu manager unavailable; camera initialization failed.");
			return false;
		}
		menus->MenuOpenCloseEventDispatcher()->AddEventSink(&g_menuListener);
		const bool installed = InstallDialogueMenuFrameHook();
		Logger::Write(installed ? "Dialogue menu frame hook installed." : "Dialogue menu frame hook installation failed.");
		return installed;
	}
}
