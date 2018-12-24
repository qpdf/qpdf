#ifndef QPDFMATRIX_HH
#define QPDFMATRIX_HH

#include <qpdf/DLL.h>
#include <string>

class QPDFMatrix
{
  public:
    QPDF_DLL
    QPDFMatrix();
    QPDF_DLL
    QPDFMatrix(double a, double b, double c,
               double d, double e, double f);

    QPDF_DLL
    std::string unparse() const;

    // This is not part of the public API. Just provide the methods we
    // need as we need them.
    QPDF_DLL
    void concat(QPDFMatrix const& other);
    QPDF_DLL
    void scale(double sx, double sy);
    QPDF_DLL
    void translate(double tx, double ty);
    // Any value other than 90, 180, or 270 is ignored
    QPDF_DLL
    void rotatex90(int angle);

    QPDF_DLL
    void transform(double x, double y, double& xp, double& yp);

  private:
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
};

#endif // QPDFMATRIX_HH
