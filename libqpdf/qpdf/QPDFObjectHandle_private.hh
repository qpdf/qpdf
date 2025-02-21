#ifndef OBJECTHANDLE_PRIVATE_HH
#define OBJECTHANDLE_PRIVATE_HH

#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QUtil.hh>

namespace qpdf
{
    class Array final: public BaseHandle
    {
      public:
        explicit Array(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }

        explicit Array(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }

        int size() const;
        std::pair<bool, QPDFObjectHandle> at(int n) const;
        bool setAt(int at, QPDFObjectHandle const& oh);
        bool insert(int at, QPDFObjectHandle const& item);
        void push_back(QPDFObjectHandle const& item);
        bool erase(int at);

        std::vector<QPDFObjectHandle> getAsVector() const;
        void setFromVector(std::vector<QPDFObjectHandle> const& items);

      private:
        QPDF_Array* array() const;
        void checkOwnership(QPDFObjectHandle const& item) const;
        QPDFObjectHandle null() const;
    };

    // BaseDictionary is only used as a base class. It does not contain any methods exposed in the
    // public API.
    class BaseDictionary: public BaseHandle
    {
      public:
        using iterator = std::map<std::string, QPDFObjectHandle>::iterator;
        using const_iterator = std::map<std::string, QPDFObjectHandle>::const_iterator;
        using reverse_iterator = std::map<std::string, QPDFObjectHandle>::reverse_iterator;
        using const_reverse_iterator =
            std::map<std::string, QPDFObjectHandle>::const_reverse_iterator;

        iterator
        begin()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.begin();
            }
            return {};
        }

        iterator
        end()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.end();
            }
            return {};
        }

        const_iterator
        cbegin()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.cbegin();
            }
            return {};
        }

        const_iterator
        cend()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.cend();
            }
            return {};
        }

        reverse_iterator
        rbegin()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.rbegin();
            }
            return {};
        }

        reverse_iterator
        rend()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.rend();
            }
            return {};
        }

        const_reverse_iterator
        crbegin()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.crbegin();
            }
            return {};
        }

        const_reverse_iterator
        crend()
        {
            if (auto d = as<QPDF_Dictionary>()) {
                return d->items.crend();
            }
            return {};
        }

        // The following methods are not part of the public API.
        bool hasKey(std::string const& key) const;
        QPDFObjectHandle getKey(std::string const& key) const;
        std::set<std::string> getKeys();
        std::map<std::string, QPDFObjectHandle> const& getAsMap() const;
        void removeKey(std::string const& key);
        void replaceKey(std::string const& key, QPDFObjectHandle value);

      protected:
        BaseDictionary() = default;
        BaseDictionary(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj) {};
        BaseDictionary(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj)) {};
        BaseDictionary(BaseDictionary const&) = default;
        BaseDictionary& operator=(BaseDictionary const&) = default;
        BaseDictionary(BaseDictionary&&) = default;
        BaseDictionary& operator=(BaseDictionary&&) = default;
        ~BaseDictionary() = default;

        QPDF_Dictionary* dict() const;
    };

    class Dictionary final: public BaseDictionary
    {
      public:
        explicit Dictionary(std::shared_ptr<QPDFObject> const& obj) :
            BaseDictionary(obj)
        {
        }

        explicit Dictionary(std::shared_ptr<QPDFObject>&& obj) :
            BaseDictionary(std::move(obj))
        {
        }
    };

    class Name final: public BaseHandle
    {
      public:
        // Put # into strings with characters unsuitable for name token
        static std::string normalize(std::string const& name);

        // Check whether name is valid utf-8 and whether it contains characters that require
        // escaping. Return {false, false} if the name is not valid utf-8, otherwise return {true,
        // true} if no characters require or {true, false} if escaping is required.
        static std::pair<bool, bool> analyzeJSONEncoding(std::string const& name);
    };

    class Stream final: public BaseHandle
    {
      public:
        explicit Stream(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }

        explicit Stream(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }

        Stream(
            QPDF& qpdf,
            QPDFObjGen og,
            QPDFObjectHandle stream_dict,
            qpdf_offset_t offset,
            size_t length);

        QPDFObjectHandle
        getDict() const
        {
            return stream()->stream_dict;
        }
        bool
        isDataModified() const
        {
            return !stream()->token_filters.empty();
        }
        void
        setFilterOnWrite(bool val)
        {
            stream()->filter_on_write = val;
        }
        bool
        getFilterOnWrite() const
        {
            return stream()->filter_on_write;
        }

        // Methods to help QPDF copy foreign streams
        size_t
        getLength() const
        {
            return stream()->length;
        }
        std::shared_ptr<Buffer>
        getStreamDataBuffer() const
        {
            return stream()->stream_data;
        }
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider>
        getStreamDataProvider() const
        {
            return stream()->stream_provider;
        }

        // See comments in QPDFObjectHandle.hh for these methods.
        bool pipeStreamData(
            Pipeline* p,
            bool* tried_filtering,
            int encode_flags,
            qpdf_stream_decode_level_e decode_level,
            bool suppress_warnings,
            bool will_retry);
        std::shared_ptr<Buffer> getStreamData(qpdf_stream_decode_level_e level);
        std::shared_ptr<Buffer> getRawStreamData();
        void replaceStreamData(
            std::shared_ptr<Buffer> data,
            QPDFObjectHandle const& filter,
            QPDFObjectHandle const& decode_parms);
        void replaceStreamData(
            std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
            QPDFObjectHandle const& filter,
            QPDFObjectHandle const& decode_parms);
        void
        addTokenFilter(std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter)
        {
            stream()->token_filters.emplace_back(token_filter);
        }
        JSON getStreamJSON(
            int json_version,
            qpdf_json_stream_data_e json_data,
            qpdf_stream_decode_level_e decode_level,
            Pipeline* p,
            std::string const& data_filename);
        qpdf_stream_decode_level_e writeStreamJSON(
            int json_version,
            JSON::Writer& jw,
            qpdf_json_stream_data_e json_data,
            qpdf_stream_decode_level_e decode_level,
            Pipeline* p,
            std::string const& data_filename,
            bool no_data_key = false);
        void
        replaceDict(QPDFObjectHandle const& new_dict)
        {
            auto s = stream();
            s->stream_dict = new_dict;
            setDictDescription();
        }

        void setDictDescription();

        static void registerStreamFilter(
            std::string const& filter_name,
            std::function<std::shared_ptr<QPDFStreamFilter>()> factory);

      private:
        QPDF_Stream::Members*
        stream() const
        {
            if (auto s = as<QPDF_Stream>()) {
                if (auto ptr = s->m.get()) {
                    return ptr;
                }
                throw std::logic_error("QPDF_Stream: unexpected nullptr");
            }
            throw std::runtime_error("operation for stream attempted on non-stream object");
            return nullptr; // unreachable
        }
        bool filterable(
            std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
            bool& specialized_compression,
            bool& lossy_compression);
        void replaceFilterData(
            QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length);

        void warn(std::string const& message);

        static std::map<std::string, std::string> filter_abbreviations;
        static std::map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>>
            filter_factories;
    };

    template <typename T>
    T*
    BaseHandle::as() const
    {
        if (!obj) {
            return nullptr;
        }
        if (std::holds_alternative<T>(obj->value)) {
            return &std::get<T>(obj->value);
        }
        if (std::holds_alternative<QPDF_Unresolved>(obj->value)) {
            return BaseHandle(QPDF::Resolver::resolved(obj->qpdf, obj->og)).as<T>();
        }
        if (std::holds_alternative<QPDF_Reference>(obj->value)) {
            // see comment in QPDF_Reference.
            return BaseHandle(std::get<QPDF_Reference>(obj->value).obj).as<T>();
        }
        return nullptr;
    }

    inline qpdf_object_type_e
    BaseHandle::type_code() const
    {
        return obj ? obj->getResolvedTypeCode() : ::ot_uninitialized;
    }

} // namespace qpdf

