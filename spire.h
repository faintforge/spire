#ifndef SPIRE_H_
#define SPIRE_H_ 1

#ifdef _WIN32
#define SP_OS_WINDOWS
#endif // _WIN32

#ifdef __linux__
#define SP_OS_LINUX
#endif //__linux__

#ifdef EMSCRIPTEN
#define SP_OS_EMSCRIPTEN
#endif // EMSCRIPTEN

#ifdef __unix__
#define SP_UNIX
#endif // __unix__

#ifdef SP_UNIX
#include <unistd.h>
#ifdef _POSIX_VERSION
#define SP_POSIX
#endif // _POSIX_VERSION
#endif // SP_UNIX

#if defined(SP_SHARED) && defined(SP_OS_WINDOWS)
    #if defined(WADDLE_IMPLEMENTATION)
        #define SP_API __declspec(dllexport)
    #else
        #define SP_API __declspec(dllimport)
    #endif
#elif defined(SP_SHADERD) && defined(SP_OS_LINUX) && defined(WADDLE_IMPLEMENTATION)
    #define SP_API __attribute__((visibility("default")))
#else
    #define SP_API extern
#endif

#ifdef SP_OS_WINDOWS
#define SP_THREAD_LOCAL __declspec(thread)
#define SP_INLINE __forceinline
#endif // SP_OS_WINDOWS

#ifdef SP_POSIX
#define SP_THREAD_LOCAL __thread
#define SP_INLINE static inline __attribute__((always_inline))
#endif // SP_POSIX

#ifndef NDEBUG
#define SP_DEBUG
#endif // NDEBUG

#include <math.h>
#include <stdlib.h>
#include <stdarg.h>

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

typedef struct SP_ArenaDesc SP_ArenaDesc;
struct SP_ArenaDesc {
    // Size of one block in the linked list. If chaining isn't enabled then it's
    // the total size of the arena.
    u64 block_size;
    // Does the arena reserve virtual address space and incrementally commit it?
    b8 virtual_memory;
    // What is the alignment of the arena. Default is 8 on 64-bit systems and 4
    // on 32-bit systems.
    u64 alignment;
    // Does the arena allocate a new node in a linked list when it runs out of
    // memory?
    b8 chaining;
};

typedef struct SP_Config SP_Config;
struct SP_Config {
    SP_ArenaDesc default_arena_desc;
    struct {
        b8 colorful;
    } logging;
};

// Basic configuration for desktop applications.
static const SP_Config SP_CONFIG_DEFAULT = {
    .default_arena_desc = {
        .block_size = 4llu << 30, // 4 GB
        .virtual_memory = true,
        .alignment = sizeof(void*),
        .chaining = true,
    },
    .logging = {
        .colorful = true,
    },
};

SP_API b8 sp_init(SP_Config config);
SP_API b8 sp_terminate(void);

SP_API void sp_dump_arena_metrics(void);

// -- Utils --------------------------------------------------------------------

#define sp_kb(V) ((u64) (V) << 10)
#define sp_mb(V) ((u64) (V) << 20)
#define sp_gb(V) ((u64) (V) << 30)

#define sp_min(A, B) ((A) > (B) ? (B) : (A))
#define sp_max(A, B) ((A) > (B) ? (A) : (B))
#define sp_clamp(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

#define sp_arrlen(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#define sp_offset(S, M) ((u64) &((S*) 0)->M)

SP_API u64 sp_fvn1a_hash(const void* data, u64 len);

#define sp_ensure(COND, MSG, ...) do { \
    if (!(COND)) {\
        sp_fatal(MSG, ##__VA_ARGS__); \
        abort(); \
    } \
} while (0)

#ifdef SP_DEBUG
#define sp_assert(COND, MSG, ...) sp_ensure(COND, MSG, ##__VA_ARGS__)
#else // SP_DEBUG
#define sp_assert(cond, msg, ...)
#endif // SP_DEBUG

// -- String -------------------------------------------------------------------

typedef struct SP_Str SP_Str;
struct SP_Str {
    const u8* data;
    u32 len;
};

// -- Arena --------------------------------------------------------------------

typedef struct SP_Arena SP_Arena;

SP_API SP_Arena* sp_arena_create(void);
SP_API SP_Arena* sp_arena_create_configurable(SP_ArenaDesc desc);
SP_API void      sp_arena_destroy(SP_Arena* arena);
SP_API void*     sp_arena_push(SP_Arena* arena, u64 size);
SP_API void*     sp_arena_push_no_zero(SP_Arena* arena, u64 size);
SP_API void      sp_arena_pop(SP_Arena* arena, u64 size);
SP_API void      sp_arena_pop_to(SP_Arena* arena, u64 pos);
SP_API void      sp_arena_clear(SP_Arena* arena);
SP_API u64       sp_arena_get_pos(const SP_Arena* arena);

typedef struct SP_ArenaMetrics SP_ArenaMetrics;
struct SP_ArenaMetrics {
    u32 id;
    SP_Str tag;
    u64 current_usage;
    u64 peak_usage;
    u64 push_operations;
    u64 pop_operations;
    u64 total_pushed_bytes;
    u64 total_popped_bytes;
};

SP_API void            sp_arena_tag(SP_Arena* arena, SP_Str tag);
SP_API SP_ArenaMetrics sp_arena_get_metrics(const SP_Arena* arena);

typedef struct SP_Temp SP_Temp;
struct SP_Temp {
    SP_Arena* arena;
    u64 pos;
};

SP_API SP_Temp sp_temp_begin(SP_Arena* arena);
SP_API void    sp_temp_end(SP_Temp temp);

// -- String -------------------------------------------------------------------

#define sp_str_lit(STR_LIT) sp_str((const u8*) (STR_LIT), sizeof(STR_LIT) - 1)
#define sp_cstr(CSTR) sp_str((const u8*) (CSTR), sp_str_cstrlen((const u8*) (CSTR)))

