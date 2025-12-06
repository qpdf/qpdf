#include <qpdf/QPDFParser.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFTokenizer_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <memory>

using namespace std::literals;
using namespace qpdf;

using ObjectPtr = std::shared_ptr<QPDFObject>;

static uint32_t const& max_nesting{global::Limits::parser_max_nesting()};

// The ParseGuard class allows QPDFParser to detect re-entrant parsing. It also provides
// special access to allow the parser to create unresolved objects and dangling references.
class QPDF::Doc::ParseGuard
{
  public:
    ParseGuard(QPDF* qpdf) :
        objects(qpdf ? &qpdf->m->objects : nullptr)
    {
        if (objects) {
            objects->inParse(true);
        }
    }

    static std::shared_ptr<QPDFObject>
    getObject(QPDF* qpdf, int id, int gen, bool parse_pdf)
    {
        return qpdf->m->objects.getObjectForParser(id, gen, parse_pdf);
    }

    ~ParseGuard()
    {
        if (objects) {
            objects->inParse(false);
        }
    }
    QPDF::Doc::Objects* objects;
};

using ParseGuard = QPDF::Doc::ParseGuard;

QPDFObjectHandle
QPDFParser::parse(InputSource& input, std::string const& object_description, QPDF* context)
{
    qpdf::Tokenizer tokenizer;
    if (auto result = QPDFParser(
                          input,
                          make_description(input.getName(), object_description),
                          object_description,
                          tokenizer,
                          nullptr,
                          context,
                          false)
                          .parse()) {
        return result;
    }
    return {QPDFObject::create<QPDF_Null>()};
}

QPDFObjectHandle
QPDFParser::parse_content(
    InputSource& input,
    std::shared_ptr<QPDFObject::Description> sp_description,
    qpdf::Tokenizer& tokenizer,
    QPDF* context)
{
    static const std::string content("content"); // GCC12 - make constexpr
    auto p = QPDFParser(
        input,
        std::move(sp_description),
        content,
        tokenizer,
        nullptr,
        context,
        true,
        0,
        0,
        context && context->doc().reconstructed_xref());
    auto result = p.parse(true);
    if (result || p.empty_) {
        // In content stream mode, leave object uninitialized to indicate EOF
        return result;
    }
    return {QPDFObject::create<QPDF_Null>()};
}

QPDFObjectHandle
QPDFParser::parse(
    InputSource& input,
    std::string const& object_description,
    QPDFTokenizer& tokenizer,
    bool& empty,
    QPDFObjectHandle::StringDecrypter* decrypter,
    QPDF* context)
{
    // ABI: This parse overload is only used by the deprecated QPDFObjectHandle::parse. It is the
    // only user of the 'empty' member. When removing this overload also remove 'empty'.
    auto p = QPDFParser(
        input,
        make_description(input.getName(), object_description),
        object_description,
        *tokenizer.m,
        decrypter,
        context,
        false);
    auto result = p.parse();
    empty = p.empty_;
    if (result) {
        return result;
    }
    return {QPDFObject::create<QPDF_Null>()};
}

QPDFObjectHandle
QPDFParser::parse(
    InputSource& input,
    std::string const& object_description,
    qpdf::Tokenizer& tokenizer,
    QPDFObjectHandle::StringDecrypter* decrypter,
    QPDF& context,
    bool sanity_checks)
{
    return QPDFParser(
               input,
               make_description(input.getName(), object_description),
               object_description,
               tokenizer,
               decrypter,
               &context,
               true,
               0,
               0,
               sanity_checks)
        .parse();
}

QPDFObjectHandle
QPDFParser::parse(
    is::OffsetBuffer& input, int stream_id, int obj_id, qpdf::Tokenizer& tokenizer, QPDF& context)
{
    return QPDFParser(
               input,
               std::make_shared<QPDFObject::Description>(
                   QPDFObject::ObjStreamDescr(stream_id, obj_id)),
               "",
               tokenizer,
               nullptr,
               &context,
               true,
               stream_id,
               obj_id)
        .parse();
}

QPDFObjectHandle
QPDFParser::parse(bool content_stream)
{
    try {
        return parse_first(content_stream);
    } catch (Error&) {
        return {};
    } catch (QPDFExc& e) {
        throw e;
    } catch (std::logic_error& e) {
        throw e;
    } catch (std::exception& e) {
        warn("treating object as null because of error during parsing: "s + e.what());
        return {};
    }
}

