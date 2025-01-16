#include <math.h>
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
#define WDL_INLINE __forceinline
#endif // WDL_OS_WINDOWS

#ifdef WDL_POSIX
#define WDL_THREAD_LOCAL __thread
#define WDL_INLINE static inline __attribute__((always_inline))
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
#define WDL_OFFSET(S, M) ((u64) &((S*) 0)->M)

WDLAPI u64 wdl_fvn1a_hash(const void* data, u64 len);

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

#define WDL_STR_LIT(STR_LIT) wdl_str((const u8*) (STR_LIT), sizeof(STR_LIT) - 1)
#define WDL_CSTR(CSTR) wdl_str((const u8*) (CSTR), wdl_str_cstrlen((const u8*) (CSTR)))

WDLAPI WDL_Str wdl_str(const u8* data, u32 len);
WDLAPI u32     wdl_str_cstrlen(const u8* cstr);
WDLAPI char*   wdl_str_to_cstr(WDL_Arena* arena, WDL_Str str);
WDLAPI b8      wdl_str_equal(WDL_Str a, WDL_Str b);

// -- Math ---------------------------------------------------------------------

typedef struct WDL_Vec2 WDL_Vec2;
struct WDL_Vec2 {
    f32 x, y;
};

WDL_INLINE WDL_Vec2 wdl_v2(f32 x, f32 y) { return (WDL_Vec2) {x, y}; }
WDL_INLINE WDL_Vec2 wdl_v2s(f32 scaler) { return (WDL_Vec2) {scaler, scaler}; }

WDL_INLINE WDL_Vec2 wdl_v2_add(WDL_Vec2 a, WDL_Vec2 b) { return wdl_v2(a.x + b.x, a.y + b.y); }
WDL_INLINE WDL_Vec2 wdl_v2_sub(WDL_Vec2 a, WDL_Vec2 b) { return wdl_v2(a.x - b.x, a.y - b.y); }
WDL_INLINE WDL_Vec2 wdl_v2_mul(WDL_Vec2 a, WDL_Vec2 b) { return wdl_v2(a.x * b.x, a.y * b.y); }
WDL_INLINE WDL_Vec2 wdl_v2_div(WDL_Vec2 a, WDL_Vec2 b) { return wdl_v2(a.x / b.x, a.y / b.y); }

WDL_INLINE WDL_Vec2 wdl_v2_adds(WDL_Vec2 vec, f32 scaler) { return wdl_v2(vec.x + scaler, vec.y + scaler); }
WDL_INLINE WDL_Vec2 wdl_v2_subs(WDL_Vec2 vec, f32 scaler) { return wdl_v2(vec.x - scaler, vec.y - scaler); }
WDL_INLINE WDL_Vec2 wdl_v2_muls(WDL_Vec2 vec, f32 scaler) { return wdl_v2(vec.x * scaler, vec.y * scaler); }
WDL_INLINE WDL_Vec2 wdl_v2_divs(WDL_Vec2 vec, f32 scaler) { return wdl_v2(vec.x / scaler, vec.y / scaler); }

WDL_INLINE f32 wdl_v2_magnitude_squared(WDL_Vec2 vec) { return vec.x * vec.x + vec.y * vec.y; }
WDL_INLINE f32 wdl_v2_magnitude(WDL_Vec2 vec) { return sqrtf(wdl_v2_magnitude_squared(vec)); }
WDL_INLINE WDL_Vec2 wdl_v2_normalized(WDL_Vec2 vec) {
    f32 inv_mag = 1.0f / wdl_v2_magnitude(vec);
    return wdl_v2_muls(vec, inv_mag);
}

typedef struct WDL_Ivec2 WDL_Ivec2;
struct WDL_Ivec2 {
    i32 x, y;
};

WDL_INLINE WDL_Ivec2 wdl_iv2(i32 x, i32 y) { return (WDL_Ivec2) {x, y}; }
WDL_INLINE WDL_Ivec2 wdl_iv2s(i32 scaler) { return (WDL_Ivec2) {scaler, scaler}; }

