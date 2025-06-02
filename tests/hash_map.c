#include "spire.h"

SP_TestResult test_hash_map_insert_get(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_hash_map_helper_hash_str,
            .equal = sp_hash_map_helper_equal_str,
            .key_size = sizeof(SP_Str),
            .value_size = sizeof(u32),
        });

    SP_Str key;
    u32 value, output;
    b8 result;

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insert failed!");


    key = sp_str_lit("life");
    value = 8;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(!result, "Insert succeeded with duplicate key!");

    key = sp_str_lit("life");
    output = 0;
    result = sp_hash_map_get(map, &key, &output);
    sp_test_assert(result, "Get can't find key in map!");
    sp_test_assert(output == 42, "Get outputs wrong value!");

    key = sp_str_lit("not in map");
    output = 0;
    result = sp_hash_map_get(map, &key, &output);
    sp_test_assert(!result, "Get finds non-existant key!");
    sp_test_assert(output == 0, "Get sets output without a valid key!");

    key = sp_str_lit("not in map");
    result = sp_hash_map_get(map, &key, NULL);
    sp_test_assert(!result, "Get found a non-existant key!");

    sp_test_success();
}

SP_TestResult test_hash_map_remove(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_hash_map_helper_hash_str,
            .equal = sp_hash_map_helper_equal_str,
            .key_size = sizeof(SP_Str),
            .value_size = sizeof(u32),
        });

    SP_Str key;
    u32 value, output;
    b8 result;

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insertion failed!");

    key = sp_str_lit("other");
    value = 8;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insertion failed!");

    key = sp_str_lit("life");
    result = sp_hash_map_remove(map, &key);
    sp_test_assert(result, "Remove failed!");

    key = sp_str_lit("life");
    result = sp_hash_map_remove(map, &key);
    sp_test_assert(!result, "Remove succeeded on a key not in map!");

    key = sp_str_lit("life");
    output = 0;
    result = sp_hash_map_get(map, &key, &output);
    sp_test_assert(!result, "Get found a non-existant key!");
    sp_test_assert(output == 0, "Get gave output for a non-existant key!");

    sp_test_success();
}

SP_TestResult test_hash_map_set(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_hash_map_helper_hash_str,
            .equal = sp_hash_map_helper_equal_str,
            .key_size = sizeof(SP_Str),
            .value_size = sizeof(u32),
        });

    SP_Str key;
    u32 value, output;
    b8 result;

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_set(map, &key, &value);
    sp_test_assert(result, "Set unique key doesn't register as a new key!");

    key = sp_str_lit("life");
    value = 8;
    result = sp_hash_map_set(map, &key, &value);
    sp_test_assert(!result, "Set duplicate key registers as a new key!");

    key = sp_str_lit("life");
    output = 8;
    result = sp_hash_map_get(map, &key, &output);
    sp_test_assert(result, "Get couldn't find key!");
    sp_test_assert(output == 8, "Get wrong output!");

    sp_test_success();
}

SP_TestResult test_hash_map_mass_insert_remove(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .key_size = sizeof(u32),
            .value_size = sizeof(u32),
        });
    const u32 count = 4096;

    for (u32 i = 0; i < count; i++) {
        u32 key = i;
        u32 value = i*i;
        b8 result = sp_hash_map_set(map, &key, &value);
        sp_test_assert(result, "Insert failed!");
    }

    // See that all keys are present.
    for (u32 i = 0; i < count; i++) {
        u32 key = i;
        b8 result = sp_hash_map_get(map, &key, NULL);
        sp_test_assert(result, "Key not found!");
    }

    // Remove every other key.
    for (u32 i = 0; i < count; i += 2) {
        u32 key = i;
        b8 result = sp_hash_map_remove(map, &key);
        sp_test_assert(result, "Remove failed!");
    }

    for (u32 i = 0; i < count; i++) {
        u32 key = i;
        b8 result = sp_hash_map_get(map, &key, NULL);
        if (i % 2 == 0) {
            sp_test_assert(!result, "Non-existant key found!");
        } else {
            sp_test_assert(result, "Key not found!");
        }
    }

    sp_test_success();
}

