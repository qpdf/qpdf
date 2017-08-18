#include <qpdf/Pl_DCT.hh>

#include <qpdf/QUtil.hh>
#include <setjmp.h>
#include <string>
#include <stdexcept>

#if BITS_IN_JSAMPLE != 8
# error "qpdf does not support libjpeg built with BITS_IN_JSAMPLE != 8"
#endif

struct qpdf_jpeg_error_mgr
{
    struct jpeg_error_mgr pub;
    jmp_buf jmpbuf;
    std::string msg;
};

static void
error_handler(j_common_ptr cinfo)
{
    qpdf_jpeg_error_mgr* jerr =
        reinterpret_cast<qpdf_jpeg_error_mgr*>(cinfo->err);
    char buf[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buf);
    jerr->msg = buf;
    longjmp(jerr->jmpbuf, 1);
}

Pl_DCT::Pl_DCT(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    action(a_decompress),
    buf("DCT compressed image")
{
}

Pl_DCT::Pl_DCT(char const* identifier, Pipeline* next,
               JDIMENSION image_width,
               JDIMENSION image_height,
               int components,
               J_COLOR_SPACE color_space,
               CompressConfig* config_callback) :
    Pipeline(identifier, next),
    action(a_compress),
    buf("DCT uncompressed image"),
    image_width(image_width),
    image_height(image_height),
    components(components),
    color_space(color_space),
    config_callback(config_callback)
{
}

Pl_DCT::~Pl_DCT()
{
}

void
Pl_DCT::write(unsigned char* data, size_t len)
{
    this->buf.write(data, len);
}

void
Pl_DCT::finish()
{
    this->buf.finish();
    PointerHolder<Buffer> b = this->buf.getBuffer();

    struct jpeg_compress_struct cinfo_compress;
    struct jpeg_decompress_struct cinfo_decompress;
    struct qpdf_jpeg_error_mgr jerr;

    cinfo_compress.err = jpeg_std_error(&(jerr.pub));
    cinfo_decompress.err = jpeg_std_error(&(jerr.pub));
    jerr.pub.error_exit = error_handler;

    bool error = false;
    if (setjmp(jerr.jmpbuf) == 0)
    {
        if (this->action == a_compress)
        {
            compress(reinterpret_cast<void*>(&cinfo_compress), b);
        }
        else
        {
            decompress(reinterpret_cast<void*>(&cinfo_decompress), b);
        }
    }
    else
    {
        error = true;
    }

    if (this->action == a_compress)
    {
        jpeg_destroy_compress(&cinfo_compress);
    }
    if (this->action == a_decompress)
    {
        jpeg_destroy_decompress(&cinfo_decompress);
    }
    if (error)
    {
        throw std::runtime_error(jerr.msg);
    }
}

void
Pl_DCT::compress(void* cinfo_p, PointerHolder<Buffer> b)
{
    struct jpeg_compress_struct* cinfo =
        reinterpret_cast<jpeg_compress_struct*>(cinfo_p);

#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
#endif
    jpeg_create_compress(cinfo);
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic pop
# endif
#endif
    unsigned char* outbuffer = 0;
    unsigned long outsize = 0;
    jpeg_mem_dest(cinfo, &outbuffer, &outsize);

    cinfo->image_width = this->image_width;
    cinfo->image_height = this->image_height;
    cinfo->input_components = this->components;
    cinfo->in_color_space = this->color_space;
    jpeg_set_defaults(cinfo);
    if (this->config_callback)
    {
        this->config_callback->apply(cinfo);
    }

    jpeg_start_compress(cinfo, TRUE);

    int width = cinfo->image_width * cinfo->input_components;
    size_t expected_size =
        cinfo->image_height * cinfo->image_width * cinfo->input_components;
    if (b->getSize() != expected_size)
    {
        throw std::runtime_error(
            "Pl_DCT: image buffer size = " +
            QUtil::int_to_string(b->getSize()) + "; expected size = " +
            QUtil::int_to_string(expected_size));
    }
    JSAMPROW row_pointer[1];
    unsigned char* buffer = b->getBuffer();
    while (cinfo->next_scanline < cinfo->image_height)
    {
        // We already verified that the buffer is big enough.
        row_pointer[0] = &buffer[cinfo->next_scanline * width];
        (void) jpeg_write_scanlines(cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(cinfo);
    this->getNext()->write(outbuffer, outsize);
    this->getNext()->finish();

    free(outbuffer);
}

void
Pl_DCT::decompress(void* cinfo_p, PointerHolder<Buffer> b)
{
    struct jpeg_decompress_struct* cinfo =
        reinterpret_cast<jpeg_decompress_struct*>(cinfo_p);

#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic push
#       pragma GCC diagnostic ignored "-Wold-style-cast"
# endif
#endif
    jpeg_create_decompress(cinfo);
#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#       pragma GCC diagnostic pop
# endif
#endif
    jpeg_mem_src(cinfo, b->getBuffer(), b->getSize());

    (void) jpeg_read_header(cinfo, TRUE);
    (void) jpeg_calc_output_dimensions(cinfo);

    int width = cinfo->output_width * cinfo->output_components;
    JSAMPARRAY buffer = (*cinfo->mem->alloc_sarray)
        (reinterpret_cast<j_common_ptr>(cinfo), JPOOL_IMAGE, width, 1);

    (void) jpeg_start_decompress(cinfo);
    while (cinfo->output_scanline < cinfo->output_height)
    {
        (void) jpeg_read_scanlines(cinfo, buffer, 1);
        this->getNext()->write(reinterpret_cast<unsigned char*>(buffer[0]),
                               width * sizeof(buffer[0][0]));
    }
    (void) jpeg_finish_decompress(cinfo);
    this->getNext()->finish();
}
