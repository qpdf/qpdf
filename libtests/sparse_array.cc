#include <qpdf/assert_test.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>

#include <iostream>

int
to_i(size_t n)
{
    return static_cast<int>(n);
}

int
main()
{
    auto obj = QPDFObject::create<QPDF_Array>(std::vector<QPDFObjectHandle>(), true);
    auto a = qpdf::Array(obj);

    assert(a.size() == 0);

    a.push_back(QPDFObjectHandle::parse("1"));
    a.push_back(QPDFObjectHandle::parse("(potato)"));
    a.push_back(QPDFObjectHandle::parse("null"));
    a.push_back(QPDFObjectHandle::parse("null"));
    a.push_back(QPDFObjectHandle::parse("/Quack"));
    assert(a.size() == 5);
    assert(a[0].isInteger() && (a[0].getIntValue() == 1));
    assert(a[1].isString() && (a[1].getStringValue() == "potato"));
    assert(a[2].isNull());
    assert(a[3].isNull());
    assert(a[4].isName() && (a[4].getName() == "/Quack"));

    a.insert(4, QPDFObjectHandle::parse("/BeforeQuack"));
    assert(a.size() == 6);
    assert(a[0].isInteger() && (a[0].getIntValue() == 1));
    assert(a[4].isName() && (a[4].getName() == "/BeforeQuack"));
    assert(a[5].isName() && (a[5].getName() == "/Quack"));

    a.insert(2, QPDFObjectHandle::parse("/Third"));
    assert(a.size() == 7);
    assert(a[1].isString() && (a[1].getStringValue() == "potato"));
    assert(a[2].isName() && (a[2].getName() == "/Third"));
    assert(a[3].isNull());
    assert(a[6].isName() && (a[6].getName() == "/Quack"));

    a.insert(0, QPDFObjectHandle::parse("/First"));
    assert(a.size() == 8);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[7].isName() && (a[7].getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 7);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[5].isNull());
    assert(a[6].isName() && (a[6].getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 6);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[3].isName() && (a[3].getName() == "/Third"));
    assert(a[4].isNull());
    assert(a[5].isNull());

    a.setAt(4, QPDFObjectHandle::parse("12"));
    assert(a[4].isInteger() && (a[4].getIntValue() == 12));
    a.setAt(4, QPDFObjectHandle::newNull());
    assert(a[4].isNull());

    a.erase(to_i(a.size()) - 1);
    assert(a.size() == 5);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[3].isName() && (a[3].getName() == "/Third"));
    assert(a[4].isNull());

    a.erase(to_i(a.size()) - 1);
    assert(a.size() == 4);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[3].isName() && (a[3].getName() == "/Third"));

    a.erase(to_i(a.size()) - 1);
    assert(a.size() == 3);
    assert(a[0].isName() && (a[0].getName() == "/First"));
    assert(a[1].isInteger() && (a[1].getIntValue() == 1));
    assert(a[2].isString() && (a[2].getStringValue() == "potato"));

    QPDF pdf;
    pdf.emptyPDF();

    obj = QPDFObject::create<QPDF_Array>(
        std::vector<QPDFObjectHandle>{10, "null"_qpdf.getObj()}, true);
    auto b = qpdf::Array(obj);
    b.setAt(5, pdf.newIndirectNull());
    b.setAt(7, "[0 1 2 3]"_qpdf);
    assert(b[3].null());
    assert(b[8].null());
    assert(b[5].indirect());
    assert(
        QPDFObjectHandle(obj).unparse() ==
        "[ null null null null null 3 0 R null [ 0 1 2 3 ] null null ]");
    auto c = QPDFObjectHandle(obj).unsafeShallowCopy();
    auto d = QPDFObjectHandle(obj).shallowCopy();
    b.get(7).setArrayItem(2, "42"_qpdf);
    assert(c.unparse() == "[ null null null null null 3 0 R null [ 0 1 42 3 ] null null ]");
    assert(d.unparse() == "[ null null null null null 3 0 R null [ 0 1 2 3 ] null null ]");

    try {
        b.setAt(3, {});
        std::cout << "inserted uninitialized object\n";
    } catch (std::logic_error&) {
    }
    QPDF pdf2;
    pdf2.emptyPDF();
    try {
        pdf.makeIndirectObject(obj);
        b.setAt(3, pdf2.getObject(1, 0));
        std::cout << "inserted uninitialized object\n";
    } catch (std::logic_error&) {
    }

    std::cout << "sparse array tests done" << '\n';
    return 0;
}
