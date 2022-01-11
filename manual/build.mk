DOC_OUT := manual/$(OUTPUT_DIR)
S_HTML_OUT := $(DOC_OUT)/singlehtml
S_HTML_TARGET := $(S_HTML_OUT)/index.html
HTML_OUT := $(DOC_OUT)/html
HTML_TARGET := $(HTML_OUT)/index.html
PDF_OUT := $(DOC_OUT)/latex
PDF_TARGET := $(PDF_OUT)/qpdf.pdf

TARGETS_manual := \
    $(DOC_OUT)/qpdf.1 \
    $(DOC_OUT)/fix-qdf.1 \
    $(DOC_OUT)/zlib-flate.1
ifeq ($(BUILD_HTML),1)
TARGETS_manual += $(HTML_TARGET) $(S_HTML_TARGET)
endif
ifeq ($(BUILD_PDF),1)
TARGETS_manual += $(PDF_TARGET)
endif

MANUAL_DEPS = $(wildcard manual/*.rst) manual/conf.py manual/_ext/qpdf.py

# Prevent targets that run $(SPHINX) from running in parallel by using
# order-only dependencies (the dependencies listed after the |) to
# avoid clashes in temporary files that cause the build to fail with
# the error "_pickle.UnpicklingError: pickle data was truncated"
$(HTML_TARGET): $(MANUAL_DEPS)
	$(SPHINX) -M html manual $(DOC_OUT) -W

$(S_HTML_TARGET): $(MANUAL_DEPS) | $(HTML_TARGET)
	$(SPHINX) -M singlehtml manual $(DOC_OUT) -W

$(PDF_TARGET): $(MANUAL_DEPS) | $(S_HTML_TARGET) $(HTML_TARGET)
	$(SPHINX) -M latexpdf manual $(DOC_OUT) -W

$(DOC_OUT)/%.1: manual/%.1.in
	sed -e 's:@PACKAGE_VERSION@:$(PACKAGE_VERSION):g' < $< > $@

# The doc-dist target must not removed $(DOC_DEST) so that it works to
# do stuff like make doc-dist DOC_DEST=$(DESTDIR)/$(docdir). Make sure
# what this does is consistent with ../README-doc.txt and the
# information in the manual and ../README.md.
.PHONY: doc-dist
doc-dist: build_manual
	@if test x"$(DOC_DEST)" = x; then \
	    echo DOC_DEST must be set 1>& 2; \
	    false; \
	fi
	if test -d $(DOC_DEST); then \
	    $(RM) -rf $(DOC_DEST)/*html $(DOC_DEST)/*.pdf; \
	else \
	    mkdir -p $(DOC_DEST); \
	fi
	cp -r $(DOC_OUT)/html doc
	cp -r $(DOC_OUT)/singlehtml doc
	cp $(PDF_TARGET) $(DOC_DEST)/qpdf-manual.pdf