WDL_INLINE WDL_Ivec2 wdl_iv2_add(WDL_Ivec2 a, WDL_Ivec2 b) { return wdl_iv2(a.x + b.x, a.y + b.y); }
WDL_INLINE WDL_Ivec2 wdl_iv2_sub(WDL_Ivec2 a, WDL_Ivec2 b) { return wdl_iv2(a.x - b.x, a.y - b.y); }
WDL_INLINE WDL_Ivec2 wdl_iv2_mul(WDL_Ivec2 a, WDL_Ivec2 b) { return wdl_iv2(a.x * b.x, a.y * b.y); }
WDL_INLINE WDL_Ivec2 wdl_iv2_div(WDL_Ivec2 a, WDL_Ivec2 b) { return wdl_iv2(a.x / b.x, a.y / b.y); }

WDL_INLINE WDL_Ivec2 wdl_iv2_adds(WDL_Ivec2 vec, i32 scaler) { return wdl_iv2(vec.x + scaler, vec.y + scaler); }
WDL_INLINE WDL_Ivec2 wdl_iv2_subs(WDL_Ivec2 vec, i32 scaler) { return wdl_iv2(vec.x - scaler, vec.y - scaler); }
WDL_INLINE WDL_Ivec2 wdl_iv2_muls(WDL_Ivec2 vec, i32 scaler) { return wdl_iv2(vec.x * scaler, vec.y * scaler); }
WDL_INLINE WDL_Ivec2 wdl_iv2_divs(WDL_Ivec2 vec, i32 scaler) { return wdl_iv2(vec.x / scaler, vec.y / scaler); }

WDL_INLINE f32 wdl_iv2_magnitude_squared(WDL_Ivec2 vec) { return vec.x * vec.x + vec.y * vec.y; }
WDL_INLINE f32 wdl_iv2_magnitude(WDL_Ivec2 vec) { return sqrtf(wdl_iv2_magnitude_squared(vec)); }

typedef struct WDL_Vec4 WDL_Vec4;
struct WDL_Vec4 {
    f32 x, y, z, w;
};

WDL_INLINE WDL_Vec4 wdl_v4(f32 x, f32 y, f32 z, f32 w) { return (WDL_Vec4) {x, y, z, w}; }
WDL_INLINE WDL_Vec4 wdl_v4s(f32 scaler) { return (WDL_Vec4) {scaler, scaler, scaler, scaler}; }

typedef struct WDL_Mat4 WDL_Mat4;
struct WDL_Mat4 {
    WDL_Vec4 a, b, c, d;
};

static const WDL_Mat4 WDL_M4_IDENTITY = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

WDL_INLINE WDL_Vec4 wdl_m4_mul_vec(WDL_Mat4 mat, WDL_Vec4 vec) {
    return (WDL_Vec4) {
        vec.x*mat.a.x + vec.y*mat.a.y + vec.z*mat.a.z + vec.w*mat.a.w,
        vec.x*mat.b.x + vec.y*mat.b.y + vec.z*mat.b.z + vec.w*mat.b.w,
        vec.x*mat.c.x + vec.y*mat.c.y + vec.z*mat.c.z + vec.w*mat.c.w,
        vec.x*mat.d.x + vec.y*mat.d.y + vec.z*mat.d.z + vec.w*mat.d.w,
    };
}

// https://en.wikipedia.org/wiki/Orthographic_projection#Geometry
WDL_INLINE WDL_Mat4 wdl_m4_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = 2.0f / (right - left);
    f32 y = 2.0f / (top - bottom);
    f32 z = -2.0f / (far - near);

    f32 x_off = -(right+left) / (right-left);
    f32 y_off = -(top+bottom) / (top-bottom);
    f32 z_off = -(far+near) / (far-near);

    return (WDL_Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}

