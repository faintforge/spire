#include <string.h>
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

typedef struct wdl_arena_t wdl_arena_t;

WDLAPI wdl_arena_t* wdl_arena_create(void);
WDLAPI wdl_arena_t* wdl_arena_create_sized(u64 size);
WDLAPI void         wdl_arena_destroy(wdl_arena_t* arena);
WDLAPI void         wdl_arena_set_align(wdl_arena_t* arena, u8 align);
WDLAPI void*        wdl_arena_push(wdl_arena_t* arena, u64 size);
WDLAPI void*        wdl_arena_push_no_zero(wdl_arena_t* arena, u64 size);
WDLAPI void         wdl_arena_pop(wdl_arena_t* arena, u64 size);
WDLAPI void         wdl_arena_pop_to(wdl_arena_t* arena, u64 pos);
WDLAPI void         wdl_arena_clear(wdl_arena_t* arena);
WDLAPI u64          wdl_arena_get_pos(wdl_arena_t* arena);

typedef struct wdl_temp_t wdl_temp_t;
struct wdl_temp_t {
    wdl_arena_t* arena;
    u64 pos;
};

WDLAPI wdl_temp_t wdl_temp_begin(wdl_arena_t* arena);
WDLAPI void       wdl_temp_end(wdl_temp_t temp);

// -- Thread context -----------------------------------------------------------

typedef struct wdl_thread_ctx_t wdl_thread_ctx_t;

WDLAPI wdl_thread_ctx_t* wdl_thread_ctx_create(void);
WDLAPI void              wdl_thread_ctx_destroy(wdl_thread_ctx_t* ctx);
WDLAPI void              wdl_thread_ctx_set(wdl_thread_ctx_t* ctx);

// -- Scratch arena ------------------------------------------------------------

typedef wdl_temp_t wdl_scratch_t;

WDLAPI wdl_scratch_t wdl_scratch_begin(wdl_arena_t* const* conflicts, u32 count);
WDLAPI void          wdl_scratch_end(wdl_scratch_t scratch);

// -- OS -----------------------------------------------------------------------

WDLAPI void* wdl_os_reserve_memory(u32 size);
WDLAPI void  wdl_os_commit_memory(void* ptr, u32 size);
WDLAPI void  wdl_os_release_memory(void* ptr, u32 size);
WDLAPI f32   wdl_os_get_time(void);
WDLAPI u32   wdl_os_get_page_size(void);

#endif // WADDLE_H_

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

typedef struct _wdl_platform_state_t _wdl_platform_state_t;
static b8 _wdl_platform_init(void);
static b8 _wdl_platform_termiante(void);

typedef struct _wdl_state_t _wdl_state_t;
struct _wdl_state_t {
    _wdl_platform_state_t *platform;
    wdl_thread_ctx_t* main_ctx;
};

static _wdl_state_t _wdl_state = {0};

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

struct wdl_arena_t {
    u64 capacity;
    u64 commit;
    u64 pos;
    u8 align;
};

wdl_arena_t* wdl_arena_create(void) {
    return wdl_arena_create_sized(WDL_GB(1));
}

wdl_arena_t* wdl_arena_create_sized(u64 capacity) {
    wdl_arena_t* arena = wdl_os_reserve_memory(capacity);
    wdl_os_commit_memory(arena, wdl_os_get_page_size());

    *arena = (wdl_arena_t) {
        .commit = wdl_os_get_page_size(),
        .align = sizeof(void*),
        .capacity = capacity,
        .pos = sizeof(wdl_arena_t),
    };
    return arena;
}

void wdl_arena_destroy(wdl_arena_t* arena) {
    wdl_os_release_memory(arena, arena->capacity);
}

void wdl_arena_set_align(wdl_arena_t* arena, u8 align) {
    arena->align = align;
}

void* wdl_arena_push(wdl_arena_t* arena, u64 size) {
    u8* ptr = wdl_arena_push_no_zero(arena, size);
    for (u32 i = 0; i < size; i++) {
        ptr[i] = 0;
    }
    // memset(ptr, 0, size);
    return ptr;
}

void* wdl_arena_push_no_zero(wdl_arena_t* arena, u64 size) {
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

void wdl_arena_pop(wdl_arena_t* arena, u64 size) {
    arena->pos = WDL_MAX(arena->pos - size, sizeof(wdl_arena_t));
}

void wdl_arena_pop_to(wdl_arena_t* arena, u64 pos) {
    arena->pos = WDL_MAX(pos, sizeof(wdl_arena_t));
}

void wdl_arena_clear(wdl_arena_t* arena) {
    arena->pos = sizeof(wdl_arena_t);
}

u64 wdl_arena_get_pos(wdl_arena_t* arena) {
    return arena->pos;
}

wdl_temp_t wdl_temp_begin(wdl_arena_t* arena) {
    return (wdl_temp_t) {
        .arena = arena,
        .pos = arena->pos,
    };
}

void wdl_temp_end(wdl_temp_t temp) {
    wdl_arena_pop_to(temp.arena, temp.pos);
}

// -- Thread context -----------------------------------------------------------

#define SCRATCH_ARENA_COUNT 2

struct wdl_thread_ctx_t {
    wdl_arena_t *scratch_arenas[SCRATCH_ARENA_COUNT];
};

WDL_THREAD_LOCAL wdl_thread_ctx_t* _wdl_thread_ctx = {0};

wdl_thread_ctx_t* wdl_thread_ctx_create(void) {
    wdl_arena_t* scratch_arenas[SCRATCH_ARENA_COUNT] = {0};
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        scratch_arenas[i] = wdl_arena_create();
    }

    wdl_thread_ctx_t* ctx = wdl_arena_push(scratch_arenas[0], sizeof(wdl_thread_ctx_t));
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        ctx->scratch_arenas[i] = scratch_arenas[i];
    }

    return ctx;
}

void wdl_thread_ctx_destroy(wdl_thread_ctx_t* ctx) {
    // Destroy the arenas in reverse order since the thread context lives on
    // the first arena.
    for (i32 i = WDL_ARRLEN(ctx->scratch_arenas) - 1; i >= 0; i--) {
        wdl_arena_destroy(ctx->scratch_arenas[i]);
    }
}

void wdl_thread_ctx_set(wdl_thread_ctx_t* ctx) {
    _wdl_thread_ctx = ctx;
}

static wdl_arena_t* get_non_conflicting_scratch_arena(wdl_arena_t* const* conflicts, u32 count) {
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

wdl_scratch_t wdl_scratch_begin(wdl_arena_t* const* conflicts, u32 count) {
    wdl_arena_t* scratch = get_non_conflicting_scratch_arena(conflicts, count);
    if (scratch == NULL) {
        return (wdl_scratch_t) {0};
    }
    return wdl_temp_begin(scratch);
}

void wdl_scratch_end(wdl_scratch_t scratch) {
    wdl_temp_end(scratch);
}

// -- OS -----------------------------------------------------------------------
// Platform specific implementation
// :platform

#ifdef WDL_POSIX

#include <unistd.h>
#include <time.h>
#include <sys/mman.h>

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
