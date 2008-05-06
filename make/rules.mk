
# Usage: $(call src_to_obj,srcs)
define src_to_obj
$(foreach F,$(1),$(dir $(F))$(OUTPUT_DIR)/$(patsubst %.cc,%.o,$(notdir $(F))))
endef

# Usage: $(call src_to_lobj,srcs)
define src_to_lobj
$(foreach F,$(1),$(dir $(F))$(OUTPUT_DIR)/$(patsubst %.cc,%.lo,$(notdir $(F))))
endef

# Usage: $(call obj_to_dep,objs)
define obj_to_dep
$(patsubst %.o,%.dep,$(1))
endef

# Usage: $(call lobj_to_dep,objs)
define lobj_to_dep
$(patsubst %.lo,%.dep,$(1))
endef

# Usage: $(call depflags,$(basename obj))
ifeq ($(GENDEPS),1)
depflags=-MD -MF $(1).dep -MP
else
depflags=
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

#                       1   2
# Usage: $(call compile,src,includes)
define compile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		$(call depflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call src_to_obj,$(1))
endef

#                          1   2
# Usage: $(call libcompile,src,includes)
define libcompile
	$(LIBTOOL) --quiet --mode=compile \
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) \
		$(call libdepflags,$(basename $(call src_to_obj,$(1)))) \
		$(foreach I,$(2),-I$(I)) \
		-c $(1) -o $(call src_to_obj,$(1)); \
	$(call fixdeps,$(basename $(call src_to_obj,$(1))))
endef

#                       1    2       3       4        5
# Usage: $(call makelib,objs,library,current,revision,age)
define makelib
	$(LIBTOOL) --mode=link \
		$(CXX) $(CXXFLAGS) -o $(2) $(1) $(LDFLAGS) $(LIBS) \
		 -rpath $(libdir) -version-info $(3):$(4):$(5)
endef

#                       1    2
# Usage: $(call makebin,objs,binary)
define makebin
	$(LIBTOOL) --mode=link \
		$(CXX) $(CXXFLAGS) $(1) -o $(2) $(LDFLAGS) \
		-Llibqpdf/$(OUTPUT_DIR) -lqpdf $(LIBS)
endef

