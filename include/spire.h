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
// Needed for clock_gettime
#define _POSIX_C_SOURCE 199309L
#include <unistd.h>
#ifdef _POSIX_VERSION
#define SP_POSIX
#endif // _POSIX_VERSION
#endif // SP_UNIX

#ifdef __GNUC__
#define SP_COMP_GCC
#endif // __GNUC__

#ifdef __clang__
#define SP_COMP_CLANG
#endif // __clang__

#ifdef _MSC_VER
#define SP_COMP_MSVC
#endif // _MSC_VER

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

#ifdef SP_COMP_MSVC
#define SP_THREAD_LOCAL __declspec(thread)
#define SP_INLINE __forceinline
#elif defined(SP_COMP_GCC)
#define SP_THREAD_LOCAL __thread
#define SP_INLINE static inline __attribute__((always_inline))
#else
#error "Unsupported compiler!"
#endif // SP_COMP_MSVC

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

// Must supply a message printf style formatted string as the first variadic
// argument.
#define sp_ensure(COND, ...) do { \
    if (!(COND)) {\
        sp_fatal(__VA_ARGS__); \
        abort(); \
    } \
} while (0)

// Must supply a message printf style formatted string as the first variadic
// argument.
#ifdef SP_DEBUG
#define sp_assert(COND, ...) sp_ensure(COND, __VA_ARGS__)
#else // SP_DEBUG
#define sp_assert(cond, ...)
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
SP_API SP_Str sp_str_substr(SP_Str source, u32 start, u32 end);

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

#define sp_fatal(...) _sp_log_internal(SP_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define sp_error(...) _sp_log_internal(SP_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define sp_warn(...) _sp_log_internal(SP_LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define sp_info(...) _sp_log_internal(SP_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define sp_debug(...) _sp_log_internal(SP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define sp_trace(...) _sp_log_internal(SP_LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

SP_API void _sp_log_internal(SP_LogLevel level, const char* file, u32 line, const char* msg, ...);

// -- Library ------------------------------------------------------------------

typedef struct SP_Lib SP_Lib;

typedef void (*SP_LibFunc)(void);

SP_API SP_Lib*    sp_lib_load(SP_Arena* arena, const char* filename);
SP_API void       sp_lib_unload(SP_Lib* lib);
SP_API SP_LibFunc sp_lib_func(SP_Lib* lib, const char* func_name);

// -- Math ---------------------------------------------------------------------

typedef union SP_Vec2 SP_Vec2;
union SP_Vec2 {
    struct {
        f32 x, y;
    };
    f32 elements[2];
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

typedef union SP_Ivec2 SP_Ivec2;
union SP_Ivec2 {
    struct {
        i32 x, y;
    };
    i32 elements[2];
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

typedef union SP_Vec4 SP_Vec4;
union SP_Vec4 {
    struct {
        f32 x, y, z, w;
    };
    f32 elements[4];
};

SP_INLINE SP_Vec4 sp_v4(f32 x, f32 y, f32 z, f32 w) { return (SP_Vec4) {x, y, z, w}; }
SP_INLINE SP_Vec4 sp_v4s(f32 scaler) { return (SP_Vec4) {scaler, scaler, scaler, scaler}; }

typedef union SP_Mat4 SP_Mat4;
union SP_Mat4 {
    struct {
        SP_Vec4 a, b, c, d;
    };
    f32 elements[16];
};

static const SP_Mat4 SP_M4_IDENTITY = {{
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
}};

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

    return (SP_Mat4) {{
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    }};
}

SP_INLINE SP_Mat4 sp_m4_inv_ortho_projection(f32 left, f32 right, f32 top, f32 bottom, f32 far, f32 near) {
    f32 x = (right - left) / 2.0f;
    f32 y = (top - bottom) / 2.0f;
    f32 z = (far - near) / -2.0f;

    f32 x_off = (left+right) / 2.0f;
    f32 y_off = (top+bottom) / 2.0f;
    f32 z_off = -(far+near) / 2.0f;

    return (SP_Mat4) {{
        {x, 0, 0, x_off},
        {0, y, 0, y_off},
        {0, 0, z, z_off},
        {0, 0, 0, 1},
    }};
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
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
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
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
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
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
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
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _sp_hash_map_getp_impl((MAP), &_key); \
    })

#define sp_hm_has(MAP, KEY) ({ \
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
        sp_assert(sizeof(__typeof__(KEY)) == sp_hm_get_key_size(MAP), \
                "Hash map key size does not match with the size of the provided key type. Expected %llu got %llu.", \
                sp_hm_get_key_size(MAP), \
                sizeof(__typeof__(KEY))); \
        __typeof__(KEY) _key = (KEY); \
        _sp_hash_map_has_impl((MAP), &_key); \
    })

#define sp_hm_remove(MAP, KEY, VALUE_TYPE) ({ \
        sp_assert((MAP) != NULL, "Hash map must not be NULL."); \
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

// -- Linked lists -------------------------------------------------------------

#define sp_null_set(p) ((p) = 0)
#define sp_null_check(p) ((p) == 0)

