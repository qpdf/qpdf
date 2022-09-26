#include <qpdf/QPDF.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

#include <cstring>
#include <exception>
#include <iostream>
#include <memory>

// This example shows you everything you need to know to implement a
// custom stream filter for encoding and decoding as well as a stream
// data provider that modifies the stream's dictionary. This example
// uses the pattern of having the stream data provider class use a
// second QPDF instance with copies of streams from the original QPDF
// so that the stream data provider can access the original stream
// data. This is implemented very efficiently inside the qpdf library as
// the second QPDF instance knows how to read the stream data from the
// original input file, so no extra copies of the original stream data
// are made.

// This example creates an imaginary filter called /XORDecode. There
// is no such filter in PDF, so the streams created by the example
// would not be usable by any PDF reader. However, the techniques here
// would work if you were going to implement support for a filter that
// qpdf does not support natively. For example, using the techniques
// shown here, it would be possible to create an application that
// downsampled or re-encoded images or that re-compressed streams
// using a more efficient "deflate" implementation than zlib.

// Comments appear throughout the code describing each piece of code
// and its purpose. You can read the file top to bottom, or you can
// start with main() and follow the flow.

// Please also see the test suite, qtest/custom-filter.test, which
// contains additional comments describing how to observe the results
// of running this example on test files that are specifically crafted
// for it.

static char const* whoami = nullptr;

class Pl_XOR: public Pipeline
{
    // This class implements a Pipeline for the made-up XOR decoder.
    // It is initialized with a single-byte "key" and just XORs each
    // byte with that key. This makes it reversible, so there is no
    // distinction between encoding and decoding.

  public:
    Pl_XOR(char const* identifier, Pipeline* next, unsigned char key);
    virtual ~Pl_XOR() = default;
    virtual void write(unsigned char const* data, size_t len) override;
    virtual void finish() override;

  private:
    unsigned char key;
};

Pl_XOR::Pl_XOR(char const* identifier, Pipeline* next, unsigned char key) :
    Pipeline(identifier, next),
    key(key)
{
}

void
Pl_XOR::write(unsigned char const* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char p = data[i] ^ this->key;
        getNext()->write(&p, 1);
    }
}

void
Pl_XOR::finish()
{
    getNext()->finish();
}

class SF_XORDecode: public QPDFStreamFilter
{
    // This class implements a QPDFStreamFilter that knows how to
    // validate and interpret decode parameters (/DecodeParms) for the
    // made-up /XORDecode stream filter. Since this is not a real
    // stream filter, no actual PDF reader would know how to interpret
    // it. This is just to illustrate how to create a stream filter.
    // In main(), we call QPDF::registerStreamFilter to tell the
    // library about the filter. See comments in QPDFStreamFilter.hh
    // for details on how to implement the methods. For purposes of
    // example, we are calling this a "specialized" compression
    // filter, which just means QPDF assumes that it should not
    // "uncompress" the stream by default.
  public:
    virtual ~SF_XORDecode() = default;
    virtual bool setDecodeParms(QPDFObjectHandle decode_parms) override;
    virtual Pipeline* getDecodePipeline(Pipeline* next) override;
    virtual bool isSpecializedCompression() override;

  private:
    unsigned char key;
    // It is the responsibility of the QPDFStreamFilter implementation
    // to ensure that the pipeline returned by getDecodePipeline() is
    // deleted when the class is deleted. The easiest way to do this
    // is to stash the pipeline in a std::shared_ptr, which enables us
    // to use the default destructor implementation.
    std::shared_ptr<Pl_XOR> pipeline;
};

