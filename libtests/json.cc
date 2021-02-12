#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <iostream>
#include <assert.h>

static void check(JSON const& j, std::string const& exp)
{
    if (exp != j.unparse())
    {
        std::cout << "Got " << j.unparse() << "; wanted " << exp << "\n";
    }
}

static void test_main()
{
    JSON jstr = JSON::makeString(
        "<1>\xcf\x80<2>\xf0\x9f\xa5\x94\\\"<3>\x03\t\b\r\n<4>");
    check(jstr,
          "\"<1>\xcf\x80<2>\xf0\x9f\xa5\x94\\\\\\\"<3>"
          "\\u0003\\t\\b\\r\\n<4>\"");
    JSON jnull = JSON::makeNull();
    check(jnull, "null");
    JSON jarr = JSON::makeArray();
    check(jarr, "[]");
    JSON jstr2 = JSON::makeString("a\tb");
    JSON jint = JSON::makeInt(16059);
    JSON jdouble = JSON::makeReal(3.14159);
    JSON jexp = JSON::makeNumber("2.1e5");
    jarr.addArrayElement(jstr2);
    jarr.addArrayElement(jnull);
    jarr.addArrayElement(jint);
    jarr.addArrayElement(jdouble);
    jarr.addArrayElement(jexp);
    check(jarr,
          "[\n"
          "  \"a\\tb\",\n"
          "  null,\n"
          "  16059,\n"
          "  3.14159,\n"
          "  2.1e5\n"
          "]");
    JSON jmap = JSON::makeDictionary();
    check(jmap, "{}");
    jmap.addDictionaryMember("b", jstr2);
    jmap.addDictionaryMember("a", jarr);
    jmap.addDictionaryMember("c\r\nd", jnull);
    jmap.addDictionaryMember("yes", JSON::makeBool(false));
    jmap.addDictionaryMember("no", JSON::makeBool(true));
    jmap.addDictionaryMember("empty_dict", JSON::makeDictionary());
    jmap.addDictionaryMember("empty_list", JSON::makeArray());
    jmap.addDictionaryMember("single", JSON::makeArray()).
        addArrayElement(JSON::makeInt(12));
    check(jmap,
          "{\n"
          "  \"a\": [\n"
          "    \"a\\tb\",\n"
          "    null,\n"
          "    16059,\n"
          "    3.14159,\n"
          "    2.1e5\n"
          "  ],\n"
          "  \"b\": \"a\\tb\",\n"
          "  \"c\\r\\nd\": null,\n"
          "  \"empty_dict\": {},\n"
          "  \"empty_list\": [],\n"
          "  \"no\": true,\n"
          "  \"single\": [\n"
          "    12\n"
          "  ],\n"
          "  \"yes\": false\n"
          "}");
    check(QPDFObjectHandle::newReal("0.12").getJSON(), "0.12");
    check(QPDFObjectHandle::newReal(".34").getJSON(), "0.34");
    check(QPDFObjectHandle::newReal("-0.56").getJSON(), "-0.56");
    check(QPDFObjectHandle::newReal("-.78").getJSON(), "-0.78");
}

static void check_schema(JSON& obj, JSON& schema, bool exp,
                         std::string const& description)
{
    std::list<std::string> errors;
    std::cout << "--- " << description << std::endl;
    assert(exp == obj.checkSchema(schema, errors));
    for (std::list<std::string>::iterator iter = errors.begin();
         iter != errors.end(); ++iter)
    {
        std::cout << *iter << std::endl;
    }
    std::cout << "---" << std::endl;
}

static void test_schema()
{
    // Since we don't have a JSON parser, use the PDF parser as a
    // shortcut for creating a complex JSON structure.
    JSON schema = QPDFObjectHandle::parse(
        "<<"
        "  /one <<"
        "    /a <<"
        "      /q (queue)"
        "      /r <<"
        "        /x (ecks)"
        "        /y (why)"
        "      >>"
        "      /s [ (esses) ]"
        "    >>"
        "  >>"
        "  /two ["
        "    <<"
        "      /goose (gander)"
        "      /glarp (enspliel)"
        "    >>"
        "  ]"
        ">>").getJSON();
    JSON three = JSON::makeDictionary();
    three.addDictionaryMember(
        "<objid>",
        QPDFObjectHandle::parse("<< /z (ebra) >>").getJSON());
    schema.addDictionaryMember("/three", three);
    JSON a = QPDFObjectHandle::parse("[(not a) (dictionary)]").getJSON();
    check_schema(a, schema, false, "top-level type mismatch");
    JSON b = QPDFObjectHandle::parse(
        "<<"
        "  /one <<"
        "    /a <<"
        "      /t (oops)"
        "      /r ["
        "        /x (ecks)"
        "        /y (why)"
        "      ]"
        "      /s << /z (esses) >>"
        "    >>"
        "  >>"
        "  /two ["
        "    <<"
        "      /goose (0 gander)"
        "      /glarp (0 enspliel)"
        "    >>"
        "    <<"
        "      /goose (1 gander)"
        "      /flarp (1 enspliel)"
        "    >>"
        "    2"
        "    [ (three) ]"
        "    <<"
        "      /goose (4 gander)"
        "      /glarp (4 enspliel)"
        "    >>"
        "  ]"
        "  /three <<"
        "    /anything << /x (oops) >>"
        "    /else << /z (okay) >>"
        "  >>"
        ">>").getJSON();
    check_schema(b, schema, false, "missing items");
    check_schema(a, a, false, "top-level schema array error");
    check_schema(b, b, false, "lower-level schema array error");
    check_schema(schema, schema, true, "pass");
}

int main()
{
    test_main();
    test_schema();

    std::cout << "end of json tests\n";
    return 0;
}