SP_API SP_Str sp_str(const u8* data, u32 len);
SP_API u32    sp_str_cstrlen(const u8* cstr);
SP_API char*  sp_str_to_cstr(SP_Arena* arena, SP_Str str);
SP_API b8     sp_str_equal(SP_Str a, SP_Str b);
SP_API SP_Str sp_str_pushf(SP_Arena* arena, const void* fmt, ...);

// -- Thread context -----------------------------------------------------------

typedef struct SP_ThreadCtx SP_ThreadCtx;

SP_API SP_ThreadCtx* sp_thread_ctx_create(void);
SP_API void          sp_thread_ctx_destroy(SP_ThreadCtx* ctx);
SP_API void          sp_thread_ctx_set(SP_ThreadCtx* ctx);

// -- Scratch arena ------------------------------------------------------------

typedef SP_Temp SP_Scratch;

SP_API SP_Scratch sp_scratch_begin(SP_Arena* const* conflicts, u32 count);
SP_API void       sp_scratch_end(SP_Scratch scratch);

// -- Logging ------------------------------------------------------------------

typedef enum SP_LogLevel {
    SP_LOG_LEVEL_FATAL,
    SP_LOG_LEVEL_ERROR,
    SP_LOG_LEVEL_WARN,
    SP_LOG_LEVEL_INFO,
    SP_LOG_LEVEL_DEBUG,
    SP_LOG_LEVEL_TRACE,

    SP_LOG_LEVEL_COUNT,
} SP_LogLevel;

#define sp_fatal(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_FATAL, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define sp_error(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_ERROR, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define sp_warn(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_WARN, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define sp_info(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_INFO, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define sp_debug(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define sp_trace(MSG, ...) _sp_log_internal(SP_LOG_LEVEL_TRACE, __FILE__, __LINE__, MSG, ##__VA_ARGS__)

SP_API void _sp_log_internal(SP_LogLevel level, const char* file, u32 line, const char* msg, ...);

// -- Library ------------------------------------------------------------------

typedef struct SP_Lib SP_Lib;

typedef void (*SP_LibFunc)(void);

SP_API SP_Lib*    sp_lib_load(SP_Arena* arena, const char* filename);
SP_API void       sp_lib_unload(SP_Lib* lib);
SP_API SP_LibFunc sp_lib_func(SP_Lib* lib, const char* func_name);

// -- Math ---------------------------------------------------------------------

typedef struct SP_Vec2 SP_Vec2;
struct SP_Vec2 {
    f32 x, y;
};

SP_INLINE SP_Vec2 sp_v2(f32 x, f32 y) { return (SP_Vec2) {x, y}; }
SP_INLINE SP_Vec2 sp_v2s(f32 scaler) { return (SP_Vec2) {scaler, scaler}; }

SP_INLINE SP_Vec2 sp_v2_add(SP_Vec2 a, SP_Vec2 b) { return sp_v2(a.x + b.x, a.y + b.y); }
SP_INLINE SP_Vec2 sp_v2_sub(SP_Vec2 a, SP_Vec2 b) { return sp_v2(a.x - b.x, a.y - b.y); }
SP_INLINE SP_Vec2 sp_v2_mul(SP_Vec2 a, SP_Vec2 b) { return sp_v2(a.x * b.x, a.y * b.y); }
SP_INLINE SP_Vec2 sp_v2_div(SP_Vec2 a, SP_Vec2 b) { return sp_v2(a.x / b.x, a.y / b.y); }

SP_INLINE SP_Vec2 sp_v2_adds(SP_Vec2 vec, f32 scaler) { return sp_v2(vec.x + scaler, vec.y + scaler); }
SP_INLINE SP_Vec2 sp_v2_subs(SP_Vec2 vec, f32 scaler) { return sp_v2(vec.x - scaler, vec.y - scaler); }
SP_INLINE SP_Vec2 sp_v2_muls(SP_Vec2 vec, f32 scaler) { return sp_v2(vec.x * scaler, vec.y * scaler); }
SP_INLINE SP_Vec2 sp_v2_divs(SP_Vec2 vec, f32 scaler) { return sp_v2(vec.x / scaler, vec.y / scaler); }

SP_INLINE f32 sp_v2_magnitude_squared(SP_Vec2 vec) { return vec.x * vec.x + vec.y * vec.y; }
SP_INLINE f32 sp_v2_magnitude(SP_Vec2 vec) { return sqrtf(sp_v2_magnitude_squared(vec)); }
SP_INLINE SP_Vec2 sp_v2_normalized(SP_Vec2 vec) {
    f32 inv_mag = 1.0f / sp_v2_magnitude(vec);
    return sp_v2_muls(vec, inv_mag);
}

typedef struct SP_Ivec2 SP_Ivec2;
struct SP_Ivec2 {
    i32 x, y;
};

SP_INLINE SP_Ivec2 sp_iv2(i32 x, i32 y) { return (SP_Ivec2) {x, y}; }
SP_INLINE SP_Ivec2 sp_iv2s(i32 scaler) { return (SP_Ivec2) {scaler, scaler}; }

SP_INLINE SP_Ivec2 sp_iv2_add(SP_Ivec2 a, SP_Ivec2 b) { return sp_iv2(a.x + b.x, a.y + b.y); }
SP_INLINE SP_Ivec2 sp_iv2_sub(SP_Ivec2 a, SP_Ivec2 b) { return sp_iv2(a.x - b.x, a.y - b.y); }
SP_INLINE SP_Ivec2 sp_iv2_mul(SP_Ivec2 a, SP_Ivec2 b) { return sp_iv2(a.x * b.x, a.y * b.y); }
SP_INLINE SP_Ivec2 sp_iv2_div(SP_Ivec2 a, SP_Ivec2 b) { return sp_iv2(a.x / b.x, a.y / b.y); }

SP_INLINE SP_Ivec2 sp_iv2_adds(SP_Ivec2 vec, i32 scaler) { return sp_iv2(vec.x + scaler, vec.y + scaler); }
SP_INLINE SP_Ivec2 sp_iv2_subs(SP_Ivec2 vec, i32 scaler) { return sp_iv2(vec.x - scaler, vec.y - scaler); }
SP_INLINE SP_Ivec2 sp_iv2_muls(SP_Ivec2 vec, i32 scaler) { return sp_iv2(vec.x * scaler, vec.y * scaler); }
SP_INLINE SP_Ivec2 sp_iv2_divs(SP_Ivec2 vec, i32 scaler) { return sp_iv2(vec.x / scaler, vec.y / scaler); }

