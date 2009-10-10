include make/$(BUILDRULES).mk

define firstelem
$(word 1,$(subst /, ,$(1)))
endef
SPC := $(subst /, ,/)
define lastelem
$(subst $(SPC),/,$(word $(words $(subst /, ,$(1))),$(subst /, ,$(1))))
endef
define objbase
$(patsubst %.$(2),%.$(3),$(firstelem)/$(OUTPUT_DIR)/$(lastelem))
endef

# Usage: $(call src_to_obj,srcs)
define src_to_obj
$(foreach F,$(1),$(call objbase,$(F),cc,$(OBJ)))
endef

# Usage: $(call c_src_to_obj,srcs)
define c_src_to_obj
$(foreach F,$(1),$(call objbase,$(F),c,$(OBJ)))
endef

# Usage: $(call src_to_lobj,srcs)
define src_to_lobj
$(foreach F,$(1),$(call objbase,$(F),cc,$(LOBJ)))
endef

# Usage: $(call c_src_to_lobj,srcs)
define c_src_to_lobj
$(foreach F,$(1),$(call objbase,$(F),c,$(LOBJ)))
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
