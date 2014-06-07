# --- Required interface definitions ---

OBJ=obj
LOBJ=obj

# Usage: $(call libname,base)
define libname
$(1).lib
endef

# Usage: $(call binname,base)
define binname
$(1).exe
endef

# --- Local Changes ---

# Filter out -g
CFLAGS := $(filter-out -g,$(CFLAGS))
CXXFLAGS := $(filter-out -g,$(CXXFLAGS))

clean::
	$(RM) *.pdb

# --- Required rule definitions ---

#                       1   2
# Usage: $(call compile,src,includes)
define compile
	cl /nologo /O2 /Zi /Gy /EHsc /MD /TP /GR $(CPPFLAGS) $(CXXFLAGS) \
		$(foreach I,$(2),-I$(I)) \
		/c $(1) /Fo$(call src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call c_compile,src,includes)
define c_compile
	cl /nologo /O2 /Zi /Gy /EHsc /MD $(CPPFLAGS) $(CFLAGS) \
		$(foreach I,$(2),-I$(I)) \
		/c $(1) /Fo$(call c_src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call libcompile,src,includes)
define libcompile
	cl /nologo /O2 /Zi /Gy /EHsc /MD /TP /GR $(CPPFLAGS) $(CXXFLAGS) \
		-DDLL_EXPORT $(foreach I,$(2),-I$(I)) \
		/c $(1) /Fo$(call src_to_obj,$(1))
endef

#                       1   2
# Usage: $(call c_libcompile,src,includes)
define c_libcompile
	cl /nologo /O2 /Zi /Gy /EHsc /MD $(CPPFLAGS) $(CXXFLAGS) \
		-DDLL_EXPORT $(foreach I,$(2),-I$(I)) \
		/c $(1) /Fo$(call c_src_to_obj,$(1))
endef

#                        1    2
# Usage: $(call makeslib,objs,library)
define makeslib
	lib /nologo /OUT:$(2) $(1)
endef

#                       1    2       3       4    5       6        7
# Usage: $(call makelib,objs,library,ldflags,libs,current,revision,age)
define makelib
	cl /nologo /O2 /Zi /Gy /EHsc /MD /LD /Fe$(basename $(2))$(shell expr $(5) - $(7)).dll $(1) \
		/link /SUBSYSTEM:CONSOLE,5.01 /incremental:no \
		$(foreach L,$(subst -L,,$(3)),/LIBPATH:$(L)) \
		$(foreach L,$(subst -l,,$(4)),$(L).lib)
	if [ -f $(basename $(2))$(shell expr $(5) - $(7)).dll.manifest ]; then \
		mt.exe -nologo -manifest $(basename $(2))$(shell expr $(5) - $(7)).dll.manifest \
			-outputresource:$(basename $(2))$(shell expr $(5) - $(7)).dll\;2; \
	fi
	mv $(basename $(2))$(shell expr $(5) - $(7)).lib $(2)
endef

#                       1    2      3       4
# Usage: $(call makebin,objs,binary,ldflags,libs)
define makebin
	cl /nologo /O2 /Zi /Gy /EHsc /MD $(1) \
		/link /SUBSYSTEM:CONSOLE,5.01 /incremental:no /OUT:$(2) \
		$(foreach L,$(subst -L,,$(3)),/LIBPATH:$(L)) \
		$(foreach L,$(subst -l,,$(4)),$(L).lib)
	if [ -f $(2).manifest ]; then \
		mt.exe -nologo -manifest $(2).manifest \
			-outputresource:$(2)\;2; \
	fi
endef

# Install target

INSTALL_DIR = install-msvc$(WINDOWS_WORDSIZE)
STATIC_LIB_NAME = qpdf.lib
include make/installwin.mk
install: installwin