bool
SF_XORDecode::setDecodeParms(QPDFObjectHandle decode_parms)
{
    // For purposes of example, we store the key in a separate stream.
    // We could just as well store the key directly in /DecodeParms,
    // but this example uses a stream to illustrate how one might do
    // that. For example, if implementing /JBIG2Decode, one would need
    // to handle the /JBIG2Globals key, which points to a stream. See
    // comments in SF_XORDecode::registerStream for additional notes
    // on this.
    try {
        // Expect /DecodeParms to be a dictionary with a /KeyStream
        // key that points to a one-byte stream whose single byte is
        // the key. If we are successful at retrieving the key, return
        // true, indicating that we are able to process with the given
        // decode parameters. Under any other circumstances, return
        // false. For other examples of QPDFStreamFilter
        // implementations, look at the classes whose names start with
        // SF_ in the qpdf library implementation.
        auto buf = decode_parms.getKey("/KeyStream").getStreamData();
        if (buf->getSize() != 1) {
            return false;
        }
        this->key = buf->getBuffer()[0];
        return true;
    } catch (std::exception& e) {
        std::cerr << "Error extracting key for /XORDecode: " << e.what()
                  << std::endl;
    }
    return false;
}

Pipeline*
SF_XORDecode::getDecodePipeline(Pipeline* next)
{
    // Return a pipeline that the qpdf library should pass the stream
    // data through. The pipeline should receive encoded data and pass
    // decoded data to "next". getDecodePipeline() can always count on
    // setDecodeParms() having been called first. The setDecodeParms()
    // method should store any parameters needed by the pipeline. To
    // ensure that the pipeline we return disappears when the class
    // disappears, stash it in a std::shared_ptr<Pl_XOR> and retrieve
    // the raw pointer from there.
    this->pipeline = std::make_shared<Pl_XOR>("xor", next, this->key);
    return this->pipeline.get();
}

bool
SF_XORDecode::isSpecializedCompression()
{
    // The default implementation of QPDFStreamFilter would return
    // false, so if you want a specialized or lossy compression
    // filter, override one of the methods as described in
    // QPDFStreamFilter.hh.
    return true;
}

class StreamReplacer: public QPDFObjectHandle::StreamDataProvider
{
    // This class implements a StreamDataProvider that, under specific
    // conditions, replaces the stream data with data encoded with the
    // made-up /XORDecode filter.

    // The flow for this class is as follows:
    //
    // * The main application iterates through streams that should be
    //   replaced and calls registerStream. registerStream in turn
    //   calls maybeReplace passing nullptr to pipeline and the
    //   address of a valid QPDFObjectHandle to dict_updates. The
    //   stream passed in for this call is the stream for the original
    //   QPDF object. It has not yet been altered, so we have access
    //   to its original dictionary and data. As described in the
    //   method, the method when called in this way makes a
    //   determination as to whether the stream should be replaced. If
    //   so, registerStream makes whatever changes are required. We
    //   have to do this now because we can't modify the stream during
    //   the writing process.
    //
    // * provideStreamData(), which is called by QPDFWriter during the
    //   write process, actually writes the modified stream data. It
    //   calls maybeReplace again, but this time it passes a valid
    //   pipeline and passes nullptr to dict_updates. In this mode,
    //   the stream dictionary has already been altered, and the
    //   original stream data is no longer directly accessible. Trying
    //   to retrieve the stream data would cause an infinite loop because
    //   it would just end up calling provideStreamData again. This is
    //   why maybeReplace uses a stashed copy of the original stream.

    // Additional explanation can be found in the method
    // implementations.

  public:
    StreamReplacer(QPDF* pdf);
    virtual ~StreamReplacer() = default;
    virtual void
    provideStreamData(QPDFObjGen const& og, Pipeline* pipeline) override;

    void registerStream(
        QPDFObjectHandle stream,
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> self);

  private:
    bool maybeReplace(
        QPDFObjGen const& og,
        QPDFObjectHandle& stream,
        Pipeline* pipeline,
        QPDFObjectHandle* dict_updates);

    // Hang onto a reference to the QPDF object containing the streams
    // we are replacing. We need this to create a new stream.
    QPDF* pdf;

    // Map the object/generation in original file to the copied stream
    // in "other". We use this to retrieve the original data.
    std::map<QPDFObjGen, QPDFObjectHandle> copied_streams;

    // Each stream gets is own "key" for the XOR filter. We use a
    // single instance of StreamReplacer for all streams, so stash all
    // the keys here.
    std::map<QPDFObjGen, unsigned char> keys;
};

StreamReplacer::StreamReplacer(QPDF* pdf) :
    pdf(pdf)
{
}

