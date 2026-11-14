// openciv4: the small part of the Win32 API the SDK uses, for non-Windows builds of the game core.
#pragma once

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <dlfcn.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#include <mach-o/dyld.h>
#else
#include <malloc.h>
#endif
#include <unistd.h>
#include <strings.h>
#include <time.h>
#include <wctype.h>

using std::max;
using std::min;

// Fixed widths: Windows' LONG and DWORD are 32-bit, while `long` is 64-bit here.
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef int32_t BOOL;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef uint32_t UINT;
typedef int16_t SHORT;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* LPVOID;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t* LPWSTR;
typedef wchar_t WCHAR;
typedef char TCHAR;
typedef const char* LPCTSTR;
typedef UINT MMRESULT;
typedef uint64_t DWORD64;
typedef uint64_t ULONGLONG;
typedef int32_t HRESULT;
typedef wchar_t* PWSTR;
typedef void* HKEY;
typedef uint32_t LSTATUS;
typedef char* LPTSTR;

struct POINT
{
	LONG x;
	LONG y;
};

#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define __forceinline inline __attribute__((always_inline))
#define __cdecl
#define __stdcall
#define WINAPI
#define APIENTRY
#define CP_ACP 0
#define CP_UTF8 65001
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3
#define DLL_PROCESS_DETACH 0

#define MB_OK 0x0
#define MB_OKCANCEL 0x1
#define MB_ABORTRETRYIGNORE 0x2
#define MB_YESNOCANCEL 0x3
#define MB_YESNO 0x4
#define MB_ICONERROR 0x10
#define MB_ICONHAND 0x10
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONQUESTION 0x20
#define MB_ICONINFORMATION 0x40
#define MB_TASKMODAL 0x2000
#define MB_SETFOREGROUND 0x10000
#define MB_TOPMOST 0x40000
#define TIMERR_NOERROR 0
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define RRF_RT_REG_SZ 2
#define HKEY_CURRENT_USER nullptr
#define HKEY_LOCAL_MACHINE nullptr
#define FAILED(hr) ((hr) < 0)
#define SUCCEEDED(hr) ((hr) >= 0)
#define _MCW_DN 0
#define _MCW_EM 0
#define _MCW_RC 0
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7

#define stricmp strcasecmp
#define strcmpi strcasecmp
#define _stricmp strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define wcsicmp wcscasecmp
#define _wcsicmp wcscasecmp
#define _vsnprintf vsnprintf
#define _snprintf snprintf
#define _strdup strdup

// MessageBox answers with the default button, as the engine's own shim does on Windows.
inline int oc4MessageBox(const char* text, const char* caption, unsigned int type)
{
	std::fprintf(stderr, "[game core] %s: %s\n", caption ? caption : "", text ? text : "");
	switch (type & 0xF)
	{
	case MB_ABORTRETRYIGNORE: return IDIGNORE;
	case MB_YESNOCANCEL:
	case MB_YESNO: return IDYES;
	default: return IDOK;
	}
}
// A function, not Win32's macro: CvDLLUtilityIFaceBase has a method of this name.
inline int MessageBox(HWND, const char* text, const char* caption, unsigned int type) { return oc4MessageBox(text, caption, type); }
inline int MessageBoxA(HWND, const char* text, const char* caption, unsigned int type) { return oc4MessageBox(text, caption, type); }

inline void OutputDebugString(const char* s) { std::fputs(s, stderr); }
#define OutputDebugStringA OutputDebugString

inline DWORD timeGetTime()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<DWORD>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

inline MMRESULT timeBeginPeriod(UINT) { return TIMERR_NOERROR; }
inline MMRESULT timeEndPeriod(UINT) { return TIMERR_NOERROR; }
inline double _wtof(const wchar_t* s) { return wcstod(s, nullptr); }

struct SYSTEMTIME
{
	WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};
inline void GetLocalTime(SYSTEMTIME* out)
{
	const time_t now = time(nullptr);
	tm t{};
	localtime_r(&now, &t);
	*out = { WORD(t.tm_year + 1900), WORD(t.tm_mon + 1), WORD(t.tm_wday), WORD(t.tm_mday), WORD(t.tm_hour), WORD(t.tm_min), WORD(t.tm_sec), 0 };
}

