#include <qpdf/SparseOHArray.hh>
#include <iostream>

#ifdef NDEBUG
// We need assert even in a release build for test code.
# undef NDEBUG
#endif
#include <cassert>

int
main()
{
    SparseOHArray a;
    assert(a.size() == 0);

    a.append(QPDFObjectHandle::parse("1"));
    a.append(QPDFObjectHandle::parse("(potato)"));
    a.append(QPDFObjectHandle::parse("null"));
    a.append(QPDFObjectHandle::parse("null"));
    a.append(QPDFObjectHandle::parse("/Quack"));
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

    a.remove_last();
    assert(a.size() == 5);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(3).isName() && (a.at(3).getName() == "/Third"));
    assert(a.at(4).isNull());

    a.remove_last();
    assert(a.size() == 4);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(3).isName() && (a.at(3).getName() == "/Third"));

    a.remove_last();
    assert(a.size() == 3);
    assert(a.at(0).isName() && (a.at(0).getName() == "/First"));
    assert(a.at(1).isInteger() && (a.at(1).getIntValue() == 1));
    assert(a.at(2).isString() && (a.at(2).getStringValue() == "potato"));

    std::cout << "sparse array tests done" << std::endl;
    return 0;
}
