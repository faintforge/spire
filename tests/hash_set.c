#include "spire.h"

SP_TestResult test_hash_set_insert_has(void* userdata) {
    SP_HashSet* set = sp_hash_set_create((SP_HashSetDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .value_size = sizeof(u32),
        });

    u32 value;
    b8 result;

    value = 42;
    result = sp_hash_set_insert(set, &value);
    sp_test_assert(result);

    value = 42;
    result = sp_hash_set_insert(set, &value);
    sp_test_assert(!result);

    value = 42;
    result = sp_hash_set_has(set, &value);
    sp_test_assert(result);

    value = 8;
    result = sp_hash_set_has(set, &value);
    sp_test_assert(!result);

    sp_test_success();
}

SP_TestResult test_hash_set_remove(void* userdata) {
    SP_HashSet* set = sp_hash_set_create((SP_HashSetDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .value_size = sizeof(u32),
        });

    u32 value;
    b8 result;

    value = 42;
    result = sp_hash_set_insert(set, &value);
    sp_test_assert(result);

    value = 8;
    result = sp_hash_set_insert(set, &value);
    sp_test_assert(result);

    value = 42;
    result = sp_hash_set_remove(set, &value);
    sp_test_assert(result);

    value = 42;
    result = sp_hash_set_remove(set, &value);
    sp_test_assert(!result);

    value = 42;
    result = sp_hash_set_has(set, &value);
    sp_test_assert(!result);

    sp_test_success();
}

SP_TestResult test_hash_set_mass_insert_remove(void* userdata) {
    SP_HashSet* set = sp_hash_set_create((SP_HashSetDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .value_size = sizeof(u32),
        });
    const u32 count = 4096;

    for (u32 i = 0; i < count; i++) {
        u32 value = i*i;
        b8 result = sp_hash_set_insert(set, &value);
        sp_test_assert(result);
    }

    // See that all values are present.
    for (u32 i = 0; i < count; i++) {
        u32 value = i*i;
        b8 result = sp_hash_set_has(set, &value);
        sp_test_assert(result);
    }

    // Remove every other value.
    for (u32 i = 0; i < count; i += 2) {
        u32 value = i*i;
        b8 result = sp_hash_set_remove(set, &value);
        sp_test_assert(result);
    }

    for (u32 i = 0; i < count; i++) {
        u32 value = i*i;
        b8 result = sp_hash_set_has(set, &value);
        if (i % 2 == 0) {
            sp_test_assert(!result);
        } else {
            sp_test_assert(result);
        }
    }

    sp_test_success();
}

SP_TestResult test_hash_set_reinsertion(void* userdata) {
    SP_HashSet* set = sp_hash_set_create((SP_HashSetDesc) {
            .allocator = sp_libc_allocator(),
            .capacity = 8,
            .collision_resolution = (u64) userdata,
            .hash = sp_fvn1a_hash,
            .equal = sp_hash_map_helper_equal_generic,
            .value_size = sizeof(u32),
        });

    u32 value;
    b8 result;

    value = 42;
    result = sp_hash_set_insert(set, &value);
    sp_test_assert(result);

    result = sp_hash_set_remove(set, &value);
    sp_test_assert(result);

    result = sp_hash_set_insert(set, &value);
    sp_test_assert(result);

    result = sp_hash_set_has(set, &value);
    sp_test_assert(result);

    sp_test_success();
}

void test_hash_set(SP_TestSuite* suite) {
    u32 hash_set_groups[2] = {
        sp_test_group_register(suite, sp_str_lit("Hash Set (Open Adressing)")),
        sp_test_group_register(suite, sp_str_lit("Hash Set (Separate Chaining)")),
    };
    void* hash_set_resolution_type[2] = {
        (void*) SP_HASH_COLLISION_RESOLUTION_OPEN_ADDRESSING,
        (void*) SP_HASH_COLLISION_RESOLUTION_SEPARATE_CHAINING,
    };

    for (u32 i = 0; i < 2; i++) {
        u32 group = hash_set_groups[i];
        void* resolution_type = hash_set_resolution_type[i];
        sp_test_register(suite, group, test_hash_set_insert_has, resolution_type);
        sp_test_register(suite, group, test_hash_set_remove, resolution_type);
        sp_test_register(suite, group, test_hash_set_mass_insert_remove, resolution_type);
        sp_test_register(suite, group, test_hash_set_reinsertion, resolution_type);
    }
}
