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

#                       1   2
# Usage: $(call compile,src,includes)
define compile
	$(CXX) $(CXXFLAGS) \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		$(CPPFLAGS) \
		-c $(1) -o $(call src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call c_compile,src,includes)
define c_compile
	$(CC) $(CFLAGS) \
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
		 -o $(2) $(1) $(4) $(3) \
		 -rpath $(libdir) -version-info $(5):$(6):$(7)
endef

#                       1    2      3       4
# Usage: $(call makebin,objs,binary,ldflags,libs)
define makebin
	$(LIBTOOL) --mode=link $(CXX) $(CXXFLAGS) $(1) -o $(2) $(4) $(3)
endef

# Install target

install: all
	./mkinstalldirs $(DESTDIR)$(libdir)/pkgconfig
	./mkinstalldirs $(DESTDIR)$(bindir)
	./mkinstalldirs $(DESTDIR)$(includedir)/qpdf
	./mkinstalldirs $(DESTDIR)$(docdir)
	./mkinstalldirs $(DESTDIR)$(mandir)/man1
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
	cp qpdf/fix-qdf $(DESTDIR)$(bindir)
	cp include/qpdf/*.h $(DESTDIR)$(includedir)/qpdf
	cp include/qpdf/*.hh $(DESTDIR)$(includedir)/qpdf
	cp doc/stylesheet.css $(DESTDIR)$(docdir)
	cp doc/qpdf-manual.html $(DESTDIR)$(docdir)
	cp doc/qpdf-manual.pdf $(DESTDIR)$(docdir)
	cp doc/*.1 $(DESTDIR)$(mandir)/man1
	cp libqpdf.pc $(DESTDIR)$(libdir)/pkgconfig
