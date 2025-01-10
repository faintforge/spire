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

#ifdef WDL_OS_WINDOWS
#define WDL_THREAD_LOCAL __declspec(thread)
#endif // WDL_OS_WINDOWS

#ifdef WDL_POSIX
#define WDL_THREAD_LOCAL __thread
#endif // WDL_POSIX

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      i8;
typedef signed short     i16;
typedef signed int       i32;
typedef signed long long i64;

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

// -----------------------------------------------------------------------------

WDLAPI b8 wdl_init(void);
WDLAPI b8 wdl_terminate(void);

// -- Misc ---------------------------------------------------------------------

#define WDL_KB(V) ((u64) (V) << 10)
#define WDL_MB(V) ((u64) (V) << 20)
#define WDL_GB(V) ((u64) (V) << 30)

#define WDL_MIN(A, B) ((A) > (B) ? (B) : (A))
#define WDL_MAX(A, B) ((A) > (B) ? (A) : (B))
#define WDL_CLAMP(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

#define WDL_ARRLEN(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

// -- Arena --------------------------------------------------------------------

typedef struct WDL_Arena WDL_Arena;

WDLAPI WDL_Arena* wdl_arena_create(void);
WDLAPI WDL_Arena* wdl_arena_create_sized(u64 size);
WDLAPI void         wdl_arena_destroy(WDL_Arena* arena);
WDLAPI void         wdl_arena_set_align(WDL_Arena* arena, u8 align);
WDLAPI void*        wdl_arena_push(WDL_Arena* arena, u64 size);
WDLAPI void*        wdl_arena_push_no_zero(WDL_Arena* arena, u64 size);
WDLAPI void         wdl_arena_pop(WDL_Arena* arena, u64 size);
WDLAPI void         wdl_arena_pop_to(WDL_Arena* arena, u64 pos);
WDLAPI void         wdl_arena_clear(WDL_Arena* arena);
WDLAPI u64          wdl_arena_get_pos(WDL_Arena* arena);

typedef struct WDL_Temp WDL_Temp;
struct WDL_Temp {
    WDL_Arena* arena;
    u64 pos;
};

WDLAPI WDL_Temp wdl_temp_begin(WDL_Arena* arena);
WDLAPI void       wdl_temp_end(WDL_Temp temp);

// -- Thread context -----------------------------------------------------------

typedef struct WDL_ThreadCtx WDL_ThreadCtx;

WDLAPI WDL_ThreadCtx* wdl_thread_ctx_create(void);
WDLAPI void              wdl_thread_ctx_destroy(WDL_ThreadCtx* ctx);
WDLAPI void              wdl_thread_ctx_set(WDL_ThreadCtx* ctx);

// -- Scratch arena ------------------------------------------------------------

typedef WDL_Temp WDL_Scratch;

WDLAPI WDL_Scratch wdl_scratch_begin(WDL_Arena* const* conflicts, u32 count);
WDLAPI void          wdl_scratch_end(WDL_Scratch scratch);

// -- Logging ------------------------------------------------------------------

typedef enum WDL_LogLevel {
    WDL_LOG_LEVEL_FATAL,
    WDL_LOG_LEVEL_ERROR,
    WDL_LOG_LEVEL_WARN,
    WDL_LOG_LEVEL_INFO,
    WDL_LOG_LEVEL_DEBUG,
    WDL_LOG_LEVEL_TRACE,

    WDL_LOG_LEVEL_COUNT,
} WDL_LogLevel;

#define WDL_FATAL(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_FATAL, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define WDL_ERROR(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_ERROR, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define WDL_WARN(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_WARN, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define WDL_INFO(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_INFO, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define WDL_DEBUG(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_DEBUG, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define WDL_TRACE(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_TRACE, __FILE__, __LINE__, MSG, ##__VA_ARGS__)

WDLAPI void _wdl_log_internal(WDL_LogLevel level, const char* file, u32 line, const char* msg, ...);

// -- Library ------------------------------------------------------------------

typedef struct WDL_Lib WDL_Lib;

typedef void (*WDL_LibFunc)(void);

WDLAPI WDL_Lib*    wdl_lib_load(WDL_Arena* arena, const char* filename);
WDLAPI void        wdl_lib_unload(WDL_Lib* lib);
WDLAPI WDL_LibFunc wdl_lib_func(WDL_Lib* lib, const char* func_name);

// -- String -------------------------------------------------------------------

typedef struct WDL_Str WDL_Str;
struct WDL_Str {
    const u8* data;
    u32 len;
};

#define WDL_STR_LIT(STR_LIT) wdl_str((STR_LIT), sizeof(STR_LIT) - 1)
#define WDL_CSTR(CSTR) wdl_str((CSTR), wdl_str_cstrlen(CSTR))

WDLAPI WDL_Str wdl_str(const u8* data, u32 len);
WDLAPI u32     wdl_str_cstrlen(const u8* cstr);
WDLAPI char*   wdl_str_to_cstr(WDL_Arena* arena, WDL_Str str);