WDL_INLINE WDL_Mat4 wdl_m4_inv_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = (right - left) / 2.0f;
    f32 y = (top - bottom) / 2.0f;
    f32 z = (far - near) / -2.0f;

    f32 x_off = (left+right) / 2.0f;
    f32 y_off = (top+bottom) / 2.0f;
    f32 z_off = -(far+near) / 2.0f;

    return (WDL_Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}

// -- Hash map -----------------------------------------------------------------

typedef struct WDL_HashMap WDL_HashMap;

typedef struct WDL_HashMapDesc WDL_HashMapDesc;
struct WDL_HashMapDesc {
    WDL_Arena* arena;
    u32 capacity;

    u64 (*hash)(const void* key, u64 size);
    b8 (*equal)(const void* a, const void* b, u64 size);

    u64 key_size;
    u64 value_size;
};

WDLAPI WDL_HashMap* wdl_hm_new(WDL_HashMapDesc desc);

#define wdl_hm_insert(MAP, KEY, VALUE) ({ \
        __typeof__(KEY) _key = (KEY); \
        __typeof__(VALUE) _value = (VALUE); \
        _wdl_hash_map_insert_impl((MAP), &_key, &_value); \
    })

#define wdl_hm_set(MAP, KEY, VALUE, VALUE_TYPE) ({ \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE _value = (VALUE); \
        VALUE_TYPE prev_value; \
        _wdl_hash_map_set_impl((MAP), &_key, &_value, &prev_value); \
        prev_value; \
    })

#define wdl_hm_get(MAP, KEY, VALUE_TYPE) ({ \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _wdl_hash_map_get_impl((MAP), &_key, &value); \
        value; \
    })

#define wdl_hm_has(MAP, KEY) ({ \
        __typeof__(KEY) _key = (KEY); \
        _wdl_hash_map_has_impl((MAP), &_key); \
    })

#define wdl_hm_remove(MAP, KEY, VALUE_TYPE) ({ \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _wdl_hash_map_remove_impl((MAP), &_key, &value); \
        value; \
    })

// Helper functions
WDLAPI u64 wdl_hm_helper_hash_str(const void* key, u64 size);
WDLAPI b8  wdl_hm_helper_equal_str(const void* a, const void* b, u64 len);
WDLAPI b8  wdl_hm_helper_equal_generic(const void* a, const void* b, u64 len);

#define wdl_hm_desc_generic(ARENA, CAPACITY, KEY_TYPE, VALUE_TYPE) ((WDL_HashMapDesc) { \
        .arena = (ARENA), \
        .capacity = (CAPACITY), \
        .hash = wdl_fvn1a_hash, \
        .equal = wdl_hm_helper_equal_generic, \
        .key_size = sizeof(KEY_TYPE), \
        .value_size = sizeof(VALUE_TYPE), \
    })

#define wdl_hm_desc_str(ARENA, CAPACITY, VALUE_TYPE) ((WDL_HashMapDesc) { \
        .arena = (ARENA), \
        .capacity = (CAPACITY), \
        .hash = wdl_hm_helper_hash_str, \
        .equal = wdl_hm_helper_equal_str, \
        .key_size = sizeof(WDL_Str), \
        .value_size = sizeof(VALUE_TYPE), \
    })


WDLAPI b8   _wdl_hash_map_insert_impl(WDL_HashMap* map, const void* key, const void* value);
WDLAPI void _wdl_hash_map_set_impl(WDL_HashMap* map, const void* key, const void* value, void* out_prev_value);
WDLAPI void _wdl_hash_map_get_impl(WDL_HashMap* map, const void* key, void* out_value);
WDLAPI b8   _wdl_hash_map_has_impl(WDL_HashMap* map, const void* key);
WDLAPI void _wdl_hash_map_remove_impl(WDL_HashMap* map, const void* key, void* out_value);

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

// -- Misc ---------------------------------------------------------------------

