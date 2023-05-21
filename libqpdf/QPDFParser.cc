#include <qpdf/QPDFParser.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_InlineImage.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Operator.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <memory>

namespace
{
    struct StackFrame
    {
        StackFrame(std::shared_ptr<InputSource> input) :
            offset(input->tell())
        {
        }

        std::vector<std::shared_ptr<QPDFObject>> olist;
        qpdf_offset_t offset;
        std::string contents_string{""};
        qpdf_offset_t contents_offset{-1};
        int null_count{0};
    };
} // namespace

QPDFObjectHandle
QPDFParser::parse(bool& empty, bool content_stream)
{
    // This method must take care not to resolve any objects. Don't
    // check the type of any object without first ensuring that it is
    // a direct object. Otherwise, doing so may have the side effect
    // of reading the object and changing the file pointer. If you do
    // this, it will cause a logic error to be thrown from
    // QPDF::inParse().

    const static std::shared_ptr<QPDFObject> null_oh = QPDF_Null::create();
    QPDF::ParseGuard pg(context);

    empty = false;

    std::shared_ptr<QPDFObject> object;
    bool set_offset = false;

    std::vector<StackFrame> stack;
    stack.push_back(StackFrame(input));
    std::vector<parser_state_e> state_stack;
    state_stack.push_back(st_top);
    qpdf_offset_t offset;
    bool done = false;
    int bad_count = 0;
    int good_count = 0;
    bool b_contents = false;
    bool is_null = false;

    while (!done) {
        bool bad = false;
        bool indirect_ref = false;
        is_null = false;
        auto& frame = stack.back();
        auto& olist = frame.olist;
        parser_state_e state = state_stack.back();
        offset = frame.offset;

        object = nullptr;
        set_offset = false;

        if (!tokenizer.nextToken(*input, object_description)) {
            warn(tokenizer.getErrorMessage());
        }

        switch (tokenizer.getType()) {
        case QPDFTokenizer::tt_eof:
            if (!content_stream) {
                QTC::TC("qpdf", "QPDFParser eof in parse");
                warn("unexpected EOF");
            }
            bad = true;
            state = st_eof;
            break;

        case QPDFTokenizer::tt_bad:
            QTC::TC("qpdf", "QPDFParser bad token in parse");
            bad = true;
            is_null = true;
            break;

        case QPDFTokenizer::tt_brace_open:
        case QPDFTokenizer::tt_brace_close:
            QTC::TC("qpdf", "QPDFParser bad brace");
            warn("treating unexpected brace token as null");
            bad = true;
            is_null = true;
            break;

        case QPDFTokenizer::tt_array_close:
            if (state == st_array) {
                state = st_stop;
            } else {
                QTC::TC("qpdf", "QPDFParser bad array close");
                warn("treating unexpected array close token as null");
                bad = true;
                is_null = true;
            }
            break;

        case QPDFTokenizer::tt_dict_close:
            if (state == st_dictionary) {
                state = st_stop;
            } else {
                QTC::TC("qpdf", "QPDFParser bad dictionary close");
                warn("unexpected dictionary close token");
                bad = true;
                is_null = true;
            }
            break;

        case QPDFTokenizer::tt_array_open:
        case QPDFTokenizer::tt_dict_open:
            if (stack.size() > 500) {
                QTC::TC("qpdf", "QPDFParser too deep");
                warn("ignoring excessively deeply nested data structure");
                bad = true;
                is_null = true;
                state = st_top;
            } else {
                state = st_start;
                state_stack.push_back(
                    (tokenizer.getType() == QPDFTokenizer::tt_array_open) ? st_array
                                                                          : st_dictionary);
                b_contents = false;
                stack.push_back(StackFrame(input));
            }
            break;

        case QPDFTokenizer::tt_bool:
            object = QPDF_Bool::create((tokenizer.getValue() == "true"));
            break;

        case QPDFTokenizer::tt_null:
            is_null = true;
            ++frame.null_count;

            break;

        case QPDFTokenizer::tt_integer:
            object = QPDF_Integer::create(
                QUtil::string_to_ll(std::string(tokenizer.getValue()).c_str()));
            break;

        case QPDFTokenizer::tt_real:
            object = QPDF_Real::create(tokenizer.getValue());
            break;

        case QPDFTokenizer::tt_name:
            {
                auto name = tokenizer.getValue();
                object = QPDF_Name::create(name);

                if (name == "/Contents") {
                    b_contents = true;
                } else {
                    b_contents = false;
                }
            }
            break;

        case QPDFTokenizer::tt_word:
            {
                auto value = tokenizer.getValue();
                auto size = olist.size();
                if (content_stream) {
                    object = QPDF_Operator::create(value);
                } else if (
                    value == "R" && state != st_top && size >= 2 && olist.back() &&
                    olist.back()->getTypeCode() == ::ot_integer &&
                    !olist.back()->getObjGen().isIndirect() && olist.at(size - 2) &&
                    olist.at(size - 2)->getTypeCode() == ::ot_integer &&
                    !olist.at(size - 2)->getObjGen().isIndirect()) {
                    if (context == nullptr) {
                        QTC::TC("qpdf", "QPDFParser indirect without context");
                        throw std::logic_error("QPDFObjectHandle::parse called without context"
                                               " on an object with indirect references");
                    }
                    auto ref_og = QPDFObjGen(
                        QPDFObjectHandle(olist.at(size - 2)).getIntValueAsInt(),
                        QPDFObjectHandle(olist.back()).getIntValueAsInt());
                    if (ref_og.isIndirect()) {
                        // This action has the desirable side effect
                        // of causing dangling references (references
                        // to indirect objects that don't appear in
                        // the PDF) in any parsed object to appear in
                        // the object cache.
                        object = context->getObject(ref_og).obj;
                        indirect_ref = true;
                    } else {
                        QTC::TC("qpdf", "QPDFParser indirect with 0 objid");
                        is_null = true;
                    }
                    olist.pop_back();
                    olist.pop_back();
                } else if ((value == "endobj") && (state == st_top)) {
                    // We just saw endobj without having read
                    // anything.  Treat this as a null and do not move
                    // the input source's offset.
                    is_null = true;
                    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
                } else {
                    QTC::TC("qpdf", "QPDFParser treat word as string");
                    warn("unknown token while reading object;"
                         " treating as string");
                    bad = true;
                    object = QPDF_String::create(value);
                }
            }
            break;

        case QPDFTokenizer::tt_string:
            {
                auto val = tokenizer.getValue();
                if (decrypter) {
                    if (b_contents) {
                        frame.contents_string = val;
                        frame.contents_offset = input->getLastOffset();
                        b_contents = false;
                    }
                    std::string s{val};
                    decrypter->decryptString(s);
                    object = QPDF_String::create(s);
                } else {
                    object = QPDF_String::create(val);
                }
            }

            break;

        default:
            warn("treating unknown token type as null while "
                 "reading object");
            bad = true;
            is_null = true;
            break;
        }

        if (object == nullptr && !is_null &&
            (!((state == st_start) || (state == st_stop) || (state == st_eof)))) {
            throw std::logic_error("QPDFObjectHandle::parseInternal: "
                                   "unexpected uninitialized object");
            is_null = true;
        }

        if (bad) {
            ++bad_count;
            good_count = 0;
        } else {
            ++good_count;
            if (good_count > 3) {
                bad_count = 0;
            }
        }
        if (bad_count > 5) {
            // We had too many consecutive errors without enough
            // intervening successful objects. Give up.
            warn("too many errors; giving up on reading object");
            state = st_top;
            is_null = true;
        }

        switch (state) {
        case st_eof:
            if (state_stack.size() > 1) {
                warn("parse error while reading object");
            }
            done = true;
            // In content stream mode, leave object uninitialized to
            // indicate EOF
            if (!content_stream) {
                is_null = true;
            }
            break;

        case st_dictionary:
        case st_array:
            if (is_null) {
                object = null_oh;
                // No need to set description for direct nulls - they probably
                // will become implicit.
            } else if (!indirect_ref) {
                setDescription(object, input->getLastOffset());
            }
            set_offset = true;
            olist.push_back(object);
            break;

        case st_top:
            done = true;
            break;

        case st_start:
            break;

        case st_stop:
            if ((state_stack.size() < 2) || (stack.size() < 2)) {
                throw std::logic_error("QPDFObjectHandle::parseInternal: st_stop encountered"
                                       " with insufficient elements in stack");
            }
            parser_state_e old_state = state_stack.back();
            state_stack.pop_back();
            if (old_state == st_array) {
                object = QPDF_Array::create(std::move(olist), frame.null_count > 100);
                setDescription(object, offset - 1);
                // The `offset` points to the next of "[".  Set the rewind
                // offset to point to the beginning of "[". This has been
                // explicitly tested with whitespace surrounding the array start
                // delimiter. getLastOffset points to the array end token and
                // therefore can't be used here.
                set_offset = true;
            } else if (old_state == st_dictionary) {
                // Convert list to map. Alternating elements are keys.  Attempt
                // to recover more or less gracefully from invalid dictionaries.
                std::set<std::string> names;
                for (auto& obj: olist) {
                    if (obj) {
                        if (obj->getTypeCode() == ::ot_name) {
                            names.insert(obj->getStringValue());
                        }
                    }
                }

                std::map<std::string, QPDFObjectHandle> dict;
                int next_fake_key = 1;
                for (auto iter = olist.begin(); iter != olist.end();) {
                    // Calculate key.
                    std::string key;
                    if (*iter && (*iter)->getTypeCode() == ::ot_name) {
                        key = (*iter)->getStringValue();
                        ++iter;
                    } else {
                        for (bool found_fake = false; !found_fake;) {
                            key = "/QPDFFake" + std::to_string(next_fake_key++);
                            found_fake = (names.count(key) == 0);
                            QTC::TC("qpdf", "QPDFParser found fake", (found_fake ? 0 : 1));
                        }
                        warn(
                            offset,
                            "expected dictionary key but found"
                            " non-name object; inserting key " +
                                key);
                    }
                    if (dict.count(key) > 0) {
                        QTC::TC("qpdf", "QPDFParser duplicate dict key");
                        warn(
                            offset,
                            "dictionary has duplicated key " + key +
                                "; last occurrence overrides earlier "
                                "ones");
                    }

                    // Calculate value.
                    std::shared_ptr<QPDFObject> val;
                    if (iter != olist.end()) {
                        val = *iter;
                        ++iter;
                    } else {
                        QTC::TC("qpdf", "QPDFParser no val for last key");
                        warn(
                            offset,
                            "dictionary ended prematurely; "
                            "using null as value for last key");
                        val = QPDF_Null::create();
                    }

                    dict[std::move(key)] = std::move(val);
                }
                if (!frame.contents_string.empty() && dict.count("/Type") &&
                    dict["/Type"].isNameAndEquals("/Sig") && dict.count("/ByteRange") &&
                    dict.count("/Contents") && dict["/Contents"].isString()) {
                    dict["/Contents"] = QPDFObjectHandle::newString(frame.contents_string);
                    dict["/Contents"].setParsedOffset(frame.contents_offset);
                }
                object = QPDF_Dictionary::create(std::move(dict));
                setDescription(object, offset - 2);
                // The `offset` points to the next of "<<". Set the rewind
                // offset to point to the beginning of "<<". This has been
                // explicitly tested with whitespace surrounding the dictionary
                // start delimiter. getLastOffset points to the dictionary end
                // token and therefore can't be used here.
                set_offset = true;
            }
            stack.pop_back();
            if (state_stack.back() == st_top) {
                done = true;
            } else {
                stack.back().olist.push_back(object);
            }
        }
    }

    if (is_null) {
        object = QPDF_Null::create();
    }
    if (!set_offset) {
        setDescription(object, offset);
    }
    return object;
}

void
QPDFParser::setDescription(std::shared_ptr<QPDFObject>& obj, qpdf_offset_t parsed_offset)
{
    if (obj) {
        obj->setDescription(context, description, parsed_offset);
    }
}

void
QPDFParser::warn(QPDFExc const& e) const
{
    // If parsing on behalf of a QPDF object and want to give a
    // warning, we can warn through the object. If parsing for some
    // other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (context) {
        context->warn(e);
    } else {
        throw e;
    }
}

void
QPDFParser::warn(qpdf_offset_t offset, std::string const& msg) const
{
    warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(), object_description, offset, msg));
}

void
QPDFParser::warn(std::string const& msg) const
{
    warn(input->getLastOffset(), msg);
}