SP_INLINE f32 sp_iv2_magnitude_squared(SP_Ivec2 vec) { return vec.x * vec.x + vec.y * vec.y; }
SP_INLINE f32 sp_iv2_magnitude(SP_Ivec2 vec) { return sqrtf(sp_iv2_magnitude_squared(vec)); }

SP_INLINE SP_Vec2  sp_iv2_to_v2(SP_Ivec2 vec) { return sp_v2(vec.x, vec.y); }
SP_INLINE SP_Ivec2 sp_v2_to_iv2(SP_Vec2 vec) { return sp_iv2(vec.x, vec.y); }

typedef struct SP_Vec4 SP_Vec4;
struct SP_Vec4 {
    f32 x, y, z, w;
};

SP_INLINE SP_Vec4 sp_v4(f32 x, f32 y, f32 z, f32 w) { return (SP_Vec4) {x, y, z, w}; }
SP_INLINE SP_Vec4 sp_v4s(f32 scaler) { return (SP_Vec4) {scaler, scaler, scaler, scaler}; }

typedef struct SP_Mat4 SP_Mat4;
struct SP_Mat4 {
    SP_Vec4 a, b, c, d;
};

static const SP_Mat4 SP_M4_IDENTITY = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

SP_INLINE SP_Vec4 sp_m4_mul_vec(SP_Mat4 mat, SP_Vec4 vec) {
    return (SP_Vec4) {
        vec.x*mat.a.x + vec.y*mat.a.y + vec.z*mat.a.z + vec.w*mat.a.w,
        vec.x*mat.b.x + vec.y*mat.b.y + vec.z*mat.b.z + vec.w*mat.b.w,
        vec.x*mat.c.x + vec.y*mat.c.y + vec.z*mat.c.z + vec.w*mat.c.w,
        vec.x*mat.d.x + vec.y*mat.d.y + vec.z*mat.d.z + vec.w*mat.d.w,
    };
}

// https://en.wikipedia.org/wiki/Orthographic_projection#Geometry
SP_INLINE SP_Mat4 sp_m4_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = 2.0f / (right - left);
    f32 y = 2.0f / (top - bottom);
    f32 z = -2.0f / (far - near);

    f32 x_off = -(right+left) / (right-left);
    f32 y_off = -(top+bottom) / (top-bottom);
    f32 z_off = -(far+near) / (far-near);

    return (SP_Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}

SP_INLINE SP_Mat4 sp_m4_inv_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = (right - left) / 2.0f;
    f32 y = (top - bottom) / 2.0f;
    f32 z = (far - near) / -2.0f;

    f32 x_off = (left+right) / 2.0f;
    f32 y_off = (top+bottom) / 2.0f;
    f32 z_off = -(far+near) / 2.0f;

    return (SP_Mat4) {
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    };
}

// -- Hash map -----------------------------------------------------------------

typedef struct SP_HashMap SP_HashMap;

typedef struct SP_HashMapDesc SP_HashMapDesc;
struct SP_HashMapDesc {
    SP_Arena* arena;
    u32 capacity;

    u64 (*hash)(const void* key, u64 size);
    b8 (*equal)(const void* a, const void* b, u64 size);

    u64 key_size;
    u64 value_size;
};

SP_API SP_HashMap* sp_hm_new(SP_HashMapDesc desc);

#define sp_hm_insert(MAP, KEY, VALUE) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        sp_assert(sizeof(__typeof__(VALUE)) == sp_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value. Expected %llu got %llu.", \
                sp_hm_get_value_size(MAP), \
                sizeof(__typeof__(VALUE))); \
        __typeof__(KEY) _key = (KEY); \
        __typeof__(VALUE) _value = (VALUE); \
        _sp_hash_map_insert_impl((MAP), &_key, &_value); \
    })

#define sp_hm_set(MAP, KEY, VALUE) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        sp_assert(sizeof(__typeof__(VALUE)) == sp_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value. Expected %llu got %llu.", \
                sp_hm_get_value_size(MAP), \
                sizeof(__typeof__(VALUE))); \
        __typeof__(KEY) _key = (KEY); \
        __typeof__(VALUE) _value = (VALUE); \
        __typeof__(VALUE) prev_value; \
        _sp_hash_map_set_impl((MAP), &_key, &_value, &prev_value); \
        prev_value; \
    })

#define sp_hm_get(MAP, KEY, VALUE_TYPE) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        sp_assert(sizeof(VALUE_TYPE) == sp_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value type. Expected %llu got %llu.", \
                sp_hm_get_value_size(MAP), \
                sizeof(VALUE_TYPE)); \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _sp_hash_map_get_impl((MAP), &_key, &value); \
        value; \
    })

#define sp_hm_getp(MAP, KEY) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _sp_hash_map_getp_impl((MAP), &_key); \
    })

#define sp_hm_has(MAP, KEY) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _sp_hash_map_has_impl((MAP), &_key); \
    })

#define sp_hm_remove(MAP, KEY, VALUE_TYPE) ({ \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        sp_assert(sizeof(VALUE_TYPE) == sp_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value type. Expected %llu got %llu.", \
                sp_hm_get_value_size(MAP), \
                sizeof(VALUE_TYPE)); \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _sp_hash_map_remove_impl((MAP), &_key, &value); \
        value; \
    })

SP_API u64 sp_hm_get_value_size(const SP_HashMap* map);
SP_API u64 sp_hm_get_key_size(const SP_HashMap* map);

// Iteration
typedef struct SP_HashMapIter SP_HashMapIter;
struct SP_HashMapIter {
    SP_HashMap* map;
    void* bucket;
    u32 index;
};

