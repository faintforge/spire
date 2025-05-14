#include "spire.h"

i32 main(void) {
    sp_init(SP_CONFIG_DEFAULT);
    SP_Arena* arena = sp_arena_create();

    SP_HashMap* map = sp_hm_new(sp_hm_desc_str(arena, 512, i32));

    sp_hm_insert(map, sp_str_lit("life"), 42);
    sp_hm_insert(map, sp_str_lit("foo"), 16);
    sp_hm_insert(map, sp_str_lit("bar"), 32);
    sp_hm_insert(map, sp_str_lit("baz"), 64);
    sp_hm_insert(map, sp_str_lit("foobar"), 48);

    i32 life = sp_hm_get(map, sp_str_lit("life"), i32);
    sp_info("life = %d", life);

    i32* foo = sp_hm_getp(map, sp_str_lit("foo"));
    sp_info("foo = %p, *foo = %d", foo, *foo);

    i32 removed = sp_hm_remove(map, sp_str_lit("foo"), i32);
    sp_info("removed (foo) = %d", removed);

    i32 non_existant = sp_hm_get(map, sp_str_lit("foo"), i32);
    sp_info("non_existant = %d", non_existant);

    i32* non_existant_ptr = sp_hm_getp(map, sp_str_lit("foo"));
    sp_info("non_existant_ptr = %p", non_existant_ptr);

    // Iterate through all key-value pairs in hash map.
    sp_info("Iteration of map:");
    for (SP_HashMapIter iter = sp_hm_iter_new(map);
            sp_hm_iter_valid(iter);
            iter = sp_hm_iter_next(iter)) {
        SP_Str* key = sp_hm_iter_get_keyp(iter);
        i32* value = sp_hm_iter_get_valuep(iter);
        sp_info("    (\"%.*s\", %d)", key->len, key->data, *value);
    }

    sp_arena_destroy(arena);
    sp_terminate();
}
