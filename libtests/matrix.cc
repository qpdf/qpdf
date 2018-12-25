#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QUtil.hh>
#include <assert.h>
#include <iostream>

static void check(QPDFMatrix const& m, std::string const& exp)
{
    std::string u = m.unparse();
    if (u != exp)
    {
        std::cout << "got " << u << ", wanted " << exp << std::endl;
    }
}

static void check_xy(double x, double y, std::string const& exp)
{
    std::string u = (QUtil::double_to_string(x, 2) + " " +
                     QUtil::double_to_string(y, 2));
    if (u != exp)
    {
        std::cout << "got " << u << ", wanted " << exp << std::endl;
    }
}

int main()
{
    QPDFMatrix m;
    check(m, "1.00000 0.00000 0.00000 1.00000 0.00000 0.00000");
    m.translate(10, 20);
    check(m, "1.00000 0.00000 0.00000 1.00000 10.00000 20.00000");
    m.scale(1.5, 2);
    check(m, "1.50000 0.00000 0.00000 2.00000 10.00000 20.00000");
    m.translate(30, 40);
    check(m, "1.50000 0.00000 0.00000 2.00000 55.00000 100.00000");
    m.concat(QPDFMatrix(1, 2, 3, 4, 5, 6));
    check(m, "1.50000 4.00000 4.50000 8.00000 62.50000 112.00000");
    m.rotatex90(90);
    check(m, "4.50000 8.00000 -1.50000 -4.00000 62.50000 112.00000");
    m.rotatex90(180);
    check(m, "-4.50000 -8.00000 1.50000 4.00000 62.50000 112.00000");
    m.rotatex90(270);
    check(m, "-1.50000 -4.00000 -4.50000 -8.00000 62.50000 112.00000");
    m.rotatex90(180);
    check(m, "1.50000 4.00000 4.50000 8.00000 62.50000 112.00000");
    m.rotatex90(12345);
    check(m, "1.50000 4.00000 4.50000 8.00000 62.50000 112.00000");

    double xp = 0;
    double yp = 0;
    m.transform(240, 480, xp, yp);
    check_xy(xp, yp, "2582.50 4912.00");

    check(QPDFMatrix(
              QPDFObjectHandle::parse(
                  "[3 1 4 1 5 9.26535]").getArrayAsMatrix()),
          "3.00000 1.00000 4.00000 1.00000 5.00000 9.26535");

    std::cout << "matrix tests done" << std::endl;
    return 0;
}