SP_API SP_HashMapIter sp_hm_iter_new(SP_HashMap* map);
SP_API b8             sp_hm_iter_valid(SP_HashMapIter iter);
SP_API SP_HashMapIter sp_hm_iter_next(SP_HashMapIter iter);
SP_API void*          sp_hm_iter_get_keyp(SP_HashMapIter iter);
SP_API void*          sp_hm_iter_get_valuep(SP_HashMapIter iter);

#define sp_hm_iter_get_key(ITER, KEY_TYPE) ({ \
        KEY_TYPE result; \
        _sp_hm_iter_get_key_impl((ITER), &result); \
        result; \
    })

#define sp_hm_iter_get_value(ITER, VALUE_TYPE) ({ \
        VALUE_TYPE result; \
        _sp_hm_iter_get_value_impl((ITER), &result); \
        result; \
    })

SP_API void _sp_hm_iter_get_key_impl(SP_HashMapIter iter, void* out_value);
SP_API void _sp_hm_iter_get_value_impl(SP_HashMapIter iter, void* out_value);

// Helper functions
SP_API u64 sp_hm_helper_hash_str(const void* key, u64 size);
SP_API b8  sp_hm_helper_equal_str(const void* a, const void* b, u64 len);
SP_API b8  sp_hm_helper_equal_generic(const void* a, const void* b, u64 len);

#define sp_hm_desc_generic(ARENA, CAPACITY, KEY_TYPE, VALUE_TYPE) ((SP_HashMapDesc) { \
        .arena = (ARENA), \
        .capacity = (CAPACITY), \
        .hash = sp_fvn1a_hash, \
        .equal = sp_hm_helper_equal_generic, \
        .key_size = sizeof(KEY_TYPE), \
        .value_size = sizeof(VALUE_TYPE), \
    })

#define sp_hm_desc_str(ARENA, CAPACITY, VALUE_TYPE) ((SP_HashMapDesc) { \
        .arena = (ARENA), \
        .capacity = (CAPACITY), \
        .hash = sp_hm_helper_hash_str, \
        .equal = sp_hm_helper_equal_str, \
        .key_size = sizeof(SP_Str), \
        .value_size = sizeof(VALUE_TYPE), \
    })


SP_API b8    _sp_hash_map_insert_impl(SP_HashMap* map, const void* key, const void* value);
SP_API void  _sp_hash_map_set_impl(SP_HashMap* map, const void* key, const void* value, void* out_prev_value);
SP_API void  _sp_hash_map_get_impl(SP_HashMap* map, const void* key, void* out_value);
SP_API void* _sp_hash_map_getp_impl(SP_HashMap* map, const void* key);
SP_API b8    _sp_hash_map_has_impl(SP_HashMap* map, const void* key);
SP_API void  _sp_hash_map_remove_impl(SP_HashMap* map, const void* key, void* out_value);

// -- OS -----------------------------------------------------------------------

SP_API void* sp_os_reserve_memory(u64 size);
SP_API void  sp_os_commit_memory(void* ptr, u64 size);
SP_API void  sp_os_decommit_memory(void* ptr, u64 size);
SP_API void  sp_os_release_memory(void* ptr, u64 size);
SP_API f32   sp_os_get_time(void);
SP_API u32   sp_os_get_page_size(void);

/*
 *  ___                 _                           _        _   _
 * |_ _|_ __ ___  _ __ | | ___ _ __ ___   ___ _ __ | |_ __ _| |_(_) ___  _ __
 *  | || '_ ` _ \| '_ \| |/ _ \ '_ ` _ \ / _ \ '_ \| __/ _` | __| |/ _ \| '_ \
 *  | || | | | | | |_) | |  __/ | | | | |  __/ | | | || (_| | |_| | (_) | | | |
 * |___|_| |_| |_| .__/|_|\___|_| |_| |_|\___|_| |_|\__\__,_|\__|_|\___/|_| |_|
 *               |_|
 */
// :implementation
#ifdef SPIRE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct _SP_PlatformState _SP_PlatformState;
static b8 _sp_platform_init(void);
static b8 _sp_platform_termiante(void);

typedef struct _SP_State _SP_State;
struct _SP_State {
    SP_Config cfg;
    _SP_PlatformState *platform;
    SP_ThreadCtx* main_ctx;

    struct {
        SP_Arena* first;
        SP_Arena* last;
        u32 curr_id;
    } arenas;
};

static _SP_State _sp_state = {0};

static SP_Config _config_set_defaults(SP_Config config) {
    if (config.default_arena_desc.block_size == 0) {
        if (config.default_arena_desc.virtual_memory) {
            config.default_arena_desc.block_size = sp_gb(4);
        } else {
            config.default_arena_desc.block_size = sp_mb(4);
        }
    }

    if (config.default_arena_desc.alignment == 0) {
        config.default_arena_desc.alignment = sizeof(void*);
    }

    return config;
}

b8 sp_init(SP_Config config) {
    if (!_sp_platform_init()) {
        return false;
    }
    _sp_state.cfg = _config_set_defaults(config);
    _sp_state.main_ctx = sp_thread_ctx_create();
    sp_thread_ctx_set(_sp_state.main_ctx);
    return true;
}

b8 sp_terminate(void) {
    if (!_sp_platform_termiante()) {
        return false;
    }
    sp_thread_ctx_set(NULL);
    sp_thread_ctx_destroy(_sp_state.main_ctx);
    return true;
}

// -- Utils --------------------------------------------------------------------

u64 sp_fvn1a_hash(const void* data, u64 len) {
    const u8* _data = data;
    u64 hash = 2166136261u;
    for (u64 i = 0; i < len; i++) {
        hash ^= *(_data + i);
        hash *= 16777619;
    }
    return hash;
}

// -- Arena --------------------------------------------------------------------

typedef struct _SP_ArenaBlock _SP_ArenaBlock;
struct _SP_ArenaBlock {
    _SP_ArenaBlock *next;
    _SP_ArenaBlock *prev;
    u64 commit;
    u8* memory;
};

static u64 _align_value(u64 value, u64 align) {
    u64 aligned = value + align - 1;    // 32 + 8 - 1 = 39
    u64 mod = aligned % align;          // 39 % 8 = 7
    aligned = aligned - mod;            // 32
    return aligned;
}

