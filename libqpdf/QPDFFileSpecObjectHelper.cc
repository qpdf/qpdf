#include <qpdf/QPDFFileSpecObjectHelper.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <string>
#include <vector>

QPDFFileSpecObjectHelper::QPDFFileSpecObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
    if (!oh.isDictionary()) {
        oh.warnIfPossible("Embedded file object is not a dictionary");
        return;
    }
    if (!oh.isDictionaryOfType("/Filespec")) {
        oh.warnIfPossible("Embedded file object's type is not /Filespec");
    }
}

static std::vector<std::string> name_keys = {"/UF", "/F", "/Unix", "/DOS", "/Mac"};

std::string
QPDFFileSpecObjectHelper::getDescription()
{
    std::string result;
    auto desc = oh().getKey("/Desc");
    if (desc.isString()) {
        result = desc.getUTF8Value();
    }
    return result;
}

std::string
QPDFFileSpecObjectHelper::getFilename()
{
    for (auto const& i: name_keys) {
        auto k = oh().getKey(i);
        if (k.isString()) {
            return k.getUTF8Value();
        }
    }
    return "";
}

std::map<std::string, std::string>
QPDFFileSpecObjectHelper::getFilenames()
{
    std::map<std::string, std::string> result;
    for (auto const& i: name_keys) {
        auto k = oh().getKey(i);
        if (k.isString()) {
            result[i] = k.getUTF8Value();
        }
    }
    return result;
}

QPDFObjectHandle
QPDFFileSpecObjectHelper::getEmbeddedFileStream(std::string const& key)
{
    auto ef = oh().getKey("/EF");
    if (!ef.isDictionary()) {
        return QPDFObjectHandle::newNull();
    }
    if (!key.empty()) {
        return ef.getKey(key);
    }
    for (auto const& i: name_keys) {
        auto k = ef.getKey(i);
        if (k.isStream()) {
            return k;
        }
    }
    return QPDFObjectHandle::newNull();
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
    auto oh = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    oh.replaceKey("/Type", QPDFObjectHandle::newName("/Filespec"));
    QPDFFileSpecObjectHelper result(oh);
    result.setFilename(filename);
    auto ef = QPDFObjectHandle::newDictionary();
    ef.replaceKey("/F", efsoh.getObjectHandle());
    ef.replaceKey("/UF", efsoh.getObjectHandle());
    oh.replaceKey("/EF", ef);
    return result;
}

QPDFFileSpecObjectHelper&
QPDFFileSpecObjectHelper::setDescription(std::string const& desc)
{
    oh().replaceKey("/Desc", QPDFObjectHandle::newUnicodeString(desc));
    return *this;
}

QPDFFileSpecObjectHelper&
QPDFFileSpecObjectHelper::setFilename(
    std::string const& unicode_name, std::string const& compat_name)
{
    auto uf = QPDFObjectHandle::newUnicodeString(unicode_name);
    oh().replaceKey("/UF", uf);
    if (compat_name.empty()) {
        QTC::TC("qpdf", "QPDFFileSpecObjectHelper empty compat_name");
        oh().replaceKey("/F", uf);
    } else {
        QTC::TC("qpdf", "QPDFFileSpecObjectHelper non-empty compat_name");
        oh().replaceKey("/F", QPDFObjectHandle::newString(compat_name));
    }
    return *this;
}
