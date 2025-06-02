#include "spire.h"

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
            config.default_arena_desc.block_size = sp_gib(4);
        } else {
            config.default_arena_desc.block_size = sp_mib(4);
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

// -- Allocator interface ------------------------------------------------------

SP_Allocator sp_libc_allocator(void) {
    return (SP_Allocator) {
        .alloc = _sp_libc_alloc_stub,
        .free = _sp_libc_free_stub,
        .realloc = _sp_libc_realloc_stub,
        .userdata = NULL,
    };
}

void* _sp_libc_alloc_stub(u64 size, void* userdata) {
    (void) userdata;
    return malloc(size);
}

void _sp_libc_free_stub(void* ptr, u64 size, void* userdata) {
    (void) userdata;
    (void) size;
    free(ptr);
}

void* _sp_libc_realloc_stub(void* ptr, u64 old_size, u64 new_size, void* userdata) {
    (void) userdata;
    (void) old_size;
    return realloc(ptr, new_size);
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

SP_Allocator sp_arena_allocator(SP_Arena* arena) {
    return (SP_Allocator) {
        .alloc = _sp_arena_alloc,
        .free = _sp_arena_free,
        .realloc = _sp_arena_realloc,
        .userdata = arena,
    };
}

void* sp_arena_push(SP_Arena* arena, u64 size) {
    u8* ptr = sp_arena_push_no_zero(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void* sp_arena_push_no_zero(SP_Arena* arena, u64 size) {
    u64 start_pos = arena->pos;

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

void* _sp_arena_alloc(u64 size, void* userdata) {
    return sp_arena_push_no_zero(userdata, size);
}

void _sp_arena_free(void* ptr, u64 size, void* userdata) {
    SP_Arena* arena = userdata;
    _SP_ArenaBlock* block = arena->last_block;
    u64 block_pos = arena->pos - arena->chain_index * arena->desc.block_size;
    sp_ensure(block_pos > size, "Arena free larger than previously allocated size.");
    u64 aligned_size = _align_value(size, arena->desc.alignment);
    if (ptr == block->memory + block_pos - aligned_size) {
        sp_arena_pop(arena, aligned_size);
    }
}

void* _sp_arena_realloc(void* ptr, u64 old_size, u64 new_size, void* userdata) {
    _sp_arena_free(ptr, old_size, userdata);
    return _sp_arena_alloc(new_size, userdata);
}

// -- Temporary arena ----------------------------------------------------------

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

SP_THREAD_LOCAL SP_ThreadCtx* _sp_thread_ctx = NULL;

SP_ThreadCtx* sp_thread_ctx_create(void) {
    SP_Arena* scratch_arenas[SCRATCH_ARENA_COUNT] = {0};
    for (u32 i = 0; i < SCRATCH_ARENA_COUNT; i++) {
        scratch_arenas[i] = sp_arena_create();
        sp_arena_tag(scratch_arenas[i], sp_str_pushf(sp_arena_allocator(scratch_arenas[i]), "scratch-%u", i));
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
        printf("%s%s\033[0;90m %s:%u: \033[0m", level_color[level], level_str[level], file, line);
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

char* sp_str_to_cstr(SP_Allocator allocator, SP_Str str)  {
    char* cstr = allocator.alloc(str.len + 1, allocator.userdata);
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

SP_Str sp_str_pushf(SP_Allocator allocator, const void* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list len_args;
    va_copy(len_args, args);
    u64 len = vsnprintf(NULL, 0, fmt, len_args);
    va_end(len_args);

    u8* buffer = allocator.alloc(len + 1, allocator.userdata);
    vsnprintf((char*) buffer, len + 1, fmt, args);
    va_end(args);
    buffer = allocator.realloc(buffer, len, len, allocator.userdata);
    return sp_str(buffer, len);
}

SP_Str sp_str_substr(SP_Str source, u32 start, u32 end) {
    sp_assert(end >= start, "Substring end must come after start.");
    return sp_str(source.data + start, end - start);
}

// -- Hash map -----------------------------------------------------------------

// =============================================================================
// GENERIC BACKEND
// =============================================================================

enum {
    _SP_HASH_BUCKET_STATE_EMPTY = 0,
    _SP_HASH_BUCKET_STATE_ALIVE = 1,
    _SP_HASH_BUCKET_STATE_DEAD = 2,
    _SP_HASH_BUCKET_STATE_MAP = 3,
};

// https://en.wikipedia.org/wiki/Quadratic_probing#Quadratic_function
static inline u32 _sp_hash_probe(u64 hash, u32 i, u32 m) {
    const f32 c1 = 0.5f;
    // Set c2 to 0 for a linear probe.
    const f32 c2 = 0.5f;
    u32 offset = c1*i + c2*i*i;
    return (u64) (hash + offset) % m;
}

static u32 _sp_hash_container_get_index(
        const void* key,
        u64 key_size,
        u64 hash,
        const u8* states,
        const u64* hashes,
        const void* keys,
        b8 accept_dead,
        u32 count,
        SP_EqualFunc equal_func
    ) {
    for (u32 i = 0; i < count; i++) {
        u32 index = _sp_hash_probe(hash, i, count);
        if (states[index] == _SP_HASH_BUCKET_STATE_EMPTY ||
            (states[index] == _SP_HASH_BUCKET_STATE_DEAD && accept_dead) ||
            (states[index] == _SP_HASH_BUCKET_STATE_ALIVE &&
            hash == hashes[index] &&
            equal_func(key, (u8*) keys + index * key_size, key_size))) {
            return index;
        }
    }
    return ~0u;
}

typedef struct _SP_HashContainerChainNode _SP_HashContainerChainNode;
struct _SP_HashContainerChainNode {
    _SP_HashContainerChainNode* next;
    _SP_HashContainerChainNode* prev;
    u8 state;
    u64 hash;
};

typedef struct _SP_HashContainerGetNodeResult _SP_HashContainerGetNodeResult;
struct _SP_HashContainerGetNodeResult {
    b8 new_key;
    _SP_HashContainerChainNode* node;
};

static _SP_HashContainerGetNodeResult _sp_hash_container_get_node(
        const void* key,
        u64 key_size,
        u64 hash,
        _SP_HashContainerChainNode* nodes,
        u64 node_size,
        u32 count,
        SP_EqualFunc equal_func,
        b8 create_new,
        _SP_HashContainerChainNode** free_list,
        SP_Allocator allocator) {
    u32 index = hash % count;
    _SP_HashContainerChainNode* node = (_SP_HashContainerChainNode*) ((u8*) nodes + node_size * index);
    while (node != NULL) {
        if (node->state == _SP_HASH_BUCKET_STATE_EMPTY) {
            return (_SP_HashContainerGetNodeResult) {
                .new_key = true,
                .node = node,
            };
        }

        if (node->state == _SP_HASH_BUCKET_STATE_ALIVE &&
            hash == node->hash &&
            equal_func(key, &node[1], key_size)) {
            return (_SP_HashContainerGetNodeResult) {
                .new_key = false,
                .node = node,
            };
        }

        if (!create_new && node->next == NULL) {
            return (_SP_HashContainerGetNodeResult) {
                .new_key = true,
                .node = NULL,
            };
        }

        if (create_new && node->next == NULL) {
            _SP_HashContainerChainNode* new_node;
            if (*free_list != NULL) {
                new_node = *free_list;
                *free_list = (*free_list)->next;
            } else {
                new_node = sp_alloc(allocator, node_size);
            }
            memset(new_node, 0, node_size);
            *new_node = (_SP_HashContainerChainNode) {
                .next = NULL,
                .prev = node,
                .state = _SP_HASH_BUCKET_STATE_ALIVE,
                .hash = hash,
            };
            node->next = new_node;
            return (_SP_HashContainerGetNodeResult) {
                .new_key = true,
                .node = new_node,
            };
        }

        node = node->next;
    }

    sp_ensure(false, "Unreachable!");
    return (_SP_HashContainerGetNodeResult) {0};
}

// =============================================================================
// HASH MAP
// =============================================================================

struct SP_HashMap {
    SP_HashMapDesc desc;
    u32 capacity;
    union {
        // Struct of Arrays
        // Used for open addressing
        struct {
            u8* state;
            u64* hashes;
            void* keys;
            void* values;
            u32 count;
        } soa;
        // Array of Structs
        // Used for separate chaining
        struct {
            _SP_HashContainerChainNode* nodes;
            _SP_HashContainerChainNode* free_list;
        } aos;
    };
};

static void _sp_hash_map_grow(SP_HashMap* map) {
    const f32 GROWTH_FACTOR = 2.0f;
    u32 new_cap = map->capacity * GROWTH_FACTOR;

    u8* new_state = sp_alloc(map->desc.allocator, new_cap * sizeof(u8));
    u64* new_hashes = sp_alloc(map->desc.allocator, new_cap * sizeof(u64));
    void* new_keys = sp_alloc(map->desc.allocator, new_cap * map->desc.key_size);
    void* new_values = sp_alloc(map->desc.allocator, new_cap * map->desc.value_size);
    memset(new_state, 0, new_cap * sizeof(u8));

    for (u32 i = 0; i < map->capacity; i++) {
        if (map->soa.state[i] == _SP_HASH_BUCKET_STATE_ALIVE) {
            const void* key = (u8*) map->soa.keys + map->desc.key_size * i;
            u32 new_index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    map->soa.hashes[i],
                    new_state,
                    new_hashes,
                    new_keys,
                    true,
                    new_cap,
                    map->desc.equal);
            u64 value_size = map->desc.value_size;
            u64 key_size = map->desc.key_size;

            new_state[new_index] = _SP_HASH_BUCKET_STATE_ALIVE;
            new_hashes[new_index] = map->soa.hashes[i];
            memcpy((u8*) new_keys + key_size * new_index,
                    (u8*) map->soa.keys + key_size * i,
                    key_size);
            memcpy((u8*) new_values + value_size * new_index,
                    (u8*) map->soa.values + value_size * i,
                    value_size);
        }
    }

    sp_free(map->desc.allocator, map->soa.state, map->capacity * sizeof(u8));
    sp_free(map->desc.allocator, map->soa.hashes, map->capacity * sizeof(u64));
    sp_free(map->desc.allocator, map->soa.keys, map->capacity * map->desc.key_size);
    sp_free(map->desc.allocator, map->soa.values, map->capacity * map->desc.value_size);

    map->soa.state = new_state;
    map->soa.hashes = new_hashes;
    map->soa.keys = new_keys;
    map->soa.values = new_values;
    map->capacity = new_cap;
}

static inline _SP_HashContainerChainNode* _sp_hash_map_get_node(SP_HashMap* map, u32 index) {
    u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
    return (_SP_HashContainerChainNode*) ((u8*) map->aos.nodes + node_size * index);
}

SP_HashMap* sp_hash_map_create(SP_HashMapDesc desc) {
    SP_HashMap* map = sp_alloc(desc.allocator, sizeof(SP_HashMap));

    u32 cap = desc.capacity;
    switch (desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            *map = (SP_HashMap) {
                .desc = desc,
                .capacity = cap,
                .soa = {
                    .state = sp_alloc(desc.allocator, cap * sizeof(u8)),
                    .hashes = sp_alloc(desc.allocator, cap * sizeof(u64)),
                    .keys = sp_alloc(desc.allocator, cap * desc.key_size),
                    .values = sp_alloc(desc.allocator, cap * desc.value_size),
                }
            };
            memset(map->soa.state, 0, cap * sizeof(u8));
            break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + desc.key_size + desc.value_size;
            *map = (SP_HashMap) {
                .desc = desc,
                .capacity = cap,
                .aos = {
                    .nodes = sp_alloc(desc.allocator, cap * node_size),
                    .free_list = NULL,
                },
            };
            memset(map->aos.nodes, 0, cap * node_size);
        } break;
    }

    return map;
}

void sp_hash_map_destroy(SP_HashMap* map) {
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            sp_free(map->desc.allocator, map->soa.state, map->capacity * sizeof(u8));
            sp_free(map->desc.allocator, map->soa.hashes, map->capacity * sizeof(u64));
            sp_free(map->desc.allocator, map->soa.keys, map->capacity * map->desc.key_size);
            sp_free(map->desc.allocator, map->soa.values, map->capacity * map->desc.value_size);
            break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            for (u32 i = 0; i < map->capacity; i++) {
                _SP_HashContainerChainNode* node = _sp_hash_map_get_node(map, i);
                while (node != NULL) {
                    sp_free(map->desc.allocator, node, node_size);
                    node = node->next;
                }
            }

            _SP_HashContainerChainNode* node = map->aos.free_list;
            while (node != NULL) {
                sp_free(map->desc.allocator, node, node_size);
                node = node->next;
            }

            sp_free(map->desc.allocator, map->aos.nodes, map->capacity * node_size);
        } break;
    }
    *map = (SP_HashMap) {0};
}

b8 sp_hash_map_insert(SP_HashMap* map, const void* key, const void* value) {
    const f32 LOAD_FACTOR = 0.75f;
    u64 hash = map->desc.hash(key, map->desc.key_size);
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING: {
            u32 index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    hash,
                    map->soa.state,
                    map->soa.hashes,
                    map->soa.keys,
                    true,
                    map->capacity,
                    map->desc.equal);
            u8 state = map->soa.state[index];
            if (state == _SP_HASH_BUCKET_STATE_ALIVE) {
                return false;
            }

            if (map->soa.count == map->capacity * LOAD_FACTOR) {
                _sp_hash_map_grow(map);
                index = _sp_hash_container_get_index(key,
                        map->desc.key_size,
                        hash,
                        map->soa.state,
                        map->soa.hashes,
                        map->soa.keys,
                        true,
                        map->capacity,
                        map->desc.equal);
            }

            if (state == _SP_HASH_BUCKET_STATE_EMPTY) {
                map->soa.count++;
            }

            u64 key_size = map->desc.key_size;
            void* stored_key = (u8*) map->soa.keys + key_size * index;
            map->soa.state[index] = _SP_HASH_BUCKET_STATE_ALIVE;
            map->soa.hashes[index] = hash;
            u64 value_size = map->desc.value_size;
            memcpy(stored_key, key, key_size);
            memcpy((u8*) map->soa.values + value_size * index, value, value_size);

            return true;
        }
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            _SP_HashContainerGetNodeResult result = _sp_hash_container_get_node(key,
                    map->desc.key_size,
                    hash,
                    map->aos.nodes,
                    node_size,
                    map->capacity,
                    map->desc.equal,
                    true,
                    &map->aos.free_list,
                    map->desc.allocator);
            if (result.new_key) {
                result.node->state = _SP_HASH_BUCKET_STATE_ALIVE;
                result.node->hash = hash;
                void* node_key = &result.node[1];
                void* node_value = (u8*) &result.node[1] + map->desc.key_size;
                memcpy(node_key, key, map->desc.key_size);
                memcpy(node_value, value, map->desc.value_size);
                return true;
            }
            return false;
        }
    }

    sp_assert(false, "Unreachable!");
    return false;
}

b8 sp_hash_map_set(SP_HashMap* map, const void* key, const void* value) {
    const f32 LOAD_FACTOR = 0.75f;
    u64 hash = map->desc.hash(key, map->desc.key_size);
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING: {
            if (map->soa.count == map->capacity * LOAD_FACTOR) {
                _sp_hash_map_grow(map);
            }

            u32 index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    hash,
                    map->soa.state,
                    map->soa.hashes,
                    map->soa.keys,
                    true,
                    map->capacity,
                    map->desc.equal);

            u64 key_size = map->desc.key_size;
            void* stored_key = (u8*) map->soa.keys + key_size * index;
            u8 new_key = map->soa.state[index] == _SP_HASH_BUCKET_STATE_EMPTY;
            map->soa.state[index] = _SP_HASH_BUCKET_STATE_ALIVE;
            map->soa.hashes[index] = hash;
            u64 value_size = map->desc.value_size;
            memcpy(stored_key, key, key_size);
            memcpy((u8*) map->soa.values + value_size * index, value, value_size);
            if (new_key) {
                map->soa.count++;
            }

            return new_key;
        }
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            _SP_HashContainerGetNodeResult result = _sp_hash_container_get_node(key,
                    map->desc.key_size,
                    hash,
                    map->aos.nodes,
                    node_size,
                    map->capacity,
                    map->desc.equal,
                    true,
                    &map->aos.free_list,
                    map->desc.allocator);
            result.node->state = _SP_HASH_BUCKET_STATE_ALIVE;
            result.node->hash = hash;
            void* node_key = &result.node[1];
            void* node_value = (u8*) &result.node[1] + map->desc.key_size;
            memcpy(node_key, key, map->desc.key_size);
            memcpy(node_value, value, map->desc.value_size);
            return result.new_key;
        }
    }

    sp_assert(false, "Unreachable!");
    return false;
}