static _SP_ArenaBlock* _sp_arena_block_alloc(u64 block_size, b8 virtual_memory) {
    block_size += sizeof(_SP_ArenaBlock);
    _SP_ArenaBlock* block = sp_os_reserve_memory(block_size);
    if (virtual_memory) {
        sp_os_commit_memory(block, sp_os_get_page_size());
    } else {
        sp_os_commit_memory(block, block_size);
    }
    *block = (_SP_ArenaBlock) {
        .memory = (u8*) block + sizeof(_SP_ArenaBlock),
        .commit = sp_os_get_page_size(),
    };
    return block;
}

static void _sp_arena_block_dealloc(_SP_ArenaBlock* block, u64 block_size) {
    sp_os_release_memory(block, block_size + sizeof(_SP_ArenaBlock));
}

struct SP_Arena {
    SP_Arena* next;
    SP_Arena* prev;
    u32 id;

    SP_ArenaDesc desc;
    u64 pos;
    _SP_ArenaBlock* first_block;
    _SP_ArenaBlock* last_block;
    // Current 'index' of the chain.
    u32 chain_index;

    // Metrics
    SP_Str tag;
    u64 peak_usage;
    u64 push_operations;
    u64 pop_operations;
    u64 total_pushed_bytes;
    u64 total_popped_bytes;
};

SP_Arena* sp_arena_create(void) {
    return sp_arena_create_configurable(_sp_state.cfg.default_arena_desc);
}

SP_Arena* sp_arena_create_configurable(SP_ArenaDesc desc) {
    _SP_ArenaBlock* block = _sp_arena_block_alloc(desc.block_size, desc.virtual_memory);
    SP_Arena* arena = (SP_Arena*) block->memory;
    *arena = (SP_Arena) {
        .prev = _sp_state.arenas.last,
        .id = _sp_state.arenas.curr_id++,
        .desc = desc,
        .chain_index = 0,
        .pos = _align_value(sizeof(SP_Arena), desc.alignment),
        .peak_usage = _align_value(sizeof(SP_Arena), desc.alignment),
        .first_block = block,
        .last_block = block,
    };

    if (_sp_state.arenas.first == NULL) {
        _sp_state.arenas.first = arena;
        _sp_state.arenas.last = arena;
    }

    _sp_state.arenas.last->next = arena;
    _sp_state.arenas.last = arena;

    return arena;
}

void sp_arena_destroy(SP_Arena* arena) {
    if (arena->prev != NULL) {
        arena->prev->next = arena->next;
    }
    if (arena->next != NULL) {
        arena->next->prev = arena->prev;
    }

    sp_os_release_memory(arena, arena->desc.block_size);
}

void* sp_arena_push(SP_Arena* arena, u64 size) {
    u8* ptr = sp_arena_push_no_zero(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void* sp_arena_push_no_zero(SP_Arena* arena, u64 size) {
    u64 start_pos = _align_value(arena->pos, arena->desc.alignment);

    u64 aligned_size = _align_value(size, arena->desc.alignment);
    u64 next_pos = start_pos + aligned_size;

    sp_ensure(size <= arena->desc.block_size, "Push size too big for arena. Increase block size.");

    arena->pos = next_pos;

    arena->peak_usage = sp_max(arena->peak_usage, arena->pos);
    arena->total_pushed_bytes += aligned_size;
    arena->push_operations++;

    if (arena->pos > (arena->chain_index + 1) * arena->desc.block_size) {
        // Crash on OOM if we can't grow.
        if (!arena->desc.chaining) {
            sp_ensure(false, "Arena is out of memory.");
        }

        _SP_ArenaBlock* block = _sp_arena_block_alloc(arena->desc.block_size, arena->desc.block_size);
        block->prev = arena->last_block;
        arena->last_block->next = block;
        arena->last_block = block;

        arena->chain_index++;
        start_pos = arena->chain_index * arena->desc.block_size;
    }

    _SP_ArenaBlock* block = arena->last_block;

    if (arena->desc.virtual_memory) {
        u64 block_pos_end = arena->pos - arena->chain_index * arena->desc.block_size;
        // Add the size of a block since that also resides on the same allocated
        // memory region.
        u64 page_aligned_pos = _align_value(block_pos_end + sizeof(_SP_ArenaBlock), sp_os_get_page_size());
        if (page_aligned_pos > block->commit) {
            block->commit = page_aligned_pos;
            sp_os_commit_memory(block, page_aligned_pos);
        }
    }

    u64 block_pos = start_pos - arena->chain_index * arena->desc.block_size;
    u8* memory = block->memory + block_pos;
    return memory;
}

void sp_arena_pop(SP_Arena* arena, u64 size) {
    sp_assert(arena->pos >= size, "Popping more than what has been allocated.");
    sp_arena_pop_to(arena, arena->pos - size);
}

void sp_arena_pop_to(SP_Arena* arena, u64 pos) {
    sp_assert(pos <= arena->pos, "Popping to a position beyond the current position.");

    u64 aligned_pos = sp_max(pos, _align_value(sizeof(SP_Arena), arena->desc.alignment));
    arena->total_popped_bytes += arena->pos - aligned_pos;
    arena->pop_operations++;

    arena->pos = aligned_pos;

    if (arena->desc.chaining) {
        u32 new_chain_index = arena->pos / arena->desc.block_size;
        while (arena->chain_index > new_chain_index) {
            _SP_ArenaBlock* last = arena->last_block;
            arena->last_block = arena->last_block->prev;
            _sp_arena_block_dealloc(last, arena->desc.block_size);
            arena->chain_index--;
        }
    }

    if (arena->desc.virtual_memory) {
        _SP_ArenaBlock* block = arena->last_block;
        u64 block_pos = arena->pos - arena->chain_index * arena->desc.block_size;
        // Add the size of a block since that also resides on the same allocated
        // memory region.
        u64 page_aligned_pos = _align_value(block_pos + sizeof(_SP_ArenaBlock), sp_os_get_page_size());
        if (page_aligned_pos < block->commit) {
            u64 unused_size = block->commit - page_aligned_pos;
            sp_os_decommit_memory((u8*) block + page_aligned_pos, unused_size);
            block->commit = page_aligned_pos;
        }
    }
}

void sp_arena_clear(SP_Arena* arena) {
    sp_arena_pop_to(arena, 0);
}

u64 sp_arena_get_pos(const SP_Arena* arena) {
    return arena->pos;
}

SP_Temp sp_temp_begin(SP_Arena* arena) {
    return (SP_Temp) {
        .arena = arena,
        .pos = arena->pos,
    };
}

void sp_temp_end(SP_Temp temp) {
    sp_arena_pop_to(temp.arena, temp.pos);
}

void sp_arena_tag(SP_Arena* arena, SP_Str tag) {
    arena->tag = tag;
}

SP_ArenaMetrics sp_arena_get_metrics(const SP_Arena* arena) {
    return (SP_ArenaMetrics) {
        .id = arena->id,
        .tag = arena->tag,
        .current_usage = arena->pos,
        .peak_usage = arena->peak_usage,
        .push_operations = arena->push_operations,
        .pop_operations = arena->pop_operations,
        .total_pushed_bytes = arena->total_pushed_bytes,
        .total_popped_bytes = arena->total_popped_bytes,
    };
}

static void print_arena_metrics(SP_ArenaMetrics metrics) {
    SP_Str tag = sp_str_lit("untagged");
    if (metrics.tag.len != 0) {
        tag = metrics.tag;
    }
    sp_info("%u (%.*s)", metrics.id, tag.len, tag.data);
    sp_info("    Current usage                %llu bytes", metrics.current_usage);
    sp_info("    Peak usage                   %llu bytes", metrics.peak_usage);
    sp_info("    Number of push operations    %llu", metrics.push_operations);
    sp_info("    Number of pop operations     %llu", metrics.pop_operations);
    sp_info("    Total bytes pushed           %llu bytes", metrics.total_pushed_bytes);
    sp_info("    Total bytes popped           %llu bytes", metrics.total_popped_bytes);
}

void sp_dump_arena_metrics(void) {
    SP_Arena* curr = _sp_state.arenas.first;
    while (curr != NULL) {
        print_arena_metrics(sp_arena_get_metrics(curr));
        curr = curr->next;
    }
}

// -- Thread context -----------------------------------------------------------

#define SCRATCH_ARENA_COUNT 2

struct SP_ThreadCtx {
    SP_Arena *scratch_arenas[SCRATCH_ARENA_COUNT];
};

SP_THREAD_LOCAL SP_ThreadCtx* _sp_thread_ctx = {0};

SP_ThreadCtx* sp_thread_ctx_create(void) {
    SP_Arena* scratch_arenas[SCRATCH_ARENA_COUNT] = {0};
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        scratch_arenas[i] = sp_arena_create();
        sp_arena_tag(scratch_arenas[i], sp_str_pushf(scratch_arenas[i], "scratch-%u", i));
    }

    SP_ThreadCtx* ctx = sp_arena_push(scratch_arenas[0], sizeof(SP_ThreadCtx));
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        ctx->scratch_arenas[i] = scratch_arenas[i];
    }

    return ctx;
}

