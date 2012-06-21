# --- Required interface definitions ---

OBJ=o
LOBJ=o

# Usage: $(call libname,base)
define libname
lib$(1).a
endef

# Usage: $(call binname,base)
define binname
$(1).exe
endef

# --- Required rule definitions ---

#                       1   2
# Usage: $(call compile,src,includes)
define compile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call c_compile,src,includes)
define c_compile
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call c_src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call libcompile,src,includes)
define libcompile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DDLL_EXPORT \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call c_libcompile,src,includes)
define c_libcompile
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDLL_EXPORT \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call c_src_to_obj,$(1))
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
	$(DLLTOOL) -l $(2) -D $$(basename `echo $(2) | sed -e 's,/lib\(.*\).a,/\1,'`$(shell expr $(5) - $(7)).dll) $(1); \
	$(CXX) -shared -o `echo $(2) | sed -e 's,/lib\(.*\).a,/\1,'`$(shell expr $(5) - $(7)).dll \
		$(1) $(3) $(4)
endef

#                       1    2      3       4
# Usage: $(call makebin,objs,binary,ldflags,libs)
define makebin
	$(CXX) $(CXXFLAGS) $(1) -o $(2) $(3) $(4)
endef

# Install target

INSTALL_DIR = install-mingw$(WINDOWS_WORDSIZE)
STATIC_LIB_NAME = libqpdf.a
include make/installwin.mk
install: installwin
	$(STRIP) $(DEST)/bin/*.exe
	$(STRIP) $(DEST)/bin/*.dll
