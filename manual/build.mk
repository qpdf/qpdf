DOC_OUT := manual/$(OUTPUT_DIR)
S_HTML_OUT := $(DOC_OUT)/singlehtml
S_HTML_TARGET := $(S_HTML_OUT)/index.html
HTML_OUT := $(DOC_OUT)/html
HTML_TARGET := $(HTML_OUT)/index.html
PDF_OUT := $(DOC_OUT)/latex
PDF_TARGET := $(PDF_OUT)/qpdf.pdf

TARGETS_manual := doc/qpdf.1 doc/fix-qdf.1 doc/zlib-flate.1
ifeq ($(BUILD_HTML),1)
TARGETS_manual += doc/qpdf-manual.html $(HTML_TARGET)
endif
ifeq ($(BUILD_PDF),1)
TARGETS_manual += doc/qpdf-manual.pdf
endif

# Prevent targets that run $(SPHINX) from running in parallel by using
# order-only dependencies (the dependencies listed after the |) to
# avoid clashes in temporary files that cause the build to fail with
# the error "_pickle.UnpicklingError: pickle data was truncated"
$(HTML_TARGET): manual/index.rst
	$(SPHINX) -M html manual $(DOC_OUT) -W

$(S_HTML_TARGET): manual/index.rst | $(HTML_TARGET)
	$(SPHINX) -M singlehtml manual $(DOC_OUT) -W

$(PDF_TARGET): manual/index.rst | $(S_HTML_TARGET) $(HTML_TARGET)
	$(SPHINX) -M latexpdf manual $(DOC_OUT) -W

# This depends on sphinx-build's singlehtml target creating index.html
# and a _static directory. If that changes, this code has to be
# adjusted. It will also be necessary to adjust the install target in
# make/libtool.mk.
doc/qpdf-manual.html: $(S_HTML_TARGET)
	mkdir -p doc
	@if [ "$(shell find $(S_HTML_OUT)/ -mindepth 1 -type d -print)" != \
             "$(S_HTML_OUT)/_static" ]; then \
	    echo "***"; \
	    echo Expected only directory in $(S_HTML_OUT) to be _static; \
	    echo "***"; \
	    false; \
	fi
	cp $< $@
	mkdir -p doc/_static
	cp -p $(S_HTML_OUT)/_static/* doc/_static

doc/qpdf-manual.pdf: $(PDF_TARGET)
	mkdir -p doc
	cp $< $@

doc/%.1: manual/%.1.in
	mkdir -p doc
	sed -e 's:@PACKAGE_VERSION@:$(PACKAGE_VERSION):g' \
	    -e 's:@docdir@:$(docdir):g' \
	    < $< > $@