b8 sp_hash_map_remove(SP_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING: {
            u32 index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    hash,
                    map->soa.state,
                    map->soa.hashes,
                    map->soa.keys,
                    false,
                    map->capacity,
                    map->desc.equal);
            if (map->soa.state[index] == _SP_HASH_BUCKET_STATE_ALIVE) {
                map->soa.state[index] = _SP_HASH_BUCKET_STATE_DEAD;
                if (out_value != NULL) {
                    u64 value_size = map->desc.value_size;
                    memcpy(out_value,
                        (u8*) map->soa.values + index * value_size,
                        value_size);
                }
                return true;
            }
            return false;
        }
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            _SP_HashContainerGetNodeResult result = _sp_hash_container_get_node(key,
                    map->desc.key_size,
                    hash,
                    map->aos.nodes,
                    node_size,
                    map->capacity,
                    map->desc.equal,
                    false,
                    &map->aos.free_list,
                    map->desc.allocator);
            if (result.new_key) {
                return false;
            }

            _SP_HashContainerChainNode* node = result.node;
            if (out_value != NULL) {
                void* node_value = (u8*) &result.node[1] + map->desc.key_size;
                memcpy(out_value,
                    node_value,
                    map->desc.value_size);
            }

            // Root node
            if (node->prev == NULL) {
                if (node->next == NULL) {
                    node->state = _SP_HASH_BUCKET_STATE_EMPTY;
                } else {
                    _SP_HashContainerChainNode* free_node = node->next;
                    memcpy(node, node->next, node_size);
                    node->prev = NULL;
                    if (node->next != NULL) {
                        node->next->prev = node;
                    }

                    free_node->next = map->aos.free_list;
                    map->aos.free_list = free_node;
                }
            } else {
                node->prev->next = node->next;
                if (node->next != NULL) {
                    node->next->prev = node->prev;
                }

                node->next = map->aos.free_list;
                map->aos.free_list = node;
            }

            return true;
        }
    }

    sp_assert(false, "Unreachable!");
    return false;
}

