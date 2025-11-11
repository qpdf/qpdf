#include <qpdf/QPDFEFStreamObjectHelper.hh>

#include <qpdf/Pipeline_private.hh>
#include <qpdf/Pl_MD5.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QUtil.hh>

using namespace qpdf;

class QPDFEFStreamObjectHelper::Members
{
};

QPDFEFStreamObjectHelper::QPDFEFStreamObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

QPDFObjectHandle
QPDFEFStreamObjectHelper::getParam(std::string const& pkey)
{
    if (auto result = oh().getDict()["/Params"][pkey]) {
        return result;
    }
    return {};
}

void
QPDFEFStreamObjectHelper::setParam(std::string const& pkey, QPDFObjectHandle const& pval)
{
    if (Dictionary Params = oh().getDict()["/Params"]) {
        Params.replace(pkey, pval);
        return;
    }
    oh().getDict().replaceKey("/Params", Dictionary({{pkey, pval}}));
}

std::string
QPDFEFStreamObjectHelper::getCreationDate()
{
    if (String CreationDate = getParam("/CreationDate")) {
        return CreationDate.utf8_value();
    }
    return {};
}

std::string
QPDFEFStreamObjectHelper::getModDate()
{
    if (String ModDate = getParam("/ModDate")) {
        return ModDate.utf8_value();
    }
    return {};
}

size_t
QPDFEFStreamObjectHelper::getSize()
{
    if (Integer Size = getParam("/Size")) {
        return Size.value<size_t>();
    }
    return 0;
}

std::string
QPDFEFStreamObjectHelper::getSubtype()
{
    if (Name Subtype = oh().getDict()["/Subtype"]) {
        if (Subtype.value().size() > 1) {
            return Subtype.value().substr(1);
        }
    }
    return {};
}

std::string
QPDFEFStreamObjectHelper::getChecksum()
{
    if (String CheckSum = getParam("/CheckSum")) {
        return CheckSum.value();
    }
    return {};
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(QPDF& qpdf, std::shared_ptr<Buffer> data)
{
    return newFromStream(qpdf.newStream(std::move(data)));
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(QPDF& qpdf, std::string const& data)
{
    return newFromStream(qpdf.newStream(data));
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(QPDF& qpdf, std::function<void(Pipeline*)> provider)
{
    auto stream = qpdf.newStream();
    stream.replaceStreamData(provider, {}, {});
    return newFromStream(stream);
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setCreationDate(std::string const& date)
{
    setParam("/CreationDate", String(date));
    return *this;
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setModDate(std::string const& date)
{
    setParam("/ModDate", String(date));
    return *this;
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setSubtype(std::string const& subtype)
{
    oh().getDict().replaceKey("/Subtype", Name("/" + subtype));
    return *this;
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::newFromStream(QPDFObjectHandle stream)
{
    QPDFEFStreamObjectHelper result(stream);
    stream.getDict().replaceKey("/Type", Name("/EmbeddedFile"));
    // The PDF spec specifies use of MD5 here and notes that it is not to be used for security. MD5
    // is known to be insecure.
    Pl_MD5 md5("EF md5");
    pl::Count count(0, &md5);
    if (!stream.pipeStreamData(&count, nullptr, 0, qpdf_dl_all)) {
        stream.warn("unable to get stream data for new embedded file stream");
    } else {
        result.setParam("/Size", Integer(count.getCount()));
        result.setParam("/CheckSum", String(QUtil::hex_decode(md5.getHexDigest())));
    }
    return result;
}
