#ifndef EXAMPLE_C_EXTEND_H
#define EXAMPLE_C_EXTEND_H

/*
 * This is an example of how to write C++ functions and make them usable with the qpdf C API. It
 * consists of three files:
 * - extend-c-api.h -- a plain C header file
 * - extend-c-api.c -- a C program that calls the function
 * - extend-c-api.cc -- a C++ file that implements the function
 */
#include <qpdf/qpdf-c.h>

/* Declare your custom function to return QPDF_ERROR_CODE and take qpdf_data and anything else you
 * need. Any errors are retrievable through the qpdf C APIs normal error-handling mechanism.
 */

#ifdef __cplusplus
extern "C" {
#endif
    QPDF_ERROR_CODE num_pages(qpdf_data qc, int* npages);
#ifdef __cplusplus
}
#endif

#endif /* EXAMPLE_C_EXTEND_H */
