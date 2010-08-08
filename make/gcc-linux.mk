#
# This file primarily exists for making it possible to test the build
# system and external library support from Linux.  However, its use is
# strongly discouraged; use the (default) libtool rules for building
# on Linux.
#

# --- Required interface definitions ---

OBJ=o
LOBJ=o

# Usage: $(call libname,base)
define libname
lib$(1).so
endef

# Usage: $(call binname,base)
define binname
$(1)
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
		$(call depflags,$(basename $(call c_src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call c_src_to_obj,$(1))
endef

define libcompile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fpic \
		$(call depflags,$(basename $(call src_to_lobj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call src_to_lobj,$(1))
endef
define c_libcompile
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -fpic \
		$(call depflags,$(basename $(call c_src_to_lobj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call c_src_to_lobj,$(1))
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
	$(RM) $(2) $(2).*
	major=$$(( $(5) - $(7) )); \
	versuffix=$$major.$(7).$(6); \
	$(CXX) $(CXXFLAGS) -shared -o $(2).$$versuffix $(1) \
		-Wl,--soname -Wl,`basename $(2)`.$$major \
		$(3) $(4) && \
	ln -s `basename $(2)`.$$versuffix $(2) && \
	ln -s `basename $(2)`.$$versuffix $(2).$$major
endef

#                       1    2      3       4
# Usage: $(call makebin,objs,binary,ldflags,libs)
define makebin
	$(CXX) $(CXXFLAGS) $(1) -o $(2) $(LDFLAGS) $(3) $(4)
endef

# Install target

install: all
	@echo Automated installation is not supported for buildrules=$(BUILDRULES)
