// POSIX stand-in for <psapi.h>: the process's own memory use, which mods log
// (Realism Invictus: DumpMemUsage, CMemoryTrace). Linux reads /proc/self/status;
// elsewhere GetProcessMemoryInfo reports failure and callers skip the log line.
#pragma once

#include "windows.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

typedef struct _PROCESS_MEMORY_COUNTERS
{
	DWORD cb;
	DWORD PageFaultCount;
	size_t PeakWorkingSetSize;
	size_t WorkingSetSize;
	size_t QuotaPeakPagedPoolUsage;
	size_t QuotaPagedPoolUsage;
	size_t QuotaPeakNonPagedPoolUsage;
	size_t QuotaNonPagedPoolUsage;
	size_t PagefileUsage;
	size_t PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;

inline HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(-1); }

// Working set = resident memory (VmRSS, peak VmHWM); pagefile usage = private data (VmData, peak VmPeak).
inline BOOL GetProcessMemoryInfo(HANDLE, PROCESS_MEMORY_COUNTERS* pmc, DWORD cb)
{
	if (pmc == nullptr || cb < sizeof(PROCESS_MEMORY_COUNTERS))
		return FALSE;
	std::memset(pmc, 0, sizeof(*pmc));
	pmc->cb = sizeof(*pmc);
#if defined(__linux__)
	std::FILE* f = std::fopen("/proc/self/status", "r");
	if (f == nullptr)
		return FALSE;
	char line[256];
	unsigned long kb = 0;
	while (std::fgets(line, sizeof(line), f))
	{
		if (std::sscanf(line, "VmRSS: %lu kB", &kb) == 1)
			pmc->WorkingSetSize = kb * 1024;
		else if (std::sscanf(line, "VmHWM: %lu kB", &kb) == 1)
			pmc->PeakWorkingSetSize = kb * 1024;
		else if (std::sscanf(line, "VmData: %lu kB", &kb) == 1)
			pmc->PagefileUsage = kb * 1024;
		else if (std::sscanf(line, "VmPeak: %lu kB", &kb) == 1)
			pmc->PeakPagefileUsage = kb * 1024;
	}
	std::fclose(f);
	return TRUE;
#else
	return FALSE;
#endif
}
