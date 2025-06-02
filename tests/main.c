#include "spire.h"

extern void test_hash_map(SP_TestSuite* suite);

i32 main(void) {
    sp_init(SP_CONFIG_DEFAULT);
    SP_TestSuite* suite = sp_test_suite_create(sp_libc_allocator());

    test_hash_map(suite);

    sp_test_suite_run(suite);
    sp_test_suite_destroy(suite);
    sp_terminate();
    return 0;
}
