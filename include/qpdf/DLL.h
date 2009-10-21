#ifndef __QPDF_DLL_HH__
#define __QPDF_DLL_HH__

#if defined(_WIN32) && defined(DLL_EXPORT)
# define QPDF_DLL __declspec(dllexport)
#else
# define QPDF_DLL
#endif

#endif /* __QPDF_DLL_HH__ */