// Doubly linked list
#define sp_dll_insert(f, l, n, p) sp_dll_insert_npz(f, l, n, p, next, prev, sp_null_check, sp_null_set)
#define sp_dll_push_back(f, l, n) sp_dll_insert_npz(f, l, n, l, next, prev, sp_null_check, sp_null_set)
#define sp_dll_push_front(f, l, n) sp_dll_insert_npz(f, l, n, (__typeof__(n)) 0, next, prev, sp_null_check, sp_null_set)

#define sp_dll_remove(f, l, n) sp_dll_remove_npz(f, l, n, next, prev, sp_null_check, sp_null_set)
#define sp_dll_pop_back(f, l) sp_dll_remove_npz(f, l, l, next, prev, sp_null_check, sp_null_set)
#define sp_dll_pop_front(f, l) sp_dll_remove_npz(f, l, f, next, prev, sp_null_check, sp_null_set)

#define sp_dll_insert_npz(f, l, n, p, next, prev, zero_check, zero_set) do { \
    if (zero_check(f)) { \
        (f) = (l) = (n); \
        zero_set((n)->next); \
        zero_set((n)->prev); \
    } else { \
        if (zero_check(p)) { \
            (n)->next = (f); \
            zero_set((n)->prev); \
            (f)->prev = (n); \
            (f) = (n); \
        } else { \
            if (!zero_check((p)->next)) { \
                (p)->next->prev = (n); \
                (n)->next = (p)->next; \
            } \
            (n)->prev = (p); \
            (p)->next = (n); \
            if ((p) == (l)) { \
                (l) = (n); \
            } \
        } \
    } \
} while (0)
#define sp_dll_push_back_npz(f, l, n, next, prev, zero_check, zero_set) sp_dll_insert_npz(f, l, n, l, next, prev, zero_check, zero_set)
#define sp_dll_push_front_npz(f, l, n, next, prev, zero_check, zero_set) sp_dll_insert_npz(f, l, n, (__typeof__(n)) 0, next, prev, zero_check, zero_set)

#define sp_dll_remove_npz(f, l, n, next, prev, zero_check, zero_set) do { \
    if (!zero_check(f)) { \
        if ((f) == (l)) { \
            zero_set(f); \
            zero_set(l); \
        } else { \
            if (!zero_check((n)->next)) { \
                (n)->next->prev = (n)->prev; \
            } \
            if (!zero_check((n)->prev)) { \
                (n)->prev->next = (n)->next; \
            } \
            if ((n) == (f)) { \
                if (!zero_check((f)->next)) { \
                    (f)->next->prev = (f)->prev; \
                } \
                (f) = (f)->next; \
            } \
            if ((n) == (l)) { \
                if (!zero_check((l)->prev)) { \
                    (l)->prev->next = (l)->next; \
                } \
                (l) = (l)->prev; \
            } \
        } \
    } \
} while (0)
#define sp_dll_pop_back_npz(f, l, n, next, prev, zero_check, zero_set) sp_dll_remove_npz(f, l, l, next, prev, zero_check, zero_set)
#define sp_dll_pop_front_npz(f, l, n, next, prev, zero_check, zero_set) sp_dll_remove_npz(f, l, f, next, prev, zero_check, zero_set)

// Singly linked list
#define sp_sll_queue_push(f, l, n) sp_sll_queue_push_nz(f, l, n, next, sp_null_check, sp_null_set)
#define sp_sll_queue_pop(f, l) sp_sll_queue_pop_nz(f, l, next, sp_null_check, sp_null_set)

#define sp_sll_queue_push_nz(f, l, n, next, zero_check, zero_set) do { \
    if (zero_check(f)) { \
        (f) = (l) = (n); \
    } else { \
        (l)->next = (n); \
        (l) = (n); \
    } \
    (n)->next = NULL; \
} while (0)
#define sp_sll_queue_pop_nz(f, l, next, zero_check, zero_set) do { \
    if ((f) == (l)) { \
        zero_set(f); \
        zero_set(l); \
    } else { \
        (f) = (f)->next; \
    }\
} while (0)

#define sp_sll_stack_push(f, n) sp_sll_stack_push_nz(f, n, next, sp_null_check)
#define sp_sll_stack_pop(f) sp_sll_stack_pop_nz(f, next, sp_null_check)

#define sp_sll_stack_push_nz(f, n, next, zero_check) do { \
    if (!zero_check(f)) { \
        (n)->next = (f); \
    } \
    (f) = (n); \
} while (0)
#define sp_sll_stack_pop_nz(f, next, zero_check) do { \
    if (!zero_check(f)) { \
        (f) = (f)->next; \
    } \
} while (0)

// -- OS -----------------------------------------------------------------------

SP_API void* sp_os_reserve_memory(u64 size);
SP_API void  sp_os_commit_memory(void* ptr, u64 size);
SP_API void  sp_os_decommit_memory(void* ptr, u64 size);
SP_API void  sp_os_release_memory(void* ptr, u64 size);
SP_API f32   sp_os_get_time(void);
SP_API u32   sp_os_get_page_size(void);

#endif // SPIRE_H_
