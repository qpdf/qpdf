#include <qpdf/QPDFFileSpecObjectHelper.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <array>
#include <string>

using namespace std::literals;
using namespace qpdf;

class QPDFFileSpecObjectHelper::Members
{
};

QPDFFileSpecObjectHelper::QPDFFileSpecObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
    if (!oh.isDictionary()) {
        warn("Embedded file object is not a dictionary");
        return;
    }
    if (!oh.isDictionaryOfType("/Filespec")) {
        warn("Embedded file object's type is not /Filespec");
    }
}

static const std::array<std::string, 5> name_keys = {"/UF"s, "/F"s, "/Unix"s, "/DOS"s, "/Mac"s};

std::string
QPDFFileSpecObjectHelper::getDescription()
{
    if (String Desc = oh().getKey("/Desc")) {
        return Desc.utf8_value();
    }
    return {};
}

std::string
QPDFFileSpecObjectHelper::getFilename()
{
    for (auto const& i: name_keys) {
        if (String k = oh()[i]) {
            return k.utf8_value();
        }
    }
    return {};
}

std::map<std::string, std::string>
QPDFFileSpecObjectHelper::getFilenames()
{
    std::map<std::string, std::string> result;
    for (auto const& i: name_keys) {
        if (String k = oh()[i]) {
            result[i] = k.utf8_value();
        }
    }
    return result;
}

QPDFObjectHandle
QPDFFileSpecObjectHelper::getEmbeddedFileStream(std::string const& key)
{
    if (Dictionary EF = oh()["/EF"]) {
        if (!key.empty() && EF.contains(key)) {
            if (auto result = EF[key]) {
                return result;
            }
        }
        for (auto const& i: name_keys) {
            if (Stream k = EF[i]) {
                return k;
            }
        }
    }
    return Null::temp();
}

QPDFObjectHandle
QPDFFileSpecObjectHelper::getEmbeddedFileStreams()
{
    return oh().getKey("/EF");
}

QPDFFileSpecObjectHelper
QPDFFileSpecObjectHelper::createFileSpec(
    QPDF& qpdf, std::string const& filename, std::string const& fullpath)
{
    return createFileSpec(
        qpdf,
        filename,
        QPDFEFStreamObjectHelper::createEFStream(qpdf, QUtil::file_provider(fullpath)));
}

QPDFFileSpecObjectHelper
QPDFFileSpecObjectHelper::createFileSpec(
    QPDF& qpdf, std::string const& filename, QPDFEFStreamObjectHelper efsoh)
{
    auto UF = String::utf16(filename);
    return {qpdf.makeIndirectObject(Dictionary(
        {{"/Type", Name("/Filespec")},
         {"/F", UF},
         {"/UF", UF},
         {"/EF", Dictionary({{"/F", efsoh}, {"/UF", efsoh}})}}))};
}

QPDFFileSpecObjectHelper&
QPDFFileSpecObjectHelper::setDescription(std::string const& desc)
{
    oh().replaceKey("/Desc", String::utf16(desc));
    return *this;
}

QPDFFileSpecObjectHelper&
QPDFFileSpecObjectHelper::setFilename(
    std::string const& unicode_name, std::string const& compat_name)
{
    auto uf = String::utf16(unicode_name);
    oh().replaceKey("/UF", uf);
    if (compat_name.empty()) {
        oh().replaceKey("/F", uf);
    } else {
        oh().replaceKey("/F", String(compat_name));
    }
    return *this;
}
