#include <qpdf/ResourceFinder.hh>

ResourceFinder::ResourceFinder() :
    saw_bad(false)
{
}

void
ResourceFinder::handleToken(QPDFTokenizer::Token const& token)
{
    if ((token.getType() == QPDFTokenizer::tt_word) &&
        (! this->last_name.empty()))
    {
        this->names.insert(this->last_name);
    }
    else if (token.getType() == QPDFTokenizer::tt_name)
    {
        this->last_name =
            QPDFObjectHandle::newName(token.getValue()).getName();
    }
    else if (token.getType() == QPDFTokenizer::tt_bad)
    {
        saw_bad = true;
    }
    writeToken(token);
}

std::set<std::string> const&
ResourceFinder::getNames() const
{
    return this->names;
}

bool
ResourceFinder::sawBad() const
{
    return this->saw_bad;
}
