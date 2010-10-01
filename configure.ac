dnl Process this file with autoconf to produce a configure script.
dnl This config.in requires autoconf 2.5 or greater.

AC_PREREQ(2.60)
AC_INIT(qpdf,2.2.1)

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([autoconf.mk])
AC_CONFIG_FILES([manual/html.xsl manual/print.xsl])
AC_CONFIG_HEADERS([libqpdf/qpdf/qpdf-config.h])

AC_PROG_CC
AC_PROG_CXX
AC_HEADER_STDC
LT_INIT([win32-dll])
AC_PROG_LIBTOOL

USE_EXTERNAL_LIBS=0
AC_MSG_CHECKING(for whether to use external libraries distribution)
AC_ARG_ENABLE(external-libs,
   AS_HELP_STRING([--enable-external-libs],
		  [whether to use external libraries distribution]),
   [if test "$enableval" = "yes"; then
      USE_EXTERNAL_LIBS=1;
    else
      USE_EXTERNAL_LIBS=0;
    fi], [BUILD_INTERNAL_LIBS=0])
if test "$BUILD_INTERNAL_LIBS" = "0"; then
   AC_MSG_RESULT(no)
else
   AC_MSG_RESULT(yes)
fi

if test "$BUILD_INTERNAL_LIBS" = "0"; then
   AC_CHECK_HEADER(zlib.h,,[MISSING_ZLIB_H=1; MISSING_ANY=1])
   AC_SEARCH_LIBS(deflate,z zlib,,[MISSING_ZLIB=1; MISSING_ANY=1])
   AC_CHECK_HEADER(pcre.h,,[MISSING_PCRE_H=1; MISSING_ANY=1])
   AC_SEARCH_LIBS(pcre_compile,pcre,,[MISSING_PCRE=1; MISSING_ANY=1])
fi

AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_CHECK_FUNCS(random)

AC_MSG_CHECKING(for gnu make >= 3.81)
make_okay=0
if make --version >/dev/null 2>&1; then
  v=`make --version | grep 'GNU Make' | sed -e 's/.*Make //'`
  maj=`echo $v | cut -d. -f 1`
  min=`echo $v | cut -d. -f 2`
  if test $maj -gt 3 -o '(' $maj -eq 3 -a $min -ge 81 ')'; then
     make_okay=1
  fi
fi
if test "$make_okay" = "1"; then
  AC_MSG_RESULT(yes)
else
  dnl Don't set MISSING_ANY=1 -- maybe user calls make something else
  MISSING_MAKE_381=1
  ISSUE_WARNINGS=1
  AC_MSG_RESULT(no)
fi

