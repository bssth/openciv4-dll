// openciv4: the Win32 functions of windows.h that need a body, for non-Windows builds of the game core.
#include "windows.h"

#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
#include <string>

short (*oc4_GetKeyState)(int) = nullptr;

namespace
{
	// The bytes 0x80-0x9F of Windows-1252; the rest match Unicode.
	const char16_t kCp1252[32] = {
		0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
		0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
	};

	std::u32string decode(UINT codePage, const char* src, int len)
	{
		std::u32string out;
		const size_t n = len < 0 ? std::strlen(src) + 1 : static_cast<size_t>(len);
		for (size_t i = 0; i < n;)
		{
			const unsigned char c = static_cast<unsigned char>(src[i]);
			if (codePage != CP_UTF8 || c < 0x80)
			{
				out.push_back(c >= 0x80 && c < 0xA0 && codePage != CP_UTF8 ? kCp1252[c - 0x80] : c);
				++i;
				continue;
			}
			const int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
			char32_t cp = extra == 3 ? c & 0x07 : extra == 2 ? c & 0x0F : c & 0x1F;
			if (extra == 0 || i + extra >= n)
			{
				out.push_back(0xFFFD);
				++i;
				continue;
			}
			for (int k = 1; k <= extra; ++k)
				cp = (cp << 6) | (static_cast<unsigned char>(src[i + k]) & 0x3F);
			out.push_back(cp);
			i += extra + 1;
		}
		return out;
	}

	void encode(UINT codePage, char32_t c, std::string& out)
	{
		if (codePage == CP_UTF8)
		{
			if (c < 0x80)
				out.push_back(static_cast<char>(c));
			else if (c < 0x800)
			{
				out.push_back(static_cast<char>(0xC0 | (c >> 6)));
				out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
			}
			else if (c < 0x10000)
			{
				out.push_back(static_cast<char>(0xE0 | (c >> 12)));
				out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (c >> 18)));
				out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
			}
			return;
		}
		if (c < 0x80 || (c >= 0xA0 && c < 0x100))
		{
			out.push_back(static_cast<char>(c));
			return;
		}
		for (int i = 0; i < 32; ++i)
			if (kCp1252[i] == c)
			{
				out.push_back(static_cast<char>(0x80 + i));
				return;
			}
		out.push_back('?');
	}
}

int MultiByteToWideChar(UINT codePage, DWORD, const char* src, int srcLen, wchar_t* dst, int dstLen)
{
	const std::u32string text = decode(codePage, src, srcLen);
	if (dstLen == 0)
		return static_cast<int>(text.size());
	const int n = std::min<int>(dstLen, static_cast<int>(text.size()));
	for (int i = 0; i < n; ++i)
		dst[i] = static_cast<wchar_t>(text[i]);
	return n;
}

int WideCharToMultiByte(UINT codePage, DWORD, const wchar_t* src, int srcLen, char* dst, int dstLen, const char*, BOOL*)
{
	const size_t n = srcLen < 0 ? std::wcslen(src) + 1 : static_cast<size_t>(srcLen);
	std::string out;
	for (size_t i = 0; i < n; ++i)
		encode(codePage, static_cast<char32_t>(src[i]), out);
	if (dstLen == 0)
		return static_cast<int>(out.size());
	const int count = std::min<int>(dstLen, static_cast<int>(out.size()));
	std::memcpy(dst, out.data(), count);
	return count;
}

DWORD GetModuleFileName(HMODULE, char* path, DWORD size)
{
	// The path of the game core itself, which is what the SDK asks for (the engine may override this).
	Dl_info info{};
	const char* name = dladdr(reinterpret_cast<const void*>(&GetModuleFileName), &info) && info.dli_fname ? info.dli_fname : "";
	std::strncpy(path, name, size);
	if (size)
		path[size - 1] = 0;
	return static_cast<DWORD>(std::strlen(path));
}

