#include <qpdf/SF_FlateLzwDecode.hh>

#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_LZWDecoder.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_TIFFPredictor.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>

SF_FlateLzwDecode::SF_FlateLzwDecode(bool lzw) :
    lzw(lzw),
    // Initialize values to their defaults as per the PDF spec
    predictor(1),
    columns(1),
    colors(1),
    bits_per_component(8),
    early_code_change(true)
{
}

bool
SF_FlateLzwDecode::setDecodeParms(QPDFObjectHandle decode_parms)
{
    if (decode_parms.isNull()) {
        return true;
    }

    bool filterable = true;
    std::set<std::string> keys = decode_parms.getKeys();
    for (auto const& key: keys) {
        auto value = decode_parms.getKey(key).asInteger();
        if (key == "/Predictor") {
            if (value) {
                this->predictor = value;
                if (!((this->predictor == 1) || (this->predictor == 2) ||
                      ((this->predictor >= 10) && (this->predictor <= 15)))) {
                    filterable = false;
                }
            } else {
                filterable = false;
            }
        } else if ((key == "/Columns") || (key == "/Colors") || (key == "/BitsPerComponent")) {
            if (value) {
                if (key == "/Columns") {
                    this->columns = value;
                } else if (key == "/Colors") {
                    this->colors = value;
                } else if (key == "/BitsPerComponent") {
                    this->bits_per_component = value;
                }
            } else {
                filterable = false;
            }
        } else if (lzw && (key == "/EarlyChange")) {
            if (value) {
                int earlychange = value;
                this->early_code_change = (earlychange == 1);
                if (!(earlychange == 0 || earlychange == 1)) {
                    filterable = false;
                }
            } else {
                filterable = false;
            }
        }
    }

    if ((this->predictor > 1) && (this->columns == 0)) {
        filterable = false;
    }

    return filterable;
}

Pipeline*
SF_FlateLzwDecode::getDecodePipeline(Pipeline* next)
{
    std::shared_ptr<Pipeline> pipeline;
    if ((this->predictor >= 10) && (this->predictor <= 15)) {
        QTC::TC("qpdf", "SF_FlateLzwDecode PNG filter");
        pipeline = std::make_shared<Pl_PNGFilter>(
            "png decode",
            next,
            Pl_PNGFilter::a_decode,
            QIntC::to_uint(this->columns),
            QIntC::to_uint(this->colors),
            QIntC::to_uint(this->bits_per_component));
        this->pipelines.push_back(pipeline);
        next = pipeline.get();
    } else if (this->predictor == 2) {
        QTC::TC("qpdf", "SF_FlateLzwDecode TIFF predictor");
        pipeline = std::make_shared<Pl_TIFFPredictor>(
            "tiff decode",
            next,
            Pl_TIFFPredictor::a_decode,
            QIntC::to_uint(this->columns),
            QIntC::to_uint(this->colors),
            QIntC::to_uint(this->bits_per_component));
        this->pipelines.push_back(pipeline);
        next = pipeline.get();
    }

    if (lzw) {
        pipeline = std::make_shared<Pl_LZWDecoder>("lzw decode", next, early_code_change);
    } else {
        pipeline = std::make_shared<Pl_Flate>("stream inflate", next, Pl_Flate::a_inflate);
    }
    this->pipelines.push_back(pipeline);
    return pipeline.get();
}

std::shared_ptr<QPDFStreamFilter>
SF_FlateLzwDecode::flate_factory()
{
    return std::make_shared<SF_FlateLzwDecode>(false);
}

std::shared_ptr<QPDFStreamFilter>
SF_FlateLzwDecode::lzw_factory()
{
    return std::make_shared<SF_FlateLzwDecode>(true);
}
