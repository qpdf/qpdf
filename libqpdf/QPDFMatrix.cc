#include <qpdf/QPDFMatrix.hh>

#include <qpdf/QUtil.hh>
#include <algorithm>

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

QPDFMatrix::QPDFMatrix(QPDFObjectHandle::Matrix const& m) :
    a(m.a),
    b(m.b),
    c(m.c),
    d(m.d),
    e(m.e),
    f(m.f)
{
}

static double fix_rounding(double d)
{
    if ((d > -0.00001) && (d < 0.00001))
    {
        d = 0.0;
    }
    return d;
}

std::string
QPDFMatrix::unparse() const
{
    return (QUtil::double_to_string(fix_rounding(a), 5) + " " +
            QUtil::double_to_string(fix_rounding(b), 5) + " " +
            QUtil::double_to_string(fix_rounding(c), 5) + " " +
            QUtil::double_to_string(fix_rounding(d), 5) + " " +
            QUtil::double_to_string(fix_rounding(e), 5) + " " +
            QUtil::double_to_string(fix_rounding(f), 5));
}

QPDFObjectHandle::Matrix
QPDFMatrix::getAsMatrix() const
{
    return QPDFObjectHandle::Matrix(a, b, c, d, e, f);
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
    const_cast<QPDFMatrix const*>(this)->transform(x, y, xp, yp);
}

void
QPDFMatrix::transform(double x, double y, double& xp, double& yp) const
{
    xp = (this->a * x) + (this->c * y) + this->e;
    yp = (this->b * x) + (this->d * y) + this->f;
}

QPDFObjectHandle::Rectangle
QPDFMatrix::transformRectangle(QPDFObjectHandle::Rectangle r)
{
    return const_cast<QPDFMatrix const*>(this)->transformRectangle(r);
}

QPDFObjectHandle::Rectangle
QPDFMatrix::transformRectangle(QPDFObjectHandle::Rectangle r) const
{
    std::vector<double> tx(4);
    std::vector<double> ty(4);
    transform(r.llx, r.lly, tx.at(0), ty.at(0));
    transform(r.llx, r.ury, tx.at(1), ty.at(1));
    transform(r.urx, r.lly, tx.at(2), ty.at(2));
    transform(r.urx, r.ury, tx.at(3), ty.at(3));
    return QPDFObjectHandle::Rectangle(
        *std::min_element(tx.begin(), tx.end()),
        *std::min_element(ty.begin(), ty.end()),
        *std::max_element(tx.begin(), tx.end()),
        *std::max_element(ty.begin(), ty.end()));
}

bool
QPDFMatrix::operator==(QPDFMatrix const& rhs) const
{
    return ((this->a == rhs.a) &&
            (this->b == rhs.b) &&
            (this->c == rhs.c) &&
            (this->d == rhs.d) &&
            (this->e == rhs.e) &&
            (this->f == rhs.f));
}
