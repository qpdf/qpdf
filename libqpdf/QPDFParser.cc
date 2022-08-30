#include <qpdf/QPDFParser.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/SparseOHArray.hh>

namespace
{
    struct StackFrame
    {
        StackFrame(std::shared_ptr<InputSource> input) :
            offset(input->tell()),
            contents_string(""),
            contents_offset(-1)
        {
        }

        std::vector<QPDFObjectHandle> olist;
        qpdf_offset_t offset;
        std::string contents_string;
        qpdf_offset_t contents_offset;
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

    QPDF::ParseGuard pg(context);

    empty = false;

    QPDFObjectHandle object;
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

    while (!done) {
        bool bad = false;
        auto& frame = stack.back();
        auto& olist = frame.olist;
        parser_state_e state = state_stack.back();
        offset = frame.offset;

        object = QPDFObjectHandle();
        set_offset = false;

        QPDFTokenizer::Token token =
            tokenizer.readToken(input, object_description, true);
        std::string const& token_error_message = token.getErrorMessage();
        if (!token_error_message.empty()) {
            // Tokens other than tt_bad can still generate warnings.
            warn(token_error_message);
        }

        switch (token.getType()) {
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
            object = QPDFObjectHandle::newNull();
            break;

        case QPDFTokenizer::tt_brace_open:
        case QPDFTokenizer::tt_brace_close:
            QTC::TC("qpdf", "QPDFParser bad brace");
            warn("treating unexpected brace token as null");
            bad = true;
            object = QPDFObjectHandle::newNull();
            break;

        case QPDFTokenizer::tt_array_close:
            if (state == st_array) {
                state = st_stop;
            } else {
                QTC::TC("qpdf", "QPDFParser bad array close");
                warn("treating unexpected array close token as null");
                bad = true;
                object = QPDFObjectHandle::newNull();
            }
            break;

        case QPDFTokenizer::tt_dict_close:
            if (state == st_dictionary) {
                state = st_stop;
            } else {
                QTC::TC("qpdf", "QPDFParser bad dictionary close");
                warn("unexpected dictionary close token");
                bad = true;
                object = QPDFObjectHandle::newNull();
            }
            break;

        case QPDFTokenizer::tt_array_open:
        case QPDFTokenizer::tt_dict_open:
            if (stack.size() > 500) {
                QTC::TC("qpdf", "QPDFParser too deep");
                warn("ignoring excessively deeply nested data structure");
                bad = true;
                object = QPDFObjectHandle::newNull();
                state = st_top;
            } else {
                state = st_start;
                state_stack.push_back(
                    (token.getType() == QPDFTokenizer::tt_array_open)
                        ? st_array
                        : st_dictionary);
                b_contents = false;
                stack.push_back(StackFrame(input));
            }
            break;

        case QPDFTokenizer::tt_bool:
            object = QPDFObjectHandle::newBool((token.getValue() == "true"));
            break;

        case QPDFTokenizer::tt_null:
            object = QPDFObjectHandle::newNull();
            break;

        case QPDFTokenizer::tt_integer:
            object = QPDFObjectHandle::newInteger(
                QUtil::string_to_ll(token.getValue().c_str()));
            break;

        case QPDFTokenizer::tt_real:
            object = QPDFObjectHandle::newReal(token.getValue());
            break;

        case QPDFTokenizer::tt_name:
            {
                std::string name = token.getValue();
                object = QPDFObjectHandle::newName(name);

                if (name == "/Contents") {
                    b_contents = true;
                } else {
                    b_contents = false;
                }
            }
            break;

        case QPDFTokenizer::tt_word:
            {
                std::string const& value = token.getValue();
                auto size = olist.size();
                if (content_stream) {
                    object = QPDFObjectHandle::newOperator(value);
                } else if (
                    (value == "R") && (state != st_top) && (size >= 2) &&
                    (!olist.back().isIndirect()) &&
                    (olist.back().isInteger()) &&
                    (!olist.at(size - 2).isIndirect()) &&
                    (olist.at(size - 2).isInteger())) {
                    if (context == nullptr) {
                        QTC::TC("qpdf", "QPDFParser indirect without context");
                        throw std::logic_error(
                            "QPDFObjectHandle::parse called without context"
                            " on an object with indirect references");
                    }
                    // Try to resolve indirect objects
                    object = QPDFObjectHandle::newIndirect(
                        context,
                        QPDFObjGen(
                            olist.at(size - 2).getIntValueAsInt(),
                            olist.back().getIntValueAsInt()));
                    olist.pop_back();
                    olist.pop_back();
                } else if ((value == "endobj") && (state == st_top)) {
                    // We just saw endobj without having read
                    // anything.  Treat this as a null and do not move
                    // the input source's offset.
                    object = QPDFObjectHandle::newNull();
                    input->seek(input->getLastOffset(), SEEK_SET);
                    empty = true;
                } else {
                    QTC::TC("qpdf", "QPDFParser treat word as string");
                    warn("unknown token while reading object;"
                         " treating as string");
                    bad = true;
                    object = QPDFObjectHandle::newString(value);
                }
            }
            break;

        case QPDFTokenizer::tt_string:
            {
                std::string val = token.getValue();
                if (decrypter) {
                    if (b_contents) {
                        frame.contents_string = val;
                        frame.contents_offset = input->getLastOffset();
                        b_contents = false;
                    }
                    decrypter->decryptString(val);
                }
                object = QPDFObjectHandle::newString(val);
            }

            break;

        default:
            warn("treating unknown token type as null while "
                 "reading object");
            bad = true;
            object = QPDFObjectHandle::newNull();
            break;
        }

        if ((!object.isInitialized()) &&
            (!((state == st_start) || (state == st_stop) ||
               (state == st_eof)))) {
            throw std::logic_error("QPDFObjectHandle::parseInternal: "
                                   "unexpected uninitialized object");
            object = QPDFObjectHandle::newNull();
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
            object = QPDFObjectHandle::newNull();
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
                object = QPDFObjectHandle::newNull();
            }
            break;

        case st_dictionary:
        case st_array:
            setDescriptionFromInput(object, input->getLastOffset());
            object.setParsedOffset(input->getLastOffset());
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
                throw std::logic_error(
                    "QPDFObjectHandle::parseInternal: st_stop encountered"
                    " with insufficient elements in stack");
            }
            parser_state_e old_state = state_stack.back();
            state_stack.pop_back();
            if (old_state == st_array) {
                object = QPDFObjectHandle::newArray(olist);
                setDescriptionFromInput(object, offset);
                // The `offset` points to the next of "[".  Set the rewind
                // offset to point to the beginning of "[". This has been
                // explicitly tested with whitespace surrounding the array start
                // delimiter. getLastOffset points to the array end token and
                // therefore can't be used here.
                object.setParsedOffset(offset - 1);
                set_offset = true;
            } else if (old_state == st_dictionary) {
                // Convert list to map. Alternating elements are keys.  Attempt
                // to recover more or less gracefully from invalid dictionaries.
                std::set<std::string> names;
                size_t n_elements = olist.size();
                for (size_t i = 0; i < n_elements; ++i) {
                    QPDFObjectHandle oh = olist.at(i);
                    if ((!oh.isIndirect()) && oh.isName()) {
                        names.insert(oh.getName());
                    }
                }

                std::map<std::string, QPDFObjectHandle> dict;
                int next_fake_key = 1;
                for (unsigned int i = 0; i < n_elements; ++i) {
                    QPDFObjectHandle key_obj = olist.at(i);
                    QPDFObjectHandle val;
                    if (key_obj.isIndirect() || (!key_obj.isName())) {
                        bool found_fake = false;
                        std::string candidate;
                        while (!found_fake) {
                            candidate = "/QPDFFake" +
                                QUtil::int_to_string(next_fake_key++);
                            found_fake = (names.count(candidate) == 0);
                            QTC::TC(
                                "qpdf",
                                "QPDFParser found fake",
                                (found_fake ? 0 : 1));
                        }
                        warn(
                            offset,
                            "expected dictionary key but found"
                            " non-name object; inserting key " +
                                candidate);
                        val = key_obj;
                        key_obj = QPDFObjectHandle::newName(candidate);
                    } else if (i + 1 >= olist.size()) {
                        QTC::TC("qpdf", "QPDFParser no val for last key");
                        warn(
                            offset,
                            "dictionary ended prematurely; "
                            "using null as value for last key");
                        val = QPDFObjectHandle::newNull();
                        setDescriptionFromInput(val, offset);
                    } else {
                        val = olist.at(++i);
                    }
                    std::string key = key_obj.getName();
                    if (dict.count(key) > 0) {
                        QTC::TC("qpdf", "QPDFParser duplicate dict key");
                        warn(
                            offset,
                            "dictionary has duplicated key " + key +
                                "; last occurrence overrides earlier "
                                "ones");
                    }
                    dict[key] = val;
                }
                if (!frame.contents_string.empty() && dict.count("/Type") &&
                    dict["/Type"].isNameAndEquals("/Sig") &&
                    dict.count("/ByteRange") && dict.count("/Contents") &&
                    dict["/Contents"].isString()) {
                    dict["/Contents"] =
                        QPDFObjectHandle::newString(frame.contents_string);
                    dict["/Contents"].setParsedOffset(frame.contents_offset);
                }
                object = QPDFObjectHandle::newDictionary(dict);
                setDescriptionFromInput(object, offset);
                // The `offset` points to the next of "<<". Set the rewind
                // offset to point to the beginning of "<<". This has been
                // explicitly tested with whitespace surrounding the dictionary
                // start delimiter. getLastOffset points to the dictionary end
                // token and therefore can't be used here.
                object.setParsedOffset(offset - 2);
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

    if (!set_offset) {
        setDescriptionFromInput(object, offset);
        object.setParsedOffset(offset);
    }
    return object;
}

void
QPDFParser::setDescriptionFromInput(
    QPDFObjectHandle oh, qpdf_offset_t offset) const
{
    oh.setObjectDescription(
        context,
        (input->getName() + ", " + object_description + " at offset " +
         QUtil::int_to_string(offset)));
}

void
QPDFParser::warn(QPDF* qpdf, QPDFExc const& e)
{
    // If parsing on behalf of a QPDF object and want to give a
    // warning, we can warn through the object. If parsing for some
    // other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (qpdf) {
        qpdf->warn(e);
    } else {
        throw e;
    }
}

void
QPDFParser::warn(qpdf_offset_t offset, std::string const& msg) const
{
    warn(
        context,
        QPDFExc(
            qpdf_e_damaged_pdf,
            input->getName(),
            object_description,
            offset,
            msg));
}

void
QPDFParser::warn(std::string const& msg) const
{
    warn(input->getLastOffset(), msg);
}