void sp_thread_ctx_destroy(SP_ThreadCtx* ctx) {
    // Destroy the arenas in reverse order since the thread context lives on
    // the first arena.
    for (i32 i = sp_arrlen(ctx->scratch_arenas) - 1; i >= 0; i--) {
        sp_arena_destroy(ctx->scratch_arenas[i]);
    }
}

void sp_thread_ctx_set(SP_ThreadCtx* ctx) {
    _sp_thread_ctx = ctx;
}

// -- Scratch arena ------------------------------------------------------------

static SP_Arena* get_non_conflicting_scratch_arena(SP_Arena* const* conflicts, u32 count) {
    if (_sp_thread_ctx == NULL) {
        return NULL;
    }

    if (count == 0) {
        return _sp_thread_ctx->scratch_arenas[0];
    }

    for (u32 i = 0; i < count; i++) {
        for (u32 j = 0; j < sp_arrlen(_sp_thread_ctx->scratch_arenas); j++) {
            if (conflicts[i] != _sp_thread_ctx->scratch_arenas[j]) {
                return _sp_thread_ctx->scratch_arenas[j];
            }
        }
    }

    return NULL;
}

SP_Scratch sp_scratch_begin(SP_Arena* const* conflicts, u32 count) {
    SP_Arena* scratch = get_non_conflicting_scratch_arena(conflicts, count);
    if (scratch == NULL) {
        return (SP_Scratch) {0};
    }
    return sp_temp_begin(scratch);
}

void sp_scratch_end(SP_Scratch scratch) {
    sp_temp_end(scratch);
}

// -- Logging ------------------------------------------------------------------

void _sp_log_internal(SP_LogLevel level, const char* file, u32 line, const char* msg, ...) {
    const char* const level_color[SP_LOG_LEVEL_COUNT] = {
        "\033[101;30m",
        "\033[0;91m",
        "\033[0;93m",
        "\033[0;92m",
        "\033[0;94m",
        "\033[0;95m",
    };
    const char* const level_str[SP_LOG_LEVEL_COUNT] = {
        "FATAL",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG",
        "TRACE",
    };

    if (_sp_state.cfg.logging.colorful) {
        printf("%s%s \033[0;90m%s:%u: \033[0m", level_color[level], level_str[level], file, line);
    } else {
        printf("%s %s:%u: ", level_str[level], file, line);
    }

    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    printf("\n");
}

// -- String -------------------------------------------------------------------

SP_Str sp_str(const u8* data, u32 len) {
    return (SP_Str) {
        .data = data,
        .len = len,
    };
}

u32 sp_str_cstrlen(const u8* cstr) {
    u32 len = 0;
    while (cstr[len] != 0) {
        len++;
    }
    return len;
}

char* sp_str_to_cstr(SP_Arena* arena, SP_Str str)  {
    char* cstr = sp_arena_push_no_zero(arena, str.len + 1);
    memcpy(cstr, str.data, str.len);
    cstr[str.len] = 0;
    return cstr;
}