b8 sp_hash_map_get(SP_HashMap* map, const void* key, void* out_value) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING: {
            u32 index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    hash,
                    map->soa.state,
                    map->soa.hashes,
                    map->soa.keys,
                    false,
                    map->capacity,
                    map->desc.equal);
            if (index == ~0u) {
                return false;
            }

            if (map->soa.state[index] == _SP_HASH_BUCKET_STATE_ALIVE) {
                if (out_value != NULL) {
                    u64 value_size = map->desc.value_size;
                    memcpy(out_value,
                        (u8*) map->soa.values + index * value_size,
                        value_size);
                }
                return true;
            }
            return false;
        }
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            _SP_HashContainerGetNodeResult result = _sp_hash_container_get_node(key,
                    map->desc.key_size,
                    hash,
                    map->aos.nodes,
                    node_size,
                    map->capacity,
                    map->desc.equal,
                    false,
                    &map->aos.free_list,
                    map->desc.allocator);
            if (!result.new_key) {
                if (out_value != NULL) {
                    void* node_value = (u8*) &result.node[1] + map->desc.key_size;
                    memcpy(out_value,
                        node_value,
                        map->desc.value_size);
                }
                return true;
            }
            return false;
        }
    }

    sp_assert(false, "Unreachable!");
    return false;
}

