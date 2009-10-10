TARGETS_lib := pcre
SRCS_lib_pcre := maketables.c get.c study.c pcre.c
RULES := ccxx

pcre.$(OBJ): chartables.c

chartables.c: $(DFTABLES_DIR)/$(call binname,dftables)
	./$(DFTABLES_DIR)/dftables chartables.c
