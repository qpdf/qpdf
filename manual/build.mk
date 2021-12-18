DOC_OUT := manual/$(OUTPUT_DIR)
S_HTML_OUT := $(DOC_OUT)/singlehtml
S_HTML_TARGET := $(S_HTML_OUT)/index.html
HTML_OUT := $(DOC_OUT)/html
HTML_TARGET := $(HTML_OUT)/index.html
PDF_OUT := $(DOC_OUT)/latex
PDF_TARGET := $(PDF_OUT)/qpdf.pdf

TARGETS_manual := doc/qpdf.1 doc/fix-qdf.1 doc/zlib-flate.1
ifeq ($(BUILD_HTML),1)
TARGETS_manual += $(HTML_TARGET) $(S_HTML_TARGET)
endif
ifeq ($(BUILD_PDF),1)
TARGETS_manual += $(PDF_TARGET)
endif

MANUAL_DEPS = $(wildcard manual/*.rst) manual/conf.py

# Prevent targets that run $(SPHINX) from running in parallel by using
# order-only dependencies (the dependencies listed after the |) to
# avoid clashes in temporary files that cause the build to fail with
# the error "_pickle.UnpicklingError: pickle data was truncated"
$(HTML_TARGET): $(MANUAL_DEPS)
	$(SPHINX) -M html manual $(DOC_OUT) -W
	mkdir -p doc
	rm -rf doc/html
	cp -r $(DOC_OUT)/html doc

$(S_HTML_TARGET): $(MANUAL_DEPS) | $(HTML_TARGET)
	$(SPHINX) -M singlehtml manual $(DOC_OUT) -W
	mkdir -p doc
	rm -rf doc/singlehtml
	cp -r $(DOC_OUT)/singlehtml doc

$(PDF_TARGET): $(MANUAL_DEPS) | $(S_HTML_TARGET) $(HTML_TARGET)
	$(SPHINX) -M latexpdf manual $(DOC_OUT) -W
	mkdir -p doc
	cp $(PDF_TARGET) doc/qpdf-manual.pdf

doc/%.1: manual/%.1.in
	mkdir -p doc
	sed -e 's:@PACKAGE_VERSION@:$(PACKAGE_VERSION):g' \
	    -e 's:@docdir@:$(docdir):g' \
	    < $< > $@
