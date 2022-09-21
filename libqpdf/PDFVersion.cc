#include <qpdf/PDFVersion.hh>

#include <qpdf/QUtil.hh>

PDFVersion::PDFVersion() :
    PDFVersion(0, 0, 0)
{
}

PDFVersion::PDFVersion(
    int major_version, int minor_version, int extension_level) :
    major_version(major_version),
    minor_version(minor_version),
    extension_level(extension_level)
{
}

bool
PDFVersion::operator<(PDFVersion const& rhs) const
{
    return (
        (this->major_version < rhs.major_version)           ? true
            : (this->major_version > rhs.major_version)     ? false
            : (this->minor_version < rhs.minor_version)     ? true
            : (this->minor_version > rhs.minor_version)     ? false
            : (this->extension_level < rhs.extension_level) ? true
                                                            : false);
}

bool
PDFVersion::operator==(PDFVersion const& rhs) const
{
    return (
        (this->major_version == rhs.major_version) &&
        (this->minor_version == rhs.minor_version) &&
        (this->extension_level == rhs.extension_level));
}

void
PDFVersion::updateIfGreater(PDFVersion const& other)
{
    if (*this < other) {
        *this = other;
    }
}

void
PDFVersion::getVersion(std::string& version, int& extension_level) const
{
    extension_level = this->extension_level;
    version = std::to_string(this->major_version) + "." +
        std::to_string(this->minor_version);
}

int
PDFVersion::getMajor() const
{
    return this->major_version;
}

int
PDFVersion::getMinor() const
{
    return this->minor_version;
}

int
PDFVersion::getExtensionLevel() const
{
    return this->extension_level;
}
