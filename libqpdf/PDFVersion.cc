#include <qpdf/PDFVersion.hh>

#include <qpdf/QUtil.hh>

PDFVersion::PDFVersion() :
    PDFVersion(0, 0, 0)
{
}

PDFVersion::PDFVersion(int major, int minor, int extension) :
    major(major),
    minor(minor),
    extension(extension)
{
}

bool
PDFVersion::operator<(PDFVersion const& rhs) const
{
    return ((this->major < rhs.major) ? true :
            (this->major > rhs.major) ? false :
            (this->minor < rhs.minor) ? true :
            (this->minor > rhs.minor) ? false :
            (this->extension < rhs.minor) ? true :
            false);
}

bool
PDFVersion::operator==(PDFVersion const& rhs) const
{
    return ((this->major == rhs.major) &&
            (this->minor == rhs.minor) &&
            (this->extension == rhs.extension));
}

void
PDFVersion::updateIfGreater(PDFVersion const& other)
{
    if (*this < other)
    {
        *this = other;
    }
}

void
PDFVersion::getVersion(std::string& version, int& extension_level) const
{
    extension_level = this->extension;
    version = QUtil::int_to_string(this->major) + "." +
        QUtil::int_to_string(this->minor);
}

int
PDFVersion::getMajor() const
{
    return this->major;
}

int
PDFVersion::getMinor() const
{
    return this->minor;
}

int
PDFVersion::getExtensionLevel() const
{
    return this->extension;
}
