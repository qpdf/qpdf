THIS=$(notdir $(abspath .))

all:
	$(MAKE) -C .. build_$(THIS)

check:
	$(MAKE) -C .. check_$(THIS)

clean:
	$(MAKE) -C .. clean_$(THIS)