AC_SUBST(GENDEPS)
GENDEPS=0
AC_MSG_CHECKING(for whether $CC supports -MD -MF x.dep -MP)
oCFLAGS=$CFLAGS
rm -f x.dep
CFLAGS="$CFLAGS -MD -MF x.dep -MP"
AC_TRY_COMPILE([#include <stdio.h>], [FILE* a = stdout],
	       qpdf_DEPFLAGS=yes,
	       qpdf_DEPFLAGS=no)
CFLAGS=$oCFLAGS
if test "$qpdf_DEPFLAGS" = "yes"; then
   if ! grep stdio.h x.dep >/dev/null 2>&1; then
      qpdf_DEPFLAGS=no
   fi
fi
rm -f x.dep
if test "$qpdf_DEPFLAGS" = "yes"; then
   AC_MSG_RESULT(yes)
   GENDEPS=1
else
   AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING(which build rules to use)
AC_SUBST(BUILDRULES)
AC_ARG_WITH(buildrules,
   AS_HELP_STRING([--with-buildrules=rules],
		  [which build rules to use; see README]),
   [BUILDRULES=$withval],
   [BUILDRULES=libtool])
AC_MSG_RESULT($BUILDRULES)

if test "$BUILDRULES" != "msvc"; then
   qpdf_USE_WALL=0
   AC_MSG_CHECKING(for whether $CC supports -Wall)
   oCFLAGS=$CFLAGS
   CFLAGS="$CFLAGS -Wall"
   AC_TRY_COMPILE([], [int a = 1; int b = a; a = b;],
		  qpdf_USE_WALL=1,
		  qpdf_USE_WALL=0)
   if test "$qpdf_USE_WALL" = "1"; then
      AC_MSG_RESULT(yes)
      CXXFLAGS="$CXXFLAGS -Wall"
   else
      AC_MSG_RESULT(no)
      CFLAGS=$oCFLAGS
   fi
fi

if test "$BUILDRULES" != "msvc"; then
   AC_MSG_CHECKING(for whether to use -Werror)
   AC_ARG_ENABLE(werror,
      AS_HELP_STRING([--enable-werror],
		     [whether to use werror (default is yes if -Wall works)]),
      [if test "$enableval" = "yes"; then
	 qpdf_USE_WERROR=1;
       else
	 qpdf_USE_WERROR=0;
       fi], [qpdf_USE_WERROR=$qpdf_USE_WALL])
   if test "$qpdf_USE_WERROR" = "1"; then
      AC_MSG_RESULT(yes)
      CFLAGS="$CFLAGS -Werror"
      CXXFLAGS="$CXXFLAGS -Werror"
   else
      AC_MSG_RESULT(no)
   fi
fi

AC_SUBST(SKIP_TEST_COMPARE_IMAGES)
AC_ARG_ENABLE(test-compare-images,
   AS_HELP_STRING([--enable-test-compare-images],
		  [whether to compare images in test suite; enabled by default, enabling requires ghostscript and tiffcmp to be available]),
   [if test "$enableval" = "no"; then
      SKIP_TEST_COMPARE_IMAGES=1
    else
      SKIP_TEST_COMPARE_IMAGES=0
    fi],
   [SKIP_TEST_COMPARE_IMAGES=0])

AC_ARG_WITH(docbook-xsl,
   AS_HELP_STRING([--with-docbook-xsl=DIR],
		  [location of docbook 4.x xml stylesheets]),
   [DOCBOOK_XSL=$withval],
   [DOCBOOK_XSL=/usr/share/xml/docbook/stylesheet/nwalsh])

DOCBOOK_XHTML=
AC_SUBST(DOCBOOK_XHTML)
AC_MSG_CHECKING(for xml to xhtml docbook stylesheets)
if test -f "$DOCBOOK_XSL/xhtml/docbook.xsl"; then
   DOCBOOK_XHTML="$DOCBOOK_XSL/xhtml/docbook.xsl"
   AC_MSG_RESULT($DOCBOOK_XHTML)
else
   AC_MSG_RESULT(no)
fi
DOCBOOK_FO=
AC_SUBST(DOCBOOK_FO)
AC_MSG_CHECKING(for xml to fo docbook stylesheets)
if test -f "$DOCBOOK_XSL/fo/docbook.xsl"; then
   DOCBOOK_FO="$DOCBOOK_XSL/fo/docbook.xsl"
   AC_MSG_RESULT($DOCBOOK_FO)
else
   AC_MSG_RESULT(no)
fi

DOCBOOKX_DTD=
AC_SUBST(DOCBOOKX_DTD)
AC_ARG_WITH(docbookx-dtd,
   AS_HELP_STRING([--with-docbookx-dtd=FILE],
		  [location of docbook 4.x xml DTD]),
   [DOCBOOKX_DTD=$withval],
   [DOCBOOKX_DTD=/usr/share/xml/docbook/schema/dtd/4/docbookx.dtd])
AC_MSG_CHECKING(for docbook 4.x xml DTD)
if test -f "$DOCBOOKX_DTD"; then
   AC_MSG_RESULT($DOCBOOKX_DTD)
else
   AC_MSG_RESULT(no)
fi

AC_CHECK_PROG(FOP,fop,fop,[])
AC_CHECK_PROG(XSLTPROC,xsltproc,xsltproc,[])
AC_CHECK_PROG(XMLLINT,xmllint,xmllint,[])

AC_ARG_ENABLE(doc-maintenance,
   AS_HELP_STRING([--enable-doc-maintenance],
		  [if set, enables all documentation options]),
   [if test "$enableval" = "yes"; then
      doc_default=1;
    else
      doc_default=0;
    fi],
   [doc_default=0])

BUILD_HTML=0
AC_SUBST(BUILD_HTML)
AC_ARG_ENABLE(html-doc,
   AS_HELP_STRING([--enable-html-doc],
		  [whether to build HTML documents]),
   [if test "$enableval" = "yes"; then
      BUILD_HTML=1;
    else
      BUILD_HTML=0;
    fi],
   [BUILD_HTML=$doc_default])
BUILD_PDF=0
AC_SUBST(BUILD_PDF)
AC_ARG_ENABLE(pdf-doc,
   AS_HELP_STRING([--enable-pdf-doc],
		  [whether to build PDF documents]),
   [if test "$enableval" = "yes"; then
      BUILD_PDF=1;
    else
      BUILD_PDF=0;
    fi],
   [BUILD_PDF=$doc_default])
VALIDATE_DOC=0
AC_SUBST(VALIDATE_DOC)
AC_ARG_ENABLE(validate-doc,
   AS_HELP_STRING([--enable-validate-doc],
		  [whether to validate xml document source]),
   [if test "$enableval" = "yes"; then
      VALIDATE_DOC=1;
    else
      VALIDATE_DOC=0;
    fi],
   [VALIDATE_DOC=$doc_default])

if test "$VALIDATE_DOC" = "1"; then
   if test "$XMLLINT" = ""; then
      MISSING_XMLLINT=1
      MISSING_ANY=1
   fi
fi
if test "$BUILD_HTML" = "1"; then
   if test "$XSLTPROC" = ""; then
      MISSING_XSLTPROC=1
      MISSING_ANY=1
   fi
   if test "$DOCBOOK_XHTML" = ""; then
      MISSING_DOCBOOK_XHTML=1
      MISSING_ANY=1
   fi
fi
if test "$BUILD_PDF" = "1"; then
   if test "$XSLTPROC" = ""; then
      MISSING_XSLTPROC=1
      MISSING_ANY=1
   fi
   if test "$DOCBOOK_FO" = ""; then
      MISSING_DOCBOOK_FO=1
      MISSING_ANY=1
   fi
   if test "$FOP" = ""; then
      MISSING_FOP=1
      MISSING_ANY=1
   fi
fi


if test "$MISSING_ANY" = "1"; then
  ISSUE_WARNINGS=1
fi
if test "$ISSUE_WARNINGS" = "1"; then
  echo ""
  echo ""
fi

if test "$MISSING_MAKE_381" = "1"; then
  AC_MSG_WARN(gnu make >= 3.81 is required)
fi

if test "$MISSING_ZLIB_H" = "1"; then
  AC_MSG_WARN(unable to find required header zlib.h)
fi

if test "$MISSING_ZLIB" = "1"; then
  AC_MSG_WARN(unable to find required library z (or zlib))
fi

if test "$MISSING_PCRE_H" = "1"; then
  AC_MSG_WARN(unable to find required header pcre.h)
fi

if test "$MISSING_PCRE" = "1"; then
  AC_MSG_WARN(unable to find required library pcre)
fi

if test "$MISSING_DOCBOOK_FO" = "1"; then
  AC_MSG_WARN(docbook fo stylesheets are required to build PDF documentation)
fi

if test "$MISSING_DOCBOOK_XHTML" = "1"; then
  AC_MSG_WARN(docbook xhmtl stylesheets are required to build HTML documentation)
fi

if test "$MISSING_FOP" = "1"; then
  AC_MSG_WARN(apache fop is required to build PDF documentation)
fi

if test "$MISSING_XMLLINT" = "1"; then
  AC_MSG_WARN(xmllint is required to validate documentation)
fi

if test "$MISSING_XSLTPROC" = "1"; then
  AC_MSG_WARN(xsltproc is required to build documentation)
fi

if test "$ISSUE_WARNINGS" = "1"; then
  echo ""
  echo ""
fi

if test "$MISSING_ANY" = "1"; then
  AC_MSG_ERROR(some required prerequisites were not found)
fi

# Do this last so it doesn't interfere with other tests.
if test "$USE_EXTERNAL_LIBS" = "1"; then
   # Don't actually check for the presence of this -- we document that
   # the user can run this and then edit autoconf.mk if they have too
   # much trouble getting it to work with a different compiler.
   CPPFLAGS="$CPPFLAGS -Iexternal-libs/include"
   LDFLAGS="$LDFLAGS -Lexternal-libs/lib-$BUILDRULES"
   LIBS="$LIBS -lz -lpcre"
fi

AC_OUTPUT()
