#include <qpdf/PCRE.hh>
#include <qpdf/QUtil.hh>

#include <stdexcept>
#include <iostream>
#include <string.h>

PCRE::NoBackref::NoBackref() :
    std::logic_error("PCRE error: no match")
{
}

PCRE::Match::Match(int nbackrefs, char const* subject)
{
    this->init(-1, nbackrefs, subject);
}

PCRE::Match::~Match()
{
    this->destroy();
}

PCRE::Match::Match(Match const& rhs)
{
    this->copy(rhs);
}

PCRE::Match&
PCRE::Match::operator=(Match const& rhs)
{
    if (this != &rhs)
    {
	this->destroy();
	this->copy(rhs);
    }
    return *this;
}

void
PCRE::Match::init(int nmatches, int nbackrefs, char const* subject)
{
    this->nmatches = nmatches;
    this->nbackrefs = nbackrefs;
    this->subject = subject;
#if ! HAVE_STD_REGEX
    this->ovecsize = 3 * (1 + nbackrefs);
    this->ovector = 0;
    if (this->ovecsize)
    {
	this->ovector = new int[this->ovecsize];
    }
#endif
}

void
PCRE::Match::copy(Match const& rhs)
{
    this->init(rhs.nmatches, rhs.nbackrefs, rhs.subject);
#if ! HAVE_STD_REGEX
    for (int i = 0; i < this->ovecsize; ++i)
    {
	this->ovector[i] = rhs.ovector[i];
    }
#endif
}

void
PCRE::Match::destroy()
{
#if ! HAVE_STD_REGEX
    delete [] this->ovector;
#endif
}

PCRE::Match::operator bool()
{
    return (this->nmatches >= 0);
}

std::string
PCRE::Match::getMatch(int n, int flags)
{
    // This method used to be implemented in terms of
    // pcre_get_substring, but that function gives you an empty string
    // for an unmatched backreference that is in range.

    int offset;
    int length;
    try
    {
	getOffsetLength(n, offset, length);
    }
    catch (NoBackref&)
    {
	if (flags & gm_no_substring_returns_empty)
	{
	    return "";
	}
	else
	{
	    throw;
	}
    }

    return std::string(this->subject).substr(offset, length);
}

void
PCRE::Match::getOffsetLength(int n, int& offset, int& length)
{
#if HAVE_STD_REGEX
    if (this->m.empty() ||
        (n > this->nmatches - 1) ||
        (! this->m[n].matched))
    {
        throw NoBackref();
    }
    else
    {
        offset = this->m[n].first - this->subject;
        length = this->m[n].second - this->m[n].first;
    }
#else
    if ((this->nmatches < 0) ||
	(n > this->nmatches - 1) ||
	(this->ovector[n * 2] == -1))
    {
	throw NoBackref();
    }
    offset = this->ovector[n * 2];
    length = this->ovector[n * 2 + 1] - offset;
#endif
}

int
PCRE::Match::getOffset(int n)
{
    int offset;
    int length;
    this->getOffsetLength(n, offset, length);
    return offset;
}

int
PCRE::Match::getLength(int n)
{
    int offset;
    int length;
    this->getOffsetLength(n, offset, length);
    return length;
}

int
PCRE::Match::nMatches() const
{
    return this->nmatches;
}

PCRE::PCRE(char const* pattern) :
    code(0)
{
#if HAVE_STD_REGEX
    this->code = new std::regex(pattern);
    this->nbackrefs = this->code->mark_count();
#else
    char const *errptr;
    int erroffset;
    this->code = pcre_compile(pattern, 0, &errptr, &erroffset, 0);
    if (this->code)
    {
	pcre_fullinfo(this->code, 0, PCRE_INFO_CAPTURECOUNT, &(this->nbackrefs));
    }
    else
    {
	std::string message = (std::string("compilation of ") + pattern +
			  " failed at offset " +
			  QUtil::int_to_string(erroffset) + ": " +
			  errptr);
	throw std::runtime_error("PCRE error: " + message);
    }
#endif
}

PCRE::~PCRE()
{
#if HAVE_STD_REGEX
    delete this->code;
#else
    pcre_free(this->code);
#endif
}