u64 wdl_fvn1a_hash(const void* data, u64 len) {
    const u8* _data = data;
    u64 hash = 2166136261u;
    for (u64 i = 0; i < len; i++) {
        hash ^= *(_data + i);
        hash *= 16777619;
    }
    return hash;
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
    memset(ptr, 0, size);
    return ptr;
}

static u64 _align_value(u64 value, u64 align) {
    u64 aligned = value + align - 1;    // 32 + 8 - 1 = 39
    u64 mod = aligned % align;          // 39 % 8 = 7
    aligned = aligned - mod;            // 32
    return aligned;
}

void* wdl_arena_push_no_zero(WDL_Arena* arena, u64 size) {
    u64 start_pos = arena->pos;

    u64 next_pos = arena->pos + size;
    u64 next_pos_aligned = _align_value(next_pos, arena->align);

    arena->pos = next_pos_aligned;

    u64 aligned_pos = _align_value(arena->pos, wdl_os_get_page_size());
    if (aligned_pos > arena->commit) {
        arena->commit = aligned_pos;
        // TODO: Handle arena OOM state.
        // if (arena->commit > arena->capacity) {}
        wdl_os_commit_memory(arena, aligned_pos);
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
        "\033[101;30mFATAL\033[0;0m",
        "\033[0;91mERROR\033[0;0m",
        "\033[0;93mWARN \033[0;0m",
        "\033[0;92mINFO \033[0;0m",
        "\033[0;94mDEBUG\033[0;0m",
        "\033[0;95mTRACE\033[0;0m",
    };

    printf("%s \033[0;90m%s:%u: \033[0m", level_str[level], file, line);

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

b8 wdl_str_equal(WDL_Str a, WDL_Str b) {
    if (a.len != b.len) {
        return false;
    }

    return memcmp(a.data, b.data, a.len) == 0;
}

// -- Hash map -----------------------------------------------------------------

typedef enum _WDL_HashMapBucketState : u8 {
    _WDL_HASH_MAP_BUCKET_STATE_EMPTY,
    _WDL_HASH_MAP_BUCKET_STATE_ALIVE,
    _WDL_HASH_MAP_BUCKET_STATE_DEAD,
} _WDL_HashMapBucketState;

typedef struct _WDL_HashMapBucket _WDL_HashMapBucket;
struct _WDL_HashMapBucket {
    _WDL_HashMapBucket* next;
    _WDL_HashMapBucket* prev;
    _WDL_HashMapBucketState state;

    WDL_Str* key;
    u32* value;
};

struct WDL_HashMap {
    WDL_HashMapDesc desc;
    _WDL_HashMapBucket* buckets;
};

WDL_HashMap* wdl_hm_new(WDL_HashMapDesc desc) {
    WDL_HashMap* map = wdl_arena_push_no_zero(desc.arena, sizeof(WDL_HashMap));
    *map = (WDL_HashMap) {
        .desc = desc,
        .buckets = wdl_arena_push(desc.arena, desc.capacity * sizeof(_WDL_HashMapBucket)),
    };
    return map;
}

b8 _wdl_hash_map_insert_impl(WDL_HashMap* map, const void* key, const void* value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state != _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_EMPTY) {
            *bucket = (_WDL_HashMapBucket) {
                .key = wdl_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = wdl_arena_push_no_zero(map->desc.arena, map->desc.value_size),
            };
        }
        bucket->state = _WDL_HASH_MAP_BUCKET_STATE_ALIVE,
        memcpy(bucket->key, key, map->desc.key_size);
    } else {
        b8 bucket_found = map->desc.equal(key, bucket->key, map->desc.key_size);
        while (!bucket_found && bucket->next != NULL) {
            bucket = bucket->next;
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return false;
            }
        }

        _WDL_HashMapBucket* new_bucket = wdl_arena_push_no_zero(map->desc.arena, sizeof(_WDL_HashMapBucket));
        *new_bucket = (_WDL_HashMapBucket) {
            .state = _WDL_HASH_MAP_BUCKET_STATE_ALIVE,
            .key = wdl_arena_push_no_zero(map->desc.arena, map->desc.key_size),
            .value = wdl_arena_push_no_zero(map->desc.arena, map->desc.value_size),
        };
        memcpy(new_bucket->key, key, map->desc.key_size);
        bucket->next = new_bucket;
        new_bucket->prev = bucket;
        bucket = new_bucket;
    }

    memcpy(bucket->value, value, map->desc.value_size);
    return true;
}

