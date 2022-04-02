#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <iostream>

#ifdef NDEBUG
// We need assert even in a release build for test code.
# undef NDEBUG
#endif
#include <cassert>

static void
check(JSON const& j, std::string const& exp)
{
    if (exp != j.unparse()) {
        std::cout << "Got " << j.unparse() << "; wanted " << exp << "\n";
    }
}

static void
test_main()
{
    JSON jstr = JSON::makeString(
        "<1>\xcf\x80<2>\xf0\x9f\xa5\x94\\\"<3>\x03\t\b\r\n<4>");
    check(
        jstr,
        "\"<1>\xcf\x80<2>\xf0\x9f\xa5\x94\\\\\\\"<3>"
        "\\u0003\\t\\b\\r\\n<4>\"");
    JSON jnull = JSON::makeNull();
    check(jnull, "null");
    assert(jnull.isNull());
    std::string value;
    assert(!jnull.getNumber(value));
    JSON jarr = JSON::makeArray();
    check(jarr, "[]");
    JSON jstr2 = JSON::makeString("a\tb");
    assert(jstr2.getString(value));
    assert(value == "a\tb");
    assert(!jstr2.getNumber(value));
    /* cSpell: ignore jbool xavalue dvalue xdvalue */
    JSON jint = JSON::makeInt(16059);
    JSON jdouble = JSON::makeReal(3.14159);
    JSON jexp = JSON::makeNumber("2.1e5");
    JSON jbool1 = JSON::makeBool(true);
    JSON jbool2 = JSON::makeBool(false);
    bool bvalue = false;
    assert(jbool1.getBool(bvalue));
    assert(bvalue);
    assert(jbool2.getBool(bvalue));
    assert(!bvalue);
    jarr.addArrayElement(jstr2);
    jarr.addArrayElement(jnull);
    jarr.addArrayElement(jint);
    jarr.addArrayElement(jdouble);
    jarr.addArrayElement(jexp);
    check(
        jarr,
        "[\n"
        "  \"a\\tb\",\n"
        "  null,\n"
        "  16059,\n"
        "  3.14159,\n"
        "  2.1e5\n"
        "]");
    std::vector<std::string> avalue;
    assert(jarr.forEachArrayItem(
        [&avalue](JSON j) { avalue.push_back(j.unparse()); }));
    std::vector<std::string> xavalue = {
        "\"a\\tb\"",
        "null",
        "16059",
        "3.14159",
        "2.1e5",
    };
    assert(avalue == xavalue);
    JSON jmap = JSON::makeDictionary();
    check(jmap, "{}");
    jmap.addDictionaryMember("b", jstr2);
    jmap.addDictionaryMember("a", jarr);
    jmap.addDictionaryMember("c\r\nd", jnull);
    jmap.addDictionaryMember("yes", JSON::makeBool(false));
    jmap.addDictionaryMember("no", JSON::makeBool(true));
    jmap.addDictionaryMember("empty_dict", JSON::makeDictionary());
    jmap.addDictionaryMember("empty_list", JSON::makeArray());
    jmap.addDictionaryMember("single", JSON::makeArray())
        .addArrayElement(JSON::makeInt(12));
    check(
        jmap,
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
    JSON jmap2 = JSON::parse(R"({"a": 1, "b": "two", "c": [true]})");
    std::map<std::string, std::string> dvalue;
    assert(jmap2.forEachDictItem(
        [&dvalue](std::string const& k, JSON j) { dvalue[k] = j.unparse(); }));
    std::map<std::string, std::string> xdvalue = {
        {"a", "1"},
        {"b", "\"two\""},
        {"c", "[\n  true\n]"},
    };
    assert(dvalue == xdvalue);
}

static void
check_schema(
    JSON& obj,
    JSON& schema,
    unsigned long flags,
    bool exp,
    std::string const& description)
{
    std::list<std::string> errors;
    std::cout << "--- " << description << std::endl;
    assert(exp == obj.checkSchema(schema, flags, errors));
    for (std::list<std::string>::iterator iter = errors.begin();
         iter != errors.end();
         ++iter) {
        std::cout << *iter << std::endl;
    }
    std::cout << "---" << std::endl;
}

static void
test_schema()
{
    /* cSpell: ignore ptional ebra */
    JSON schema = JSON::parse(R"(
{
  "one": {
    "a": {
      "q": "queue",
      "r": {
        "x": "ecks"
      },
      "s": [
        "esses"
      ]
    }
  },
  "two": [
    {
      "goose": "gander",
      "glarp": "enspliel"
    }
  ],
  "three": {
    "<objid>": {
      "z": "ebra",
      "o": "ptional"
    }
  }
}
)");

    JSON a = JSON::parse(R"(["not a", "dictionary"])");
    check_schema(a, schema, 0, false, "top-level type mismatch");
    JSON b = JSON::parse(R"(
{
  "one": {
    "a": {
      "t": "oops",
      "r": [
        "x",
        "ecks",
        "y",
        "why"
      ],
      "s": {
        "z": "esses"
      }
    }
  },
  "two": [
    {
      "goose": "0 gander",
      "glarp": "0 enspliel"
    },
    {
      "goose": "1 gander",
      "flarp": "1 enspliel"
    },
    2,
    [
      "three"
    ],
    {
      "goose": "4 gander",
      "glarp": 4
    }
  ],
  "three": {
    "anything": {
      "x": "oops",
      "o": "okay"
    },
    "else": {
      "z": "okay"
    }
  }
}
)");

    check_schema(b, schema, 0, false, "missing items");
    check_schema(a, a, 0, false, "top-level schema array error");
    check_schema(b, b, 0, false, "lower-level schema array error");

    JSON bad_schema = JSON::parse(R"({"a": true, "b": "potato?"})");
    check_schema(bad_schema, bad_schema, 0, false, "bad schema field type");

    JSON good = JSON::parse(R"(
{
  "one": {
    "a": {
      "q": "potato",
      "r": {
        "x": [1, null]
      },
      "s": [
        null,
        "anything"
      ]
    }
  },
  "two": [
    {
      "glarp": "enspliel",
      "goose": 3.14
    }
  ],
  "three": {
    "<objid>": {
      "z": "ebra"
    }
  }
}
)");
    check_schema(good, schema, 0, false, "not optional");
    check_schema(good, schema, JSON::f_optional, true, "pass");
}

int
main()
{
    test_main();
    test_schema();

    std::cout << "end of json tests\n";
    return 0;
}