inline QPDF_Dictionary::QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items) :
    items(std::move(items))
{
}

inline std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFObject> parent, std::string_view const& static_descr, std::string var_descr)
{
    auto n = QPDFObject::create<QPDF_Null>();
    n->setChildDescription(parent->getQPDF(), parent, static_descr, var_descr);
    return n;
}

inline QPDF_Real::QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes) :
    val(QUtil::double_to_string(value, decimal_places, trim_trailing_zeroes))
{
}

template <typename T, typename... Args>
inline std::shared_ptr<QPDFObject>
QPDFObject::create(Args&&... args)
{
    return std::make_shared<QPDFObject>(std::forward<T>(T(std::forward<Args>(args)...)));
}

inline qpdf::Array
QPDFObjectHandle::as_array(qpdf::typed options) const
{
    if (options & qpdf::error) {
        assertType("array", false);
    }
    if (options & qpdf::any_flag || type_code() == ::ot_array ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Array(obj);
    }
    return qpdf::Array(std::shared_ptr<QPDFObject>());
}

inline qpdf::Dictionary
QPDFObjectHandle::as_dictionary(qpdf::typed options) const
{
    if (options & qpdf::any_flag || type_code() == ::ot_dictionary ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Dictionary(obj);
    }
    if (options & qpdf::error) {
        assertType("dictionary", false);
    }
    return qpdf::Dictionary(std::shared_ptr<QPDFObject>());
}

inline qpdf::Stream
QPDFObjectHandle::as_stream(qpdf::typed options) const
{
    if (options & qpdf::any_flag || type_code() == ::ot_stream ||
        (options & qpdf::optional && type_code() == ::ot_null)) {
        return qpdf::Stream(obj);
    }
    if (options & qpdf::error) {
        assertType("stream", false);
    }
    return qpdf::Stream(std::shared_ptr<QPDFObject>());
}

#endif // OBJECTHANDLE_PRIVATE_HH