// -- OS -----------------------------------------------------------------------

WDLAPI void* wdl_os_reserve_memory(u32 size);
WDLAPI void  wdl_os_commit_memory(void* ptr, u32 size);
WDLAPI void  wdl_os_release_memory(void* ptr, u32 size);
WDLAPI f32   wdl_os_get_time(void);
WDLAPI u32   wdl_os_get_page_size(void);

/*
 *  ___                 _                           _        _   _
 * |_ _|_ __ ___  _ __ | | ___ _ __ ___   ___ _ __ | |_ __ _| |_(_) ___  _ __
 *  | || '_ ` _ \| '_ \| |/ _ \ '_ ` _ \ / _ \ '_ \| __/ _` | __| |/ _ \| '_ \
 *  | || | | | | | |_) | |  __/ | | | | |  __/ | | | || (_| | |_| | (_) | | | |
 * |___|_| |_| |_| .__/|_|\___|_| |_| |_|\___|_| |_|\__\__,_|\__|_|\___/|_| |_|
 *               |_|
 */
// :implementation
#ifdef WADDLE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct _WDL_PlatformState _WDL_PlatformState;
static b8 _wdl_platform_init(void);
static b8 _wdl_platform_termiante(void);

typedef struct _WDL_State _WDL_State;
struct _WDL_State {
    _WDL_PlatformState *platform;
    WDL_ThreadCtx* main_ctx;
};

static _WDL_State _wdl_state = {0};

b8 wdl_init(void) {
    if (!_wdl_platform_init()) {
        return false;
    }
    _wdl_state.main_ctx = wdl_thread_ctx_create();
    wdl_thread_ctx_set(_wdl_state.main_ctx);
    return true;
}

b8 wdl_terminate(void) {
    if (!_wdl_platform_termiante()) {
        return false;
    }
    wdl_thread_ctx_set(NULL);
    wdl_thread_ctx_destroy(_wdl_state.main_ctx);
    return true;
}

// -- Arena --------------------------------------------------------------------

struct WDL_Arena {
    u64 capacity;
    u64 commit;
    u64 pos;
    u8 align;
};

WDL_Arena* wdl_arena_create(void) {
    return wdl_arena_create_sized(WDL_GB(1));
}

WDL_Arena* wdl_arena_create_sized(u64 capacity) {
    WDL_Arena* arena = wdl_os_reserve_memory(capacity);
    wdl_os_commit_memory(arena, wdl_os_get_page_size());

    *arena = (WDL_Arena) {
        .commit = wdl_os_get_page_size(),
        .align = sizeof(void*),
        .capacity = capacity,
        .pos = sizeof(WDL_Arena),
    };
    return arena;
}

void wdl_arena_destroy(WDL_Arena* arena) {
    wdl_os_release_memory(arena, arena->capacity);
}

void wdl_arena_set_align(WDL_Arena* arena, u8 align) {
    arena->align = align;
}

void* wdl_arena_push(WDL_Arena* arena, u64 size) {
    u8* ptr = wdl_arena_push_no_zero(arena, size);
    for (u32 i = 0; i < size; i++) {
        ptr[i] = 0;
    }
    // memset(ptr, 0, size);
    return ptr;
}

void* wdl_arena_push_no_zero(WDL_Arena* arena, u64 size) {
    u64 start_pos = arena->pos;

    u64 next_pos = arena->pos + size;
    next_pos += arena->align - 1;
    u64 offset = next_pos % arena->align;
    u64 next_pos_aligned = next_pos - offset;
    arena->pos += next_pos_aligned;

    while (arena->pos >= arena->commit) {
        arena->commit += wdl_os_get_page_size();
        // TODO: Handle arena OOM state.
        // if (arena->commit > arena->capacity) {}
        wdl_os_commit_memory(arena, arena->commit);
    }

    return (u8*) arena + start_pos;
}

void wdl_arena_pop(WDL_Arena* arena, u64 size) {
    arena->pos = WDL_MAX(arena->pos - size, sizeof(WDL_Arena));
}

void wdl_arena_pop_to(WDL_Arena* arena, u64 pos) {
    arena->pos = WDL_MAX(pos, sizeof(WDL_Arena));
}

void wdl_arena_clear(WDL_Arena* arena) {
    arena->pos = sizeof(WDL_Arena);
}

u64 wdl_arena_get_pos(WDL_Arena* arena) {
    return arena->pos;
}

WDL_Temp wdl_temp_begin(WDL_Arena* arena) {
    return (WDL_Temp) {
        .arena = arena,
        .pos = arena->pos,
    };
}

void wdl_temp_end(WDL_Temp temp) {
    wdl_arena_pop_to(temp.arena, temp.pos);
}

// -- Thread context -----------------------------------------------------------

#define SCRATCH_ARENA_COUNT 2

struct WDL_ThreadCtx {
    WDL_Arena *scratch_arenas[SCRATCH_ARENA_COUNT];
};

WDL_THREAD_LOCAL WDL_ThreadCtx* _wdl_thread_ctx = {0};

