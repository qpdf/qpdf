#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <iostream>

bool report(QPDFObjectHandle oh, long long item, long long exp_item)
{
    QPDFNumberTreeObjectHelper nh(oh);
    QPDFObjectHandle o1;
    long long offset = 0;
    bool f1 = nh.findObjectAtOrBelow(item, o1, offset);
    QPDFObjectHandle o2;
    bool f2 = nh.findObject(item, o2);

    bool failed = false;
    auto show = [&failed, &oh, &item] () {
        if (! failed)
        {
            failed = true;
            std::cout << "key = " << item << ", oh = "
                      << oh.unparseResolved() << std::endl;
        }
    };

    auto mk_wanted = [](long long i) {
        return ((i == -1)
                ? "end"
                : (QUtil::int_to_string(i) +
                   "/(-" + QUtil::int_to_string(i) + "-)"));
    };
    std::string i1_wanted = mk_wanted(exp_item);
    std::string i2_wanted = mk_wanted(item == exp_item ? item : -1);
    auto mk_actual = [](bool found, long long v, QPDFObjectHandle& o) {
        return (found
                ? QUtil::int_to_string(v) + "/" + o.unparse()
                : "end");
    };
    std::string i1_actual = mk_actual(f1, item - offset, o1);
    std::string i2_actual = mk_actual(f2, item, o2);

    if (i1_wanted != i1_actual)
    {
        show();
        std::cout << "i1: wanted " << i1_wanted
                  << ", got " << i1_actual
                  << std::endl;
    }
    if (i2_wanted != i2_actual)
    {
        show();
        std::cout << "i2: wanted " << i2_wanted
                  << ", got " << i2_actual
                  << std::endl;
    }

    return failed;
}

int main()
{
    QPDF q;
    q.emptyPDF();

    auto mk = [&q] (std::vector<int> const& v) {
        auto nums = QPDFObjectHandle::newArray();
        for (auto i: v)
        {
            nums.appendItem(QPDFObjectHandle::newInteger(i));
            nums.appendItem(QPDFObjectHandle::newString(
                                "-" + QUtil::int_to_string(i) + "-"));
        }
        auto limits = QPDFObjectHandle::newArray();
        limits.appendItem(QPDFObjectHandle::newInteger(v.at(0)));
        limits.appendItem(QPDFObjectHandle::newInteger(v.at(v.size() - 1)));
        auto node = q.makeIndirectObject(QPDFObjectHandle::newDictionary());
        node.replaceKey("/Nums", nums);
        node.replaceKey("/Limits", limits);
        return node;
    };

    bool any_failures = false;
    auto r = [&any_failures](QPDFObjectHandle& oh, int item, int exp) {
        if (report(oh, item, exp))
        {
            any_failures = true;
        }
    };

    auto a = mk({2, 3, 5, 9, 11, 12, 14, 18});
    r(a, 1, -1);
    r(a, 2, 2);
    r(a, 9, 9);
    r(a, 11, 11);
    r(a, 10, 9);
    r(a, 7, 5);
    r(a, 18, 18);
    r(a, 19, 18);

    auto b = mk({2, 4});
    r(b, 1, -1);
    r(b, 2, 2);
    r(b, 3, 2);
    r(b, 4, 4);
    r(b, 5, 4);

    auto c = mk({3});
    r(c, 1, -1);
    r(c, 3, 3);
    r(c, 5, 3);

    auto d = mk({2, 3, 5, 9, 10, 12, 14, 18, 19, 20});
    r(d, 1, -1);
    r(d, 2, 2);
    r(d, 18, 18);
    r(d, 14, 14);
    r(d, 19, 19);
    r(d, 20, 20);
    r(d, 25, 20);

    if (! any_failures)
    {
        std::cout << "all tests passed" << std::endl;
    }
}
