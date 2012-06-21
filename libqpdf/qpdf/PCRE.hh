// This is a C++ wrapper class around Philip Hazel's perl-compatible
// regular expressions library.
//

#ifndef __PCRE_HH__
#define __PCRE_HH__

#include <qpdf/DLL.h>

#ifdef _WIN32
# define PCRE_STATIC
#endif
#include <pcre.h>
#include <string>
#include <stdexcept>

// Note: this class does not encapsulate all features of the PCRE
// package -- only those that I actually need right now are here.

class PCRE
{
  public:
    // This is thrown when an attempt is made to access a non-existent
    // back reference.
    class NoBackref: public std::logic_error
    {
      public:
	QPDF_DLL
	NoBackref();
	virtual ~NoBackref() throw() {}
    };

    class Match
    {
	friend class PCRE;
      public:
	QPDF_DLL
	Match(int nbackrefs, char const* subject);
	QPDF_DLL
	Match(Match const&);
	QPDF_DLL
	Match& operator=(Match const&);
	QPDF_DLL
	~Match();
	QPDF_DLL
	operator bool();

	// All the back reference accessing routines may throw the
	// special exception NoBackref (derived from Exception) if the
	// back reference does not exist.  Exception will be thrown
	// for other error conditions.  This allows callers to trap
	// this condition explicitly when they care about the
	// difference between a backreference matching an empty string
	// and not matching at all.

	// see getMatch flags below
	QPDF_DLL
	std::string getMatch(int n, int flags = 0);
	QPDF_DLL
	void getOffsetLength(int n, int& offset, int& length);
	QPDF_DLL
	int getOffset(int n);
	QPDF_DLL
	int getLength(int n);

	// nMatches returns the number of available matches including
	// match 0 which is the whole string.  In other words, if you
	// have one backreference in your expression and the
	// expression matches, nMatches() will return 2, getMatch(0)
	// will return the whole string, getMatch(1) will return the
	// text that matched the backreference, and getMatch(2) will
	// throw an exception because it is out of range.
	QPDF_DLL
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
    QPDF_DLL
    PCRE(char const* pattern, int options = 0);
    QPDF_DLL
    ~PCRE();

    QPDF_DLL
    Match match(char const* subject, int options = 0, int startoffset = 0,
		int size = -1);

    QPDF_DLL
    static void test(int n = 0);

  private:
    // prohibit copying and assignment
    PCRE(PCRE const&);
    PCRE& operator=(PCRE const&);

    pcre* code;
    int nbackrefs;
};

#endif // __PCRE_HH__
