INDOC = manual/qpdf-manual
OUTDOC = manual/$(OUTPUT_DIR)/qpdf-manual

TARGETS_manual := doc/qpdf.1 doc/fix-qdf.1 doc/zlib-flate.1
ifeq ($(BUILD_HTML),1)
TARGETS_manual += doc/qpdf-manual.html
endif
ifeq ($(BUILD_PDF),1)
TARGETS_manual += doc/qpdf-manual.pdf
endif

VALIDATE=manual/$(OUTPUT_DIR)/validate

ifeq ($(VALIDATE_DOC),1)

$(VALIDATE): $(INDOC).xml
	$(XMLLINT) --noout --dtdvalid $(DOCBOOKX_DTD) $<
	touch $(VALIDATE)

else

$(VALIDATE):
	touch $(VALIDATE)

endif

$(OUTDOC).pdf: $(OUTDOC).fo qpdf/build/qpdf
	$(FOP) $< -pdf $@.tmp
	qpdf/build/qpdf --linearize $@.tmp $@

$(OUTDOC).html: $(INDOC).xml manual/html.xsl $(VALIDATE)
	$(XSLTPROC) --output $@ manual/html.xsl $<

.PRECIOUS: $(OUTDOC).fo
$(OUTDOC).fo: $(INDOC).xml manual/print.xsl $(VALIDATE)
	$(XSLTPROC) --output $@ manual/print.xsl $<

doc/%.1: manual/%.1.in
	sed -e 's:@PACKAGE_VERSION@:$(PACKAGE_VERSION):g' \
	    -e 's:@docdir@:$(docdir):g' \
	    < $< > $@

doc/%: manual/$(OUTPUT_DIR)/%
	cp $< $@