bool
StreamReplacer::maybeReplace(
    QPDFObjGen const& og,
    QPDFObjectHandle& stream,
    Pipeline* pipeline,
    QPDFObjectHandle* dict_updates)
{
    // As described in the class comments, this method is called
    // twice. Before writing has started pipeline is nullptr, and
    // dict_updates is provided. In this mode, we figure out whether
    // we should replace the stream and, if so, take care of the
    // necessary setup. When we are actually ready to supply the data,
    // this method is called again with pipeline populated and
    // dict_updates as a nullptr. In this mode, we are not allowed to
    // change anything, since writing is already in progress. We
    // must simply provide the stream data.

    // The return value indicates whether or not we should replace the
    // stream. If the first call returns false, there will be no
    // second call. If the second call returns false, something went
    // wrong since the method should always make the same decision for
    // a given stream.

    // For this example, all the determination logic could have
    // appeared inside the if (dict_updates) block rather than being
    // duplicated, but in some cases, there may be a reason to
    // duplicate things. For example, if you wanted to write code that
    // re-encoded an image if the new encoding was more efficient,
    // you'd have to actually try it out. Then you would either have
    // to cache the result somewhere or just repeat the calculations,
    // depending on space/time constraints, etc.

    // In our contrived example, we are replacing the data for all
    // streams that have /DoXOR = true in the stream dictionary. If
    // this were a more realistic application, our criteria would be
    // more sensible. For example, an image downsampler might choose
    // to replace a stream that represented an image with a high pixel
    // density.
    auto dict = stream.getDict();
    auto mark = dict.getKey("/DoXOR");
    if (!(mark.isBool() && mark.getBoolValue())) {
        return false;
    }

    // We can't replace the stream data if we can't get the original
    // stream data for any reason. A more realistic application may
    // actually look at the data here as well, or it may be able to
    // make all its decisions from the stream dictionary. However,
    // it's a good idea to make sure we can retrieve the filtered data
    // if we are going to need it later.
    std::shared_ptr<Buffer> out;
    try {
        out = stream.getStreamData();
    } catch (...) {
        return false;
    }

    if (dict_updates) {
        // It's not safe to make any modifications to any objects
        // during the writing process since the updated objects may
        // have already been written. In this mode, when dict_updates
        // is provided, we have not started writing. Store the
        // modifications we intend to make to the stream dictionary
        // here. We're just storing /OrigLength for purposes of
        // example. Again, a realistic application would make other
        // changes. For example, an image resampler might change the
        // dimensions or other properties of the image.
        dict_updates->replaceKey(
            "/OrigLength",
            QPDFObjectHandle::newInteger(QIntC::to_longlong(out->getSize())));
        // We are also storing the "key" that we will access when
        // writing the data.
        this->keys[og] = QIntC::to_uchar(
            (og.getObj() * QIntC::to_int(out->getSize())) & 0xff);
    }

    if (pipeline) {
        unsigned char key = this->keys[og];
        Pl_XOR p("xor", pipeline, key);
        p.write(out->getBuffer(), out->getSize());
        p.finish();
    }
    return true;
}

