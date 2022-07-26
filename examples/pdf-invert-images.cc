#include <qpdf/Buffer.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile.pdf outfile.pdf [in-password]"
              << std::endl
              << "Invert some images in infile.pdf;"
              << " write output to outfile.pdf" << std::endl;
    exit(2);
}

// Derive a class from StreamDataProvider to provide updated stream
// data. The main purpose of using this object is to avoid having to
// allocate memory up front for the objects. We want to replace the
// stream data with a function of the original stream data. In order
// to do this without actually holding all the images in memory, we
// create copies of the streams. Copying the streams doesn't actually
// copy the data. Internally, the qpdf library is holding onto the
// location of the original stream data, which makes it possible for
// the StreamDataProvider to access it when it needs it.
class ImageInverter: public QPDFObjectHandle::StreamDataProvider
{
  public:
    virtual ~ImageInverter() = default;
    virtual void
    provideStreamData(QPDFObjGen const& og, Pipeline* pipeline) override;

    void registerImage(
        QPDFObjectHandle image,
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> self);

  private:
    std::map<QPDFObjGen, QPDFObjectHandle> copied_images;
};

void
ImageInverter::registerImage(
    QPDFObjectHandle image,
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> self)
{
    // replaceStreamData requires a pointer holder to the stream data
    // provider, but there's no way for us to generate one ourselves,
    // so we have to have it handed to us. Don't be tempted to have
    // the class contain a std::shared_ptr to itself as a member. Doing
    // this will prevent the class from ever being deleted since the
    // reference count will never drop to zero (and std::shared_ptr
    // doesn't have weak references).

    QPDFObjGen og(image.getObjGen());
    // Store information about the images based on the object and
    // generation number. Recall that a single image object may be
    // used more than once, so no need to update the same stream
    // multiple times.
    if (this->copied_images.count(og) > 0) {
        return;
    }
    this->copied_images[og] = image.copyStream();

    // Register our stream data provider for this stream. Future calls
    // to getStreamData or pipeStreamData will use the new
    // information. Provide null for both filter and decode
    // parameters. Note that this does not mean the image data will be
    // uncompressed when we write the file. By default, QPDFWriter
    // will use /FlateDecode for anything that is uncompressed or
    // filterable in the input QPDF object, so we don't have to deal
    // with it explicitly here. We could explicitly use /DCTDecode and
    // write through a DCT filter if we wanted.
    image.replaceStreamData(
        self, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
}

void
ImageInverter::provideStreamData(QPDFObjGen const& og, Pipeline* pipeline)
{
    // Use the object and generation number supplied to look up the
    // image data.  Then invert the image data and write the inverted
    // data to the pipeline.
    std::shared_ptr<Buffer> data =
        this->copied_images[og].getStreamData(qpdf_dl_all);
    size_t size = data->getSize();
    unsigned char* buf = data->getBuffer();
    unsigned char ch;
    for (size_t i = 0; i < size; ++i) {
        ch = QIntC::to_uchar(0xff - buf[i]);
        pipeline->write(&ch, 1);
    }
    pipeline->finish();
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For test suite
    bool static_id = false;
    if ((argc > 1) && (strcmp(argv[1], " --static-id") == 0)) {
        static_id = true;
        --argc;
        ++argv;
    }

    if (!((argc == 3) || (argc == 4))) {
        usage();
    }

    char const* infilename = argv[1];
    char const* outfilename = argv[2];
    char const* password = (argc == 4) ? argv[3] : "";

    try {
        QPDF qpdf;
        qpdf.processFile(infilename, password);

        ImageInverter* inv = new ImageInverter;
        auto p = std::shared_ptr<QPDFObjectHandle::StreamDataProvider>(inv);

        // For each page...
        for (auto& page: QPDFPageDocumentHelper(qpdf).getAllPages()) {
            // Get all images on the page.
            for (auto& iter: page.getImages()) {
                QPDFObjectHandle& image = iter.second;
                QPDFObjectHandle image_dict = image.getDict();
                QPDFObjectHandle color_space = image_dict.getKey("/ColorSpace");
                QPDFObjectHandle bits_per_component =
                    image_dict.getKey("/BitsPerComponent");

                // For our example, we can only work with images 8-bit
                // grayscale images that we can fully decode.  Use
                // pipeStreamData with a null pipeline to determine
                // whether the image is filterable.  Directly inspect
                // keys to determine the image type.
                if (image.pipeStreamData(
                        nullptr, qpdf_ef_compress, qpdf_dl_all) &&
                    color_space.isNameAndEquals("/DeviceGray") &&
                    bits_per_component.isInteger() &&
                    (bits_per_component.getIntValue() == 8)) {
                    inv->registerImage(image, p);
                }
            }
        }

        // Write out a new file
        QPDFWriter w(qpdf, outfilename);
        if (static_id) {
            // For the test suite, uncompress streams and use static
            // IDs.
            w.setStaticID(true); // for testing only
        }
        w.write();
        std::cout << whoami << ": new file written to " << outfilename
                  << std::endl;
    } catch (std::exception& e) {
        std::cerr << whoami << " processing file " << infilename << ": "
                  << e.what() << std::endl;
        exit(2);
    }

    return 0;
}
