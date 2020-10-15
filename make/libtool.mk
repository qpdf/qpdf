# --- Required interface definitions ---

# LIBTOOL needs bash
SHELL=/bin/bash

OBJ=o
LOBJ=lo

# Usage: $(call libname,base)
define libname
lib$(1).la
endef

# Usage: $(call binname,base)
define binname
$(1)
endef

# --- Private definitions ---

ifeq ($(HAVE_LD_VERSION_SCRIPT), 1)
LD_VERSION_FLAGS=-Wl,--version-script=libqpdf.map
else
LD_VERSION_FLAGS=
endif

# Usage: $(call libdepflags,$(basename obj))
# Usage: $(call fixdeps,$(basename obj))
ifeq ($(GENDEPS),1)
libdepflags=-MD -MF $(1).tdep -MP
fixdeps=sed -e 's/\.o:/.lo:/' < $(1).tdep > $(1).dep

else
libdepflags=
fixdeps=
endif

# --- Required rule definitions ---

#                       1   2        3
# Usage: $(call compile,src,includes,xflags)
define compile
	$(CXX) $(CXXFLAGS) $(3) \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		$(CPPFLAGS) \
		-c $(1) -o $(call src_to_obj,$(1))
endef

#                         1   2        3
# Usage: $(call c_compile,src,includes,xflags)
define c_compile
	$(CC) $(CFLAGS) $(3) \
		$(call depflags,$(basename $(call c_src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		$(CPPFLAGS) \
		-c $(1) -o $(call c_src_to_obj,$(1))
endef

#                          1   2
# Usage: $(call libcompile,src,includes)
define libcompile
	$(LIBTOOL) --quiet --mode=compile \
		$(CXX) $(CXXFLAGS) \
		$(call libdepflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		$(CPPFLAGS) \
		-c $(1) -o $(call src_to_obj,$(1)); \
	$(call fixdeps,$(basename $(call src_to_obj,$(1))))
endef

#                          1   2
# Usage: $(call libcompile,src,includes)
define c_libcompile
	$(LIBTOOL) --quiet --mode=compile \
		$(CC) $(CFLAGS) \
		$(call libdepflags,$(basename $(call c_src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		$(CPPFLAGS) \
		-c $(1) -o $(call c_src_to_obj,$(1)); \
	$(call fixdeps,$(basename $(call src_to_obj,$(1))))
endef

#                        1    2
# Usage: $(call makeslib,objs,library)
define makeslib
	$(RM) $2
	$(AR) cru $(2) $(1)
	$(RANLIB) $(2)
endef

#                       1    2       3       4    5       6        7
# Usage: $(call makelib,objs,library,ldflags,libs,current,revision,age)
define makelib
	$(LIBTOOL) --mode=link \
		$(CXX) $(CXXFLAGS) $(LD_VERSION_FLAGS) \
		 -o $(2) $(1) $(3) $(4) \
		 -rpath $(libdir) -version-info $(5):$(6):$(7) -no-undefined
endef

#                       1    2      3       4    5
# Usage: $(call makebin,objs,binary,ldflags,libs,xlinkflags)
define makebin
	$(LIBTOOL) --mode=link $(CXX) $(CXXFLAGS) $(5) $(1) -o $(2) $(3) $(4)
endef

# Install target

install: all
	./mkinstalldirs -m 0755 $(DESTDIR)$(libdir)/pkgconfig
	./mkinstalldirs -m 0755 $(DESTDIR)$(bindir)
	./mkinstalldirs -m 0755 $(DESTDIR)$(includedir)/qpdf
	./mkinstalldirs -m 0755 $(DESTDIR)$(docdir)
	./mkinstalldirs -m 0755 $(DESTDIR)$(mandir)/man1
	$(LIBTOOL) --mode=install ./install-sh \
		libqpdf/$(OUTPUT_DIR)/libqpdf.la \
		$(DESTDIR)$(libdir)/libqpdf.la
	$(LIBTOOL) --finish $(DESTDIR)$(libdir)
	$(LIBTOOL) --mode=install ./install-sh \
		qpdf/$(OUTPUT_DIR)/qpdf \
		$(DESTDIR)$(bindir)/qpdf
	$(LIBTOOL) --mode=install ./install-sh \
		zlib-flate/$(OUTPUT_DIR)/zlib-flate \
		$(DESTDIR)$(bindir)/zlib-flate
	$(LIBTOOL) --mode=install ./install-sh \
		qpdf/$(OUTPUT_DIR)/fix-qdf \
		$(DESTDIR)$(bindir)/fix-qdf
	./install-sh -m 0644 include/qpdf/*.h $(DESTDIR)$(includedir)/qpdf
	./install-sh -m 0644 include/qpdf/*.hh $(DESTDIR)$(includedir)/qpdf
	./install-sh -m 0644 doc/stylesheet.css $(DESTDIR)$(docdir)
	./install-sh -m 0644 libqpdf.pc $(DESTDIR)$(libdir)/pkgconfig
	if [ -f doc/qpdf-manual.html ]; then \
		./install-sh -m 0644 doc/qpdf-manual.html $(DESTDIR)$(docdir); \
	fi
	if [ -f doc/qpdf-manual.pdf ]; then \
		./install-sh -m 0644 doc/qpdf-manual.pdf $(DESTDIR)$(docdir); \
	fi
	./install-sh -m 0644 doc/*.1 $(DESTDIR)$(mandir)/man1
