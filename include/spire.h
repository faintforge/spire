// =============================================================================
//
// Spire is a core/utility library built around arena allocators.
//
// FEATURES:
// - Utils
// - Arena allocator
// - Length based strings
// - Logging
// - Math
// - Hash map
// - Linked list macros
// - Color
// - OS abstraction layer
//      - Memory management
//      - Time
//      - Page size
//      - Dynamic library
//
// =============================================================================

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

// =============================================================================

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

// =============================================================================
// UTILS
//
// The utility functions are just misc functions that don't fit into any section
// and don't deserve their own.
// =============================================================================

// Byte size conversions. These function in powers of two.
#define sp_kib(V) ((u64) (V) << 10)
#define sp_mib(V) ((u64) (V) << 20)
#define sp_gib(V) ((u64) (V) << 30)

// Returns the smaller of A and B.
#define sp_min(A, B) ((A) > (B) ? (B) : (A))

// Returns the larget of A and B.
#define sp_max(A, B) ((A) > (B) ? (A) : (B))

// Make it so V never becomes smaller than MIN and larger than MAX.
#define sp_clamp(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

// Length or stack allocated array.
// **DOES NOT WORK FOR HEAP ALLOCATED ARRAYS**
// Usage:
//      int arr[16];
//      sp_arrlen(arr);
#define sp_arrlen(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

// Byte offset of struct member.
#define sp_offset(S, M) ((u64) &((S*) 0)->M)

// Simple hashing function. Not the most efficient or causes the least
// collisions but it's small.
SP_API u64 sp_fvn1a_hash(const void* data, u64 len);

// Ensure some condition is always true. If it's not, print a message and
// segfault. A message must always be supplied as the first variadic argument.
#define sp_ensure(COND, ...) do { \
    if (!(COND)) {\
        sp_fatal(__VA_ARGS__); \
        abort(); \
    } \
} while (0)

#ifdef SP_DEBUG
// Same as sp_ensure but only runs in debug builds.
#define sp_assert(COND, ...) sp_ensure(COND, __VA_ARGS__)
#else // SP_DEBUG
// Same as sp_ensure but only runs in debug builds.
#define sp_assert(cond, ...)
#endif // SP_DEBUG

// =============================================================================
// ALLOCATOR INTERFACE
// =============================================================================

typedef void* (*SP_Alloc)(u64 size, void* userdata);
typedef void (*SP_Free)(void* ptr, u64 size, void* userdata);
typedef void* (*SP_Realloc)(void* ptr, u64 old_size, u64 new_size, void* userdata);

typedef struct SP_Allocator SP_Allocator;
struct SP_Allocator {
    SP_Alloc alloc;
    SP_Free free;
    SP_Realloc realloc;
    void* userdata;
};

SP_API SP_Allocator sp_libc_allocator(void);

#define sp_alloc(ALLOCATOR, SIZE) ((ALLOCATOR).alloc((SIZE), (ALLOCATOR).userdata))
#define sp_free(ALLOCATOR, PTR, SIZE) ((ALLOCATOR).free((PTR), (SIZE), (ALLOCATOR).userdata))
#define sp_realloc(ALLOCATOR, PTR, OLD_SIZE, NEW_SIZE) ((ALLOCATOR).realloc((PTR), (OLD_SIZE), (NEW_SIZE), (ALLOCATOR).userdata))

SP_API void* _sp_libc_alloc_stub(u64 size, void* userdata);
SP_API void _sp_libc_free_stub(void* ptr, u64 size, void* userdata);
SP_API void* _sp_libc_realloc_stub(void* ptr, u64 old_size, u64 new_size, void* userdata);

// =============================================================================
// STRING
//
// Length based strings. Any string function that creates a new string (one that
// takes a SP_Allocator as an argument) needs to have the 'data' field of the
// string manually freed.
// =============================================================================

typedef struct SP_Str SP_Str;
struct SP_Str {
    const u8* data;
    u32 len;
};

// Create a string from a string literal ("this kind of string").
#define sp_str_lit(STR_LIT) ((SP_Str) {(const u8*) (STR_LIT), sizeof(STR_LIT) - 1})

// Create a string from a null termianted string.
#define sp_cstr(CSTR) ((SP_Str) {(const u8*) (CSTR), sp_str_cstrlen((const u8*) (CSTR))})