QPDFObjectHandle
QPDFParser::parse_first(bool content_stream)
{
    // This method must take care not to resolve any objects. Don't check the type of any object
    // without first ensuring that it is a direct object. Otherwise, doing so may have the side
    // effect of reading the object and changing the file pointer. If you do this, it will cause a
    // logic error to be thrown from QPDF::inParse().

    QPDF::Doc::ParseGuard pg(context_);
    start_ = input_.tell();
    if (!tokenizer_.nextToken(input_, object_description_)) {
        warn(tokenizer_.getErrorMessage());
    }

    switch (tokenizer_.getType()) {
    case QPDFTokenizer::tt_eof:
        if (content_stream) {
            // In content stream mode, leave object uninitialized to indicate EOF
            empty_ = true;
            return {};
        }
        warn("unexpected EOF");
        return {};

    case QPDFTokenizer::tt_bad:
        return {};

    case QPDFTokenizer::tt_brace_open:
    case QPDFTokenizer::tt_brace_close:
        warn("treating unexpected brace token as null");
        return {};

    case QPDFTokenizer::tt_array_close:
        warn("treating unexpected array close token as null");
        return {};

    case QPDFTokenizer::tt_dict_close:
        warn("unexpected dictionary close token");
        return {};

    case QPDFTokenizer::tt_array_open:
    case QPDFTokenizer::tt_dict_open:
        stack_.clear();
        stack_.emplace_back(
            input_,
            (tokenizer_.getType() == QPDFTokenizer::tt_array_open) ? st_array : st_dictionary_key);
        frame_ = &stack_.back();
        return parse_remainder(content_stream);

    case QPDFTokenizer::tt_bool:
        return with_description<QPDF_Bool>(tokenizer_.getValue() == "true");

    case QPDFTokenizer::tt_null:
        return {QPDFObject::create<QPDF_Null>()};

    case QPDFTokenizer::tt_integer:
        return with_description<QPDF_Integer>(QUtil::string_to_ll(tokenizer_.getValue().c_str()));

    case QPDFTokenizer::tt_real:
        return with_description<QPDF_Real>(tokenizer_.getValue());

    case QPDFTokenizer::tt_name:
        return with_description<QPDF_Name>(tokenizer_.getValue());

    case QPDFTokenizer::tt_word:
        {
            auto const& value = tokenizer_.getValue();
            if (content_stream) {
                return with_description<QPDF_Operator>(value);
            } else if (value == "endobj") {
                // We just saw endobj without having read anything. Nothing in the PDF spec appears
                // to allow empty objects, but they have been encountered in actual PDF files and
                // Adobe Reader appears to ignore them. Treat this as a null and do not move the
                // input source's offset.
                empty_ = true;
                input_.seek(input_.getLastOffset(), SEEK_SET);
                if (!content_stream) {
                    warn("empty object treated as null");
                }
                return {};
            } else {
                warn("unknown token while reading object; treating as string");
                return with_description<QPDF_String>(value);
            }
        }

    case QPDFTokenizer::tt_string:
        if (decrypter_) {
            std::string s{tokenizer_.getValue()};
            decrypter_->decryptString(s);
            return with_description<QPDF_String>(s);
        } else {
            return with_description<QPDF_String>(tokenizer_.getValue());
        }

    default:
        warn("treating unknown token type as null while reading object");
        return {};
    }
}

