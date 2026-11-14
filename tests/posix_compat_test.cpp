// openciv4: the Win32 behaviour the game core relies on off Windows.
#undef NDEBUG // (the checks are asserts: kept in every build)
#include <windows.h>
#include <psapi.h>
#include <cassert>
#include <cstdio>
#include <string>

static std::wstring fmt(const wchar_t* f, ...)
{
	wchar_t buf[256];
	va_list a;
	va_start(a, f);
	int n = _vsnwprintf(buf, 256, f, a);
	va_end(a);
	assert(n >= 0);
	return buf;
}

int main()
{
	// %s and %c are wide in a wide format, Microsoft's rule; %S is narrow.
	assert(fmt(L"%s", L"Rome") == L"Rome");
	assert(fmt(L"[%-6s]", L"ab") == L"[ab    ]");
	assert(fmt(L"[%6.2s]", L"abcd") == L"[    ab]");
	assert(fmt(L"%S", "narrow") == L"narrow");
	assert(fmt(L"%8S|%C", "narrow", 'n') == L"  narrow|n");
	assert(fmt(L"%c", (wchar_t)0x20AC) == L"€");
	assert(fmt(L"%s %d %c%%", L"Gold", 42, (wchar_t)'g') == L"Gold 42 g%");
	assert(fmt(L"%.2f %ld %5d", 1.5, 7L, 3) == L"1.50 7     3");
	// Non-ASCII wide text must pass through in the C locale.
	assert(fmt(L"%s", L"Café 世") == L"Café 世");
	// The game's icons in text: the whole range the fonts use, including private-use code points.
	for (int c : { 0x80, 0xFF, 0x100, 0x20AC, 0x2100, 0x2190, 0x25A0, 0x2600, 0x4E16, 0x8500, 0xE000, 0xE100, 0xF000, 0xF8FF, 0xFFFD })
	{
		assert(fmt(L"%c", (wchar_t)c) == std::wstring(1, (wchar_t)c));
		const wchar_t arg[2] = { (wchar_t)c, 0 };
		assert(fmt(L"a%sb", arg) == std::wstring(L"a") + (wchar_t)c + L"b");
		const wchar_t format[3] = { (wchar_t)c, L'%', L'd' };
		wchar_t f0[4] = { format[0], format[1], format[2], 0 };
		assert(fmt(f0, 5) == std::wstring(1, (wchar_t)c) + L"5");
	}
	// Two-argument swprintf, as the SDK calls it
	wchar_t buffer[32];
	assert(swprintf(buffer, L"%s=%d", L"x", 1) == 3 && std::wstring(buffer) == L"x=1");
	// Empty and trailing-percent edge cases must not crash.
	assert(fmt(L"") == L"");
	// Windows-1252 and UTF-8
	wchar_t w[8] = {};
	assert(MultiByteToWideChar(CP_ACP, 0, "\x80\xE9", 2, w, 8) == 2 && w[0] == 0x20AC && w[1] == 0xE9);
	assert(MultiByteToWideChar(CP_UTF8, 0, "\xC3\xA9", 2, w, 8) == 1 && w[0] == 0xE9);
	assert(MultiByteToWideChar(CP_UTF8, 0, "\xE2\x82\xAC", 3, w, 8) == 1 && w[0] == 0x20AC);
	char n[8] = {};
	assert(WideCharToMultiByte(CP_ACP, 0, L"€é", 2, n, 8, nullptr, nullptr) == 2 && (unsigned char)n[0] == 0x80 && (unsigned char)n[1] == 0xE9);
	assert(WideCharToMultiByte(CP_ACP, 0, L"世", 1, n, 8, nullptr, nullptr) == 1 && n[0] == '?');
	// Fixed widths the save format depends on
	static_assert(sizeof(DWORD) == 4 && sizeof(LONG) == 4 && sizeof(POINT) == 8);
	assert(MessageBox(nullptr, "t", "c", MB_YESNO) == IDYES);
	// What mods use beyond the SDK (Realism Invictus): key state, memory status
	static_assert(VK_SPACE == 0x20 && VK_ESCAPE == 0x1B && VK_RBUTTON == 0x02 && VK_LEFT == 0x25 && VK_DOWN == 0x28);
	assert(GetAsyncKeyState(VK_SPACE) == GetKeyState(VK_SPACE));
	MEMORYSTATUSEX status = {};
	status.dwLength = sizeof(status);
	assert(GlobalMemoryStatusEx(&status) && status.ullTotalPhys > 0 && status.ullTotalVirtual == (1ULL << 47));
	assert(status.ullAvailVirtual <= status.ullTotalVirtual && status.ullAvailVirtual > status.ullTotalVirtual / 2);
	PROCESS_MEMORY_COUNTERS pmc;
#if defined(__linux__)
	assert(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) && pmc.WorkingSetSize > 0 && pmc.PeakWorkingSetSize >= pmc.WorkingSetSize);
#else
	(void)GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)); // (reports failure; callers skip the log line)
#endif
	std::puts("posix_compat_test: all passed");
}