BOOL SetCurrentDirectoryW(const wchar_t* path)
{
	char narrow[4096];
	const int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, sizeof narrow, nullptr, nullptr);
	return n > 0 && n <= int(sizeof narrow) && chdir(narrow) == 0;
}

DWORD GetModuleFileNameW(HMODULE, wchar_t* path, DWORD size)
{
	char narrow[4096];
	uint32_t length = sizeof narrow;
#ifdef __APPLE__
	if (_NSGetExecutablePath(narrow, &length) != 0 || !size)
		return 0;
#else
	const ssize_t got = readlink("/proc/self/exe", narrow, length - 1);
	if (got < 0 || !size)
		return 0;
	narrow[got] = 0;
#endif
	const int n = MultiByteToWideChar(CP_UTF8, 0, narrow, -1, path, int(size));
	path[size - 1] = 0;
	return n > 0 ? DWORD(n - 1) : 0;
}

int oc4_vswprintf(wchar_t* out, size_t count, const wchar_t* format, va_list args)
{
	std::wstring posix;
	for (const wchar_t* p = format; *p; ++p)
	{
		posix.push_back(*p);
		if (*p != L'%')
			continue;
		++p;
		if (*p == L'%')
		{
			posix.push_back(*p);
			continue;
		}
		while (*p && std::wcschr(L"-+ #0123456789.*", *p))
			posix.push_back(*p++);
		const bool hasLength = *p == L'l' || *p == L'h' || *p == L'L' || *p == L'z' || *p == L'j' || *p == L't';
		if (*p == L's' || *p == L'c')
			posix.push_back(L'l'); // wide argument, Microsoft's default in a wide format
		else if (*p == L'S' || *p == L'C')
		{
			posix.push_back(*p == L'S' ? L's' : L'c'); // narrow argument, the C standard's default in a wide format
			continue;
		}
		else if (hasLength)
		{
			while (*p == L'l' || *p == L'h' || *p == L'L' || *p == L'z' || *p == L'j' || *p == L't')
				posix.push_back(*p++);
		}
		if (!*p)
			break;
		posix.push_back(*p);
	}
	va_list copy;
	va_copy(copy, args);
#ifdef __APPLE__
	// macOS refuses characters above U+00FF in the C locale, which is what the game's icons in text are.
	// A locale of our own, so that the process's stays as it is.
	static const locale_t kWide = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
	const int result = vswprintf_l(out, count, kWide, posix.c_str(), copy);
#else
	const int result = vswprintf(out, count, posix.c_str(), copy);
#endif
	va_end(copy);
	return result;
}

BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX* status)
{
	if (status == nullptr)
		return FALSE;
	const DWORDLONG page = static_cast<DWORDLONG>(sysconf(_SC_PAGESIZE));
	const DWORDLONG total = page * static_cast<DWORDLONG>(sysconf(_SC_PHYS_PAGES));
#ifdef _SC_AVPHYS_PAGES
	const DWORDLONG avail = page * static_cast<DWORDLONG>(sysconf(_SC_AVPHYS_PAGES));
#else
	const DWORDLONG avail = total / 2;
#endif
	DWORDLONG used = 0; // the process's address space in use (VmSize)
#if defined(__linux__)
	if (std::FILE* f = std::fopen("/proc/self/status", "r"))
	{
		char line[256];
		unsigned long kb = 0;
		while (std::fgets(line, sizeof(line), f))
			if (std::sscanf(line, "VmSize: %lu kB", &kb) == 1)
				used = static_cast<DWORDLONG>(kb) * 1024;
		std::fclose(f);
	}
#endif
	status->dwMemoryLoad = total ? static_cast<DWORD>(100 - avail * 100 / total) : 0;
	status->ullTotalPhys = total;
	status->ullAvailPhys = avail;
	status->ullTotalPageFile = total;
	status->ullAvailPageFile = avail;
	status->ullTotalVirtual = 1ULL << 47;
	status->ullAvailVirtual = status->ullTotalVirtual - used;
	status->ullAvailExtendedVirtual = 0;
	return TRUE;
}
