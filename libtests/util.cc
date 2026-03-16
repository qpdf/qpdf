#include <qpdf/assert_test.h>

// This program tests utility functions in the qpdf::util namespace

#include <qpdf/Util.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/global.hh>

#include <limits>

using namespace qpdf::util;

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " n filename1 [arg2]" << '\n';
    exit(2);
}

// Test util::fits
static void
test_0(QPDF&, char const*)
{
    // uint32_t: inside, lower bound, upper bound, out-of-range (both bounds)
    assert(fits<uint32_t>(123u));
    assert(fits<uint32_t>(static_cast<unsigned long long>(0u)));
    assert(fits<uint32_t>(std::numeric_limits<uint32_t>::max()));
    // too small (below lower bound) and too large (above upper bound)
    assert(!fits<uint32_t>(-1));
    assert(!fits<uint32_t>(
        static_cast<unsigned long long>(std::numeric_limits<uint32_t>::max()) + 1ULL));

    // int32_t: inside, lower bound, upper bound, out-of-range (both bounds)
    assert(fits<int32_t>(0));
    assert(fits<int32_t>(std::numeric_limits<int32_t>::min()));
    assert(fits<int32_t>(std::numeric_limits<int32_t>::max()));
    long long below_int_min = static_cast<long long>(std::numeric_limits<int32_t>::min()) - 1LL;
    long long above_int_max = static_cast<long long>(std::numeric_limits<int32_t>::max()) + 1LL;
    // too small / too large for int32_t
    assert(!fits<int32_t>(below_int_min));
    assert(!fits<int32_t>(above_int_max));

    // fits<uint16_t(int) : uint16_t contained in int
    assert(fits<uint16_t>(12345));
    assert(fits<uint16_t>(static_cast<int>(0)));
    assert(fits<uint16_t>(static_cast<int>(std::numeric_limits<uint16_t>::max())));
    // too small / too large for uint16_t (int input)
    assert(!fits<uint16_t>(-1));
    assert(!fits<uint16_t>(static_cast<int>(std::numeric_limits<uint16_t>::max()) + 1));

    // Test fits<int>(uint16_t) -- unsigned small values should fit into signed int
    assert(fits<int>(static_cast<uint16_t>(0u)));
    assert(fits<int>(static_cast<uint16_t>(std::numeric_limits<uint16_t>::max())));

    // Test fits<uint16_t>(uint32_t) -- small uint32_t fits, large does not
    assert(fits<uint16_t>(static_cast<uint32_t>(100u)));
    assert(!fits<uint16_t>(
        static_cast<uint32_t>(static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()) + 1u)));

    // Test fits<uint32_t>(int32_t) -- non-negative int32_t values fit, negatives do not
    assert(fits<uint32_t>(static_cast<int32_t>(0)));
    assert(fits<uint32_t>(static_cast<int32_t>(std::numeric_limits<int32_t>::max())));
    assert(!fits<uint32_t>(static_cast<int32_t>(-1)));
}

static void
test_1(QPDF&, char const*)
{
    // happy path
    auto v32 = to<uint32_t>(123);
    assert(v32 == 123u);

    auto si = to<int32_t>(-5);
    assert(si == -5);

    // negative -> unsigned should throw
    assert(throws<std::range_error>([]() { to<uint32_t>(-1); }));

    // too-large int -> uint16_t should throw
    assert(throws<std::range_error>([]() {
        to<uint16_t>(static_cast<int>(std::numeric_limits<uint16_t>::max()) + 1);
    }));

    // exceeding int32_t range should throw
    assert(throws<std::range_error>([]() {
        to<int32_t>(static_cast<long long>(std::numeric_limits<int32_t>::max()) + 1LL);
    }));
}

void
runtest(int n, char const* filename1, char const* arg2)
{
    // Most tests here are crafted to work on specific files.  Look at
    // the test suite to see how the test is invoked to find the file
    // that the test is supposed to operate on.

    std::set<int> ignore_filename = {0, 1};

    QPDF pdf;
    std::shared_ptr<char> file_buf;
    FILE* filep = nullptr;
    if (ignore_filename.contains(n)) {
        // Ignore filename argument entirely
    } else {
        size_t size = 0;
        QUtil::read_file_into_memory(filename1, file_buf, size);
        pdf.processMemoryFile(filename1, file_buf.get(), size);
    }

    std::map<int, void (*)(QPDF&, char const*)> test_functions = {{0, test_0}, {1, test_1}};

    auto fn = test_functions.find(n);
    if (fn == test_functions.end()) {
        throw std::runtime_error(std::string("invalid test ") + std::to_string(n));
    }
    (fn->second)(pdf, arg2);

    if (filep) {
        fclose(filep);
    }
    std::cout << "test " << n << " done" << '\n';
}

int
main(int argc, char* argv[])
{
    QUtil::setLineBuf(stdout);
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    if ((argc < 3) || (argc > 4)) {
        usage();
    }

    try {
        int n = QUtil::string_to_int(argv[1]);
        char const* filename1 = argv[2];
        char const* arg2 = (argc >= 4) ? argv[3] : "";
        runtest(n, filename1, arg2);
    } catch (std::exception& e) {
        std::cerr << e.what() << '\n';
        exit(2);
    }

    return 0;
}
