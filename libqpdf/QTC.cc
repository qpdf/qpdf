#include <qpdf/QTC.hh>

#include <set>
#include <stdio.h>
#include <qpdf/QUtil.hh>

static bool tc_active(char const* const scope)
{
    std::string value;
    return (QUtil::get_env("TC_SCOPE", &value) && (value == scope));
}

void QTC::TC(char const* const scope, char const* const ccase, int n)
{
    static std::set<std::pair<std::string, int> > cache;

    if (! tc_active(scope))
    {
	return;
    }

    std::string filename;
#ifdef _WIN32
# define TC_ENV "TC_WIN_FILENAME"
#else
# define TC_ENV "TC_FILENAME"
#endif
    if (! QUtil::get_env(TC_ENV, &filename))
    {
	return;
    }
#undef TC_ENV

    if (cache.count(std::make_pair(ccase, n)))
    {
	return;
    }
    cache.insert(std::make_pair(ccase, n));

    FILE* tc = QUtil::safe_fopen(filename.c_str(), "ab");
    fprintf(tc, "%s %d\n", ccase, n);
    fclose(tc);
}
