#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QUtil.hh>
#include <iostream>

static bool any_failures = false;

bool
report(QPDF& q, QPDFObjectHandle oh, long long item, long long exp_item)
{
    QPDFNumberTreeObjectHelper nh(oh, q);
    QPDFObjectHandle o1;
    long long offset = 0;
    bool f1 = nh.findObjectAtOrBelow(item, o1, offset);
    QPDFObjectHandle o2;
    bool f2 = nh.findObject(item, o2);

    bool failed = false;
    auto show = [&failed, &oh, &item]() {
        if (!failed) {
            failed = true;
            std::cout << "key = " << item << ", oh = " << oh.unparseResolved()
                      << std::endl;
        }
    };

    auto mk_wanted = [](long long i) {
        return (
            (i == -1) ? "end"
                      : (QUtil::int_to_string(i) + "/(-" +
                         QUtil::int_to_string(i) + "-)"));
    };
    std::string i1_wanted = mk_wanted(exp_item);
    std::string i2_wanted = mk_wanted(item == exp_item ? item : -1);
    auto mk_actual = [](bool found, long long v, QPDFObjectHandle& o) {
        return (found ? QUtil::int_to_string(v) + "/" + o.unparse() : "end");
    };
    std::string i1_actual = mk_actual(f1, item - offset, o1);
    std::string i2_actual = mk_actual(f2, item, o2);

    if (i1_wanted != i1_actual) {
        show();
        std::cout << "i1: wanted " << i1_wanted << ", got " << i1_actual
                  << std::endl;
    }
    if (i2_wanted != i2_actual) {
        show();
        std::cout << "i2: wanted " << i2_wanted << ", got " << i2_actual
                  << std::endl;
    }

    return failed;
}

void
test_bsearch()
{
    QPDF q;
    q.emptyPDF();

    auto mk = [&q](std::vector<int> const& v) {
        auto nums = QPDFObjectHandle::newArray();
        for (auto i: v) {
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

    auto r = [&q](QPDFObjectHandle& oh, int item, int exp) {
        if (report(q, oh, item, exp)) {
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

    if (!any_failures) {
        std::cout << "bsearch tests passed" << std::endl;
    }
}

QPDFObjectHandle
new_node(QPDF& q, std::string const& key)
{
    auto dict = QPDFObjectHandle::newDictionary();
    dict.replaceKey(key, QPDFObjectHandle::newArray());
    return q.makeIndirectObject(dict);
}

static void
check_find(
    QPDFNameTreeObjectHelper& nh,
    std::string const& key,
    bool prev_if_not_found)
{
    auto i = nh.find(key, prev_if_not_found);
    std::cout << "find " << key << " (" << prev_if_not_found << "): ";
    if (i == nh.end()) {
        std::cout << "not found";
    } else {
        std::cout << (*i).first << " -> " << (*i).second.unparse();
    }
    std::cout << std::endl;
}

void
test_depth()
{
    int constexpr NITEMS = 3;
    QPDF q;
    q.emptyPDF();
    auto root = q.getRoot();
    auto n0 = new_node(q, "/Kids");
    root.replaceKey("/NT", n0);
    auto k0 = root.getKey("/NT").getKey("/Kids");
    for (int i1 = 0; i1 < NITEMS; ++i1) {
        auto n1 = new_node(q, "/Kids");
        k0.appendItem(n1);
        auto k1 = n1.getKey("/Kids");
        for (int i2 = 0; i2 < NITEMS; ++i2) {
            auto n2 = new_node(q, "/Kids");
            k1.appendItem(n2);
            auto k2 = n2.getKey("/Kids");
            for (int i3 = 0; i3 < NITEMS; ++i3) {
                auto n3 = new_node(q, "/Names");
                k2.appendItem(n3);
                auto items = n3.getKey("/Names");
                std::string first;
                std::string last;
                for (int i4 = 0; i4 < NITEMS; ++i4) {
                    int val =
                        (((((i1 * NITEMS) + i2) * NITEMS) + i3) * NITEMS) + i4;
                    std::string str = QUtil::int_to_string(10 * val, 6);
                    items.appendItem(QPDFObjectHandle::newString(str));
                    items.appendItem(QPDFObjectHandle::newString("val " + str));
                    if (i4 == 0) {
                        first = str;
                    } else if (i4 == NITEMS - 1) {
                        last = str;
                    }
                }
                auto limits = QPDFObjectHandle::newArray();
                n3.replaceKey("/Limits", limits);
                limits.appendItem(QPDFObjectHandle::newString(first));
                limits.appendItem(QPDFObjectHandle::newString(last));
            }
            auto limits = QPDFObjectHandle::newArray();
            n2.replaceKey("/Limits", limits);
            limits.appendItem(
                k2.getArrayItem(0).getKey("/Limits").getArrayItem(0));
            limits.appendItem(
                k2.getArrayItem(NITEMS - 1).getKey("/Limits").getArrayItem(1));
        }
        auto limits = QPDFObjectHandle::newArray();
        n1.replaceKey("/Limits", limits);
        limits.appendItem(k1.getArrayItem(0).getKey("/Limits").getArrayItem(0));
        limits.appendItem(
            k1.getArrayItem(NITEMS - 1).getKey("/Limits").getArrayItem(1));
    }

    QPDFNameTreeObjectHelper nh(n0, q);
    std::cout << "--- forward ---" << std::endl;
    for (auto i: nh) {
        std::cout << i.first << " -> " << i.second.unparse() << std::endl;
    }
    std::cout << "--- backward ---" << std::endl;
    for (auto i = nh.last(); i.valid(); --i) {
        std::cout << (*i).first << " -> " << (*i).second.unparse() << std::endl;
    }

    // Find
    check_find(nh, "000300", false);
    check_find(nh, "000305", true);
    check_find(nh, "000305", false);
    check_find(nh, "00000", false);
    check_find(nh, "00000", true);
    check_find(nh, "000800", false);
    check_find(nh, "000805", false);
    check_find(nh, "000805", true);
}

int
main()
{
    test_bsearch();
    test_depth();

    return 0;
}
