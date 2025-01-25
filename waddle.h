#ifndef WADDLE_H_
#define WADDLE_H_ 1

#ifdef _WIN32
#define WDL_OS_WINDOWS
#endif // _WIN32

#ifdef __linux__
#define WDL_OS_LINUX
#endif //__linux__

#ifdef EMSCRIPTEN
#define WDL_OS_EMSCRIPTEN
#endif // EMSCRIPTEN

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

#ifndef NDEBUG
#define WDL_DEBUG
#endif // NDEBUG

#include <math.h>
#include <stdlib.h>

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

typedef struct WDL_ArenaDesc WDL_ArenaDesc;
struct WDL_ArenaDesc {
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

typedef struct WDL_Config WDL_Config;
struct WDL_Config {
    WDL_ArenaDesc default_arena_desc;
    struct {
        b8 colorful;
    } logging;
};

// Basic configuration for desktop applications.
static const WDL_Config WDL_CONFIG_DEFAULT = {
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

WDLAPI b8 wdl_init(WDL_Config config);
WDLAPI b8 wdl_terminate(void);

// -- Utils --------------------------------------------------------------------

#define wdl_kb(V) ((u64) (V) << 10)
#define wdl_mb(V) ((u64) (V) << 20)
#define wdl_gb(V) ((u64) (V) << 30)

#define wdl_min(A, B) ((A) > (B) ? (B) : (A))
#define wdl_max(A, B) ((A) > (B) ? (A) : (B))
#define wdl_clamp(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

#define wdl_arrlen(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#define wdl_offset(S, M) ((u64) &((S*) 0)->M)

WDLAPI u64 wdl_fvn1a_hash(const void* data, u64 len);

#define wdl_ensure(COND, MSG, ...) do { \
    if (!(COND)) {\
        wdl_fatal(MSG, ##__VA_ARGS__); \
        abort(); \
    } \
} while (0)

#ifdef WDL_DEBUG
#define wdl_assert(COND, MSG, ...) wdl_ensure(COND, MSG, ##__VA_ARGS__)
#else // WDL_DEBUG
#define wdl_assert(cond, msg, ...)
#endif // WDL_DEBUG

// -- Arena --------------------------------------------------------------------

typedef struct WDL_Arena WDL_Arena;

WDLAPI WDL_Arena* wdl_arena_create(void);
WDLAPI WDL_Arena* wdl_arena_create_configurable(WDL_ArenaDesc desc);
WDLAPI void       wdl_arena_destroy(WDL_Arena* arena);
WDLAPI void*      wdl_arena_push(WDL_Arena* arena, u64 size);
WDLAPI void*      wdl_arena_push_no_zero(WDL_Arena* arena, u64 size);
WDLAPI void       wdl_arena_pop(WDL_Arena* arena, u64 size);
WDLAPI void       wdl_arena_pop_to(WDL_Arena* arena, u64 pos);
WDLAPI void       wdl_arena_clear(WDL_Arena* arena);
WDLAPI u64        wdl_arena_get_pos(WDL_Arena* arena);

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

#define wdl_fatal(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_FATAL, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define wdl_error(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_ERROR, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define wdl_warn(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_WARN, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define wdl_info(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_INFO, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define wdl_debug(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_DEBUG, __FILE__, __LINE__, MSG, ##__VA_ARGS__)
#define wdl_trace(MSG, ...) _wdl_log_internal(WDL_LOG_LEVEL_TRACE, __FILE__, __LINE__, MSG, ##__VA_ARGS__)

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

#define wdl_str_lit(STR_LIT) wdl_str((const u8*) (STR_LIT), sizeof(STR_LIT) - 1)
#define wdl_cstr(CSTR) wdl_str((const u8*) (CSTR), wdl_str_cstrlen((const u8*) (CSTR)))

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
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        wdl_assert(sizeof(__typeof__(VALUE)) == wdl_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value. Expected %llu got %llu.", \
                wdl_hm_get_value_size(MAP), \
                sizeof(__typeof__(VALUE))); \
        __typeof__(KEY) _key = (KEY); \
        __typeof__(VALUE) _value = (VALUE); \
        _wdl_hash_map_insert_impl((MAP), &_key, &_value); \
    })

#define wdl_hm_set(MAP, KEY, VALUE) ({ \
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        wdl_assert(sizeof(__typeof__(VALUE)) == wdl_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value. Expected %llu got %llu.", \
                wdl_hm_get_value_size(MAP), \
                sizeof(__typeof__(VALUE))); \
        __typeof__(KEY) _key = (KEY); \
        __typeof__(VALUE) _value = (VALUE); \
        __typeof__(VALUE) prev_value; \
        _wdl_hash_map_set_impl((MAP), &_key, &_value, &prev_value); \
        prev_value; \
    })

#define wdl_hm_get(MAP, KEY, VALUE_TYPE) ({ \
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        wdl_assert(sizeof(VALUE_TYPE) == wdl_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value type. Expected %llu got %llu.", \
                wdl_hm_get_value_size(MAP), \
                sizeof(VALUE_TYPE)); \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _wdl_hash_map_get_impl((MAP), &_key, &value); \
        value; \
    })

#define wdl_hm_getp(MAP, KEY) ({ \
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _wdl_hash_map_getp_impl((MAP), &_key); \
    })

#define wdl_hm_has(MAP, KEY) ({ \
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _wdl_hash_map_has_impl((MAP), &_key); \
    })

#define wdl_hm_remove(MAP, KEY, VALUE_TYPE) ({ \
        wdl_assert(sizeof(__typeof__(KEY)) == wdl_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                wdl_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        wdl_assert(sizeof(VALUE_TYPE) == wdl_hm_get_value_size(MAP), \
                "Hash map value size does not match with the size of the provided value type. Expected %llu got %llu.", \
                wdl_hm_get_value_size(MAP), \
                sizeof(VALUE_TYPE)); \
        __typeof__(KEY) _key = (KEY); \
        VALUE_TYPE value; \
        _wdl_hash_map_remove_impl((MAP), &_key, &value); \
        value; \
    })