void _wdl_hash_map_set_impl(WDL_HashMap* map, const void* key, const void* value, void* out_prev_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    b8 new_key = true;
    if (bucket->state != _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_EMPTY) {
            *bucket = (_WDL_HashMapBucket) {
                .key = wdl_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = wdl_arena_push_no_zero(map->desc.arena, map->desc.value_size),
            };
        }
        bucket->state = _WDL_HASH_MAP_BUCKET_STATE_ALIVE,
        memcpy(bucket->key, key, map->desc.key_size);
    } else {
        b8 bucket_found = map->desc.equal(key, bucket->key, map->desc.key_size);
        while (!bucket_found && bucket->next != NULL) {
            bucket = bucket->next;
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                bucket_found = true;
                new_key = false;
                break;
            }
        }

        if (!bucket_found) {
            _WDL_HashMapBucket* new_bucket = wdl_arena_push_no_zero(map->desc.arena, sizeof(_WDL_HashMapBucket));
            *new_bucket = (_WDL_HashMapBucket) {
                .state = _WDL_HASH_MAP_BUCKET_STATE_ALIVE,
                .key = wdl_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = wdl_arena_push_no_zero(map->desc.arena, map->desc.value_size),
            };
            memcpy(new_bucket->key, key, map->desc.key_size);
            new_bucket->prev = bucket;
            bucket->next = new_bucket;
            bucket = new_bucket;
        }
    }

    if (out_prev_value != NULL) {
        if (!new_key) {
            memcpy(out_prev_value, bucket->value, map->desc.value_size);
        } else {
            memset(out_prev_value, 0, map->desc.value_size);
        }
    }

    memcpy(bucket->value, value, map->desc.value_size);
}

void _wdl_hash_map_get_impl(WDL_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                memcpy(out_value, bucket->value, map->desc.value_size);
                return;
            }
            bucket = bucket->next;
        }
    }

    memset(out_value, 0, map->desc.value_size);
}

b8 _wdl_hash_map_has_impl(WDL_HashMap* map, const void* key) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return true;
            }
            bucket = bucket->next;
        }
    }
    return false;
}

void _wdl_hash_map_remove_impl(WDL_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                memcpy(out_value, bucket->value, map->desc.value_size);
                if (bucket->prev != NULL) {
                    bucket->prev->next = bucket->next;
                } else {
                    // Start of the chain.
                    if (bucket->next == NULL) {
                        bucket->state = _WDL_HASH_MAP_BUCKET_STATE_DEAD;
                    } else {
                        if (bucket->next->next != NULL) {
                            bucket->next->next->prev = bucket;
                        }
                        *bucket = *bucket->next;
                        bucket->prev = NULL;
                    }
                }
                return;
            }
            bucket = bucket->next;
        }
    }
}

// Helper functions

u64 wdl_hm_helper_hash_str(const void* key, u64 size) {
    (void) size;
    const WDL_Str* _key = key;
    return wdl_fvn1a_hash(_key->data, _key->len);
}

b8 wdl_hm_helper_equal_str(const void* a, const void* b, u64 len) {
    (void) len;
    const WDL_Str* _a = a;
    const WDL_Str* _b = b;
    return wdl_str_equal(*_a, *_b);
}

b8 wdl_hm_helper_equal_generic(const void* a, const void* b, u64 len) {
    return memcmp(a, b, len) == 0;
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