SP_API SP_Str sp_str(const u8* data, u32 len);
SP_API b8 sp_str_equal(SP_Str a, SP_Str b);

// Convert a string into a null termianted string.
SP_API char* sp_str_to_cstr(SP_Allocator allocator, SP_Str str);

// Push a formatted string onto an arena. The 'fmt' arg is a printf style
// formatting string.
SP_API SP_Str sp_str_pushf(SP_Allocator allocator, const void* fmt, ...);

// Returns a substring of 'source'.
// Includes the 'start' index.
// Excludes the 'end' index.
SP_API SP_Str sp_str_substr(SP_Str source, u32 start, u32 end);

// Calculate the length of a null termianted string.
// This function only exists as to not bring in the 'string.h' header because of
// the 'sp_cstr' macro.
SP_API u32 sp_str_cstrlen(const u8* cstr);

// =============================================================================
// ARENA ALLOCATOR
//
// An arena allocator could be thought of as a stack. It's an allocation
// strategy similar to the stack.
//
// Learn more about arenas:
// - Article: https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
// - Talk: https://www.youtube.com/watch?v=TZ5a3gCCZYo
// =============================================================================

typedef struct SP_Arena SP_Arena;

// Create a default configured arena as specified when sp_init was called.
SP_API SP_Arena* sp_arena_create(void);
SP_API SP_Arena* sp_arena_create_configurable(SP_ArenaDesc desc);

// Free memory allocated by arena.
SP_API void sp_arena_destroy(SP_Arena* arena);

// Fit arena into a more generic allocator interface.
SP_API SP_Allocator sp_arena_allocator(SP_Arena* arena);

// Allocate 'size' bytes on the arena. If there isn't enough memory on the arena
// and 'chaining' or 'virtual_memory' is set at arena creation, the memory
// region will expand.
//
// Returns a pointer to some zero-initialized memory on the arena.
//
// This call will crash the application if:
// - 'block_size' is reached on a 'virtual_memory' arena
// - 'size' exceeds 'block_size'
// - arena runs out of memory in a non chained arena
SP_API void* sp_arena_push(SP_Arena* arena, u64 size);

// Same as 'sp_arena_push' but without zero-initialization on the memory
// returned.
SP_API void* sp_arena_push_no_zero(SP_Arena* arena, u64 size);

// Pop 'size' bytes off of the arena.
SP_API void sp_arena_pop(SP_Arena* arena, u64 size);

// Pop to a certain position in the arena.
SP_API void sp_arena_pop_to(SP_Arena* arena, u64 pos);

// Clear all memory pushed onto the arena.
SP_API void sp_arena_clear(SP_Arena* arena);

// Returns the position of the arena cursor. This also indicates how much memory
// is currently used. All arena information is stored in itself so this should
// never return 0.
SP_API u64 sp_arena_get_pos(const SP_Arena* arena);

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

// Gives an arena a 'tag' which makes arenas easier to recognize when debugging.
SP_API void sp_arena_tag(SP_Arena* arena, SP_Str tag);

// Get usage metrics of an arena.
SP_API SP_ArenaMetrics sp_arena_get_metrics(const SP_Arena* arena);

SP_API void* _sp_arena_alloc(u64 size, void* userdata);
SP_API void _sp_arena_free(void* ptr, u64 size, void* userdata);
SP_API void* _sp_arena_realloc(void* ptr, u64 old_size, u64 new_size, void* userdata);

// =============================================================================
// TEMPORARY ARENA
//
// A temporary arena is a snapshot of the current arena usage when 'begin' is
// called. When the temporary arena isn't needed, calling 'end' will pop off all
// the newly pushed bytes. This could be seen as a sort of stack.
// =============================================================================

typedef struct SP_Temp SP_Temp;
struct SP_Temp {
    SP_Arena* arena;
    u64 pos;
};

SP_API SP_Temp sp_temp_begin(SP_Arena* arena);
SP_API void    sp_temp_end(SP_Temp temp);

// =============================================================================
// THREAD CONTEXT
//
// The thread context provides per thread scratch arenas. These functions should
// rarely be used. They're used during initialization.
//
// Currently threading isn't implemented, so creating and setting a context per
// new thread needs to be done manually, for now.
// =============================================================================

typedef struct SP_ThreadCtx SP_ThreadCtx;

