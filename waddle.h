#ifndef WADDLE_H_
#define WADDLE_H_ 1

#ifdef _WIN32
#define WDL_OS_WINDOWS
#endif // _WIN32

#ifdef __linux__
#define WDL_OS_LINUX
#endif //__linux__

#ifdef __emscripten__
#define WDL_OS_EMSCRIPTEN
#endif // __emscripten__

#ifdef __unix__
#define WDL_UNIX
#endif // __unix__

#ifdef WDL_UNIX
#include <unistd.h>
#ifdef _POSIX_VERSION
#define WDL_POSIX
#endif // _POSIX_VERSION
#endif // WDL_UNIX

#if defined(WDL_SHARED) && defined(WDL_OS_WINDOWS)
    #if defined(WADDLE_IMPLEMENTATION)
        #define WDLAPI __declspec(dllexport)
    #else
        #define WDLAPI __declspec(dllimport)
    #endif
#elif defined(WDL_SHADERD) && defined(WDL_OS_LINUX) && defined(WADDLE_IMPLEMENTATION)
    #define WDLAPI __attribute__((visibility("default")))
#else
    #define WDLAPI extern
#endif

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef u8 b8;
typedef u32 b32;

#ifndef true
#define true 1
#endif // true

#ifndef false
#define false 0
#endif // false

#ifndef NULL
#define NULL ((void*) 0)
#endif // NULL

WDLAPI b8 wdl_init(void);
WDLAPI b8 wdl_terminate(void);

// -- OS -----------------------------------------------------------------------

WDLAPI void* wdl_os_reserve_memory(u32 size);
WDLAPI void  wdl_os_commit_memory(void* ptr, u32 size);
WDLAPI void  wdl_os_release_memory(void* ptr, u32 size);
WDLAPI f32   wdl_os_get_time(void);
WDLAPI u32   wdl_os_get_page_size(void);

#endif // WADDLE_H_

//
//  ___                 _                           _        _   _
// |_ _|_ __ ___  _ __ | | ___ _ __ ___   ___ _ __ | |_ __ _| |_(_) ___  _ __
//  | || '_ ` _ \| '_ \| |/ _ \ '_ ` _ \ / _ \ '_ \| __/ _` | __| |/ _ \| '_ \
//  | || | | | | | |_) | |  __/ | | | | |  __/ | | | || (_| | |_| | (_) | | | |
// |___|_| |_| |_| .__/|_|\___|_| |_| |_|\___|_| |_|\__\__,_|\__|_|\___/|_| |_|
//               |_|
// :implementation
#ifdef WADDLE_IMPLEMENTATION

typedef struct _wdl_platform_state_t _wdl_platform_state_t;
static b8 _wdl_platform_init(void);
static b8 _wdl_platform_termiante(void);

typedef struct _wdl_state_t _wdl_state_t;
struct _wdl_state_t {
    _wdl_platform_state_t *platform;
};

static _wdl_state_t _wdl_state = {0};

b8 wdl_init(void) {
    if (!_wdl_platform_init()) {
        return false;
    }
    return true;
}

b8 wdl_terminate(void) {
    if (!_wdl_platform_termiante()) {
        return false;
    }
    return true;
}

// -- OS -----------------------------------------------------------------------
// Platform specific implementation
// :platform

#ifdef WDL_POSIX

#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <stdio.h>

struct _wdl_platform_state_t {
    u32 page_size;
    f32 start_time;
};

static f32 _wdl_posix_time_stamp(void)  {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (f32) tp.tv_sec + (f32) tp.tv_nsec / 1e9;
}

b8 _wdl_platform_init(void) {
    _wdl_platform_state_t* platform = wdl_os_reserve_memory(sizeof(_wdl_platform_state_t));
    if (platform == NULL) {
        return false;
    }
    wdl_os_commit_memory(platform, sizeof(_wdl_platform_state_t));

    *platform = (_wdl_platform_state_t) {
        .page_size = getpagesize(),
        .start_time = _wdl_posix_time_stamp(),
    };
    _wdl_state.platform = platform;

    return true;
}

b8 _wdl_platform_termiante(void) {
    wdl_os_release_memory(_wdl_state.platform, sizeof(_wdl_platform_state_t));
    return true;
}

void* wdl_os_reserve_memory(u32 size) {
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr;
}

void  wdl_os_commit_memory(void* ptr, u32 size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void  wdl_os_release_memory(void* ptr, u32 size) {
    munmap(ptr, size);
}

f32 wdl_os_get_time(void) {
    return _wdl_posix_time_stamp() - _wdl_state.platform->start_time;
}

u32 wdl_os_get_page_size(void) {
    return _wdl_state.platform->page_size;
}

#endif // WDL_POSIX

#endif // WADDLE_IMPLEMENTATION