WDL_ThreadCtx* wdl_thread_ctx_create(void) {
    WDL_Arena* scratch_arenas[SCRATCH_ARENA_COUNT] = {0};
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        scratch_arenas[i] = wdl_arena_create();
    }

    WDL_ThreadCtx* ctx = wdl_arena_push(scratch_arenas[0], sizeof(WDL_ThreadCtx));
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        ctx->scratch_arenas[i] = scratch_arenas[i];
    }

    return ctx;
}

void wdl_thread_ctx_destroy(WDL_ThreadCtx* ctx) {
    // Destroy the arenas in reverse order since the thread context lives on
    // the first arena.
    for (i32 i = WDL_ARRLEN(ctx->scratch_arenas) - 1; i >= 0; i--) {
        wdl_arena_destroy(ctx->scratch_arenas[i]);
    }
}

void wdl_thread_ctx_set(WDL_ThreadCtx* ctx) {
    _wdl_thread_ctx = ctx;
}

// -- Scratch arena ------------------------------------------------------------

static WDL_Arena* get_non_conflicting_scratch_arena(WDL_Arena* const* conflicts, u32 count) {
    if (_wdl_thread_ctx == NULL) {
        return NULL;
    }

    if (count == 0) {
        return _wdl_thread_ctx->scratch_arenas[0];
    }

    for (u32 i = 0; i < count; i++) {
        for (u32 j = 0; j < WDL_ARRLEN(_wdl_thread_ctx->scratch_arenas); j++) {
            if (conflicts[i] != _wdl_thread_ctx->scratch_arenas[j]) {
                return _wdl_thread_ctx->scratch_arenas[j];
            }
        }
    }

    return NULL;
}

WDL_Scratch wdl_scratch_begin(WDL_Arena* const* conflicts, u32 count) {
    WDL_Arena* scratch = get_non_conflicting_scratch_arena(conflicts, count);
    if (scratch == NULL) {
        return (WDL_Scratch) {0};
    }
    return wdl_temp_begin(scratch);
}

void wdl_scratch_end(WDL_Scratch scratch) {
    wdl_temp_end(scratch);
}

// -- Logging ------------------------------------------------------------------

void _wdl_log_internal(WDL_LogLevel level, const char* file, u32 line, const char* msg, ...) {
    const char* const level_str[WDL_LOG_LEVEL_COUNT] = {
        "[FATAL]",
        "[ERROR]",
        "[WARN] ",
        "[INFO] ",
        "[DEBUG]",
        "[TRACE]",
    };

    printf("%s %s:%u: ", level_str[level], file, line);

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");

}

// -- String -------------------------------------------------------------------

WDL_Str wdl_str(const u8* data, u32 len) {
    return (WDL_Str) {
        .data = data,
        .len = len,
    };
}

u32 wdl_str_cstrlen(const u8* cstr) {
    u32 len = 0;
    while (cstr[len] != 0) {
        len++;
    }
    return len;
}

char* wdl_str_to_cstr(WDL_Arena* arena, WDL_Str str)  {
    char* cstr = wdl_arena_push_no_zero(arena, str.len + 1);
    memcpy(cstr, str.data, str.len);
    cstr[str.len] = 0;
    return cstr;
}

// -- OS -----------------------------------------------------------------------
// Platform specific implementation
// :platform

#ifdef WDL_POSIX

#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <dlfcn.h>

struct _WDL_PlatformState {
    u32 page_size;
    f32 start_time;
};

static f32 _wdl_posix_time_stamp(void)  {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (f32) tp.tv_sec + (f32) tp.tv_nsec / 1e9;
}

b8 _wdl_platform_init(void) {
    _WDL_PlatformState* platform = wdl_os_reserve_memory(sizeof(_WDL_PlatformState));
    if (platform == NULL) {
        return false;
    }
    wdl_os_commit_memory(platform, sizeof(_WDL_PlatformState));

    *platform = (_WDL_PlatformState) {
        .page_size = getpagesize(),
        .start_time = _wdl_posix_time_stamp(),
    };
    _wdl_state.platform = platform;

    return true;
}

b8 _wdl_platform_termiante(void) {
    wdl_os_release_memory(_wdl_state.platform, sizeof(_WDL_PlatformState));
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

// -- Library ------------------------------------------------------------------

struct WDL_Lib {
    void* handle;
};

WDL_Lib* wdl_lib_load(WDL_Arena* arena, const char* filename) {
    WDL_Lib* lib = wdl_arena_push_no_zero(arena, sizeof(WDL_Lib));
    lib->handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    return lib;
}

void wdl_lib_unload(WDL_Lib* lib) {
    dlclose(lib->handle);
    lib->handle = NULL;
}

WDL_LibFunc wdl_lib_func(WDL_Lib* lib, const char* func_name) {
    return dlsym(lib->handle, func_name);
}

#endif // WDL_POSIX

#endif // WADDLE_IMPLEMENTATION
#endif // WADDLE_H_
