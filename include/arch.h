#pragma once

// 平台宏定义
#define MG_ARCH_UNIX 1     // Linux, BSD, macOS
#define MG_ARCH_WIN32 2    // Windows

// 自动检测平台
#if !defined(MG_ARCH)
  #if defined(__unix__) || defined(__APPLE__)
    #define MG_ARCH MG_ARCH_UNIX
  #elif defined(_WIN32)
    #define MG_ARCH MG_ARCH_WIN32
  #else
    #error "Unsupported platform. Please define MG_ARCH=MG_ARCH_UNIX or MG_ARCH_WIN32"
  #endif
#endif

// 大端检测
#define MG_BIG_ENDIAN (*(uint16_t *) "\0\xff" < 0x100)

// 平台相关头文件
#if MG_ARCH == MG_ARCH_UNIX
  #include "arch_unix.h"
#elif MG_ARCH == MG_ARCH_WIN32
  #include "arch_win32.h"
#endif