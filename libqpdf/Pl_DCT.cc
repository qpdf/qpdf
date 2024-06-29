#include <qpdf/Pl_DCT.hh>

#include "qpdf/QPDFLogger.hh"
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>

#include <csetjmp>
#include <stdexcept>
#include <string>

#if BITS_IN_JSAMPLE != 8
# error "qpdf does not support libjpeg built with BITS_IN_JSAMPLE != 8"
#endif

namespace
{
    struct qpdf_jpeg_error_mgr
    {
        struct jpeg_error_mgr pub;
        jmp_buf jmpbuf;
        std::string msg;
    };
} // namespace

static void
error_handler(j_common_ptr cinfo)
{
    auto* jerr = reinterpret_cast<qpdf_jpeg_error_mgr*>(cinfo->err);
    char buf[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buf);
    jerr->msg = buf;
    longjmp(jerr->jmpbuf, 1);
}

Pl_DCT::Members::Members(size_t corrupt_data_limit) :
    action(a_decompress),
    buf("DCT compressed image"),
    corrupt_data_limit(corrupt_data_limit)
{
}

Pl_DCT::Members::Members(
    JDIMENSION image_width,
    JDIMENSION image_height,
    int components,
    J_COLOR_SPACE color_space,
    CompressConfig* config_callback) :
    action(a_compress),
    buf("DCT uncompressed image"),
    image_width(image_width),
    image_height(image_height),
    components(components),
    color_space(color_space),
    config_callback(config_callback)
{
}

Pl_DCT::Pl_DCT(char const* identifier, Pipeline* next) :
    Pl_DCT(identifier, next, 0)
{
}

Pl_DCT::Pl_DCT(char const* identifier, Pipeline* next, size_t corrupt_data_limit) :
    Pipeline(identifier, next),
    m(new Members(corrupt_data_limit))
{
}

Pl_DCT::Pl_DCT(
    char const* identifier,
    Pipeline* next,
    JDIMENSION image_width,
    JDIMENSION image_height,
    int components,
    J_COLOR_SPACE color_space,
    CompressConfig* config_callback) :
    Pipeline(identifier, next),
    m(new Members(image_width, image_height, components, color_space, config_callback))
{
}

Pl_DCT::~Pl_DCT() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
Pl_DCT::write(unsigned char const* data, size_t len)
{
    m->buf.write(data, len);
}

void
Pl_DCT::finish()
{
    m->buf.finish();

    // Using a std::shared_ptr<Buffer> here and passing it into compress and decompress causes a
    // memory leak with setjmp/longjmp. Just use a pointer and delete it.
    Buffer* b = m->buf.getBuffer();
    if (b->getSize() == 0) {
        // Special case: empty data will never succeed and probably means we're calling finish a
        // second time from an exception handler.
        delete b;
        this->getNext()->finish();
        return;
    }

    struct jpeg_compress_struct cinfo_compress;
    struct jpeg_decompress_struct cinfo_decompress;
    struct qpdf_jpeg_error_mgr jerr;

    cinfo_compress.err = jpeg_std_error(&(jerr.pub));
    cinfo_decompress.err = jpeg_std_error(&(jerr.pub));
    jerr.pub.error_exit = error_handler;

    bool error = false;
    // The jpeg library is a "C" library, so we use setjmp and longjmp for exception handling.
    if (setjmp(jerr.jmpbuf) == 0) {
        try {
            if (m->action == a_compress) {
                compress(reinterpret_cast<void*>(&cinfo_compress), b);
            } else {
                decompress(reinterpret_cast<void*>(&cinfo_decompress), b);
            }
        } catch (std::exception& e) {
            // Convert an exception back to a longjmp so we can ensure that the right cleanup
            // happens. This will get converted back to an exception.
            jerr.msg = e.what();
            longjmp(jerr.jmpbuf, 1);
        }
    } else {
        error = true;
    }
    delete b;

    if (m->action == a_compress) {
        jpeg_destroy_compress(&cinfo_compress);
    }
    if (m->action == a_decompress) {
        jpeg_destroy_decompress(&cinfo_decompress);
    }
    if (error) {
        throw std::runtime_error(jerr.msg);
    }
}

