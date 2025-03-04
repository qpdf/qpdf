#include <qpdf/assert_test.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>

#include <iostream>

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
    assert(a.at(0).second.isInteger() && (a.at(0).second.getIntValue() == 1));
    assert(a.at(1).second.isString() && (a.at(1).second.getStringValue() == "potato"));
    assert(a.at(2).second.isNull());
    assert(a.at(3).second.isNull());
    assert(a.at(4).second.isName() && (a.at(4).second.getName() == "/Quack"));

    a.insert(4, QPDFObjectHandle::parse("/BeforeQuack"));
    assert(a.size() == 6);
    assert(a.at(0).second.isInteger() && (a.at(0).second.getIntValue() == 1));
    assert(a.at(4).second.isName() && (a.at(4).second.getName() == "/BeforeQuack"));
    assert(a.at(5).second.isName() && (a.at(5).second.getName() == "/Quack"));

    a.insert(2, QPDFObjectHandle::parse("/Third"));
    assert(a.size() == 7);
    assert(a.at(1).second.isString() && (a.at(1).second.getStringValue() == "potato"));
    assert(a.at(2).second.isName() && (a.at(2).second.getName() == "/Third"));
    assert(a.at(3).second.isNull());
    assert(a.at(6).second.isName() && (a.at(6).second.getName() == "/Quack"));

    a.insert(0, QPDFObjectHandle::parse("/First"));
    assert(a.size() == 8);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(7).second.isName() && (a.at(7).second.getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 7);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(5).second.isNull());
    assert(a.at(6).second.isName() && (a.at(6).second.getName() == "/Quack"));

    a.erase(6);
    assert(a.size() == 6);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(3).second.isName() && (a.at(3).second.getName() == "/Third"));
    assert(a.at(4).second.isNull());
    assert(a.at(5).second.isNull());

    a.setAt(4, QPDFObjectHandle::parse("12"));
    assert(a.at(4).second.isInteger() && (a.at(4).second.getIntValue() == 12));
    a.setAt(4, QPDFObjectHandle::newNull());
    assert(a.at(4).second.isNull());

    a.erase(a.size() - 1);
    assert(a.size() == 5);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(3).second.isName() && (a.at(3).second.getName() == "/Third"));
    assert(a.at(4).second.isNull());

    a.erase(a.size() - 1);
    assert(a.size() == 4);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(3).second.isName() && (a.at(3).second.getName() == "/Third"));

    a.erase(a.size() - 1);
    assert(a.size() == 3);
    assert(a.at(0).second.isName() && (a.at(0).second.getName() == "/First"));
    assert(a.at(1).second.isInteger() && (a.at(1).second.getIntValue() == 1));
    assert(a.at(2).second.isString() && (a.at(2).second.getStringValue() == "potato"));

    QPDF pdf;
    pdf.emptyPDF();

    obj = QPDFObject::create<QPDF_Array>(
        std::vector<QPDFObjectHandle>{10, "null"_qpdf.getObj()}, true);
    auto b = qpdf::Array(obj);
    b.setAt(5, pdf.newIndirectNull());
    b.setAt(7, "[0 1 2 3]"_qpdf);
    assert(b.at(3).second.isNull());
    assert(b.at(8).second.isNull());
    assert(b.at(5).second.isIndirect());
    assert(
        QPDFObjectHandle(obj).unparse() ==
        "[ null null null null null 3 0 R null [ 0 1 2 3 ] null null ]");
    auto c = QPDFObjectHandle(obj).unsafeShallowCopy();
    auto d = QPDFObjectHandle(obj).shallowCopy();
    b.at(7).second.setArrayItem(2, "42"_qpdf);
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

    std::cout << "sparse array tests done" << std::endl;
    return 0;
}
