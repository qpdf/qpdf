#include <qpdf/QPDF.hh>

#include <qpdf/FileInputSource.hh>

void
QPDF::JSONReactor::dictionaryStart()
{
    // QXXXXQ
}

void
QPDF::JSONReactor::arrayStart()
{
    // QXXXXQ
}

void
QPDF::JSONReactor::containerEnd(JSON const& value)
{
    // QXXXXQ
}

void
QPDF::JSONReactor::topLevelScalar()
{
    // QXXXXQ
}

bool
QPDF::JSONReactor::dictionaryItem(std::string const& key, JSON const& value)
{
    // QXXXXQ
    return true;
}

bool
QPDF::JSONReactor::arrayItem(JSON const& value)
{
    // QXXXXQ
    return true;
}

void
QPDF::createFromJSON(std::string const& json_file)
{
    createFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::createFromJSON(std::shared_ptr<InputSource> is)
{
    importJSON(is, true);
}

void
QPDF::updateFromJSON(std::string const& json_file)
{
    updateFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::updateFromJSON(std::shared_ptr<InputSource> is)
{
    importJSON(is, false);
}

void
QPDF::importJSON(std::shared_ptr<InputSource>, bool must_be_complete)
{
    // QXXXQ
}