QPDFObjectHandle
QPDFParser::parse_remainder(bool content_stream)
{
    // This method must take care not to resolve any objects. Don't check the type of any object
    // without first ensuring that it is a direct object. Otherwise, doing so may have the side
    // effect of reading the object and changing the file pointer. If you do this, it will cause a
    // logic error to be thrown from QPDF::inParse().

    bad_count_ = 0;
    bool b_contents = false;

    while (true) {
        if (!tokenizer_.nextToken(input_, object_description_)) {
            warn(tokenizer_.getErrorMessage());
        }
        ++good_count_; // optimistically

        if (int_count_ != 0) {
            // Special handling of indirect references. Treat integer tokens as part of an indirect
            // reference until proven otherwise.
            if (tokenizer_.getType() == QPDFTokenizer::tt_integer) {
                if (++int_count_ > 2) {
                    // Process the oldest buffered integer.
                    add_int(int_count_);
                }
                last_offset_buffer_[int_count_ % 2] = input_.getLastOffset();
                int_buffer_[int_count_ % 2] = QUtil::string_to_ll(tokenizer_.getValue().c_str());
                continue;

            } else if (
                int_count_ >= 2 && tokenizer_.getType() == QPDFTokenizer::tt_word &&
                tokenizer_.getValue() == "R") {
                if (!context_) {
                    throw std::logic_error(
                        "QPDFParser::parse called without context on an object with indirect "
                        "references");
                }
                auto id = QIntC::to_int(int_buffer_[(int_count_ - 1) % 2]);
                auto gen = QIntC::to_int(int_buffer_[(int_count_) % 2]);
                if (!(id < 1 || gen < 0 || gen >= 65535)) {
                    add(ParseGuard::getObject(context_, id, gen, parse_pdf_));
                } else {
                    add_bad_null(
                        "treating bad indirect reference (" + std::to_string(id) + " " +
                        std::to_string(gen) + " R) as null");
                }
                int_count_ = 0;
                continue;

            } else if (int_count_ > 0) {
                // Process the buffered integers before processing the current token.
                if (int_count_ > 1) {
                    add_int(int_count_ - 1);
                }
                add_int(int_count_);
                int_count_ = 0;
            }
        }

        switch (tokenizer_.getType()) {
        case QPDFTokenizer::tt_eof:
            warn("parse error while reading object");
            if (content_stream) {
                // In content stream mode, leave object uninitialized to indicate EOF
                return {};
            }
            warn("unexpected EOF");
            return {};

        case QPDFTokenizer::tt_bad:
            check_too_many_bad_tokens();
            add_null();
            continue;

        case QPDFTokenizer::tt_brace_open:
        case QPDFTokenizer::tt_brace_close:
            add_bad_null("treating unexpected brace token as null");
            continue;

        case QPDFTokenizer::tt_array_close:
            if (frame_->state == st_array) {
                auto object = frame_->null_count > 100
                    ? QPDFObject::create<QPDF_Array>(std::move(frame_->olist), true)
                    : QPDFObject::create<QPDF_Array>(std::move(frame_->olist));
                set_description(object, frame_->offset - 1);
                // The `offset` points to the next of "[".  Set the rewind offset to point to the
                // beginning of "[". This has been explicitly tested with whitespace surrounding the
                // array start delimiter. getLastOffset points to the array end token and therefore
                // can't be used here.
                if (stack_.size() <= 1) {
                    return object;
                }
                stack_.pop_back();
                frame_ = &stack_.back();
                add(std::move(object));
            } else {
                if (sanity_checks_) {
                    // During sanity checks, assume nesting of containers is corrupt and object is
                    // unusable.
                    warn("unexpected array close token; giving up on reading object");
                    return {};
                }
                add_bad_null("treating unexpected array close token as null");
            }
            continue;

        case QPDFTokenizer::tt_dict_close:
            if (frame_->state <= st_dictionary_value) {
                // Attempt to recover more or less gracefully from invalid dictionaries.
                auto& dict = frame_->dict;

                if (frame_->state == st_dictionary_value) {
                    warn(
                        frame_->offset,
                        "dictionary ended prematurely; using null as value for last key");
                    dict[frame_->key] = QPDFObject::create<QPDF_Null>();
                }
                if (!frame_->olist.empty()) {
                    if (sanity_checks_) {
                        warn(
                            frame_->offset,
                            "expected dictionary keys but found non-name objects; ignoring");
                    } else {
                        fix_missing_keys();
                    }
                }

                if (!frame_->contents_string.empty() && dict.contains("/Type") &&
                    dict["/Type"].isNameAndEquals("/Sig") && dict.contains("/ByteRange") &&
                    dict.contains("/Contents") && dict["/Contents"].isString()) {
                    dict["/Contents"] = QPDFObjectHandle::newString(frame_->contents_string);
                    dict["/Contents"].setParsedOffset(frame_->contents_offset);
                }
                auto object = QPDFObject::create<QPDF_Dictionary>(std::move(dict));
                set_description(object, frame_->offset - 2);
                // The `offset` points to the next of "<<". Set the rewind offset to point to the
                // beginning of "<<". This has been explicitly tested with whitespace surrounding
                // the dictionary start delimiter. getLastOffset points to the dictionary end token
                // and therefore can't be used here.
                if (stack_.size() <= 1) {
                    return object;
                }
                stack_.pop_back();
                frame_ = &stack_.back();
                add(std::move(object));
            } else {
                if (sanity_checks_) {
                    // During sanity checks, assume nesting of containers is corrupt and object is
                    // unusable.
                    warn("unexpected dictionary close token; giving up on reading object");
                    return {};
                }
                add_bad_null("unexpected dictionary close token");
            }
            continue;

        case QPDFTokenizer::tt_array_open:
        case QPDFTokenizer::tt_dict_open:
            if (stack_.size() > max_nesting) {
                limits_error(
                    "parser-max-nesting", "ignoring excessively deeply nested data structure");
            }
            b_contents = false;
            stack_.emplace_back(
                input_,
                (tokenizer_.getType() == QPDFTokenizer::tt_array_open) ? st_array
                                                                       : st_dictionary_key);
            frame_ = &stack_.back();
            continue;

        case QPDFTokenizer::tt_bool:
            add_scalar<QPDF_Bool>(tokenizer_.getValue() == "true");
            continue;

        case QPDFTokenizer::tt_null:
            add_null();
            continue;

        case QPDFTokenizer::tt_integer:
            if (!content_stream) {
                // Buffer token in case it is part of an indirect reference.
                last_offset_buffer_[1] = input_.getLastOffset();
                int_buffer_[1] = QUtil::string_to_ll(tokenizer_.getValue().c_str());
                int_count_ = 1;
            } else {
                add_scalar<QPDF_Integer>(QUtil::string_to_ll(tokenizer_.getValue().c_str()));
            }
            continue;

        case QPDFTokenizer::tt_real:
            add_scalar<QPDF_Real>(tokenizer_.getValue());
            continue;

        case QPDFTokenizer::tt_name:
            if (frame_->state == st_dictionary_key) {
                frame_->key = tokenizer_.getValue();
                frame_->state = st_dictionary_value;
                b_contents = decrypter_ && frame_->key == "/Contents";
                continue;
            } else {
                add_scalar<QPDF_Name>(tokenizer_.getValue());
            }
            continue;

        case QPDFTokenizer::tt_word:
            if (content_stream) {
                add_scalar<QPDF_Operator>(tokenizer_.getValue());
                continue;
            }

            if (sanity_checks_) {
                if (tokenizer_.getValue() == "endobj" || tokenizer_.getValue() == "endstream") {
                    // During sanity checks, assume an unexpected endobj or endstream indicates that
                    // we are parsing past the end of the object.
                    warn(
                        "unexpected 'endobj' or 'endstream' while reading object; giving up on "
                        "reading object");
                    return {};
                }

                add_bad_null("unknown token while reading object; treating as null");
                continue;
            }

            warn("unknown token while reading object; treating as string");
            check_too_many_bad_tokens();
            add_scalar<QPDF_String>(tokenizer_.getValue());

            continue;

        case QPDFTokenizer::tt_string:
            {
                auto const& val = tokenizer_.getValue();
                if (decrypter_) {
                    if (b_contents) {
                        frame_->contents_string = val;
                        frame_->contents_offset = input_.getLastOffset();
                        b_contents = false;
                    }
                    std::string s{val};
                    decrypter_->decryptString(s);
                    add_scalar<QPDF_String>(s);
                } else {
                    add_scalar<QPDF_String>(val);
                }
            }
            continue;

        default:
            add_bad_null("treating unknown token type as null while reading object");
        }
    }
}