SP_API SP_ThreadCtx* sp_thread_ctx_create(void);
SP_API void          sp_thread_ctx_destroy(SP_ThreadCtx* ctx);
SP_API void          sp_thread_ctx_set(SP_ThreadCtx* ctx);

// =============================================================================
// SCRATCH ARENA
//
// Scratch arenas are per thread temporary arenas. They're useful when you need
// to dynamically allocate something *during* an operation, but not need it to
// be persistant.
// =============================================================================

typedef SP_Temp SP_Scratch;

SP_API SP_Scratch sp_scratch_begin(SP_Arena* const* conflicts, u32 count);
SP_API void       sp_scratch_end(SP_Scratch scratch);

// =============================================================================
// LOGGING
//
// This is a very bare bones logging system right now. It only supports printing
// to stdout with some extra nice information like file and line.
//
// Log format:
// TYPE file:line: message
// =============================================================================

// All macros work the same, only with a different log level. The first argument
// ALWAYS needs to be a printf style format string.
#define sp_fatal(...) _sp_log_internal(SP_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define sp_error(...) _sp_log_internal(SP_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define sp_warn(...) _sp_log_internal(SP_LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define sp_info(...) _sp_log_internal(SP_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define sp_debug(...) _sp_log_internal(SP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define sp_trace(...) _sp_log_internal(SP_LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

typedef enum SP_LogLevel {
    SP_LOG_LEVEL_FATAL,
    SP_LOG_LEVEL_ERROR,
    SP_LOG_LEVEL_WARN,
    SP_LOG_LEVEL_INFO,
    SP_LOG_LEVEL_DEBUG,
    SP_LOG_LEVEL_TRACE,

    SP_LOG_LEVEL_COUNT,
} SP_LogLevel;

// Internal function. You can call it manually if you want, it's just cumbersome
// to do so.
SP_API void _sp_log_internal(SP_LogLevel level, const char* file, u32 line, const char* msg, ...);

// =============================================================================
// MATH
//
// This part provides some nice linear algebra functions. It's very much NOT
// complete and is missing A LOT of features. Functions get added as they are
// needed which explains the strange function spread.
//
// The matrices are stored in row-major layout so they need transposing when
// using in OpenGL, which use a column-major layout.
// =============================================================================

typedef union SP_Vec2 SP_Vec2;
union SP_Vec2 {
    struct {
        f32 x, y;
    };
    f32 elements[2];
};

SP_INLINE SP_Vec2 sp_v2(f32 x, f32 y) { return (SP_Vec2) {{x, y}}; }
SP_INLINE SP_Vec2 sp_v2s(f32 scaler) { return (SP_Vec2) {{scaler, scaler}}; }

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

SP_INLINE SP_Ivec2 sp_iv2(i32 x, i32 y) { return (SP_Ivec2) {{x, y}}; }
SP_INLINE SP_Ivec2 sp_iv2s(i32 scaler) { return (SP_Ivec2) {{scaler, scaler}}; }

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

SP_INLINE SP_Vec4 sp_v4(f32 x, f32 y, f32 z, f32 w) { return (SP_Vec4) {{x, y, z, w}}; }
SP_INLINE SP_Vec4 sp_v4s(f32 scaler) { return (SP_Vec4) {{scaler, scaler, scaler, scaler}}; }

typedef union SP_Mat2 SP_Mat2;
union SP_Mat2 {
    struct {
        SP_Vec2 a, b;
    };
    f32 elements[4];
};

static const SP_Mat2 SP_M2_IDENTITY = {{
    {{1.0f, 0.0f}},
    {{0.0f, 1.0f}},
}};

SP_INLINE SP_Vec2 sp_m2_mul_vec(SP_Mat2 mat, SP_Vec2 vec) {
    return (SP_Vec2) {{
        mat.a.x * vec.x + mat.a.y * vec.y,
        mat.b.x * vec.x + mat.b.y * vec.y,
    }};
}

SP_INLINE SP_Mat2 sp_m2_rot(f32 radians) {
    // https://en.wikipedia.org/wiki/Rotation_matrix
    f32 cos = cosf(radians);
    f32 sin = sinf(radians);
    return (SP_Mat2) {{
        {{cos, -sin}},
        {{sin, cos}},
    }};
}

