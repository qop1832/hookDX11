#include "GInterface.h"
#include <string>
#include <iostream>

inline static bool SafeToExit = false;


void Empty() { 
	OutputDebugStringA("WOW.�ϵ�������...\n");
}


LONG NTAPI GInterface::VectoredHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
	if (ExceptionInfo->ExceptionRecord->ExceptionCode != 0x80000003 && ExceptionInfo->ExceptionRecord->ExceptionCode != 0xC0000094)
	{
		// MessageBoxA(nullptr, "������, ����!", std::to_string(ExceptionInfo->ExceptionRecord->ExceptionCode).c_str(), MB_OK);
		OutputDebugStringA("WOW.1�����ϵ�...\n");
		EXCEPTION_CONTINUE_SEARCH;
	}
	else
	{
		if (ExceptionInfo->ExceptionRecord->ExceptionCode == 0xC0000094)
		{
			OutputDebugStringA("WOW.2�����ϵ�...\n");
		//	ExceptionInfo->ContextRecord->Rip = (uint64_t)LuaCall;
			ExceptionInfo->ContextRecord->Rip = (uintptr_t)Empty;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

void GInterface::WaitForReload()
{
	//if (Globals::Registered && !WoWObjectManager::InGame())
	//{
	//	Globals::Registered = false;
	//}

	Sleep(100);

	//if (!Globals::Registered && WoWObjectManager::InGame())
	//{
	//	Globals::Registered = true;
	//	Sleep(1000);
	//}
}

void GInterface::Init(HMODULE hModule)
{
	// Console
	FILE* pFile = nullptr;
	AllocConsole();
	freopen_s(&pFile, "CONOUT$", "w", stdout);

	SetUnhandledExceptionFilter(VectoredHandler);
        // Removed AntiCrashHandler

	std::thread::thread(Monitor, hModule).detach();
}


void GInterface::Monitor(HMODULE hModule)
{
	while (!SafeToExit)
		WaitForReload();

	Sleep(100);

  // Removed AntiCrashHandler
	FreeConsole();

	//menu's
	//Settings::bot::fishing::Enabled = false;
	//Settings::bot::Grinding::Enabled = false;
	//Settings::Drawing::Enabled = false;
	//Settings::Drawing::Radar::Enabled = false;
	//Settings::Drawing::EntityViewer::Enabled = false;
	//Settings::UI::Windows::Menu::GetInvItems = false;

	////GameMethods::Cooldowns.clear();
	//Globals::LocalPlayer = nullptr;
	//Globals::Objects.clear();
	//delete Globals::Nav;

	FreeLibrary(hModule);
}