void* sp_hash_map_getp(SP_HashMap* map, const void* key) {
    u64 hash = map->desc.hash(key, map->desc.key_size);
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING: {
            u32 index = _sp_hash_container_get_index(key,
                    map->desc.key_size,
                    hash,
                    map->soa.state,
                    map->soa.hashes,
                    map->soa.keys,
                    false,
                    map->capacity,
                    map->desc.equal);
            if (index == ~0u) {
                return false;
            }

            if (map->soa.state[index] == _SP_HASH_BUCKET_STATE_ALIVE) {
                u64 value_size = map->desc.value_size;
                return (u8*) map->soa.values + index * value_size;
            }
            return false;
        }
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            u64 node_size = sizeof(_SP_HashContainerChainNode) + map->desc.key_size + map->desc.value_size;
            _SP_HashContainerGetNodeResult result = _sp_hash_container_get_node(key,
                    map->desc.key_size,
                    hash,
                    map->aos.nodes,
                    node_size,
                    map->capacity,
                    map->desc.equal,
                    false,
                    &map->aos.free_list,
                    map->desc.allocator);
            if (!result.new_key) {
                return (u8*) &result.node[1] + map->desc.key_size;
            }
            return NULL;
        }
    }

    sp_assert(false, "Unreachable!");
    return NULL;
}

