//kris. b
// Loader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define EXPORT extern "C" __declspec(dllexport)
#include <iostream>
#include <Windows.h>
#include "Detours.h"

EXPORT void Test()
{
	printf("test 123");
}

std::wstring CharToWString(const char* text)
{
	const size_t size = strlen(text) + 1;
	wchar_t* wText = new wchar_t[size];
	mbstowcs(wText, text, size);
	std::wstring ret(wText);
	delete[] wText;
	return ret;
}

int main(int argc, char* argv[])
{
    std::wcout << "Hello World!\n";

	std::wstring argument;
	for (int i = 1; i < argc; i++)
	{
		argument += CharToWString(argv[i]) + std::wstring(L" ");
	}
	std::wcout << L"argument passed: " << argument << L"\n\n";

	argument = L"Warhammer2.exe " + argument;

	std::wcout << L"argument that will be passed to the game: " << argument << L"\n\n";

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	if (!DetourCreateProcessWithDllEx(L"Warhammer2.exe",
		(LPWSTR)argument.data(), NULL, NULL, TRUE,
		CREATE_DEFAULT_ERROR_MODE,// | CREATE_SUSPENDED,
		NULL, NULL, &si, &pi,
		"minhook_test.dll", NULL))
	{
		std::wcout << L"failed to inject\n";
		MessageBox(0, L"failed", 0, 0);
	}
	else
	{
		std::wcout << L"inject ok\n";
	}

	ResumeThread(pi.hThread);

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(&si);
	CloseHandle(&pi);

	return EXIT_SUCCESS;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
