// This is an example of how to write C++ functions and make them usable with the qpdf C API. It
// consists of three files:
// - extend-c-api.h -- a plain C header file
// - extend-c-api.c -- a C program that calls the function
// - extend-c-api.cc -- a C++ file that implements the function

#include "extend-c-api.h"

// Here, we add a function to get the number of pages in a PDF file and make it callable through the
// C API.

// This is a normal C++ function that works with QPDF in a normal way. It doesn't do anything
// special to be callable from C.
int
numPages(std::shared_ptr<QPDF> qpdf)
{
    return qpdf->getRoot().getKey("/Pages").getKey("/Count").getIntValueAsInt();
}

// Now we define the glue that makes our function callable using the C API.

// This is the C++ implementation of the C function.
QPDF_ERROR_CODE
num_pages(qpdf_data qc, int* npages)
{
    // Call qpdf_c_wrap to convert any exception our function might through to a QPDF_ERROR_CODE
    // and attach it to the qpdf_data object in the same way as other functions in the C API.
    return qpdf_c_wrap(qc, [&qc, &npages]() { *npages = numPages(qpdf_c_get_qpdf(qc)); });
}
