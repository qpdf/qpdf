include make/$(BUILDRULES).mk

# Usage: $(call src_to_obj,srcs)
define src_to_obj
$(foreach F,$(1),$(dir $(F))$(OUTPUT_DIR)/$(patsubst %.cc,%.$(OBJ),$(notdir $(F))))
endef

# Usage: $(call c_src_to_obj,srcs)
define c_src_to_obj
$(foreach F,$(1),$(dir $(F))$(OUTPUT_DIR)/$(patsubst %.c,%.$(OBJ),$(notdir $(F))))
endef

# Usage: $(call src_to_lobj,srcs)
define src_to_lobj
$(foreach F,$(1),$(dir $(F))$(OUTPUT_DIR)/$(patsubst %.cc,%.$(LOBJ),$(notdir $(F))))
endef

# Usage: $(call obj_to_dep,objs)
define obj_to_dep
$(patsubst %.$(OBJ),%.dep,$(1))
endef

# Usage: $(call lobj_to_dep,objs)
define lobj_to_dep
$(patsubst %.$(LOBJ),%.dep,$(1))
endef

# Usage: $(call depflags,$(basename obj))
ifeq ($(GENDEPS),1)
depflags=-MD -MF $(1).dep -MP
else
depflags=
endif