typedef union SP_Mat4 SP_Mat4;
union SP_Mat4 {
    struct {
        SP_Vec4 a, b, c, d;
    };
    f32 elements[16];
};

static const SP_Mat4 SP_M4_IDENTITY = {{
    {{1.0f, 0.0f, 0.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 0.0f, 1.0f}},
}};

SP_INLINE SP_Vec4 sp_m4_mul_vec(SP_Mat4 mat, SP_Vec4 vec) {
    return (SP_Vec4) {{
        vec.x*mat.a.x + vec.y*mat.a.y + vec.z*mat.a.z + vec.w*mat.a.w,
        vec.x*mat.b.x + vec.y*mat.b.y + vec.z*mat.b.z + vec.w*mat.b.w,
        vec.x*mat.c.x + vec.y*mat.c.y + vec.z*mat.c.z + vec.w*mat.c.w,
        vec.x*mat.d.x + vec.y*mat.d.y + vec.z*mat.d.z + vec.w*mat.d.w,
    }};
}

SP_INLINE SP_Mat4 sp_m4_mul_mat(SP_Mat4 left, SP_Mat4 right) {
    // u32 lmat_cols = 4;
    // u32 lmat_rows = 4;
    // u32 rmat_cols = 4;
    // printf("{{ ");
    // for (u32 y = 0; y < lmat_rows; y++) {
    //     for (u32 col = 0; col < rmat_cols; col++) {
    //         for (u32 x = 0; x < lmat_cols; x++) {
    //             printf("l[%d]*r[%d]", y*lmat_cols+x, x*rmat_cols+col);
    //             if (x < lmat_cols - 1) {
    //                 printf("+");
    //             }
    //         }
    //         if (col < rmat_cols - 1) {
    //             printf(", ");
    //         }
    //     }
    //     if (y < lmat_cols - 1) {
    //         printf(" }},\n{{ ");
    //     }
    // }
    // printf(" }},\n");

    const f32* l = left.elements;
    const f32* r = right.elements;
    return (SP_Mat4) {{
        {{ l[0]*r[0]+l[1]*r[4]+l[2]*r[8]+l[3]*r[12], l[0]*r[1]+l[1]*r[5]+l[2]*r[9]+l[3]*r[13], l[0]*r[2]+l[1]*r[6]+l[2]*r[10]+l[3]*r[14], l[0]*r[3]+l[1]*r[7]+l[2]*r[11]+l[3]*r[15] }},
        {{ l[4]*r[0]+l[5]*r[4]+l[6]*r[8]+l[7]*r[12], l[4]*r[1]+l[5]*r[5]+l[6]*r[9]+l[7]*r[13], l[4]*r[2]+l[5]*r[6]+l[6]*r[10]+l[7]*r[14], l[4]*r[3]+l[5]*r[7]+l[6]*r[11]+l[7]*r[15] }},
        {{ l[8]*r[0]+l[9]*r[4]+l[10]*r[8]+l[11]*r[12], l[8]*r[1]+l[9]*r[5]+l[10]*r[9]+l[11]*r[13], l[8]*r[2]+l[9]*r[6]+l[10]*r[10]+l[11]*r[14], l[8]*r[3]+l[9]*r[7]+l[10]*r[11]+l[11]*r[15] }},
        {{ l[12]*r[0]+l[13]*r[4]+l[14]*r[8]+l[15]*r[12], l[12]*r[1]+l[13]*r[5]+l[14]*r[9]+l[15]*r[13], l[12]*r[2]+l[13]*r[6]+l[14]*r[10]+l[15]*r[14], l[12]*r[3]+l[13]*r[7]+l[14]*r[11]+l[15]*r[15] }},
    }};
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
        {{x, 0, 0, x_off}},
        {{0, y, 0, y_off}},
        {{0, 0, z, z_off}},
        {{0, 0, 0, 1}},
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
        {{x, 0, 0, x_off}},
        {{0, y, 0, y_off}},
        {{0, 0, z, z_off}},
        {{0, 0, 0, 1}},
    }};
}

// =============================================================================
// HASH MAP
// =============================================================================

typedef enum HashCollisionResolution {
    SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING,
    SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING,
} SP_HashCollisionResolution;

typedef u64 (*SP_HashFunc)(const void* data, u64 len);
typedef b8 (*SP_EqualFunc)(const void* a, const void* b, u64 size);