PCRE::Match
PCRE::match(char const* subject)
{
    Match result(this->nbackrefs, subject);
#if HAVE_STD_REGEX
    if (std::regex_search(subject, result.m, *this->code))
    {
	result.nmatches = result.m.size();
        // For backward compatibility with pcre library, don't count
        // trailing unmatched subexpressions.
        while ((result.nmatches >= 0) &&
               (! result.m[result.nmatches-1].matched))
        {
            --result.nmatches;
        }
    }
    else
    {
        result.nmatches = -1;
    }
#else
    int status = pcre_exec(this->code, 0, subject, strlen(subject),
                           0 /*startoffset*/, 0 /*options*/,
			   result.ovector, result.ovecsize);
    if (status >= 0)
    {
	result.nmatches = status;
    }
    else
    {
	std::string message;

	switch (status)
	{
	  case PCRE_ERROR_NOMATCH:
	    break;

	  case PCRE_ERROR_BADOPTION:
	    message = "bad option passed to PCRE::match()";
	    throw std::logic_error(message);
	    break;

	  case PCRE_ERROR_NOMEMORY:
	    message = "insufficient memory";
	    throw std::runtime_error(message);
	    break;

	  case PCRE_ERROR_NULL:
	  case PCRE_ERROR_BADMAGIC:
	  case PCRE_ERROR_UNKNOWN_NODE:
	  default:
	    message = "pcre_exec returned " + QUtil::int_to_string(status);
	    throw std::logic_error(message);
	}
    }
#endif

    return result;
}

void
PCRE::test(int n)
{
    try
    {
	try
	{
	    PCRE pcre1("?**");
	}
	catch (std::exception& e)
	{
	    std::cout << "invalid regex" << std::endl;
	}

	PCRE pcre2("^([^\\s:]*)\\s*:\\s*(.*?)\\s*$");
	PCRE::Match m2 = pcre2.match("key: value one two three ");
	if (m2)
	{
	    std::cout << m2.nMatches() << std::endl;
	    std::cout << m2.getMatch(0) << std::endl;
	    std::cout << m2.getOffset(0) << std::endl;
	    std::cout << m2.getLength(0) << std::endl;
	    std::cout << m2.getMatch(1) << std::endl;
	    std::cout << m2.getOffset(1) << std::endl;
	    std::cout << m2.getLength(1) << std::endl;
	    std::cout << m2.getMatch(2) << std::endl;
	    std::cout << m2.getOffset(2) << std::endl;
	    std::cout << m2.getLength(2) << std::endl;
	    try
	    {
		std::cout << m2.getMatch(3) << std::endl;
	    }
	    catch (std::exception& e)
	    {
		std::cout << e.what() << std::endl;
	    }
	    try
	    {
		std::cout << m2.getOffset(3) << std::endl;
	    }
	    catch (std::exception& e)
	    {
		std::cout << e.what() << std::endl;
	    }
	}
	PCRE pcre3("^(a+)(b+)?$");
	PCRE::Match m3 = pcre3.match("aaa");
	try
	{
	    if (m3)
	    {
		std::cout << m3.nMatches() << std::endl;
		std::cout << m3.getMatch(0) << std::endl;
		std::cout << m3.getMatch(1) << std::endl;
		std::cout << "-"
			  << m3.getMatch(
			      2, Match::gm_no_substring_returns_empty)
			  << "-" << std::endl;
		std::cout << "hello" << std::endl;
		std::cout << m3.getMatch(2) << std::endl;
		std::cout << "can't see this" << std::endl;
	    }
	}
	catch (std::exception& e)
	{
	    std::cout << e.what() << std::endl;
	}

	// backref: 1   2 3        4      5
	PCRE pcre4("^((?:(a(b)?)(?:,(c))?)|(c))?$");
	static char const* candidates[] = {
	    "qqqcqqq",		// no match
	    "ab,c",		// backrefs: 0, 1, 2, 3, 4
	    "ab",		// backrefs: 0, 1, 2, 3
	    "a",		// backrefs: 0, 1, 2
	    "a,c",		// backrefs: 0, 1, 2, 4
	    "c",		// backrefs: 0, 1, 5
	    "",			// backrefs: 0
	    0
	};
	for (char const** p = candidates; *p; ++p)
	{
	    PCRE::Match m(pcre4.match(*p));
	    if (m)
	    {
		int nmatches = m.nMatches();
		for (int i = 0; i < nmatches; ++i)
		{
		    std::cout << *p << ": " << i << ": ";
		    try
		    {
			std::string match = m.getMatch(i);
			std::cout << match;
		    }
		    catch (NoBackref&)
		    {
			std::cout << "no backref (getMatch)";
		    }
		    std::cout << std::endl;

		    std::cout << *p << ": " << i << ": ";
		    try
		    {
			int offset;
			int length;
			m.getOffsetLength(i, offset, length);
			std::cout << offset << ", " << length;
		    }
		    catch (NoBackref&)
		    {
			std::cout << "no backref (getOffsetLength)";
		    }
		    std:: cout << std::endl;
		}
	    }
	    else
	    {
		std::cout << *p << ": no match" << std::endl;
	    }
	}
    }
    catch (std::exception& e)
    {
	std::cout << "unexpected exception: " << e.what() << std::endl;
    }
}