void
QPDFParser::add(std::shared_ptr<QPDFObject>&& obj)
{
    if (frame_->state != st_dictionary_value) {
        // If state is st_dictionary_key then there is a missing key. Push onto olist for
        // processing once the tt_dict_close token has been found.
        frame_->olist.emplace_back(std::move(obj));
    } else {
        if (auto res = frame_->dict.insert_or_assign(frame_->key, std::move(obj)); !res.second) {
            warn_duplicate_key();
        }
        frame_->state = st_dictionary_key;
    }
}

void
QPDFParser::add_null()
{
    const static ObjectPtr null_obj = QPDFObject::create<QPDF_Null>();

    if (frame_->state != st_dictionary_value) {
        // If state is st_dictionary_key then there is a missing key. Push onto olist for
        // processing once the tt_dict_close token has been found.
        frame_->olist.emplace_back(null_obj);
    } else {
        if (auto res = frame_->dict.insert_or_assign(frame_->key, null_obj); !res.second) {
            warn_duplicate_key();
        }
        frame_->state = st_dictionary_key;
    }
    ++frame_->null_count;
}

void
QPDFParser::add_bad_null(std::string const& msg)
{
    warn(msg);
    check_too_many_bad_tokens();
    add_null();
}

void
QPDFParser::add_int(int count)
{
    auto obj = QPDFObject::create<QPDF_Integer>(int_buffer_[count % 2]);
    obj->setDescription(context_, description_, last_offset_buffer_[count % 2]);
    add(std::move(obj));
}

