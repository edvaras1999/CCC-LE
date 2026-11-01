#include "common/IPrefix.h"

#include <string>

#include "CameraDirector.h"

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
		constexpr UInt32 kDialogueMenuVTableAddress = 0x010E4C9C;
		constexpr UInt32 kDialogueMenuNextFrameSlot = 5;
		constexpr float kTargetFov = 55.0f;
		constexpr float kAimHeight = 82.0f;
		constexpr float kBlendRate = 7.0f;
		constexpr float kPi = 3.14159265358979323846f;

		bool g_dialogueOpen = false;
		bool g_restoreCamera = false;
		bool g_haveSnapshot = false;
		float g_savedWorldFov = 65.0f;
		float g_savedFirstPersonFov = 65.0f;
		float g_savedRotZ = 0.0f;
		float g_savedRotX = 0.0f;
		DWORD g_lastTick = 0;
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

		[[nodiscard]] Actor* DialogueActor()
		{
			auto* topics = *reinterpret_cast<MenuTopicManager**>(kMenuTopicManagerAddress);
			if (!topics) {
				return nullptr;
			}

			UInt32 handle = topics->talkingHandle;
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

		[[nodiscard]] PlayerCharacter* Player()
		{
			auto** player = reinterpret_cast<PlayerCharacter**>(kPlayerPointerAddress);
			return player ? *player : nullptr;
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
				} else {
					g_dialogueOpen = false;
					RestoreCameraState();
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
					state != PlayerCamera::kCameraState_ThirdPerson1 &&
					state != PlayerCamera::kCameraState_ThirdPerson2) {
					return;
				}

				auto* target = DialogueActor();
				auto* player = Player();
				if (!target || !player || target == static_cast<Actor*>(player)) {
					return;
				}

				const float dx = target->pos.x - camera->pos.x;
				const float dy = target->pos.y - camera->pos.y;
				const float dz = target->pos.z + kAimHeight - camera->pos.z;
				const float horizontal = std::sqrt(dx * dx + dy * dy);
				if (horizontal < 1.0f) {
					return;
				}

				const float desiredZ = std::atan2(dx, dy);
				const float desiredX = std::atan2(dz, horizontal);
				camera->rotZ = BlendAngle(camera->rotZ, desiredZ, blend);
				camera->rotX = BlendAngle(camera->rotX, desiredX, blend);
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
			if (!*slot || *slot == reinterpret_cast<void*>(&DialogueMenuNextFrameHook)) {
				return false;
			}

			DWORD oldProtection = 0;
			if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtection)) {
				return false;
			}
			g_originalDialogueMenuNextFrame = reinterpret_cast<DialogueMenuNextFrameFn>(*slot);
			*slot = reinterpret_cast<void*>(&DialogueMenuNextFrameHook);
			DWORD ignored = 0;
			VirtualProtect(slot, sizeof(void*), oldProtection, &ignored);
			return g_originalDialogueMenuNextFrame != nullptr;
		}

	}

	bool Initialize(SKSETaskInterface* a_tasks)
	{
		if (!a_tasks) {
			return false;
		}

		auto* menus = MenuManager::GetSingleton();
		if (!menus || !menus->MenuOpenCloseEventDispatcher()) {
			return false;
		}
		menus->MenuOpenCloseEventDispatcher()->AddEventSink(&g_menuListener);
		return InstallDialogueMenuFrameHook();
	}
}

