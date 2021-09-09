#include <qpdf/QPDFEFStreamObjectHelper.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_MD5.hh>
#include <qpdf/Pl_Discard.hh>

QPDFEFStreamObjectHelper::QPDFEFStreamObjectHelper(
    QPDFObjectHandle oh) :
    QPDFObjectHelper(oh),
    m(new Members())
{
}

QPDFEFStreamObjectHelper::Members::Members()
{
}

QPDFObjectHandle
QPDFEFStreamObjectHelper::getParam(std::string const& pkey)
{
    auto params = this->oh.getDict().getKey("/Params");
    if (params.isDictionary())
    {
        return params.getKey(pkey);
    }
    return QPDFObjectHandle::newNull();
}

void
QPDFEFStreamObjectHelper::setParam(
    std::string const& pkey, QPDFObjectHandle const& pval)
{
    auto params = this->oh.getDict().getKey("/Params");
    if (! params.isDictionary())
    {
        params = QPDFObjectHandle::newDictionary();
        this->oh.getDict().replaceKey("/Params", params);
    }
    params.replaceKey(pkey, pval);
}

std::string
QPDFEFStreamObjectHelper::getCreationDate()
{
    auto val = getParam("/CreationDate");
    if (val.isString())
    {
        return val.getUTF8Value();
    }
    return "";
}

std::string
QPDFEFStreamObjectHelper::getModDate()
{
    auto val = getParam("/ModDate");
    if (val.isString())
    {
        return val.getUTF8Value();
    }
    return "";
}

size_t
QPDFEFStreamObjectHelper::getSize()
{
    auto val = getParam("/Size");
    if (val.isInteger())
    {
        return QIntC::to_size(val.getUIntValueAsUInt());
    }
    return 0;
}

std::string
QPDFEFStreamObjectHelper::getSubtype()
{
    auto val = this->oh.getDict().getKey("/Subtype");
    if (val.isName())
    {
        auto n = val.getName();
        if (n.length() > 1)
        {
            return n.substr(1);
        }
    }
    return "";
}

std::string
QPDFEFStreamObjectHelper::getChecksum()
{
    auto val = getParam("/CheckSum");
    if (val.isString())
    {
        return val.getStringValue();
    }
    return "";
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(
    QPDF& qpdf, PointerHolder<Buffer> data)
{
    return newFromStream(QPDFObjectHandle::newStream(&qpdf, data));
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(
    QPDF& qpdf, std::string const& data)
{
    return newFromStream(QPDFObjectHandle::newStream(&qpdf, data));
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::createEFStream(
    QPDF& qpdf, std::function<void(Pipeline*)> provider)
{
    auto stream = QPDFObjectHandle::newStream(&qpdf);
    stream.replaceStreamData(provider,
                             QPDFObjectHandle::newNull(),
                             QPDFObjectHandle::newNull());
    return newFromStream(stream);
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setCreationDate(std::string const& date)
{
    setParam("/CreationDate", QPDFObjectHandle::newString(date));
    return *this;
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setModDate(std::string const& date)
{
    setParam("/ModDate", QPDFObjectHandle::newString(date));
    return *this;
}

QPDFEFStreamObjectHelper&
QPDFEFStreamObjectHelper::setSubtype(std::string const& subtype)
{
    this->oh.getDict().replaceKey(
        "/Subtype", QPDFObjectHandle::newName("/" + subtype));
    return *this;
}

QPDFEFStreamObjectHelper
QPDFEFStreamObjectHelper::newFromStream(QPDFObjectHandle stream)
{
    QPDFEFStreamObjectHelper result(stream);
    stream.getDict().replaceKey(
        "/Type", QPDFObjectHandle::newName("/EmbeddedFile"));
    Pl_Discard discard;
    Pl_MD5 md5("EF md5", &discard);
    Pl_Count count("EF size", &md5);
    if (! stream.pipeStreamData(&count, nullptr, 0, qpdf_dl_all))
    {
        stream.warnIfPossible(
            "unable to get stream data for new embedded file stream");
    }
    else
    {
        result.setParam(
            "/Size", QPDFObjectHandle::newInteger(count.getCount()));
        result.setParam(
            "/CheckSum", QPDFObjectHandle::newString(
                QUtil::hex_decode(md5.getHexDigest())));
    }
    return result;
}