WDLAPI u64 wdl_hm_get_value_size(const WDL_HashMap* map);
WDLAPI u64 wdl_hm_get_key_size(const WDL_HashMap* map);

// Iteration
typedef struct WDL_HashMapIter WDL_HashMapIter;
struct WDL_HashMapIter {
    WDL_HashMap* map;
    void* bucket;
    u32 index;
};

WDLAPI WDL_HashMapIter wdl_hm_iter_new(WDL_HashMap* map);
WDLAPI b8              wdl_hm_iter_valid(WDL_HashMapIter iter);
WDLAPI WDL_HashMapIter wdl_hm_iter_next(WDL_HashMapIter iter);
WDLAPI void*           wdl_hm_iter_get_keyp(WDL_HashMapIter iter);
WDLAPI void*           wdl_hm_iter_get_valuep(WDL_HashMapIter iter);

#define wdl_hm_iter_get_key(ITER, KEY_TYPE) ({ \
        KEY_TYPE result; \
        _wdl_hm_iter_get_key_impl((ITER), &result); \
        result; \
    })

#define wdl_hm_iter_get_value(ITER, VALUE_TYPE) ({ \
        VALUE_TYPE result; \
        _wdl_hm_iter_get_value_impl((ITER), &result); \
        result; \
    })

WDLAPI void _wdl_hm_iter_get_key_impl(WDL_HashMapIter iter, void* out_value);
WDLAPI void _wdl_hm_iter_get_value_impl(WDL_HashMapIter iter, void* out_value);

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