b8 sp_str_equal(SP_Str a, SP_Str b) {
    if (a.len != b.len) {
        return false;
    }

    return memcmp(a.data, b.data, a.len) == 0;
}

SP_Str sp_str_pushf(SP_Arena* arena, const void* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list len_args;
    va_copy(len_args, args);
    u64 len = vsnprintf(NULL, 0, fmt, len_args) + 1;
    va_end(len_args);
    u8* buffer = sp_arena_push_no_zero(arena, len);
    vsnprintf((char*) buffer, len, fmt, args);
    va_end(args);
    // Remove the null terminator.
    sp_arena_pop(arena, 1);
    return sp_str(buffer, len - 1);
}

// -- Hash map -----------------------------------------------------------------

typedef enum _SP_HashMapBucketState {
    _SP_HASH_MAP_BUCKET_STATE_EMPTY,
    _SP_HASH_MAP_BUCKET_STATE_ALIVE,
    _SP_HASH_MAP_BUCKET_STATE_DEAD,
} _SP_HashMapBucketState;

typedef struct _SP_HashMapBucket _SP_HashMapBucket;
struct _SP_HashMapBucket {
    _SP_HashMapBucket* next;
    _SP_HashMapBucket* prev;
    _SP_HashMapBucketState state;

    void* key;
    void* value;
};

struct SP_HashMap {
    SP_HashMapDesc desc;
    _SP_HashMapBucket* buckets;
};

SP_HashMap* sp_hm_new(SP_HashMapDesc desc) {
    SP_HashMap* map = sp_arena_push_no_zero(desc.arena, sizeof(SP_HashMap));
    *map = (SP_HashMap) {
        .desc = desc,
        .buckets = sp_arena_push(desc.arena, desc.capacity * sizeof(_SP_HashMapBucket)),
    };
    return map;
}

b8 _sp_hash_map_insert_impl(SP_HashMap* map, const void* key, const void* value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state != _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
        if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_EMPTY) {
            *bucket = (_SP_HashMapBucket) {
                .key = sp_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = sp_arena_push_no_zero(map->desc.arena, map->desc.value_size),
            };
        }
        bucket->state = _SP_HASH_MAP_BUCKET_STATE_ALIVE,
        memcpy(bucket->key, key, map->desc.key_size);
    } else {
        b8 bucket_found = map->desc.equal(key, bucket->key, map->desc.key_size);
        while (!bucket_found && bucket->next != NULL) {
            bucket = bucket->next;
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return false;
            }
        }

        if (bucket_found) {
            return false;
        }

        _SP_HashMapBucket* new_bucket = sp_arena_push_no_zero(map->desc.arena, sizeof(_SP_HashMapBucket));
        *new_bucket = (_SP_HashMapBucket) {
            .state = _SP_HASH_MAP_BUCKET_STATE_ALIVE,
            .key = sp_arena_push_no_zero(map->desc.arena, map->desc.key_size),
            .value = sp_arena_push_no_zero(map->desc.arena, map->desc.value_size),
        };
        memcpy(new_bucket->key, key, map->desc.key_size);
        bucket->next = new_bucket;
        new_bucket->prev = bucket;
        bucket = new_bucket;
    }

    memcpy(bucket->value, value, map->desc.value_size);
    return true;
}

void _sp_hash_map_set_impl(SP_HashMap* map, const void* key, const void* value, void* out_prev_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    b8 new_key = true;
    if (bucket->state != _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
        if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_EMPTY) {
            *bucket = (_SP_HashMapBucket) {
                .key = sp_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = sp_arena_push_no_zero(map->desc.arena, map->desc.value_size),
            };
        }
        bucket->state = _SP_HASH_MAP_BUCKET_STATE_ALIVE,
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
            _SP_HashMapBucket* new_bucket = sp_arena_push_no_zero(map->desc.arena, sizeof(_SP_HashMapBucket));
            *new_bucket = (_SP_HashMapBucket) {
                .state = _SP_HASH_MAP_BUCKET_STATE_ALIVE,
                .key = sp_arena_push_no_zero(map->desc.arena, map->desc.key_size),
                .value = sp_arena_push_no_zero(map->desc.arena, map->desc.value_size),
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

void _sp_hash_map_get_impl(SP_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
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

void* _sp_hash_map_getp_impl(SP_HashMap* map, const void* key) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return bucket->value;
            }
            bucket = bucket->next;
        }
    }

    return NULL;
}

b8 _sp_hash_map_has_impl(SP_HashMap* map, const void* key) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return true;
            }
            bucket = bucket->next;
        }
    }
    return false;
}

