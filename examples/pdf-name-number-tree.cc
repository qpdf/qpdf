#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <iostream>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " outfile.pdf" << std::endl
              << "Create some name/number trees and write to a file"
              << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 2) {
        usage();
    }

    char const* outfilename = argv[1];

    QPDF qpdf;
    qpdf.emptyPDF();

    // This example doesn't do anything particularly useful other than
    // just illustrate how to use the APIs for name and number trees.
    // It also demonstrates use of the iterators for dictionaries and
    // arrays introduced at the same time with qpdf 10.2.

    // To use this example, compile it and run it. Study the output
    // and compare it to what you expect. When done, look at the
    // generated output file in a text editor to inspect the structure
    // of the trees as left in the file.

    // We're just going to create some name and number trees, hang
    // them off the document catalog (root), and write an empty PDF to
    // a file. The PDF will have no pages and won't be viewable, but
    // you can look at it in a text editor to see the resulting
    // structure of the PDF.

    // Create a dictionary off the root where we will hang our name
    // and number tree.
    auto root = qpdf.getRoot();
    auto example = QPDFObjectHandle::newDictionary();
    root.replaceKey("/Example", example);

    // Create a name tree, attach it to the file, and add some items.
    auto name_tree = QPDFNameTreeObjectHelper::newEmpty(qpdf);
    auto name_tree_oh = name_tree.getObjectHandle();
    example.replaceKey("/NameTree", name_tree_oh);
    name_tree.insert("K", QPDFObjectHandle::newUnicodeString("king"));
    name_tree.insert("Q", QPDFObjectHandle::newUnicodeString("queen"));
    name_tree.insert("R", QPDFObjectHandle::newUnicodeString("rook"));
    name_tree.insert("B", QPDFObjectHandle::newUnicodeString("bishop"));
    name_tree.insert("N", QPDFObjectHandle::newUnicodeString("knight"));
    auto iter =
        name_tree.insert("P", QPDFObjectHandle::newUnicodeString("pawn"));
    // Look at the iterator
    std::cout << "just inserted " << iter->first << " -> "
              << iter->second.unparse() << std::endl;
    --iter;
    std::cout << "predecessor: " << iter->first << " -> "
              << iter->second.unparse() << std::endl;
    ++iter;
    ++iter;
    std::cout << "successor: " << iter->first << " -> "
              << iter->second.unparse() << std::endl;

    // Use range-for iteration
    std::cout << "Name tree items:" << std::endl;
    for (auto i: name_tree) {
        std::cout << "  " << i.first << " -> " << i.second.unparse()
                  << std::endl;
    }

    // This is a small tree, so everything will be at the root. We can
    // look at it using dictionary and array iterators.
    std::cout << "Keys in name tree object:" << std::endl;
    QPDFObjectHandle names;
    for (auto const& i: name_tree_oh.ditems()) {
        std::cout << i.first << std::endl;
        if (i.first == "/Names") {
            names = i.second;
        }
    }
    // Values in names array:
    std::cout << "Values in names:" << std::endl;
    for (auto& i: names.aitems()) {
        std::cout << "  " << i.unparse() << std::endl;
    }

    // pre 10.2 API
    std::cout << "Has Q?: " << name_tree.hasName("Q") << std::endl;
    std::cout << "Has W?: " << name_tree.hasName("W") << std::endl;
    QPDFObjectHandle obj;
    std::cout << "Found W?: " << name_tree.findObject("W", obj) << std::endl;
    std::cout << "Found Q?: " << name_tree.findObject("Q", obj) << std::endl;
    std::cout << "Q: " << obj.unparse() << std::endl;

    // 10.2 API
    iter = name_tree.find("Q");
    std::cout << "Q: " << iter->first << " -> " << iter->second.unparse()
              << std::endl;
    iter = name_tree.find("W");
    std::cout << "W found: " << (iter != name_tree.end()) << std::endl;
    // Allow find to return predecessor
    iter = name_tree.find("W", true);
    std::cout << "W's predecessor: " << iter->first << " -> "
              << iter->second.unparse() << std::endl;

    // We can also remove items
    std::cout << "Remove P: " << name_tree.remove("P", &obj) << std::endl;
    std::cout << "Value removed: " << obj.unparse() << std::endl;
    std::cout << "Has P?: " << name_tree.hasName("P") << std::endl;
    // Or we can remove using an iterator
    iter = name_tree.find("K");
    std::cout << "Find K: " << iter->second.unparse() << std::endl;
    iter.remove();
    std::cout << "Iter after removing K: " << iter->first << " -> "
              << iter->second.unparse() << std::endl;
    std::cout << "Has K?: " << name_tree.hasName("K") << std::endl;

    // Illustrate some more advanced usage using number trees. These
    // calls work for name trees too.

    // The safe way to populate a tree is to call insert repeatedly as
    // above, but if you know you are definitely inserting items in
    // order, it is more efficient to insert them using insertAfter,
    // which avoids doing a binary search through the tree for each
    // insertion. Note that if you don't insert items in order using
    // this method, you will create an invalid tree.
    auto number_tree = QPDFNumberTreeObjectHelper::newEmpty(qpdf);
    auto number_tree_oh = number_tree.getObjectHandle();
    example.replaceKey("/NumberTree", number_tree_oh);
    auto iter2 = number_tree.begin();
    for (int i = 7; i <= 350; i += 7) {
        iter2.insertAfter(
            i, QPDFObjectHandle::newString("-" + std::to_string(i) + "-"));
    }
    std::cout << "Numbers:" << std::endl;
    int n = 1;
    for (auto& i: number_tree) {
        std::cout << i.first << " -> " << i.second.getUTF8Value();
        if (n % 5) {
            std::cout << ", ";
        } else {
            std::cout << std::endl;
        }
        ++n;
    }

    // When you remove an item with an iterator, the iterator
    // advances. This makes it possible to filter while iterating.
    // Remove all items that are multiples of 5.
    iter2 = number_tree.begin();
    while (iter2 != number_tree.end()) {
        if (iter2->first % 5 == 0) {
            iter2.remove(); // also advances
        } else {
            ++iter2;
        }
    }
    std::cout << "Numbers after filtering:" << std::endl;
    n = 1;
    for (auto& i: number_tree) {
        std::cout << i.first << " -> " << i.second.getUTF8Value();
        if (n % 5) {
            std::cout << ", ";
        } else {
            std::cout << std::endl;
        }
        ++n;
    }

    // Write to an output file
    QPDFWriter w(qpdf, outfilename);
    w.setQDFMode(true);
    w.setStaticID(true); // for testing only
    w.write();

    return 0;
}
