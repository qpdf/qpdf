installwin: all
	$(RM) -r $(INSTALL_DIR)
	mkdir $(INSTALL_DIR)
	mkdir $(INSTALL_DIR)/bin
	mkdir $(INSTALL_DIR)/lib
	mkdir $(INSTALL_DIR)/include
	mkdir $(INSTALL_DIR)/include/qpdf
	mkdir $(INSTALL_DIR)/doc
	cp libqpdf/$(OUTPUT_DIR)/$(STATIC_LIB_NAME) $(INSTALL_DIR)/lib
	cp libqpdf/$(OUTPUT_DIR)/qpdf*.dll $(INSTALL_DIR)/bin
	cp qpdf/$(OUTPUT_DIR)/bin/qpdf.exe $(INSTALL_DIR)/bin
	cp zlib-flate/$(OUTPUT_DIR)/bin/zlib-flate.exe $(INSTALL_DIR)/bin
	cp qpdf/fix-qdf $(INSTALL_DIR)/bin
	cp include/qpdf/*.h $(INSTALL_DIR)/include/qpdf
	cp include/qpdf/*.hh $(INSTALL_DIR)/include/qpdf
	cp doc/stylesheet.css $(INSTALL_DIR)/doc
	cp doc/qpdf-manual.html $(INSTALL_DIR)/doc
	cp doc/qpdf-manual.pdf $(DESTDIR)$(docdir)