WDLAPI b8    _wdl_hash_map_insert_impl(WDL_HashMap* map, const void* key, const void* value);
WDLAPI void  _wdl_hash_map_set_impl(WDL_HashMap* map, const void* key, const void* value, void* out_prev_value);
WDLAPI void  _wdl_hash_map_get_impl(WDL_HashMap* map, const void* key, void* out_value);
WDLAPI void* _wdl_hash_map_getp_impl(WDL_HashMap* map, const void* key);
WDLAPI b8    _wdl_hash_map_has_impl(WDL_HashMap* map, const void* key);
WDLAPI void  _wdl_hash_map_remove_impl(WDL_HashMap* map, const void* key, void* out_value);

// -- OS -----------------------------------------------------------------------

WDLAPI void* wdl_os_reserve_memory(u64 size);
WDLAPI void  wdl_os_commit_memory(void* ptr, u64 size);
WDLAPI void  wdl_os_decommit_memory(void* ptr, u64 size);
WDLAPI void  wdl_os_release_memory(void* ptr, u64 size);
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
    WDL_Config cfg;
    _WDL_PlatformState *platform;
    WDL_ThreadCtx* main_ctx;
};

static _WDL_State _wdl_state = {0};

static WDL_Config _config_set_defaults(WDL_Config config) {
    if (config.default_arena_desc.block_size == 0) {
        if (config.default_arena_desc.virtual_memory) {
            config.default_arena_desc.block_size = wdl_gb(4);
        } else {
            config.default_arena_desc.block_size = wdl_mb(4);
        }
    }

    if (config.default_arena_desc.alignment == 0) {
        config.default_arena_desc.alignment = sizeof(void*);
    }

    return config;
}