// Iteration
SP_HashMapIter sp_hash_map_iter_init(SP_HashMap* map) {
    SP_HashMapIter iter = {
        .map = map,
        .index = ~0u,
        .node = NULL,
    };

    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            for (u32 i = 0; i < map->capacity; i++) {
                if (map->soa.state[i] == _SP_HASH_BUCKET_STATE_ALIVE) {
                    iter.index = i;
                    break;
                }
            }
            break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING:
            for (u32 i = 0; i < map->capacity; i++) {
                _SP_HashContainerChainNode* node = _sp_hash_map_get_node(map, i);
                if (node->state == _SP_HASH_BUCKET_STATE_ALIVE) {
                    iter.index = i;
                    iter.node = node;
                    break;
                }
            }
            break;
    }
    return iter;
}

b8 sp_hash_map_iter_valid(SP_HashMapIter iter) {
    return iter.map != NULL && iter.index < iter.map->capacity;
}

SP_HashMapIter sp_hash_map_iter_next(SP_HashMapIter iter) {
    sp_assert(sp_hash_map_iter_valid(iter), "Hash map iterator must be valid!");

    SP_HashMap* map = iter.map;
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            for (u32 i = iter.index + 1; i < map->capacity; i++) {
                if (map->soa.state[i] == _SP_HASH_BUCKET_STATE_ALIVE) {
                    iter.index = i;
                    return iter;
                }
            }
        break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            _SP_HashContainerChainNode* node = iter.node;
            if (node->next != NULL) {
                iter.node = node->next;
                return iter;
            }

            for (u32 i = iter.index + 1; i < map->capacity; i++) {
                _SP_HashContainerChainNode* node = _sp_hash_map_get_node(map, i);
                if (node->state == _SP_HASH_BUCKET_STATE_ALIVE) {
                    iter.index = i;
                    iter.node = node;
                    return iter;
                }
            }
        } break;
    }
    return (SP_HashMapIter) {0};
}

