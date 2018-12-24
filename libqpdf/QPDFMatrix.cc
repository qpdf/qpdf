#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QUtil.hh>

QPDFMatrix::QPDFMatrix() :
    a(1.0),
    b(0.0),
    c(0.0),
    d(1.0),
    e(0.0),
    f(0.0)
{
}

QPDFMatrix::QPDFMatrix(double a, double b, double c,
                       double d, double e, double f) :
    a(a),
    b(b),
    c(c),
    d(d),
    e(e),
    f(f)
{
}

std::string
QPDFMatrix::unparse() const
{
    return (QUtil::double_to_string(a, 5) + " " +
            QUtil::double_to_string(b, 5) + " " +
            QUtil::double_to_string(c, 5) + " " +
            QUtil::double_to_string(d, 5) + " " +
            QUtil::double_to_string(e, 5) + " " +
            QUtil::double_to_string(f, 5));
}

void
QPDFMatrix::concat(QPDFMatrix const& other)
{
    double ap = (this->a * other.a) + (this->c * other.b);
    double bp = (this->b * other.a) + (this->d * other.b);
    double cp = (this->a * other.c) + (this->c * other.d);
    double dp = (this->b * other.c) + (this->d * other.d);
    double ep = (this->a * other.e) + (this->c * other.f) + this->e;
    double fp = (this->b * other.e) + (this->d * other.f) + this->f;
    this-> a = ap;
    this-> b = bp;
    this-> c = cp;
    this-> d = dp;
    this-> e = ep;
    this-> f = fp;
}

void
QPDFMatrix::scale(double sx, double sy)
{
    concat(QPDFMatrix(sx, 0, 0, sy, 0, 0));
}

void
QPDFMatrix::translate(double tx, double ty)
{
    concat(QPDFMatrix(1, 0, 0, 1, tx, ty));
}

void
QPDFMatrix::rotatex90(int angle)
{
    switch (angle)
    {
      case 90:
        concat(QPDFMatrix(0, 1, -1, 0, 0, 0));
        break;
      case 180:
        concat(QPDFMatrix(-1, 0, 0, -1, 0, 0));
        break;
      case 270:
        concat(QPDFMatrix(0, -1, 1, 0, 0, 0));
        break;
      default:
        // ignore
        break;
    }
}

void
QPDFMatrix::transform(double x, double y, double& xp, double& yp)
{
    xp = (this->a * x) + (this->c * y) + this->e;
    yp = (this->b * x) + (this->d * y) + this->f;
}