void _sp_hash_map_remove_impl(SP_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _SP_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                memcpy(out_value, bucket->value, map->desc.value_size);
                if (bucket->prev != NULL) {
                    bucket->prev->next = bucket->next;
                } else {
                    // Start of the chain.
                    if (bucket->next == NULL) {
                        bucket->state = _SP_HASH_MAP_BUCKET_STATE_DEAD;
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

// Iteration

SP_HashMapIter sp_hm_iter_new(SP_HashMap* map) {
    SP_HashMapIter iter = {
        .map = map,
    };

    u32 idx = 0;
    _SP_HashMapBucket* bucket;
    while (idx < map->desc.capacity) {
        bucket = &map->buckets[idx];
        if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
            break;
        }
        idx++;
    }

    iter.index = idx;
    iter.bucket = bucket;

    return iter;
}

b8 sp_hm_iter_valid(SP_HashMapIter iter) {
    return iter.index < iter.map->desc.capacity;
}

SP_HashMapIter sp_hm_iter_next(SP_HashMapIter iter) {
    _SP_HashMapBucket* bucket = iter.bucket;
    if (bucket->next != NULL) {
        iter.bucket = bucket->next;
        return iter;
    }

    u32 idx = iter.index + 1;
    while (idx < iter.map->desc.capacity) {
        bucket = &iter.map->buckets[idx];
        if (bucket->state == _SP_HASH_MAP_BUCKET_STATE_ALIVE) {
            break;
        }
        idx++;
    }
    iter.index = idx;
    iter.bucket = bucket;

    return iter;
}

void* sp_hm_iter_get_keyp(SP_HashMapIter iter) {
    _SP_HashMapBucket* bucket = iter.bucket;
    return bucket->key;
}

void* sp_hm_iter_get_valuep(SP_HashMapIter iter) {
    _SP_HashMapBucket* bucket = iter.bucket;
    return bucket->value;
}

void _sp_hm_iter_get_key_impl(SP_HashMapIter iter, void* out_value) {
    _SP_HashMapBucket* bucket = iter.bucket;
    memcpy(out_value, bucket->key, iter.map->desc.key_size);
}

void _sp_hm_iter_get_value_impl(SP_HashMapIter iter, void* out_value) {
    _SP_HashMapBucket* bucket = iter.bucket;
    memcpy(out_value, bucket->value, iter.map->desc.value_size);
}

u64 sp_hm_get_key_size(const SP_HashMap* map) {
    return map->desc.key_size;
}

u64 sp_hm_get_value_size(const SP_HashMap* map) {
    return map->desc.value_size;
}

// Helper functions

u64 sp_hm_helper_hash_str(const void* key, u64 size) {
    (void) size;
    const SP_Str* _key = key;
    return sp_fvn1a_hash(_key->data, _key->len);
}

b8 sp_hm_helper_equal_str(const void* a, const void* b, u64 len) {
    (void) len;
    const SP_Str* _a = a;
    const SP_Str* _b = b;
    return sp_str_equal(*_a, *_b);
}

b8 sp_hm_helper_equal_generic(const void* a, const void* b, u64 len) {
    return memcmp(a, b, len) == 0;
}

// -- OS -----------------------------------------------------------------------
// Platform specific implementation
// :platform

#ifdef SP_POSIX

#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <dlfcn.h>

struct _SP_PlatformState {
    f32 start_time;
};

static f32 _sp_posix_time_stamp(void)  {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (f32) tp.tv_sec + (f32) tp.tv_nsec / 1e9;
}

b8 _sp_platform_init(void) {
    _SP_PlatformState* platform = sp_os_reserve_memory(sizeof(_SP_PlatformState));
    if (platform == NULL) {
        return false;
    }
    sp_os_commit_memory(platform, sizeof(_SP_PlatformState));

    *platform = (_SP_PlatformState) {
        .start_time = _sp_posix_time_stamp(),
    };
    _sp_state.platform = platform;

    return true;
}

b8 _sp_platform_termiante(void) {
    sp_os_release_memory(_sp_state.platform, sizeof(_SP_PlatformState));
    return true;
}

void* sp_os_reserve_memory(u64 size) {
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr;
}

void  sp_os_commit_memory(void* ptr, u64 size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void  sp_os_decommit_memory(void* ptr, u64 size) {
    mprotect(ptr, size, PROT_NONE);
}

void  sp_os_release_memory(void* ptr, u64 size) {
    munmap(ptr, size);
}

f32 sp_os_get_time(void) {
    return _sp_posix_time_stamp() - _sp_state.platform->start_time;
}

u32 sp_os_get_page_size(void) {
    return getpagesize();
}

// -- Library ------------------------------------------------------------------

struct SP_Lib {
    void* handle;
};

SP_Lib* sp_lib_load(SP_Arena* arena, const char* filename) {
    SP_Lib* lib = sp_arena_push_no_zero(arena, sizeof(SP_Lib));
    lib->handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
    return lib;
}

void sp_lib_unload(SP_Lib* lib) {
    dlclose(lib->handle);
    lib->handle = NULL;
}

SP_LibFunc sp_lib_func(SP_Lib* lib, const char* func_name) {
    return dlsym(lib->handle, func_name);
}

#endif // SP_POSIX

#ifdef SP_OS_WINDOWS

#include <windows.h>

struct _SP_PlatformState {
    f32 start_time;
};

static f32 _sp_win32_time_stamp() {
    LARGE_INTEGER freq;
    LARGE_INTEGER time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&time);
    return (f32) time.QuadPart / (f32) freq.QuadPart;
}

b8 _sp_platform_init(void) {
    _SP_PlatformState* platform = sp_os_reserve_memory(sizeof(_SP_PlatformState));
    if (platform == NULL) {
        return false;
    }
    sp_os_commit_memory(platform, sizeof(_SP_PlatformState));

    *platform = (_SP_PlatformState){
        .start_time = _sp_win32_time_stamp(),
    };
    _sp_state.platform = platform;

    return true;
}

b8 _sp_platform_termiante(void) {
    sp_os_release_memory(_sp_state.platform, sizeof(_SP_PlatformState));
    return true;
}

void* sp_os_reserve_memory(u64 size) {
    void* ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    return ptr;
}

void  sp_os_commit_memory(void* ptr, u64 size) {
    VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void  sp_os_decommit_memory(void* ptr, u64 size) {
    VirtualFree(ptr, MEM_DECOMMIT, size);
}

void  sp_os_release_memory(void* ptr, u64 size) {
    VirtualFree(ptr, MEM_RELEASE, size);
}

f32 sp_os_get_time(void) {
    return _sp_win32_time_stamp() - _sp_state.platform->start_time;
}

u32 sp_os_get_page_size(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
}

// -- Library ------------------------------------------------------------------

struct SP_Lib {
    HMODULE handle;
};

SP_Lib* sp_lib_load(SP_Arena* arena, const char* filename) {
    SP_Lib* lib = sp_arena_push_no_zero(arena, sizeof(SP_Lib));
    lib->handle = LoadLibraryA(filename);
    return lib;
}

void sp_lib_unload(SP_Lib* lib) {
    FreeLibrary(lib->handle);
    lib->handle = NULL;
}

SP_LibFunc sp_lib_func(SP_Lib* lib, const char* func_name) {
    return (SP_LibFunc) GetProcAddress(lib->handle, func_name);
}

#endif // SP_OS_WINDOWS

#endif // SPIRE_IMPLEMENTATION
#endif // SPIRE_H_