typedef struct SP_HashMapDesc SP_HashMapDesc;
struct SP_HashMapDesc {
    SP_Allocator allocator;
    u32 capacity;
    SP_HashCollisionResolution collision_resolution;
    SP_HashFunc hash;
    SP_EqualFunc equal;

    u64 key_size;
    u64 value_size;
};

typedef struct SP_HashMap SP_HashMap;

SP_API SP_HashMap* sp_hash_map_create(SP_HashMapDesc desc);
SP_API void sp_hash_map_destroy(SP_HashMap* map);
SP_API b8 sp_hash_map_insert(SP_HashMap* map, const void* key, const void* value);
SP_API b8 sp_hash_map_set(SP_HashMap* map, const void* key, const void* value);
SP_API b8 sp_hash_map_remove(SP_HashMap* map, const void* key, void* out_value);
SP_API b8 sp_hash_map_get(SP_HashMap* map, const void* key, void* out_value);
SP_API void* sp_hash_map_getp(SP_HashMap* map, const void* key);

typedef struct SP_HashMapIter SP_HashMapIter;
struct SP_HashMapIter {
    SP_HashMap* map;
    void* node;
    u32 index;
};

SP_API SP_HashMapIter sp_hash_map_iter_init(SP_HashMap* map);
SP_API b8 sp_hash_map_iter_valid(SP_HashMapIter iter);
SP_API SP_HashMapIter sp_hash_map_iter_next(SP_HashMapIter iter);
SP_API void sp_hash_map_iter_get_key(SP_HashMapIter iter, void* out_key);
SP_API void sp_hash_map_iter_get_value(SP_HashMapIter iter, void* out_value);
SP_API void* sp_hash_map_iter_get_valuep(SP_HashMapIter iter);

// Helper functions
SP_API u64 sp_hash_map_helper_hash_str(const void* key, u64 size);
SP_API b8  sp_hash_map_helper_equal_str(const void* a, const void* b, u64 len);
SP_API b8  sp_hash_map_helper_equal_generic(const void* a, const void* b, u64 len);

#define sp_hash_map_desc_generic(ALLOCATOR, CAPACITY, COLLISION_RESOLUTION, KEY_TYPE, VALUE_TYPE) ((SP_HashMapDesc) { \
        .allocator = (ALLOCATOR), \
        .capacity = (CAPACITY), \
        .collision_resolution = (COLLISION_RESOLUTION), \
        .hash = sp_fvn1a_hash, \
        .equal = sp_hash_map_helper_equal_generic, \
        .key_size = sizeof(KEY_TYPE), \
        .value_size = sizeof(VALUE_TYPE), \
    })

#define sp_hash_map_desc_str(ARENA, CAPACITY, COLLISION_RESOLUTION, VALUE_TYPE) ((SP_HashMapDesc) { \
        .allocator = (ARENA), \
        .capacity = (CAPACITY), \
        .collision_resolution = (COLLISION_RESOLUTION), \
        .hash = sp_hash_map_helper_hash_str, \
        .equal = sp_hash_map_helper_equal_str, \
        .key_size = sizeof(SP_Str), \
        .value_size = sizeof(VALUE_TYPE), \
    })

// =============================================================================
// LINKED LISTS
//
// A set of macros to help with:
// - Doubly linked lists
// - Queue (Singly linked list)
// - Stack (Singly linked list)
//
// All these macros expect a struct with a next, and prev if it's a doubly
// linked list macro, but this can be changed by using the *_npz or *_nz macros.
//
// Usage:
// typedef struct Node Node;
// struct Node {
//     Node* next;
//     i32 value;
// };
// ...
// Node* first = NULL;
// Node* last = NULL;
// Node node = {.value = 32};
// sp_sll_queue_push(first, last, &node);
// =============================================================================

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
    (n)->next = (f); \
    (f) = (n); \
} while (0)
#define sp_sll_stack_pop_nz(f, next, zero_check) do { \
    if (!zero_check(f)) { \
        (f) = (f)->next; \
    } \
} while (0)

// =============================================================================
// COLOR
//
// RGBA color stored in a float in the range of [0-1].
// =============================================================================

typedef struct SP_Color SP_Color;
struct SP_Color {
    f32 r, g, b, a;
};