void
StreamReplacer::registerStream(
    QPDFObjectHandle stream,
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> self)
{
    QPDFObjGen og(stream.getObjGen());

    // We don't need to process a stream more than once. In this
    // example, we are just iterating through objects, but if we were
    // doing something like iterating through images on pages, we
    // might realistically encounter the same stream more than once.
    if (this->copied_streams.count(og) > 0) {
        return;
    }
    // Store something in copied_streams so that we don't
    // double-process even in the negative case. This gets replaced
    // later if needed.
    this->copied_streams[og] = QPDFObjectHandle::newNull();

    // Call maybeReplace with dict_updates. In this mode, it
    // determines whether we should replace the stream data and, if
    // so, supplies dictionary updates we should make.
    bool should_replace = false;
    QPDFObjectHandle dict_updates = QPDFObjectHandle::newDictionary();
    try {
        should_replace = maybeReplace(og, stream, nullptr, &dict_updates);
    } catch (std::exception& e) {
        stream.warnIfPossible(
            std::string("exception while attempting to replace: ") + e.what());
    }

    if (should_replace) {
        // Copy the stream so we can get to the original data from the
        // stream data provider. This doesn't actually copy any data,
        // but the copy retains the original stream data after the
        // original one is modified.
        this->copied_streams[og] = stream.copyStream();
        // Update the stream dictionary with any changes.
        auto dict = stream.getDict();
        for (auto const& k: dict_updates.getKeys()) {
            dict.replaceKey(k, dict_updates.getKey(k));
        }
        // Create the key stream that will be referenced from
        // /DecodeParms. We have to do this now since you can't modify
        // or create objects during write.
        char p[1] = {static_cast<char>(this->keys[og])};
        std::string p_str(p, 1);
        QPDFObjectHandle dp_stream = this->pdf->newStream(p_str);
        // Create /DecodeParms as expected by our fictitious
        // /XORDecode filter.
        QPDFObjectHandle decode_parms =
            QPDFObjectHandle::newDictionary({{"/KeyStream", dp_stream}});
        stream.replaceStreamData(
            self, QPDFObjectHandle::newName("/XORDecode"), decode_parms);
        // Further, if /ProtectXOR = true, we disable filtering on write
        // so that QPDFWriter will not decode the stream even though we
        // have registered a stream filter for /XORDecode.
        auto protect = dict.getKey("/ProtectXOR");
        if (protect.isBool() && protect.getBoolValue()) {
            stream.setFilterOnWrite(false);
        }
    }
}

void
StreamReplacer::provideStreamData(QPDFObjGen const& og, Pipeline* pipeline)
{
    QPDFObjectHandle orig = this->copied_streams[og];
    // call maybeReplace again, this time with the pipeline and no
    // dict_updates. In this mode, maybeReplace doesn't make any
    // changes. We have to hand it the original stream data, which we
    // get from copied_streams.
    if (!maybeReplace(og, orig, pipeline, nullptr)) {
        // Since this only gets called for streams we already
        // determined we are replacing, a false return would indicate
        // a logic error.
        throw std::logic_error(
            "should_replace return false in provideStreamData");
    }
}

static void
process(
    char const* infilename, char const* outfilename, bool decode_specialized)
{
    QPDF qpdf;
    qpdf.processFile(infilename);

    // Create a single StreamReplacer instance. The interface requires
    // a std::shared_ptr in various places, so allocate a StreamReplacer
    // and stash it in a std::shared_ptr.
    StreamReplacer* replacer = new StreamReplacer(&qpdf);
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> p(replacer);

    for (auto& o: qpdf.getAllObjects()) {
        if (o.isStream()) {
            // Call registerStream for every stream. Only ones that
            // registerStream decides to replace will actually be
            // replaced.
            replacer->registerStream(o, p);
        }
    }

    QPDFWriter w(qpdf, outfilename);
    if (decode_specialized) {
        w.setDecodeLevel(qpdf_dl_specialized);
    }
    // For the test suite, use static IDs.
    w.setStaticID(true); // for testing only
    w.write();
    std::cout << whoami << ": new file written to " << outfilename << std::endl;
}

static void
usage()
{
    std::cerr << "\n"
              << "Usage: " << whoami
              << " [--decode-specialized] infile outfile\n"
              << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    char const* infilename = nullptr;
    char const* outfilename = nullptr;
    bool decode_specialized = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--decode-specialized") == 0) {
            decode_specialized = true;
        } else if (!infilename) {
            infilename = argv[i];
        } else if (!outfilename) {
            outfilename = argv[i];
        } else {
            usage();
        }
    }
    if (!(infilename && outfilename)) {
        usage();
    }

    try {
        // Register our fictitious filter. This enables QPDFWriter to
        // decode our streams. This is not a real filter, so no real
        // PDF reading application would be able to interpret it. This
        // is just for illustrative purposes.
        QPDF::registerStreamFilter(
            "/XORDecode", [] { return std::make_shared<SF_XORDecode>(); });
        // Do the actual processing.
        process(infilename, outfilename, decode_specialized);
    } catch (std::exception& e) {
        std::cerr << whoami << ": exception: " << e.what() << std::endl;
        exit(2);
    }

    return 0;
}
