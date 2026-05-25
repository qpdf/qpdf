#include <qpdf/Pl_DCT.hh>

#include <qpdf/global_private.hh>

#include <stdexcept>

using namespace qpdf;

// Stub implementation that throws runtime errors for all JPEG operations
class Pl_DCT::Members
{
  public:
    // For compression
    Members(
        JDIMENSION image_width,
        JDIMENSION image_height,
        int components,
        J_COLOR_SPACE color_space,
        CompressConfig* config_callback) :
        action(a_compress)
    {
    }

    // For decompression
    Members() :
        action(a_decompress)
    {
    }

    Members(Members const&) = delete;

    ~Members() = default;

    action_e action;
};

Pl_DCT::Pl_DCT(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(std::make_unique<Members>())
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_DCT with nullptr as next");
    }
}

// The limits are not used by the stub, but they are still stored in the global state so that
// they stay consistent with the qpdf global limits/options API.
void
Pl_DCT::setMemoryLimit(long limit)
{
    global::Limits::dct_max_memory(limit);
}

void
Pl_DCT::setScanLimit(int limit)
{
    global::Limits::dct_max_progressive_scans(limit);
}

void
Pl_DCT::setThrowOnCorruptData(bool treat_as_error)
{
    global::options::dct_throw_on_corrupt_data(treat_as_error);
}

Pl_DCT::Pl_DCT(
    char const* identifier,
    Pipeline* next,
    JDIMENSION image_width,
    JDIMENSION image_height,
    int components,
    J_COLOR_SPACE color_space,
    CompressConfig* compress_callback) :
    Pipeline(identifier, next),
    m(std::make_unique<Members>(
        image_width, image_height, components, color_space, compress_callback))
{
}

// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
Pl_DCT::~Pl_DCT() = default;

void
Pl_DCT::write(unsigned char const* data, size_t len)
{
    throw std::runtime_error("Pl_DCT: JPEG functionality not available (compiled without libjpeg)");
}

void
Pl_DCT::finish()
{
    throw std::runtime_error("Pl_DCT: JPEG functionality not available (compiled without libjpeg)");
}

std::unique_ptr<Pl_DCT::CompressConfig>
Pl_DCT::make_compress_config(std::function<void(void*)> f)
{
    throw std::runtime_error("Pl_DCT: JPEG functionality not available (compiled without libjpeg)");
}