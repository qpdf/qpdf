#include <qpdf/QPDF_Dictionary.hh>

#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Name.hh>

QPDF_Dictionary::QPDF_Dictionary(
    std::map<std::string, QPDFObjectHandle> const& items) :
    items(items)
{
}

QPDF_Dictionary::~QPDF_Dictionary()
{
}

void
QPDF_Dictionary::releaseResolved()
{
    for (std::map<std::string, QPDFObjectHandle>::iterator iter =
	     this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	QPDFObjectHandle::ReleaseResolver::releaseResolved((*iter).second);
    }
}

std::string
QPDF_Dictionary::unparse()
{
    std::string result = "<< ";
    for (std::map<std::string, QPDFObjectHandle>::iterator iter =
	     this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	result += QPDF_Name::normalizeName((*iter).first) +
	    " " + (*iter).second.unparse() + " ";
    }
    result += ">>";
    return result;
}

bool
QPDF_Dictionary::hasKey(std::string const& key)
{
    return ((this->items.count(key) > 0) &&
	    (! this->items[key].isNull()));
}

QPDFObjectHandle
QPDF_Dictionary::getKey(std::string const& key)
{
    // PDF spec says fetching a non-existent key from a dictionary
    // returns the null object.
    if (this->items.count(key))
    {
	// May be a null object
	return (*(this->items.find(key))).second;
    }
    else
    {
	return QPDFObjectHandle::newNull();
    }
}

std::set<std::string>
QPDF_Dictionary::getKeys()
{
    std::set<std::string> result;
    for (std::map<std::string, QPDFObjectHandle>::const_iterator iter =
	     this->items.begin();
	 iter != this->items.end(); ++iter)
    {
	if (hasKey((*iter).first))
	{
	    result.insert((*iter).first);
	}
    }
    return result;
}

void
QPDF_Dictionary::replaceKey(std::string const& key,
			    QPDFObjectHandle const& value)
{
    // add or replace value
    this->items[key] = value;
}

void
QPDF_Dictionary::removeKey(std::string const& key)
{
    // no-op if key does not exist
    this->items.erase(key);
}

void
QPDF_Dictionary::replaceOrRemoveKey(std::string const& key,
				    QPDFObjectHandle value)
{
    if (value.isNull())
    {
	removeKey(key);
    }
    else
    {
	replaceKey(key, value);
    }
}