namespace
{
    struct dct_pipeline_dest
    {
        struct jpeg_destination_mgr pub; /* public fields */
        unsigned char* buffer;
        size_t size;
        Pipeline* next;
    };
} // namespace

static void
init_pipeline_destination(j_compress_ptr)
{
}

static boolean
empty_pipeline_output_buffer(j_compress_ptr cinfo)
{
    QTC::TC("libtests", "Pl_DCT empty_pipeline_output_buffer");
    auto* dest = reinterpret_cast<dct_pipeline_dest*>(cinfo->dest);
    dest->next->write(dest->buffer, dest->size);
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = dest->size;
    return TRUE;
}

static void
term_pipeline_destination(j_compress_ptr cinfo)
{
    QTC::TC("libtests", "Pl_DCT term_pipeline_destination");
    auto* dest = reinterpret_cast<dct_pipeline_dest*>(cinfo->dest);
    dest->next->write(dest->buffer, dest->size - dest->pub.free_in_buffer);
}

static void
jpeg_pipeline_dest(j_compress_ptr cinfo, unsigned char* outbuffer, size_t size, Pipeline* next)
{
    cinfo->dest = static_cast<struct jpeg_destination_mgr*>(
        // line-break
        (*cinfo->mem->alloc_small)(
            reinterpret_cast<j_common_ptr>(cinfo), JPOOL_PERMANENT, sizeof(dct_pipeline_dest)));
    auto* dest = reinterpret_cast<dct_pipeline_dest*>(cinfo->dest);
    dest->pub.init_destination = init_pipeline_destination;
    dest->pub.empty_output_buffer = empty_pipeline_output_buffer;
    dest->pub.term_destination = term_pipeline_destination;
    dest->pub.next_output_byte = dest->buffer = outbuffer;
    dest->pub.free_in_buffer = dest->size = size;
    dest->next = next;
}

static void
init_buffer_source(j_decompress_ptr)
{
}

static boolean
fill_buffer_input_buffer(j_decompress_ptr)
{
    // The whole JPEG data is expected to reside in the supplied memory buffer, so any request for
    // more data beyond the given buffer size is treated as an error.
    throw std::runtime_error("invalid jpeg data reading from buffer");
    return TRUE;
}

static void
skip_buffer_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    if (num_bytes < 0) {
        throw std::runtime_error(
            "reading jpeg: jpeg library requested skipping a negative number of bytes");
    }
    size_t to_skip = QIntC::to_size(num_bytes);
    if ((to_skip > 0) && (to_skip <= cinfo->src->bytes_in_buffer)) {
        cinfo->src->next_input_byte += to_skip;
        cinfo->src->bytes_in_buffer -= to_skip;
    } else if (to_skip != 0) {
        cinfo->src->next_input_byte += cinfo->src->bytes_in_buffer;
        cinfo->src->bytes_in_buffer = 0;
    }
}

static void
term_buffer_source(j_decompress_ptr)
{
}

static void
jpeg_buffer_src(j_decompress_ptr cinfo, Buffer* buffer)
{
    cinfo->src = reinterpret_cast<jpeg_source_mgr*>(
        // line-break
        (*cinfo->mem->alloc_small)(
            reinterpret_cast<j_common_ptr>(cinfo), JPOOL_PERMANENT, sizeof(jpeg_source_mgr)));

    jpeg_source_mgr* src = cinfo->src;
    src->init_source = init_buffer_source;
    src->fill_input_buffer = fill_buffer_input_buffer;
    src->skip_input_data = skip_buffer_input_data;
    src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->term_source = term_buffer_source;
    src->bytes_in_buffer = buffer->getSize();
    src->next_input_byte = buffer->getBuffer();
}