void sp_hash_map_iter_get_key(SP_HashMapIter iter, void* out_key) {
    sp_assert(sp_hash_map_iter_valid(iter), "Hash map iterator must be valid!");

    SP_HashMap* map = iter.map;
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            memcpy(out_key, (u8*) map->soa.keys + iter.index * map->desc.key_size, map->desc.key_size);
            break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            _SP_HashContainerChainNode* node = iter.node;
            memcpy(out_key, &node[1], map->desc.key_size);
        } break;
    }
}

void sp_hash_map_iter_get_value(SP_HashMapIter iter, void* out_value) {
    sp_assert(sp_hash_map_iter_valid(iter), "Hash map iterator must be valid!");

    SP_HashMap* map = iter.map;
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            memcpy(out_value, (u8*) map->soa.values + iter.index * map->desc.value_size, map->desc.value_size);
            break;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            _SP_HashContainerChainNode* node = iter.node;
            memcpy(out_value, (u8*) &node[1] + map->desc.key_size, map->desc.value_size);
        } break;
    }
}

void* sp_hash_map_iter_get_valuep(SP_HashMapIter iter) {
    sp_assert(sp_hash_map_iter_valid(iter), "Hash map iterator must be valid!");

    SP_HashMap* map = iter.map;
    switch (map->desc.collision_resolution) {
        case SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING:
            return (u8*) map->soa.values + iter.index * map->desc.value_size;
        case SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING: {
            _SP_HashContainerChainNode* node = iter.node;
            return (u8*) &node[1] + map->desc.key_size;
        }
    }

    return NULL;
}

