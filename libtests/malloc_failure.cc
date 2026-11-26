#include <qpdf/assert_test.h>
#include <qpdf/Pl_Buffer.hh>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <new>

static constexpr size_t data_size = 4093;
static bool fail_allocation = false;
static bool allocation_failed = false;

extern "C" void* __real_malloc(size_t);

extern "C" void*
__wrap_malloc(size_t size)
{
    if (fail_allocation && size == data_size) {
        fail_allocation = false;
        allocation_failed = true;
        errno = ENOMEM;
        return nullptr;
    }
    return __real_malloc(size);
}

int
main()
{
    unsigned char data[data_size];
    memset(data, 'A', data_size);

    Pl_Buffer buffer("allocation failure test");
    buffer.write(data, data_size);
    buffer.finish();

    unsigned char* output = nullptr;
    size_t size = 0;
    bool caught = false;

    fail_allocation = true;
    try {
        buffer.getMallocBuffer(&output, &size);
    } catch (std::bad_alloc const&) {
        caught = true;
    }
    fail_allocation = false;

    assert(allocation_failed);
    assert(caught);
    assert(output == nullptr);

    // The failed allocation must not discard the buffered data.
    buffer.getMallocBuffer(&output, &size);
    assert(output != nullptr);
    assert(size == data_size);
    assert(memcmp(output, data, data_size) == 0);
    free(output);

    // After a successful extraction, the buffer must be empty.
    output = nullptr;
    buffer.getMallocBuffer(&output, &size);
    assert(output == nullptr);
    assert(size == 0);
    return 0;
}
