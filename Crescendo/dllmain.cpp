// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "detours.h"
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include "crc32.h"
#include <iomanip>
#pragma comment(lib, "detours.lib")
using namespace std;

map<DWORD, wstring> REPList;

PVOID g_pOldMultiByteToWideChar = NULL;
typedef int(WINAPI* PfuncMultiByteToWideChar)(
	_In_      UINT   CodePage,
	_In_      DWORD  dwFlags,
	_In_      LPCSTR lpMultiByteStr,
	_In_      int    cbMultiByte,
	_Out_opt_ LPWSTR lpWideCharStr,
	_In_      int    cchWideChar);

void memcopy(void* dest, void*src, size_t size)
{
	DWORD oldProtect;
	VirtualProtect(dest, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(dest, src, size);
}

char* wtocUTF(LPCTSTR str)
{
	DWORD dwMinSize;
	dwMinSize = WideCharToMultiByte(CP_UTF8, NULL, str, -1, NULL, 0, NULL, FALSE); //计算长度
	char* out = new char[dwMinSize];
	WideCharToMultiByte(CP_UTF8, NULL, str, -1, out, dwMinSize, NULL, FALSE);//转换
	return out;
}

char* wtocGBK(LPCTSTR str)
{
	DWORD dwMinSize;
	dwMinSize = WideCharToMultiByte(936, NULL, str, -1, NULL, 0, NULL, FALSE); //计算长度
	char* out = new char[dwMinSize];
	WideCharToMultiByte(936, NULL, str, -1, out, dwMinSize, NULL, FALSE);//转换
	return out;
}

LPWSTR ctowUTF(char* str)
{
	DWORD dwMinSize;
	dwMinSize = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0); //计算长度
	LPWSTR out = new wchar_t[dwMinSize];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, out, dwMinSize);//转换
	return out;
}

int WINAPI NewMultiByteToWideChar(UINT cp, DWORD dwFg, LPCSTR lpMBS, int cbMB, LPWSTR lpWCS, int ccWC)
{
	__asm
	{
		pushad
	}
	CRC32 crc;
	int ret = 0;
	ret = ((PfuncMultiByteToWideChar)g_pOldMultiByteToWideChar)(cp, dwFg, lpMBS, cbMB, lpWCS, ccWC);
	if (lpWCS != 0 && (USHORT)* lpWCS > 0x20)//检测所有的有效字符
	{
		wstring wstr = lpWCS;
		DWORD strcrc = crc.Calc((char*)lpWCS, ret);
		auto scitr = REPList.find(strcrc);
		if (scitr != REPList.end())
		{
			wcscpy(lpWCS, scitr->second.c_str());
			ret = scitr->second.length();
		}
	}
	__asm
	{
		popad
	}
	return ret;
}

PVOID g_pOldCreateFontIndirectA = NULL;
typedef int (WINAPI *PfuncCreateFontIndirectA)(LOGFONTA *lplf);
int WINAPI NewCreateFontIndirectA(LOGFONTA *lplf)
{
	if (lplf->lfCharSet == 0x80)
	{
		lplf->lfCharSet = ANSI_CHARSET;//SYSTEM FONT
	}
	return ((PfuncCreateFontIndirectA)g_pOldCreateFontIndirectA)(lplf);
}

void LoadStringMap()
{
	ifstream fin("Crescendo.ini");
	const int LineMax = 0x99999;//其实用不到这么大2333
	char str[LineMax];
	if (fin.is_open())
	{
		while (fin.getline(str, LineMax))
		{
			auto wtmp = ctowUTF(str);
			wstring wline = wtmp;
			wstring crcval = wline.substr(2, 8);
			wstring wstr = wline.substr(11);
			DWORD crc = wcstoul(crcval.c_str(), NULL, 16);
			REPList.insert(pair<DWORD, wstring>(crc, wstr));
		}
	}
	else
	{
		MessageBox(0, L"配置文件读取失败", L"ERROR", MB_OK);
	}
}

void MyCharSet()
{
	BYTE Patch1[] = { 0xC6 };
	BYTE Patch2[] = { 0x45 };
	BYTE Patch3[] = { 0xD7 };
	BYTE Patch4[] = { 0x86 };
	BYTE Patch5[] = { 0x90 };
	BYTE Patch6[] = { 0x90 };

	BYTE Patch7[] = { 0x86 };

	memcopy((void*)0x49BAA1, Patch1, sizeof(Patch1));
	memcopy((void*)0x49BAA2, Patch2, sizeof(Patch2));
	memcopy((void*)0x49BAA3, Patch3, sizeof(Patch3));
	memcopy((void*)0x49BAA4, Patch4, sizeof(Patch4));
	memcopy((void*)0x49BAA5, Patch5, sizeof(Patch5));
	memcopy((void*)0x49BAA6, Patch6, sizeof(Patch6));

	memcopy((void*)0x51235A, Patch7, sizeof(Patch7));

}

void HookStart()
{
	g_pOldMultiByteToWideChar = DetourFindFunction("Kernel32.dll", "MultiByteToWideChar");
	DetourTransactionBegin();
	DetourAttach(&g_pOldMultiByteToWideChar, NewMultiByteToWideChar);
	DetourTransactionCommit();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	g_pOldCreateFontIndirectA = DetourFindFunction("GDI32.dll", "CreateFontIndirectA");
	DetourAttach(&g_pOldCreateFontIndirectA, NewCreateFontIndirectA);
	DetourTransactionCommit();

	MyCharSet();
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		LoadStringMap();
		HookStart();
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void dummy(void) {
	return;
}