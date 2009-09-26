// This is a C++ wrapper class around Philip Hazel's perl-compatible
// regular expressions library.
//

#ifndef __PCRE_HH__
#define __PCRE_HH__

#ifdef _WIN32
# define PCRE_STATIC
#endif
#include <pcre.h>
#include <string>

#include <qpdf/QEXC.hh>

// Note: this class does not encapsulate all features of the PCRE
// package -- only those that I actually need right now are here.

class PCRE
{
  public:
    class Exception: public QEXC::General
    {
      public:
	DLL_EXPORT
	Exception(std::string const& message);
	DLL_EXPORT
	virtual ~Exception() throw() {}
    };

    // This is thrown when an attempt is made to access a non-existent
    // back reference.
    class NoBackref: public Exception
    {
      public:
	DLL_EXPORT
	NoBackref();
	DLL_EXPORT
	virtual ~NoBackref() throw() {}
    };

    class Match
    {
	friend class PCRE;
      public:
	DLL_EXPORT
	Match(int nbackrefs, char const* subject);
	DLL_EXPORT
	Match(Match const&);
	DLL_EXPORT
	Match& operator=(Match const&);
	DLL_EXPORT
	~Match();
	DLL_EXPORT
	operator bool();

	// All the back reference accessing routines may throw the
	// special exception NoBackref (derived from Exception) if the
	// back reference does not exist.  Exception will be thrown
	// for other error conditions.  This allows callers to trap
	// this condition explicitly when they care about the
	// difference between a backreference matching an empty string
	// and not matching at all.

	// see getMatch flags below
	DLL_EXPORT
	std::string getMatch(int n, int flags = 0)
	    throw(QEXC::General, Exception);
	DLL_EXPORT
	void getOffsetLength(int n, int& offset, int& length) throw(Exception);
	DLL_EXPORT
	int getOffset(int n) throw(Exception);
	DLL_EXPORT
	int getLength(int n) throw(Exception);

	// nMatches returns the number of available matches including
	// match 0 which is the whole string.  In other words, if you
	// have one backreference in your expression and the
	// expression matches, nMatches() will return 2, getMatch(0)
	// will return the whole string, getMatch(1) will return the
	// text that matched the backreference, and getMatch(2) will
	// throw an exception because it is out of range.
	DLL_EXPORT
	int nMatches() const;

	// Flags for getMatch

	// getMatch on a substring that didn't match should return
	// empty string instead of throwing an exception
	static int const gm_no_substring_returns_empty = (1 << 0);

      private:
	void init(int nmatches, int nbackrefs, char const* subject);
	void copy(Match const&);
	void destroy();

	int nbackrefs;
	char const* subject;
	int* ovector;
	int ovecsize;
	int nmatches;
    };

    // The value passed in as options is passed to pcre_exec.  See man
    // pcreapi for details.
    DLL_EXPORT
    PCRE(char const* pattern, int options = 0) throw(Exception);
    DLL_EXPORT
    ~PCRE();

    DLL_EXPORT
    Match match(char const* subject, int options = 0, int startoffset = 0,
		int size = -1)
	throw(QEXC::General, Exception);

    DLL_EXPORT
    static void test(int n = 0);

  private:
    // prohibit copying and assignment
    PCRE(PCRE const&);
    PCRE& operator=(PCRE const&);

    pcre* code;
    int nbackrefs;
};

#endif // __PCRE_HH__