b8 wdl_init(WDL_Config config) {
    if (!_wdl_platform_init()) {
        return false;
    }
    _wdl_state.cfg = _config_set_defaults(config);
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

// -- Utils --------------------------------------------------------------------

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

typedef struct _WDL_ArenaBlock _WDL_ArenaBlock;
struct _WDL_ArenaBlock {
    _WDL_ArenaBlock *next;
    _WDL_ArenaBlock *prev;
    u64 commit;
    void* memory;
};

static u64 _align_value(u64 value, u64 align) {
    u64 aligned = value + align - 1;    // 32 + 8 - 1 = 39
    u64 mod = aligned % align;          // 39 % 8 = 7
    aligned = aligned - mod;            // 32
    return aligned;
}

static _WDL_ArenaBlock* _wdl_arena_block_alloc(u64 block_size, b8 virtual_memory) {
    block_size += sizeof(_WDL_ArenaBlock);
    _WDL_ArenaBlock* block = wdl_os_reserve_memory(block_size);
    if (virtual_memory) {
        wdl_os_commit_memory(block, wdl_os_get_page_size());
    } else {
        wdl_os_commit_memory(block, block_size);
    }
    *block = (_WDL_ArenaBlock) {
        .memory = (void*) block + sizeof(_WDL_ArenaBlock),
        .commit = wdl_os_get_page_size(),
    };
    return block;
}

static void _wdl_arena_block_dealloc(_WDL_ArenaBlock* block, u64 block_size) {
    wdl_os_release_memory(block, block_size + sizeof(_WDL_ArenaBlock));
}

struct WDL_Arena {
    WDL_ArenaDesc desc;
    u64 pos;
    _WDL_ArenaBlock* first_block;
    _WDL_ArenaBlock* last_block;
    // Current 'index' of the chain.
    u32 chain_index;
};

WDL_Arena* wdl_arena_create(void) {
    return wdl_arena_create_configurable(_wdl_state.cfg.default_arena_desc);
}

WDL_Arena* wdl_arena_create_configurable(WDL_ArenaDesc desc) {
    _WDL_ArenaBlock* block = _wdl_arena_block_alloc(desc.block_size, desc.virtual_memory);
    WDL_Arena* arena = block->memory;
    *arena = (WDL_Arena) {
        .desc = desc,
        .chain_index = 0,
        .pos = _align_value(sizeof(WDL_Arena), desc.alignment),
        .first_block = block,
        .last_block = block,
    };
    return arena;
}

void wdl_arena_destroy(WDL_Arena* arena) {
    wdl_os_release_memory(arena, arena->desc.block_size);
}

void* wdl_arena_push(WDL_Arena* arena, u64 size) {
    u8* ptr = wdl_arena_push_no_zero(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void* wdl_arena_push_no_zero(WDL_Arena* arena, u64 size) {
    u64 start_pos = _align_value(arena->pos, arena->desc.alignment);

    u64 next_pos = start_pos + size;
    u64 next_pos_aligned = _align_value(next_pos, arena->desc.alignment);

    wdl_ensure(size <= arena->desc.block_size, "Push size too big for arena. Increase block size.");

    arena->pos = next_pos_aligned;

    if (arena->pos > (arena->chain_index + 1) * arena->desc.block_size) {
        // Crash on OOM if we can't grow.
        if (!arena->desc.chaining) {
            wdl_ensure(false, "Arena is out of memory.");
        }

        _WDL_ArenaBlock* block = _wdl_arena_block_alloc(arena->desc.block_size, arena->desc.block_size);
        block->prev = arena->last_block;
        arena->last_block->next = block;
        arena->last_block = block;

        arena->chain_index++;
        start_pos = arena->chain_index * arena->desc.block_size;
    }

    _WDL_ArenaBlock* block = arena->last_block;

    if (arena->desc.virtual_memory) {
        u64 block_pos_end = arena->pos - arena->chain_index * arena->desc.block_size;
        // Add the size of a block since that also resides on the same allocated
        // memory region.
        u64 page_aligned_pos = _align_value(block_pos_end + sizeof(_WDL_ArenaBlock), wdl_os_get_page_size());
        if (page_aligned_pos > block->commit) {
            block->commit = page_aligned_pos;
            wdl_os_commit_memory(block, page_aligned_pos);
        }
    }

    u64 block_pos = start_pos - arena->chain_index * arena->desc.block_size;
    void* memory = block->memory + block_pos;
    return memory;
}

void wdl_arena_pop(WDL_Arena* arena, u64 size) {
    wdl_arena_pop_to(arena, arena->pos - size);
}

void wdl_arena_pop_to(WDL_Arena* arena, u64 pos) {
    arena->pos = wdl_max(pos, _align_value(sizeof(WDL_Arena), arena->desc.alignment));

    if (arena->desc.chaining) {
        u32 new_chain_index = arena->pos / arena->desc.block_size;
        while (arena->chain_index > new_chain_index) {
            _WDL_ArenaBlock* last = arena->last_block;
            arena->last_block = arena->last_block->prev;
            _wdl_arena_block_dealloc(last, arena->desc.block_size);
            arena->chain_index--;
        }
    }

    if (arena->desc.virtual_memory) {
        _WDL_ArenaBlock* block = arena->last_block;
        u64 block_pos = arena->pos - arena->chain_index * arena->desc.block_size;
        // Add the size of a block since that also resides on the same allocated
        // memory region.
        u64 page_aligned_pos = _align_value(block_pos + sizeof(_WDL_ArenaBlock), wdl_os_get_page_size());
        if (page_aligned_pos < block->commit) {
            u64 unused_size = block->commit - page_aligned_pos;
            wdl_os_decommit_memory((void*) block + page_aligned_pos, unused_size);
            wdl_debug("Decommited page at boundry point %llu", page_aligned_pos);
            wdl_debug("%llu unused bytes", unused_size);
            block->commit = page_aligned_pos;
        }
    }
}

void wdl_arena_clear(WDL_Arena* arena) {
    wdl_arena_pop_to(arena, 0);
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
    for (i32 i = wdl_arrlen(ctx->scratch_arenas) - 1; i >= 0; i--) {
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
        for (u32 j = 0; j < wdl_arrlen(_wdl_thread_ctx->scratch_arenas); j++) {
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
    const char* const level_color[WDL_LOG_LEVEL_COUNT] = {
        "\033[101;30m",
        "\033[0;91m",
        "\033[0;93m",
        "\033[0;92m",
        "\033[0;94m",
        "\033[0;95m",
    };
    const char* const level_str[WDL_LOG_LEVEL_COUNT] = {
        "FATAL",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG",
        "TRACE",
    };

    if (_wdl_state.cfg.logging.colorful) {
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

    void* key;
    void* value;
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

        if (bucket_found) {
            return false;
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

void* _wdl_hash_map_getp_impl(WDL_HashMap* map, const void* key) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    u32 index = hash % map->desc.capacity;

    _WDL_HashMapBucket* bucket = &map->buckets[index];
    if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
        while (bucket != NULL) {
            if (map->desc.equal(key, bucket->key, map->desc.key_size)) {
                return bucket->value;
            }
            bucket = bucket->next;
        }
    }

    return NULL;
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

// Iteration

WDL_HashMapIter wdl_hm_iter_new(WDL_HashMap* map) {
    WDL_HashMapIter iter = {
        .map = map,
    };

    u32 idx = 0;
    _WDL_HashMapBucket* bucket;
    while (idx < map->desc.capacity) {
        bucket = &map->buckets[idx];
        if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
            break;
        }
        idx++;
    }

    iter.index = idx;
    iter.bucket = bucket;

    return iter;
}

b8 wdl_hm_iter_valid(WDL_HashMapIter iter) {
    return iter.index < iter.map->desc.capacity;
}

WDL_HashMapIter wdl_hm_iter_next(WDL_HashMapIter iter) {
    _WDL_HashMapBucket* bucket = iter.bucket;
    if (bucket->next != NULL) {
        iter.bucket = bucket->next;
        return iter;
    }

    u32 idx = iter.index + 1;
    while (idx < iter.map->desc.capacity) {
        bucket = &iter.map->buckets[idx];
        if (bucket->state == _WDL_HASH_MAP_BUCKET_STATE_ALIVE) {
            break;
        }
        idx++;
    }
    iter.index = idx;
    iter.bucket = bucket;

    return iter;
}

void* wdl_hm_iter_get_keyp(WDL_HashMapIter iter) {
    _WDL_HashMapBucket* bucket = iter.bucket;
    return bucket->key;
}

void* wdl_hm_iter_get_valuep(WDL_HashMapIter iter) {
    _WDL_HashMapBucket* bucket = iter.bucket;
    return bucket->value;
}

void _wdl_hm_iter_get_key_impl(WDL_HashMapIter iter, void* out_value) {
    _WDL_HashMapBucket* bucket = iter.bucket;
    memcpy(out_value, bucket->key, iter.map->desc.key_size);
}

void _wdl_hm_iter_get_value_impl(WDL_HashMapIter iter, void* out_value) {
    _WDL_HashMapBucket* bucket = iter.bucket;
    memcpy(out_value, bucket->value, iter.map->desc.value_size);
}

u64 wdl_hm_get_key_size(const WDL_HashMap* map) {
    return map->desc.key_size;
}

u64 wdl_hm_get_value_size(const WDL_HashMap* map) {
    return map->desc.value_size;
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
        .start_time = _wdl_posix_time_stamp(),
    };
    _wdl_state.platform = platform;

    return true;
}

b8 _wdl_platform_termiante(void) {
    wdl_os_release_memory(_wdl_state.platform, sizeof(_WDL_PlatformState));
    return true;
}

void* wdl_os_reserve_memory(u64 size) {
    void* ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr;
}

void  wdl_os_commit_memory(void* ptr, u64 size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void  wdl_os_decommit_memory(void* ptr, u64 size) {
    mprotect(ptr, size, PROT_NONE);
}

void  wdl_os_release_memory(void* ptr, u64 size) {
    munmap(ptr, size);
}

f32 wdl_os_get_time(void) {
    return _wdl_posix_time_stamp() - _wdl_state.platform->start_time;
}

u32 wdl_os_get_page_size(void) {
    return getpagesize();
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
