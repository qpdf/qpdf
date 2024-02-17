#include <qpdf/assert_test.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Array.hh>

#include <iostream>

int
main()
{
    auto obj = QPDF_Array::create({}, true);
    QPDF_Array& a = *obj->as<QPDF_Array>();

    assert(a.size() == 0);

    a.push_back(QPDFObjectHandle::parse("1"));
    a.push_back(QPDFObjectHandle::parse("(potato)"));
    a.push_back(QPDFObjectHandle::parse("null"));
    a.push_back(QPDFObjectHandle::parse("null"));
    a.push_back(QPDFObjectHandle::parse("/Quack"));
    assert(a.size() == 5);
    assert(a.at(0).isInteger() && (a.at(0).getIntValue() == 1));
    assert(a.at(1).isString() && (a.at(1).getStringValue() == "potato"));
    assert(a.at(2).isNull());
    assert(a.at(3).isNull());
    assert(a.at(4).isName() && (a.at(4).getName() == "/Quack"));

    a.insert(4, QPDFObjectHandle::parse("/BeforeQuack"));
    assert(a.size() == 6);
    assert(a.at(0).isInteger() && (a.at(0).getIntValue() == 1));
    assert(a.at(4).isName() && (a.at(4).getName() == "/BeforeQuack"));
    assert(a.at(5).isName() && (a.at(5).getName() == "/Quack"));

    a.insert(2, QPDFObjectHandle::parse("/Third"));
    assert(a.size() == 7);
    assert(a.at(1).isString() && (a.at(1).getStringValue() == "potato"));
    assert(a.at(2).isName() && (a.at(2).getName() == "/Third"));
    assert(a.at(3).isNull());
    assert(a.at(6).isName() && (a.at(6).getName() == "/Quack"));

    a.insert(0, QPDFObjectHandle::parse("/First"));
    assert(a.size() == 8);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(7).isName() && (a.at(7).getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 7);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(5).isNull());
    assert(a.at(6).isName() && (a.at(6).getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 6);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(3).isName() && (a.at(3).getName() == "/Third"));
    assert(a.at(4).isNull());
    assert(a.at(5).isNull());

    a.setAt(4, QPDFObjectHandle::parse("12"));
    assert(a.at(4).isInteger() && (a.at(4).getIntValue() == 12));
    a.setAt(4, QPDFObjectHandle::newNull());
    assert(a.at(4).isNull());

    a.erase(a.size() - 1);
    assert(a.size() == 5);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(3).isName() && (a.at(3).getName() == "/Third"));
    assert(a.at(4).isNull());

    a.erase(a.size() - 1);
    assert(a.size() == 4);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(3).isName() && (a.at(3).getName() == "/Third"));

    a.erase(a.size() - 1);
    assert(a.size() == 3);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(2).isString() && (a.at(2).getStringValue() == "potato"));

    QPDF pdf;
    pdf.emptyPDF();

    obj = QPDF_Array::create({10, "null"_qpdf.getObj()}, true);
    QPDF_Array& b = *obj->as<QPDF_Array>();
    b.setAt(5, pdf.getObject(5, 0));
    b.setAt(7, "[0 1 2 3]"_qpdf);
    assert(b.at(3).isNull());
    assert(b.at(8).isNull());
    assert(b.at(5).isIndirect());
    assert(b.unparse() == "[ null null null null null 5 0 R null [ 0 1 2 3 ] null null ]");
    auto c = b.copy(true);
    auto d = b.copy(false);
    b.at(7).setArrayItem(2, "42"_qpdf);
    assert(c->unparse() == "[ null null null null null 5 0 R null [ 0 1 42 3 ] null null ]");
    assert(d->unparse() == "[ null null null null null 5 0 R null [ 0 1 2 3 ] null null ]");

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

    std::cout << "sparse array tests done" << std::endl;
    return 0;
}
