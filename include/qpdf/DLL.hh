#ifndef __QPDF_DLL_HH__
#define __QPDF_DLL_HH__

#ifdef _WIN32
# define DLL_EXPORT __declspec(dllexport)
#else
# define DLL_EXPORT
#endif

#endif // __QPDF_DLL_HH__
