Summary: Command-line tools and library for transforming PDF files
Name: qpdf
Version: 3.0.a0
Release: 1%{?dist}
License: Artistic
Group: System Environment/Libraries
URL: http://qpdf.sourceforge.net/

Source: %{name}-%{version}.tar.gz

Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: zlib-devel
BuildRequires: pcre-devel

%description
QPDF is a program that does structural, content-preserving
transformations on PDF files.  It could have been called something
like pdf-to-pdf.  It also provides many useful capabilities to
developers of PDF-producing software or for people who just want to
look at the innards of a PDF file to learn more about how they work.

QPDF offers many capabilities such as linearization (web
optimization), encrypt, and decription of PDF files.  Note that QPDF
does not have the capability to create PDF files from scratch; it is
only used to create PDF files with special characteristics starting
from other PDF files or to inspect or extract information from
existing PDF files.

%package devel
Summary: Development files for qpdf PDF manipulation library
Group: Development/Libraries
Requires: %{name} = %{version}-%{release} zlib-devel pcre-devel

%description devel
The qpdf-devel package contains header files and libraries necessary
for developing programs using the qpdf library.

%package static
Summary: Static QPDF library
Group: Development/Libraries
Requires: %{name}-devel = %{version}-%{release}

%description static
The qpdf-static package contains the static qpdf library.

%prep
%setup -q

%build
%configure --disable-test-compare-images --docdir='${datarootdir}'/doc/%{name}-%{version}
make %{?_smp_mflags}
make check

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall
# %doc below clobbers our docdir, so we have to copy it to a safe
# place so we can install it using %doc.  We should still set docdir
# properly when configuring so that it gets substituted properly by
# autoconf.
cp -a $RPM_BUILD_ROOT%{_datadir}/doc/%{name}-%{version} install-docs
mkdir -p install-examples/examples
cp -p examples/*.cc examples/*.c install-examples/examples
# Red Hat doesn't ship .la files.
rm -f $RPM_BUILD_ROOT%{_libdir}/libqpdf.la

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc README TODO Artistic-2.0 install-docs/*
%{_bindir}/*
%{_libdir}/libqpdf*.so.*
%{_mandir}/man1/*

%files devel
%defattr(-,root,root)
%doc install-examples/examples
%{_includedir}/*
%{_libdir}/libqpdf*.so

%files static
%defattr(-,root,root)
%{_libdir}/libqpdf*.a

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Mon Apr 28 2008 Jay Berkenbilt <ejb@ql.org> - 2.0-1
- Initial packaging
