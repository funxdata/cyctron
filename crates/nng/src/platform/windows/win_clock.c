//
// Copyright 2024 Garrett D'Amore <garrett@damore.org>
// Copyright 2017 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "core/nng_impl.h"

#include <time.h>
#include <windows.h>

#ifdef NNG_PLATFORM_WINDOWS

// ============================================================
// MinGW 兼容性：提供 timespec_get 实现
// ============================================================
#ifdef __MINGW32__
#ifndef TIME_UTC
#define TIME_UTC 1
#endif

static inline int nng_timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    
    FILETIME ft;
    ULARGE_INTEGER uli;
    
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // Windows FILETIME 从 1601-01-01 开始，转换为 Unix 时间（1970-01-01）
    // 偏移量：116444736000000000 个 100ns 单位
    uli.QuadPart -= 116444736000000000ULL;
    
    ts->tv_sec = uli.QuadPart / 10000000ULL;
    ts->tv_nsec = (uli.QuadPart % 10000000ULL) * 100;
    
    return TIME_UTC;
}

// 新版 mingw-w64 已在 <time.h> 中声明 timespec_get，直接定义会类型冲突；
// 统一重定向到内部实现，既兼容新头文件，也保证 Windows 7 上仍有实现。
#define timespec_get(ts, base) nng_timespec_get((ts), (base))
#endif // __MINGW32__

nni_time
nni_clock(void)
{
	// We are limited by the system clock, but that is ok.
	return (GetTickCount64());
}

int
nni_time_get(uint64_t *seconds, uint32_t *nanoseconds)
{
	struct timespec ts;
	if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
		*seconds     = ts.tv_sec;
		*nanoseconds = ts.tv_nsec;
		return (0);
	}
	return (nni_win_error(GetLastError()));
}

void
nni_msleep(nni_duration dur)
{
	uint64_t exp;

	exp = (uint64_t) GetTickCount64() + dur;

	// Sleep() would be our preferred API, if it didn't have a nasty
	// feature where it rounds *down*.  We always want to sleep *at
	// least* the requested amount of time, and never ever less.
	// If we wind up sleeping less, then we will sleep(1) in the hope
	// of waiting until the next clock tick.

	Sleep((DWORD) dur);
	while ((uint64_t) GetTickCount64() < exp) {
		Sleep(1);
	}
}

#endif // NNG_PLATFORM_WINDOWS