SP_API SP_Color sp_color_rgba_f(f32 r, f32 g, f32 b, f32 a);
SP_API SP_Color sp_color_rgb_f(f32 r, f32 g, f32 b);

// Convert [0-255] ints into a [0-1] float range color.
SP_API SP_Color sp_color_rgba_i(u8 r, u8 g, u8 b, u8 a);
SP_API SP_Color sp_color_rgb_i(u8 r, u8 g, u8 b);

// Convert a single hex number, with RGBA channels, into a color (0x112233ff).
SP_API SP_Color sp_color_rgba_hex(u32 hex);
// Convert a single hex number, with RGB channels, into a color (0x112233).
SP_API SP_Color sp_color_rgb_hex(u32 hex);

SP_API SP_Color sp_color_hsl(f32 hue, f32 saturation, f32 lightness);
SP_API SP_Color sp_color_hsv(f32 hue, f32 saturation, f32 value);

// Expand a color into its parts. Useful for functions like glClearColor().
// Usage:
// glClearColor(sp_color_arg(SP_COLOR_BLACK));
#define sp_color_arg(color) (color).r, (color).g, (color).b, (color).a

#define SP_COLOR_WHITE ((SP_Color) {1.0f, 1.0f, 1.0f, 1.0f})
#define SP_COLOR_BLACK ((SP_Color) {0.0f, 0.0f, 0.0f, 1.0f})
#define SP_COLOR_RED ((SP_Color) {1.0f, 0.0f, 0.0f, 1.0f})
#define SP_COLOR_GREEN ((SP_Color) {0.0f, 1.0f, 0.0f, 1.0f})
#define SP_COLOR_BLUE ((SP_Color) {0.0f, 0.0f, 1.0f, 1.0f})
#define SP_COLOR_TRANSPARENT ((SP_Color) {0.0f, 0.0f, 0.0f, 0.0f})

// =============================================================================
// TESTING
//
// The testing framework works with suites, groups and tests. A suite contains
// groups, and a group contains tests.
//
// When running a suite all groups will be run in sequential order.
// =============================================================================

typedef struct SP_TestResult SP_TestResult;
struct SP_TestResult {
    b8 successful;
    const char* file;
    u32 line;
    const char* reason;
};

typedef SP_TestResult (*SP_TestFunc)(void* userdata);

typedef struct SP_TestSuite SP_TestSuite;

extern SP_TestSuite* sp_test_suite_create(SP_Allocator allocator);
extern void sp_test_suite_destroy(SP_TestSuite* suite);
extern void sp_test_suite_run(SP_TestSuite* suite);
extern u32 sp_test_group_register(SP_TestSuite* suite, SP_Str name);
#define sp_test_register(SUITE, GROUP, FUNC, USERDATA) _sp_test_register(SUITE, GROUP, FUNC, sp_str_lit(#FUNC), USERDATA)

#define sp_test_assert(COND) do { \
    if (!(COND)) { \
        return (SP_TestResult) { \
            .successful = false, \
            .file = __FILE__, \
            .line = __LINE__, \
            .reason = #COND, \
        }; \
    } \
} while (0)
#define sp_test_success() return (SP_TestResult) { .successful = true, }

extern void _sp_test_register(SP_TestSuite* suite, u32 group, SP_TestFunc func, SP_Str name, void* userdata);

// =============================================================================
// OS
//
// The OS abstraction layer.
// =============================================================================

SP_API void* sp_os_reserve_memory(u64 size);
SP_API void  sp_os_commit_memory(void* ptr, u64 size);
SP_API void  sp_os_decommit_memory(void* ptr, u64 size);
SP_API void  sp_os_release_memory(void* ptr, u64 size);
SP_API u32   sp_os_get_page_size(void);

// Get time in seconds since initialization.
SP_API f32 sp_os_get_time(void);

// =============================================================================
// DYNAMIC LIBRARY
//
// Load dynamic libraries and their functions.
// =============================================================================

typedef struct SP_Lib SP_Lib;

typedef void (*SP_LibFunc)(void);

SP_API SP_Lib*    sp_lib_load(SP_Arena* arena, const char* filename);
SP_API void       sp_lib_unload(SP_Lib* lib);
SP_API SP_LibFunc sp_lib_func(SP_Lib* lib, const char* func_name);

#endif // SPIRE_H_
