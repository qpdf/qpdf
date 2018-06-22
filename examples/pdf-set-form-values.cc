#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " infile.pdf outfile.pdf value"
	      << std::endl
	      << "Set the value of all text fields to a specified value"
              << std::endl;
    exit(2);
}


int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if (argc != 4)
    {
        usage();
    }

    char const* infilename = argv[1];
    char const* outfilename = argv[2];
    char const* value = argv[3];

    // This is a contrived example that just goes through a file page
    // by page and sets the value of any text fields it finds to a
    // fixed value as given on the command line. The purpose here is
    // to illustrate use of the helper classes around interactive
    // forms.

    try
    {
	QPDF qpdf;
	qpdf.processFile(infilename);

        // We will iterate through form fields by starting at the page
        // level and looking at each field for each page. We could
        // also called QPDFAcroFormDocumentHelper::getFormFields to
        // iterate at the field level, but doing it as below
        // illustrates how we can map from annotations to fields.

        QPDFAcroFormDocumentHelper afdh(qpdf);
        QPDFPageDocumentHelper pdh(qpdf);
        std::vector<QPDFPageObjectHelper> pages = pdh.getAllPages();
        for (std::vector<QPDFPageObjectHelper>::iterator page_iter =
                 pages.begin();
             page_iter != pages.end(); ++page_iter)
        {
            // Get all widget annotations for each page. Widget
            // annotations are the ones that contain the details of
            // what's in a form field.
            std::vector<QPDFAnnotationObjectHelper> annotations =
                afdh.getWidgetAnnotationsForPage(*page_iter);
            for (std::vector<QPDFAnnotationObjectHelper>::iterator annot_iter =
                     annotations.begin();
                 annot_iter != annotations.end(); ++annot_iter)
            {
                // For each annotation, find its associated field. If
                // it's a text field, set its value. This will
                // automatically update the document to indicate that
                // appearance streams need to be regenerated. At the
                // time of this writing, qpdf doesn't have any helper
                // code to assist with appearance stream generation,
                // though there's nothing that prevents it from being
                // possible.
                QPDFFormFieldObjectHelper ffh =
                    afdh.getFieldForAnnotation(*annot_iter);
                if (ffh.getFieldType() == "/Tx")
                {
                    // Set the value. This will automatically set
                    // /NeedAppearances to true. If you don't want to
                    // do that, pass false as the second argument. For
                    // details see comments in
                    // QPDFFormFieldObjectHelper.hh.
                    ffh.setV(value);
                }
            }
        }

	// Write out a new file
	QPDFWriter w(qpdf, outfilename);
        w.setStaticID(true); // for testing only
	w.write();
    }
    catch (std::exception &e)
    {
	std::cerr << whoami << " processing file " << infilename << ": "
		  << e.what() << std::endl;
	exit(2);
    }

    return 0;
}
