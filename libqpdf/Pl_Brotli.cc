#include <qpdf/Pl_Brotli.hh>

#include <qpdf/Pl_Flate.hh>

#include <brotli/decode.h>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <stdexcept>

using namespace qpdf;

struct Pl_Brotli::State
{
    // Bound decoder allocations as well as output. In particular, enabling the
    // large-window extension must not bypass qpdf's decompression memory limit.
    struct alignas(std::max_align_t) Allocation
    {
        size_t size;
    };

    size_t allocated_{0};
    unsigned long long limit_{Pl_Flate::memory_limit()};
    BrotliDecoderState* decoder_{nullptr};

    static void*
    allocate(void* opaque, size_t size)
    {
        auto& self = *static_cast<State*>(opaque);
        if (size > std::numeric_limits<size_t>::max() - sizeof(Allocation)) {
            return nullptr;
        }
        size_t total = size + sizeof(Allocation);
        if (total > std::numeric_limits<size_t>::max() - self.allocated_ ||
            (self.limit_ && (total > self.limit_ || self.allocated_ > self.limit_ - total))) {
            return nullptr;
        }
        auto* allocation = static_cast<Allocation*>(std::malloc(total));
        if (!allocation) {
            return nullptr;
        }
        allocation->size = total;
        self.allocated_ += total;
        return allocation + 1;
    }

    static void
    release(void* opaque, void* address)
    {
        if (address) {
            auto& self = *static_cast<State*>(opaque);
            auto* allocation = static_cast<Allocation*>(address) - 1;
            self.allocated_ -= allocation->size;
            std::free(allocation);
        }
    }

    State()
    {
        decoder_ = BrotliDecoderCreateInstance(allocate, release, this);
        if (!decoder_) {
            throw std::runtime_error(
                "Brotli decoder allocation failed (memory limit or out of memory)");
        }
        // Required by EXTN-BROTLI-1. Shared dictionaries and RFC 9841 framing
        // are not enabled; PDF streams contain just the Brotli bitstream.
        if (!BrotliDecoderSetParameter(decoder_, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1)) {
            BrotliDecoderDestroyInstance(decoder_);
            throw std::runtime_error("Unable to enable large-window Brotli decoding");
        }
    }

    ~State()
    {
        BrotliDecoderDestroyInstance(decoder_);
    }
};

Pl_Brotli::Pl_Brotli(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    state_(std::make_unique<State>())
{
}

Pl_Brotli::~Pl_Brotli() = default;

void
Pl_Brotli::write(unsigned char const* data, size_t len)
{
    if (!len) {
        return;
    }
    if (complete_) {
        throw std::runtime_error("Trailing data after Brotli stream");
    }
    for (;;) {
        size_t available = outbuf_.size();
        auto* output = outbuf_.data();
        auto result = BrotliDecoderDecompressStream(
            state_->decoder_, &len, &data, &available, &output, nullptr);
        if (result == BROTLI_DECODER_RESULT_ERROR) {
            throw std::runtime_error(
                std::string("Brotli decoding failed: ") +
                BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state_->decoder_)));
        }
        auto count = outbuf_.size() - available;
        auto limit_ = state_->limit_;
        if (limit_ && (count > limit_ || written_ > limit_ - count)) {
            throw std::runtime_error("Brotli decompression memory limit exceeded");
        }
        written_ += count;
        if (count) {
            next()->write(outbuf_.data(), count);
        }
        if (result == BROTLI_DECODER_RESULT_SUCCESS) {
            complete_ = true;
            if (len) {
                throw std::runtime_error("Trailing data after Brotli stream");
            }
            return;
        }
        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            return;
        }
        // NEEDS_MORE_OUTPUT: drain pending output even when all input has
        // already been consumed. No decoded stream is accumulated here.
    }
}

void
Pl_Brotli::finish()
{
    if (!complete_) {
        throw std::runtime_error("Truncated Brotli stream");
    }
    next()->finish();
}
