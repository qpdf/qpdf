#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDFWriter.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " infile.pdf outfile.pdf [in-password]"
	      << std::endl
	      << "Invert some images in infile.pdf;"
	      << " write output to outfile.pdf" << std::endl;
    exit(2);
}

// Derive a class from StreamDataProvider to provide updated stream
// data.  The main purpose of using this object is to avoid having to
// allocate memory up front for the objects.  A real application might
// use temporary files in order to avoid having to allocate all the
// memory.  Here, we're not going to worry about that since the goal
// is really to show how to use this facility rather than to show the
// best possible way to write an image inverter.  This class still
// illustrates dynamic creation of the new stream data.
class ImageInverter: public QPDFObjectHandle::StreamDataProvider
{
  public:
    virtual ~ImageInverter()
    {
    }
    virtual void provideStreamData(int objid, int generation,
				   Pipeline* pipeline);

    // Map [og] = image object
    std::map<QPDFObjGen, QPDFObjectHandle> image_objects;
    // Map [og] = image data
    std::map<QPDFObjGen, PointerHolder<Buffer> > image_data;
};

void
ImageInverter::provideStreamData(int objid, int generation,
				 Pipeline* pipeline)
{
    // Use the object and generation number supplied to look up the
    // image data.  Then invert the image data and write the inverted
    // data to the pipeline.
    PointerHolder<Buffer> data =
        this->image_data[QPDFObjGen(objid, generation)];
    size_t size = data->getSize();
    unsigned char* buf = data->getBuffer();
    unsigned char ch;
    for (size_t i = 0; i < size; ++i)
    {
	ch = static_cast<unsigned char>(0xff) - buf[i];
	pipeline->write(&ch, 1);
    }
    pipeline->finish();
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    // For test suite
    bool static_id = false;
    if ((argc > 1) && (strcmp(argv[1], " --static-id") == 0))
    {
        static_id = true;
        --argc;
        ++argv;
    }

    if (! ((argc == 3) || (argc == 4)))
    {
	usage();
    }

    char const* infilename = argv[1];
    char const* outfilename = argv[2];
    char const* password = (argc == 4) ? argv[3] : "";

    try
    {
	QPDF qpdf;
	qpdf.processFile(infilename, password);

	ImageInverter* inv = new ImageInverter;
	PointerHolder<QPDFObjectHandle::StreamDataProvider> p = inv;

	// For each page...
	std::vector<QPDFObjectHandle> pages = qpdf.getAllPages();
	for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	     iter != pages.end(); ++iter)
	{
	    QPDFObjectHandle& page = *iter;
	    // Get all images on the page.
	    std::map<std::string, QPDFObjectHandle> images =
		page.getPageImages();
	    for (std::map<std::string, QPDFObjectHandle>::iterator iter =
		     images.begin();
		 iter != images.end(); ++iter)
	    {
		QPDFObjectHandle& image = (*iter).second;
		QPDFObjectHandle image_dict = image.getDict();
		QPDFObjectHandle color_space =
		    image_dict.getKey("/ColorSpace");
		QPDFObjectHandle bits_per_component =
		    image_dict.getKey("/BitsPerComponent");

		// For our example, we can only work with images 8-bit
		// grayscale images that we can fully decode.  Use
		// pipeStreamData with a null pipeline to determine
		// whether the image is filterable.  Directly inspect
		// keys to determine the image type.
		if (image.pipeStreamData(0, true, false, false) &&
		    color_space.isName() &&
		    bits_per_component.isInteger() &&
		    (color_space.getName() == "/DeviceGray") &&
		    (bits_per_component.getIntValue() == 8))
		{
		    // Store information about the images based on the
		    // object and generation number.  Recall that a single
		    // image object may be used more than once.
		    QPDFObjGen og = image.getObjGen();
		    if (inv->image_objects.count(og) == 0)
		    {
			inv->image_objects[og] = image;
			inv->image_data[og] = image.getStreamData();

			// Register our stream data provider for this
			// stream.  Future calls to getStreamData or
			// pipeStreamData will use the new
			// information.  Provide null for both filter
			// and decode parameters.  Note that this does
			// not mean the image data will be
			// uncompressed when we write the file.  By
			// default, QPDFWriter will use /FlateDecode
			// for anything that is uncompressed or
			// filterable in the input QPDF object, so we
			// don't have to deal with it explicitly here.
			image.replaceStreamData(
			    p,
			    QPDFObjectHandle::newNull(),
			    QPDFObjectHandle::newNull());
		    }
		}
	    }
	}

	// Write out a new file
	QPDFWriter w(qpdf, outfilename);
	if (static_id)
	{
	    // For the test suite, uncompress streams and use static
	    // IDs.
	    w.setStaticID(true); // for testing only
	}
	w.write();
	std::cout << whoami << ": new file written to " << outfilename
		  << std::endl;
    }
    catch (std::exception &e)
    {
	std::cerr << whoami << " processing file " << infilename << ": "
		  << e.what() << std::endl;
	exit(2);
    }

    return 0;
}