inline void Sleep(DWORD ms) { usleep(ms * 1000); }
inline DWORD GetEnvironmentVariableA(const char* name, char* out, DWORD size)
{
	const char* value = std::getenv(name);
	if (!value || !size)
		return 0;
	std::strncpy(out, value, size);
	out[size - 1] = 0;
	return DWORD(std::strlen(out));
}
inline int _putenv_s(const char* name, const char* value) { return setenv(name, value, 1); }
#ifdef __APPLE__
inline size_t _msize(void* p) { return malloc_size(p); }
#else
inline size_t _msize(void* p) { return malloc_usable_size(p); }
#endif
// The FPU state is the same on every thread here: nothing to carry across.
inline int _controlfp_s(unsigned int* current, unsigned int, unsigned int) { if (current) *current = 0; return 0; }
inline HMODULE GetModuleHandleA(const char*) { return nullptr; }
inline void* GetProcAddress(HMODULE, const char* name) { return dlsym(RTLD_DEFAULT, name); }
// No registry here: every lookup misses, and the install comes from the command line or OC4_CIV4_DIR.
inline LSTATUS RegGetValueW(HKEY, const wchar_t*, const wchar_t*, DWORD, DWORD*, void*, DWORD*) { return ERROR_FILE_NOT_FOUND; }
BOOL SetCurrentDirectoryW(const wchar_t* path);
DWORD GetModuleFileNameW(HMODULE module, wchar_t* path, DWORD size);

// Keyboard state comes from the engine, which owns the window; it sets this when it starts.
extern short (*oc4_GetKeyState)(int);
inline short GetKeyState(int key) { return oc4_GetKeyState ? oc4_GetKeyState(key) : 0; }
// GlobalMemoryStatusEx as on 64-bit Windows: user address space 128 TB (mods compare used address
// space against it, a 32-bit concern that never triggers at x64), physical memory from the system.
typedef unsigned long long DWORDLONG;
typedef struct _MEMORYSTATUSEX
{
	DWORD dwLength;
	DWORD dwMemoryLoad;
	DWORDLONG ullTotalPhys;
	DWORDLONG ullAvailPhys;
	DWORDLONG ullTotalPageFile;
	DWORDLONG ullAvailPageFile;
	DWORDLONG ullTotalVirtual;
	DWORDLONG ullAvailVirtual;
	DWORDLONG ullAvailExtendedVirtual;
} MEMORYSTATUSEX;
BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX* status);

// The engine reports the current state either way, so the asynchronous query is the same call.
inline short GetAsyncKeyState(int key) { return GetKeyState(key); }

// The core only converts narrow text the mods wrote in Windows-1252 or UTF-8.
int MultiByteToWideChar(UINT codePage, DWORD flags, const char* src, int srcLen, wchar_t* dst, int dstLen);
int WideCharToMultiByte(UINT codePage, DWORD flags, const wchar_t* src, int srcLen, char* dst, int dstLen, const char* defaultChar, BOOL* usedDefault);
DWORD GetModuleFileName(HMODULE module, char* path, DWORD size);
#define GetModuleFileNameA GetModuleFileName

inline DWORD GetEnvironmentVariableW(const wchar_t* name, wchar_t* out, DWORD size)
{
	char narrow[256];
	const int length = WideCharToMultiByte(CP_UTF8, 0, name, -1, narrow, sizeof narrow, nullptr, nullptr);
	if (length <= 0 || length >= int(sizeof narrow))
		return 0;
	narrow[length] = 0;
	const char* value = std::getenv(narrow);
	if (!value || !size)
		return 0;
	const int written = MultiByteToWideChar(CP_UTF8, 0, value, -1, out, int(size));
	out[size - 1] = 0;
	return written > 0 ? DWORD(written - 1) : 0;
}

// Wide printf. Microsoft's %s and %c take wide arguments in a wide format, POSIX's take narrow ones, so
// these translate the format (literals and text from the game's XML alike) to POSIX before formatting.
int oc4_vswprintf(wchar_t* out, size_t count, const wchar_t* format, va_list args);
inline int _vsnwprintf(wchar_t* out, size_t count, const wchar_t* format, va_list args) { return oc4_vswprintf(out, count, format, args); }
inline int oc4_swprintf(wchar_t* out, size_t count, const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = oc4_vswprintf(out, count, format, args);
	va_end(args);
	return result;
}
// Win32's two-argument swprintf, as unbounded as the original.
template <size_t N>
inline int swprintf(wchar_t (&out)[N], const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = oc4_vswprintf(out, N, format, args);
	va_end(args);
	return result;
}
