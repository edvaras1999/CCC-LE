#include "Logger.h"

#include <Windows.h>
#include <ShlObj.h>

#include <cstdio>

#pragma comment(lib, "Shell32.lib")

namespace Logger
{
	namespace
	{
		char g_logPath[MAX_PATH]{};
		SRWLOCK g_logLock = SRWLOCK_INIT;
	}

	void Initialize()
	{
		char documents[MAX_PATH]{};
		if (SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, documents) != S_OK) {
			OutputDebugStringA("[CCC LE] Could not resolve the Documents folder for the log.\n");
			return;
		}

		char myGames[MAX_PATH]{};
		char skyrim[MAX_PATH]{};
		char skse[MAX_PATH]{};
		if (sprintf_s(myGames, "%s\\My Games", documents) < 0 ||
			sprintf_s(skyrim, "%s\\Skyrim", myGames) < 0 ||
			sprintf_s(skse, "%s\\SKSE", skyrim) < 0 ||
			sprintf_s(g_logPath, "%s\\CinematicConversationCamera.log", skse) < 0) {
			OutputDebugStringA("[CCC LE] Could not construct the log path.\n");
			g_logPath[0] = '\0';
			return;
		}

		CreateDirectoryA(myGames, nullptr);
		CreateDirectoryA(skyrim, nullptr);
		CreateDirectoryA(skse, nullptr);

		FILE* file = nullptr;
		if (fopen_s(&file, g_logPath, "wb") == 0 && file) {
			fputs("Cinematic Conversation Camera LE log started.\n", file);
			fclose(file);
		} else {
			OutputDebugStringA("[CCC LE] Could not create the log file.\n");
		}
	}

	void Write(const char* a_message)
	{
		if (!a_message) {
			return;
		}

		OutputDebugStringA("[CCC LE] ");
		OutputDebugStringA(a_message);
		OutputDebugStringA("\n");

		if (!g_logPath[0]) {
			return;
		}

		AcquireSRWLockExclusive(&g_logLock);
		FILE* file = nullptr;
		if (fopen_s(&file, g_logPath, "ab") == 0 && file) {
			SYSTEMTIME now{};
			GetLocalTime(&now);
			fprintf(file, "[%04u-%02u-%02u %02u:%02u:%02u] %s\n",
				now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, a_message);
			fclose(file);
		} else {
			OutputDebugStringA("[CCC LE] Could not append to the log file.\n");
		}
		ReleaseSRWLockExclusive(&g_logLock);
	}
}