template <typename T, typename... Args>
void
QPDFParser::add_scalar(Args&&... args)
{
    auto limit = Limits::parser_max_container_size(bad_count_ || sanity_checks_);
    if (frame_->olist.size() >= limit || frame_->dict.size() >= limit) {
        // Stop adding scalars. We are going to abort when the close token or a bad token is
        // encountered.
        max_bad_count_ = 1;
        check_too_many_bad_tokens(); // always throws Error()
    }
    auto obj = QPDFObject::create<T>(std::forward<Args>(args)...);
    obj->setDescription(context_, description_, input_.getLastOffset());
    add(std::move(obj));
}

template <typename T, typename... Args>
QPDFObjectHandle
QPDFParser::with_description(Args&&... args)
{
    auto obj = QPDFObject::create<T>(std::forward<Args>(args)...);
    obj->setDescription(context_, description_, start_);
    return {obj};
}

void
QPDFParser::set_description(ObjectPtr& obj, qpdf_offset_t parsed_offset)
{
    if (obj) {
        obj->setDescription(context_, description_, parsed_offset);
    }
}

void
QPDFParser::fix_missing_keys()
{
    std::set<std::string> names;
    for (auto& obj: frame_->olist) {
        if (obj.raw_type_code() == ::ot_name) {
            names.insert(obj.obj_sp()->getStringValue());
        }
    }
    int next_fake_key = 1;
    for (auto const& item: frame_->olist) {
        while (true) {
            const std::string key = "/QPDFFake" + std::to_string(next_fake_key++);
            const bool found_fake = !frame_->dict.contains(key) && !names.contains(key);
            QTC::TC("qpdf", "QPDFParser found fake", (found_fake ? 0 : 1));
            if (found_fake) {
                warn(
                    frame_->offset,
                    "expected dictionary key but found non-name object; inserting key " + key);
                frame_->dict[key] = item;
                break;
            }
        }
    }
}

void
QPDFParser::check_too_many_bad_tokens()
{
    auto limit = Limits::parser_max_container_size(bad_count_ || sanity_checks_);
    if (frame_->olist.size() >= limit || frame_->dict.size() >= limit) {
        if (bad_count_) {
            limits_error(
                "parser-max-container-size-damaged",
                "encountered errors while parsing an array or dictionary with more than " +
                    std::to_string(limit) + " elements; giving up on reading object");
        }
        limits_error(
            "parser-max-container-size",
            "encountered an array or dictionary with more than " + std::to_string(limit) +
                " elements during xref recovery; giving up on reading object");
    }
    if (max_bad_count_ && --max_bad_count_ == 0) {
        limits_error(
            "parser-max-errors", "too many errors during parsing; treating object as null");
    }
    if (good_count_ > 4) {
        good_count_ = 0;
        bad_count_ = 1;
        return;
    }
    if (++bad_count_ > 5 ||
        (frame_->state != st_array && std::cmp_less(max_bad_count_, frame_->olist.size()))) {
        // Give up after 5 errors in close proximity or if the number of missing dictionary keys
        // exceeds the remaining number of allowable total errors.
        warn("too many errors; giving up on reading object");
        throw Error();
    }
    good_count_ = 0;
}

void
QPDFParser::limits_error(std::string const& limit, std::string const& msg)
{
    Limits::error();
    warn("limits error("s + limit + "): " + msg);
    throw Error();
}

void
QPDFParser::warn(QPDFExc const& e) const
{
    // If parsing on behalf of a QPDF object and want to give a warning, we can warn through the
    // object. If parsing for some other reason, such as an explicit creation of an object from a
    // string, then just throw the exception.
    if (context_) {
        context_->warn(e);
    } else {
        throw e;
    }
}

void
QPDFParser::warn_duplicate_key()
{
    warn(
        frame_->offset,
        "dictionary has duplicated key " + frame_->key +
            "; last occurrence overrides earlier ones");
}

void
QPDFParser::warn(qpdf_offset_t offset, std::string const& msg) const
{
    if (stream_id_) {
        std::string descr = "object "s + std::to_string(obj_id_) + " 0";
        std::string name = context_->getFilename() + " object stream " + std::to_string(stream_id_);
        warn(QPDFExc(qpdf_e_damaged_pdf, name, descr, offset, msg));
    } else {
        warn(QPDFExc(qpdf_e_damaged_pdf, input_.getName(), object_description_, offset, msg));
    }
}

void
QPDFParser::warn(std::string const& msg) const
{
    warn(input_.getLastOffset(), msg);
}