void
Pl_DCT::compress(void* cinfo_p, Buffer* b)
{
    auto* cinfo = reinterpret_cast<jpeg_compress_struct*>(cinfo_p);

#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    jpeg_create_compress(cinfo);
#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || defined(__clang__))
# pragma GCC diagnostic pop
#endif
    static int const BUF_SIZE = 65536;
    auto outbuffer_ph = std::make_unique<unsigned char[]>(BUF_SIZE);
    unsigned char* outbuffer = outbuffer_ph.get();
    jpeg_pipeline_dest(cinfo, outbuffer, BUF_SIZE, this->getNext());

    cinfo->image_width = m->image_width;
    cinfo->image_height = m->image_height;
    cinfo->input_components = m->components;
    cinfo->in_color_space = m->color_space;
    jpeg_set_defaults(cinfo);
    if (m->config_callback) {
        m->config_callback->apply(cinfo);
    }

    jpeg_start_compress(cinfo, TRUE);

    unsigned int width = cinfo->image_width * QIntC::to_uint(cinfo->input_components);
    size_t expected_size = QIntC::to_size(cinfo->image_height) *
        QIntC::to_size(cinfo->image_width) * QIntC::to_size(cinfo->input_components);
    if (b->getSize() != expected_size) {
        throw std::runtime_error(
            "Pl_DCT: image buffer size = " + std::to_string(b->getSize()) +
            "; expected size = " + std::to_string(expected_size));
    }
    JSAMPROW row_pointer[1];
    unsigned char* buffer = b->getBuffer();
    while (cinfo->next_scanline < cinfo->image_height) {
        // We already verified that the buffer is big enough.
        row_pointer[0] = &buffer[cinfo->next_scanline * width];
        (void)jpeg_write_scanlines(cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(cinfo);
    this->getNext()->finish();
}

void
Pl_DCT::decompress(void* cinfo_p, Buffer* b)
{
    auto* cinfo = reinterpret_cast<jpeg_decompress_struct*>(cinfo_p);

#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || defined(__clang__))
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    jpeg_create_decompress(cinfo);
#if ((defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406) || defined(__clang__))
# pragma GCC diagnostic pop
#endif

#ifdef QPDF_OSS_FUZZ
    // Limit the memory used to decompress JPEG files during fuzzing. Excessive memory use during
    // fuzzing is due to corrupt JPEG data which sometimes cannot be detected before
    // jpeg_start_decompress is called. During normal use of qpdf very large JPEGs can occasionally
    // occur legitimately and therefore must be allowed during normal operations.
    cinfo->mem->max_memory_to_use = 1'000'000'000;
    // For some corrupt files the memory used internally by libjpeg stays within the above limits
    // even though the size written to the next pipeline is significantly larger.
    m->corrupt_data_limit = 100'000'000;
#endif
    jpeg_buffer_src(cinfo, b);

    (void)jpeg_read_header(cinfo, TRUE);
    (void)jpeg_calc_output_dimensions(cinfo);
    unsigned int width = cinfo->output_width * QIntC::to_uint(cinfo->output_components);
    if (cinfo->err->num_warnings == 0 || m->corrupt_data_limit == 0 ||
        (width * QIntC::to_uint(cinfo->output_height)) < m->corrupt_data_limit) {
        // err->num_warnings is the number of corrupt data warnings emitted.
        // err->msg_code could also be the code of an informational message.
        JSAMPARRAY buffer = (*cinfo->mem->alloc_sarray)(
            reinterpret_cast<j_common_ptr>(cinfo), JPOOL_IMAGE, width, 1);

        (void)jpeg_start_decompress(cinfo);
        while (cinfo->output_scanline < cinfo->output_height) {
            (void)jpeg_read_scanlines(cinfo, buffer, 1);
            getNext()->write(buffer[0], width * sizeof(buffer[0][0]));
        }
        (void)jpeg_finish_decompress(cinfo);
    } else {
        *QPDFLogger::defaultLogger()->getError() << "corrupt JPEG data ignored" << "\n";
    }
    getNext()->finish();
}