// Helper functions

u64 sp_hash_map_helper_hash_str(const void* key, u64 size) {
    (void) size;
    const SP_Str* _key = key;
    return sp_fvn1a_hash(_key->data, _key->len);
}

b8 sp_hash_map_helper_equal_str(const void* a, const void* b, u64 len) {
    (void) len;
    const SP_Str* _a = a;
    const SP_Str* _b = b;
    return sp_str_equal(*_a, *_b);
}

b8 sp_hash_map_helper_equal_generic(const void* a, const void* b, u64 len) {
    return memcmp(a, b, len) == 0;
}

// -- Color --------------------------------------------------------------------

SP_Color sp_color_rgba_f(f32 r, f32 g, f32 b, f32 a) {
    return (SP_Color) {r, g, b, a};
}

SP_Color sp_color_rgba_i(u8 r, u8 g, u8 b, u8 a) {
    return (SP_Color) {r/255.0f, g/255.0f, b/255.0f, a/255.0f};
}

SP_Color sp_color_rgba_hex(u32 hex) {
    return (SP_Color) {
        .r = (f32) (hex >> 8 * 3 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .a = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
    };
}

SP_Color sp_color_rgb_f(f32 r, f32 g, f32 b) {
    return (SP_Color) {r, g, b, 1.0f};
}

SP_Color sp_color_rgb_i(u8 r, u8 g, u8 b) {
    return (SP_Color) {r/255.0f, g/255.0f, b/255.0f, 1.0f};
}

SP_Color sp_color_rgb_hex(u32 hex) {
    return (SP_Color) {
        .r = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
        .a = 1.0f,
    };
}

SP_Color sp_color_hsl(f32 hue, f32 saturation, f32 lightness) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
    SP_Color color = {0};
    f32 chroma = (1 - fabsf(2 * lightness - 1)) * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (SP_Color) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (SP_Color) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (SP_Color) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (SP_Color) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (SP_Color) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (SP_Color) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = lightness-chroma / 2.0f;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

SP_Color sp_color_hsv(f32 hue, f32 saturation, f32 value) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB
    SP_Color color = {0};
    f32 chroma = value * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (SP_Color) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (SP_Color) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (SP_Color) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (SP_Color) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (SP_Color) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (SP_Color) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = value - chroma;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

// -- Testing ------------------------------------------------------------------

typedef struct SP_Test SP_Test;
struct SP_Test {
    SP_Str name;
    SP_TestFunc func;
    void* userdata;
};

typedef struct SP_TestGroup SP_TestGroup;
struct SP_TestGroup {
    SP_Str name;
    SP_Test* tests;
    u32 test_capacity;
    u32 test_count;
};

struct SP_TestSuite {
    SP_Allocator allocator;
    SP_TestGroup* groups;
    u32 group_capacity;
    u32 group_count;
};

SP_TestSuite* sp_test_suite_create(SP_Allocator allocator) {
    SP_TestSuite* suite = sp_alloc(allocator, sizeof(SP_TestSuite));
    *suite = (SP_TestSuite) {
        .allocator = allocator,
        .groups = sp_alloc(allocator, 8 * sizeof(SP_TestGroup)),
        .group_capacity = 8,
        .group_count = 0,
    };
    return suite;
}

void sp_test_suite_destroy(SP_TestSuite* suite) {
    for (u32 i = 0; i < suite->group_count; i++) {
        SP_TestGroup* group = &suite->groups[i];
        sp_free(suite->allocator, group->tests, group->test_capacity * sizeof(SP_Test));
    }
    sp_free(suite->allocator, suite->groups, suite->group_capacity * sizeof(SP_TestGroup));
}

void sp_test_suite_run(SP_TestSuite* suite) {
    u32 tests_run = 0;
    u32 successfully_run_tests = 0;
    for (u32 i = 0; i < suite->group_count; i++) {
        SP_TestGroup group = suite->groups[i];
        printf("--- Running %u tests in group %.*s ---\n", group.test_count, group.name.len, group.name.data);
        u32 successful = 0;
        for (u32 j = 0; j < suite->groups[i].test_count; j++) {
            SP_Test test = group.tests[j];
            SP_TestResult result = test.func(test.userdata);
            if (result.successful) {
                printf("%.*s ... \033[0;92mOK\033[0m\n", test.name.len, test.name.data);
                successful++;
            } else {
                printf("%.*s ... \033[1;91mFAILED\033[0m\n", test.name.len, test.name.data);
                printf("    %s:%u: %.*s\n", result.file, result.line, result.reason.len, result.reason.data);
            }
        }
        tests_run += group.test_count;
        successfully_run_tests += successful;

        printf("\n");

        if (successful == group.test_count) {
            printf("Result: \033[0;92m%u/%u\033[0m\n", successful, group.test_count);
        } else {
            printf("Result: \033[1;91m%u/%u\033[0m\n", successful, group.test_count);
        }

        printf("\n");
    }

    printf("--- SUITE RESULT ---\n");
    const char *status_color;
    if (successfully_run_tests == tests_run) {
        status_color = "\033[0;92m";
    } else {
        status_color = "\033[0;91m";
    }

    printf("Tests run: \033[0;94m%u\033[0m\n", tests_run);
    printf("Tests passed: %s%u\033[0m\n", status_color, successfully_run_tests);
    printf("Tests failed: %s%u\033[0m\n", status_color, tests_run - successfully_run_tests);
    printf("Summary: %s%u/%u\033[0m\n", status_color, successfully_run_tests, tests_run);
}

u32 sp_test_group_register(SP_TestSuite* suite, SP_Str name) {
    if (suite->group_count == suite->group_capacity) {
        suite->groups = sp_realloc(suite->allocator,
                suite->groups,
                suite->group_capacity * sizeof(SP_TestGroup),
                suite->group_capacity * 2 * sizeof(SP_TestGroup));
        suite->group_capacity *= 2;
    }

    u32 group = suite->group_count;
    suite->groups[group] = (SP_TestGroup) {
        .name = name,
        .tests = sp_alloc(suite->allocator, 8 * sizeof(SP_Test)),
        .test_capacity = 8,
        .test_count = 0,
    };

    suite->group_count++;
    return group;
}

void _sp_test_register(SP_TestSuite* suite, u32 group, SP_TestFunc func, SP_Str name, void* userdata) {
    sp_assert(group < suite->group_count, "Group %u not registered in suite!", group);
    SP_TestGroup* _group = &suite->groups[group];
    if (_group->test_count == _group->test_capacity) {
        _group->tests = sp_realloc(suite->allocator,
                _group->tests,
                _group->test_capacity * sizeof(SP_TestGroup),
                _group->test_capacity * 2 * sizeof(SP_TestGroup));
        _group->test_capacity *= 2;
    }

    u32 test = _group->test_count;
    _group->tests[test] = (SP_Test) {
        .name = name,
        .func = func,
        .userdata = userdata,
    };

    _group->test_count++;
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
    return sysconf(_SC_PAGESIZE);
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
    // I'm so sorry for this conversion, -Wpedantic requires this in order to
    // get a function pointer.
    SP_LibFunc func;
    *(void**) &func = dlsym(lib->handle, func_name);
    return func;
}

#endif // SP_POSIX

#ifdef SP_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct _SP_PlatformState {
    f32 start_time;
};

static f32 _sp_win32_time_stamp(void) {
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