SP_TestResult test_hash_map_reinsertion(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_hash_map_helper_hash_str,
            .equal = sp_hash_map_helper_equal_generic,
            .key_size = sizeof(SP_Str),
            .value_size = sizeof(u32),
        });

    SP_Str key;
    u32 value, output;
    b8 result;

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insert failed!");

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(!result, "Insert duplicate key succeeded!");

    key = sp_str_lit("life");
    result = sp_hash_map_remove(map, &key);
    sp_test_assert(result, "Remove failed!");

    key = sp_str_lit("life");
    value = 84;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insert failed!");

    key = sp_str_lit("life");
    output = 0;
    result = sp_hash_map_get(map, &key, &output);
    sp_test_assert(result, "Get failed!");
    sp_test_assert(output == 84, "Get output wrong!");

    sp_test_success();
}

SP_TestResult test_hash_map_iteration(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 1024,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .key_size = sizeof(u32),
            .value_size = sizeof(u32),
        });
    enum { COUNT = 4096 };

    for (u32 i = 0; i < COUNT; i++) {
        u32 key = i;
        u32 value = i*i;
        b8 result = sp_hash_map_set(map, &key, &value);
        sp_test_assert(result, "Insert failed!");
    }

    u32 iteration_keys[COUNT] = {0};
    u32 iteration_values[COUNT] = {0};
    u32 key_index[COUNT] = {0};
    u32 i = 0;
    for (HashMapIter iter = sp_hash_map_iter_init(map);
            sp_hash_map_iter_valid(iter);
            iter = sp_hash_map_iter_next(iter)) {
        u32 key = 0;
        u32 value = 0;
        sp_hash_map_iter_get_key(iter, &key);
        sp_hash_map_iter_get_value(iter, &value);

        sp_test_assert(value == key*key, "Key-value pair doesn't match!");

        iteration_keys[i] = key;
        iteration_values[i] = value;
        key_index[iteration_keys[i]] = i;
        i++;
    }

    for (u32 i = 0; i < COUNT; i++) {
        u32 index = key_index[i];
        sp_test_assert(iteration_keys[index] == i, "Iteration key doesn't match!");
        sp_test_assert(iteration_values[index] == i*i, "Iteration value doesn't match!");
    }

    sp_test_success();
}

SP_TestResult test_hash_map_get_pointer(void* userdata) {
    SP_HashMap* map = sp_hash_map_create((SP_HashMapDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_hash_map_helper_hash_str,
            .equal = sp_hash_map_helper_equal_generic,
            .key_size = sizeof(SP_Str),
            .value_size = sizeof(u32),
        });

    SP_Str key;
    u32 value;
    u32* output;
    b8 result;

    key = sp_str_lit("life");
    value = 42;
    result = sp_hash_map_insert(map, &key, &value);
    sp_test_assert(result, "Insert failed!");

    key = sp_str_lit("life");
    output = sp_hash_map_getp(map, &key);
    sp_test_assert(output != NULL, "Get pointer failed!");
    sp_test_assert(*output == 42, "Get pointer output wrong!");

    key = sp_str_lit("not in map");
    output = sp_hash_map_getp(map, &key);
    sp_test_assert(output == NULL, "Get pointer failed!");

    sp_test_success();
}

void test_hash_map(SP_TestSuite* suite) {
    u32 hash_map_groups[2] = {
        sp_test_group_register(suite, sp_str_lit("Hash Map (Open Adressing)")),
        sp_test_group_register(suite, sp_str_lit("Hash Map (Separate Chaining)")),
    };
    void* hash_map_resolution_type[2] = {
        (void*) SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING,
        (void*) SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING,
    };

    for (u32 i = 0; i < 2; i++) {
        u32 group = hash_map_groups[i];
        void* resolution_type = hash_map_resolution_type[i];
        sp_test_register(suite, group, test_hash_map_insert_get, resolution_type);
        sp_test_register(suite, group, test_hash_map_remove, resolution_type);
        sp_test_register(suite, group, test_hash_map_set, resolution_type);
        sp_test_register(suite, group, test_hash_map_mass_insert_remove, resolution_type);
        sp_test_register(suite, group, test_hash_map_reinsertion, resolution_type);
        sp_test_register(suite, group, test_hash_map_iteration, resolution_type);
        sp_test_register(suite, group, test_hash_map_get_pointer, resolution_type);
    }
}
