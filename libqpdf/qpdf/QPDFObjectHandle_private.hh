#ifndef OBJECTHANDLE_PRIVATE_HH
#define OBJECTHANDLE_PRIVATE_HH

#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QUtil.hh>

#include <concepts>
#include <utility>

using namespace std::literals;

namespace qpdf
{
    class Array final: public BaseHandle
    {
      public:
        Array() = default;

        explicit Array(std::vector<QPDFObjectHandle> const& items);

        explicit Array(std::vector<QPDFObjectHandle>&& items);

        Array(Array const& other) :
            BaseHandle(other.obj)
        {
        }

        static Array empty();

        Array&
        operator=(Array const& other)
        {
            if (obj != other.obj) {
                obj = other.obj;
            }
            return *this;
        }

        Array(Array&&) = default;
        Array& operator=(Array&&) = default;

        explicit Array(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }

        explicit Array(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }

        Array(QPDFObjectHandle const& oh) :
            BaseHandle(oh.resolved_type_code() == ::ot_array ? oh : QPDFObjectHandle())
        {
        }

        Array&
        operator=(QPDFObjectHandle const& oh)
        {
            assign(::ot_array, oh);
            return *this;
        }

        Array(QPDFObjectHandle&& oh) :
            BaseHandle(oh.resolved_type_code() == ::ot_array ? std::move(oh) : QPDFObjectHandle())
        {
        }

        Array&
        operator=(QPDFObjectHandle&& oh)
        {
            assign(::ot_array, std::move(oh));
            return *this;
        }

        QPDFObjectHandle const& operator[](size_t n) const;

        QPDFObjectHandle const& operator[](int n) const;

        using iterator = std::vector<QPDFObjectHandle>::iterator;
        using const_iterator = std::vector<QPDFObjectHandle>::const_iterator;
        using const_reverse_iterator = std::vector<QPDFObjectHandle>::const_reverse_iterator;

        iterator begin();

        iterator end();

        const_iterator cbegin();

        const_iterator cend();

        const_reverse_iterator crbegin();

        const_reverse_iterator crend();

        // Return the number of elements in the array. Return 0 if the object is not an array.
        size_t size() const;
        QPDFObjectHandle get(size_t n) const;
        QPDFObjectHandle get(int n) const;
        bool set(size_t at, QPDFObjectHandle const& oh);
        bool set(int at, QPDFObjectHandle const& oh);
        bool insert(size_t at, QPDFObjectHandle const& item);
        bool insert(int at, QPDFObjectHandle const& item);
        void push_back(QPDFObjectHandle const& item);
        bool erase(size_t at);
        bool erase(int at);

        std::vector<QPDFObjectHandle> getAsVector() const;
        void setFromVector(std::vector<QPDFObjectHandle> const& items);

      private:
        QPDF_Array* array() const;
        void checkOwnership(QPDFObjectHandle const& item) const;
        QPDFObjectHandle null() const;

        std::unique_ptr<std::vector<QPDFObjectHandle>> sp_elements{};
    };

    // BaseDictionary is only used as a base class. It does not contain any methods exposed in the
    // public API.
    class BaseDictionary: public BaseHandle
    {
      public:
        // The following methods are not part of the public API.
        std::set<std::string> getKeys();
        std::map<std::string, QPDFObjectHandle> const& getAsMap() const;
        void replaceKey(std::string const& key, QPDFObjectHandle value);

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

      protected:
        BaseDictionary() = default;

        explicit BaseDictionary(std::map<std::string, QPDFObjectHandle> const& dict) :
            BaseHandle(QPDFObject::create<QPDF_Dictionary>(dict))
        {
        }

        explicit BaseDictionary(std::map<std::string, QPDFObjectHandle>&& dict) :
            BaseHandle(QPDFObject::create<QPDF_Dictionary>(std::move(dict)))
        {
        }

        explicit BaseDictionary(std::shared_ptr<QPDFObject> const& obj) :
            BaseHandle(obj)
        {
        }
        explicit BaseDictionary(std::shared_ptr<QPDFObject>&& obj) :
            BaseHandle(std::move(obj))
        {
        }
        BaseDictionary(BaseDictionary const&) = default;
        BaseDictionary& operator=(BaseDictionary const&) = default;
        BaseDictionary(BaseDictionary&&) = default;
        BaseDictionary& operator=(BaseDictionary&&) = default;

        explicit BaseDictionary(QPDFObjectHandle const& oh) :
            BaseHandle(oh.resolved_type_code() == ::ot_dictionary ? oh : QPDFObjectHandle())
        {
        }

        explicit BaseDictionary(QPDFObjectHandle&& oh) :
            BaseHandle(
                oh.resolved_type_code() == ::ot_dictionary ? std::move(oh) : QPDFObjectHandle())
        {
        }
        ~BaseDictionary() = default;

        QPDF_Dictionary* dict() const;
    };

    // Dictionary only defines con/destructors. All other methods are inherited from BaseDictionary.
    class Dictionary final: public BaseDictionary
    {
      public:
        Dictionary() = default;
        explicit Dictionary(std::map<std::string, QPDFObjectHandle>&& dict);
        explicit Dictionary(std::shared_ptr<QPDFObject> const& obj);

        static Dictionary empty();

        Dictionary(Dictionary const&) = default;
        Dictionary& operator=(Dictionary const&) = default;
        Dictionary(Dictionary&&) = default;
        Dictionary& operator=(Dictionary&&) = default;

        Dictionary(QPDFObjectHandle const& oh) :
            BaseDictionary(oh)
        {
        }

        Dictionary&
        operator=(QPDFObjectHandle const& oh)
        {
            assign(::ot_dictionary, oh);
            return *this;
        }

        Dictionary(QPDFObjectHandle&& oh) :
            BaseDictionary(std::move(oh))
        {
        }

        Dictionary&
        operator=(QPDFObjectHandle&& oh)
        {
            assign(::ot_dictionary, std::move(oh));
            return *this;
        }

        ~Dictionary() = default;
    };

    class Integer final: public BaseHandle
    {
      public:
        Integer() = default;
        Integer(Integer const&) = default;
        Integer(Integer&&) = default;
        Integer& operator=(Integer const&) = default;
        Integer& operator=(Integer&&) = default;
        ~Integer() = default;

        explicit Integer(long long value);

        explicit Integer(std::integral auto value) :
            Integer(static_cast<long long>(value))
        {
            if constexpr (
                std::numeric_limits<decltype(value)>::max() >
                std::numeric_limits<long long>::max()) {
                if (value > std::numeric_limits<long long>::max()) {
                    throw std::overflow_error("overflow constructing Integer");
                }
            }
        }

        Integer(QPDFObjectHandle const& oh) :
            BaseHandle(oh.type_code() == ::ot_integer ? oh : QPDFObjectHandle())
        {
        }

        Integer(QPDFObjectHandle&& oh) :
            BaseHandle(oh.type_code() == ::ot_integer ? std::move(oh) : QPDFObjectHandle())
        {
        }

        // Return the integer value. If the object is not a valid Integer, throw a
        // std::invalid_argument exception. If the object is out of range for the target type,
        // throw a std::overflow_error or std::underflow_error exception.
        template <std::integral T>
        operator T() const
        {
            auto v = value();

            if (std::cmp_greater(v, std::numeric_limits<T>::max())) {
                throw std::overflow_error("Integer conversion overflow");
            }
            if (std::cmp_less(v, std::numeric_limits<T>::min())) {
                throw std::underflow_error("Integer conversion underflow");
            }
            return static_cast<T>(v);
        }

        // Return the integer value. If the object is not a valid integer, throw a
        // std::invalid_argument exception.
        int64_t value() const;

        // Return true if object value is equal to the 'rhs' value. Return false if the object is
        // not a valid Integer.
        friend bool
        operator==(Integer const& lhs, std::integral auto rhs)
        {
            return lhs && std::cmp_equal(lhs.value(), rhs);
        }

        // Compare the object value to the 'rhs' value. Throw a std::invalid_argument exception if
        // the object is not a valid Integer.
        friend std::strong_ordering
        operator<=>(Integer const& lhs, std::integral auto rhs)
        {
            if (!lhs) {
                throw lhs.invalid_error("Integer");
            }
            if (std::cmp_less(lhs.value(), rhs)) {
                return std::strong_ordering::less;
            }
            return std::cmp_greater(lhs.value(), rhs) ? std::strong_ordering::greater
                                                      : std::strong_ordering::equal;
        }
    };

    bool
    operator==(std::integral auto lhs, Integer const& rhs)
    {
        return rhs == lhs;
    }

    std::strong_ordering
    operator<=>(std::integral auto lhs, Integer const& rhs)
    {
        return rhs <=> lhs;
    }

    class Name final: public BaseHandle
    {
      public:
        // Put # into strings with characters unsuitable for name token
        static std::string normalize(std::string const& name);

        // Check whether name is valid utf-8 and whether it contains characters that require
        // escaping. Return {false, false} if the name is not valid utf-8, otherwise return {true,
        // true} if no characters require or {true, false} if escaping is required.
        static std::pair<bool, bool> analyzeJSONEncoding(std::string const& name);

        Name() = default;
        Name(Name const&) = default;
        Name(Name&&) = default;
        Name& operator=(Name const&) = default;
        Name& operator=(Name&&) = default;
        ~Name() = default;

        explicit Name(std::string const&);
        explicit Name(std::string&&);

        Name(QPDFObjectHandle const& oh) :
            BaseHandle(oh.type_code() == ::ot_name ? oh : QPDFObjectHandle())
        {
        }

        Name(QPDFObjectHandle&& oh) :
            BaseHandle(oh.type_code() == ::ot_name ? std::move(oh) : QPDFObjectHandle())
        {
        }

        // Return the name value. If the object is not a valid Name, throw a
        // std::invalid_argument exception.
        operator std::string() const&
        {
            return value();
        }

        // Return the integer value. If the object is not a valid integer, throw a
        // std::invalid_argument exception.
        std::string const& value() const;

        // Return true if object value is equal to the 'rhs' value. Return false if the object is
        // not a valid Name.
        friend bool
        operator==(Name const& lhs, std::string_view rhs)
        {
            return lhs && lhs.value() == rhs;
        }
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

        Stream() = default;
        Stream(Stream const&) = default;
        Stream(Stream&&) = default;
        Stream& operator=(Stream const&) = default;
        Stream& operator=(Stream&&) = default;
        ~Stream() = default;

        Stream(QPDFObjectHandle const& oh) :
            BaseHandle(oh.type_code() == ::ot_stream ? oh : QPDFObjectHandle())
        {
        }

        Stream(QPDFObjectHandle&& oh) :
            BaseHandle(oh.type_code() == ::ot_stream ? std::move(oh) : QPDFObjectHandle())
        {
        }

        Stream(
            QPDF& qpdf,
            QPDFObjGen og,
            QPDFObjectHandle stream_dict,
            qpdf_offset_t offset,
            size_t length);

        Stream copy();

        void copy_data_to(Stream& target);

        Dictionary
        getDict() const
        {
            return {stream()->stream_dict};
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
        std::string getStreamData(qpdf_stream_decode_level_e level);
        std::string getRawStreamData();
        void replaceStreamData(
            std::string&& data,
            QPDFObjectHandle const& filter,
            QPDFObjectHandle const& decode_parms);
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
        bool isRootMetadata() const;

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
            qpdf_stream_decode_level_e decode_level,
            std::vector<std::shared_ptr<QPDFStreamFilter>>& filters);
        void replaceFilterData(
            QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length);

        void warn(std::string const& message);

        static std::map<std::string, std::string> filter_abbreviations;
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
            return BaseHandle(QPDF::Doc::Resolver::resolved(obj->qpdf, obj->og)).as<T>();
        }
        if (std::holds_alternative<QPDF_Reference>(obj->value)) {
            // see comment in QPDF_Reference.
            return BaseHandle(std::get<QPDF_Reference>(obj->value).obj).as<T>();
        }
        return nullptr;
    }

    inline BaseHandle::BaseHandle(QPDFObjectHandle const& oh) :
        obj(oh.obj)
    {
    }

    inline BaseHandle::BaseHandle(QPDFObjectHandle&& oh) :
        obj(std::move(oh.obj))
    {
    }

    inline void
    BaseHandle::assign(qpdf_object_type_e required, BaseHandle const& other)
    {
        if (obj != other.obj) {
            obj = other.resolved_type_code() == required ? other.obj : nullptr;
        }
    }

    inline void
    BaseHandle::assign(qpdf_object_type_e required, BaseHandle&& other)
    {
        if (obj != other.obj) {
            obj = other.resolved_type_code() == required ? std::move(other.obj) : nullptr;
        }
    }

    inline QPDFObjGen
    BaseHandle::id_gen() const
    {
        return obj ? obj->og : QPDFObjGen();
    }

    inline bool
    BaseHandle::indirect() const
    {
        return obj ? obj->og.isIndirect() : false;
    }

    inline void
    BaseHandle::no_ci_stop_if(bool condition, std::string const& message) const
    {
        if (condition) {
            if (qpdf()) {
                throw QPDFExc(qpdf_e_damaged_pdf, "", description(), 0, message);
            }
            throw std::runtime_error(message);
        }
    }

    inline void
    BaseHandle::no_ci_stop_damaged_if(bool condition, std::string const& message) const
    {
        if (condition) {
            throw std::runtime_error(message);
        }
    }

    inline void
    BaseHandle::no_ci_warn_if(bool condition, std::string const& warning) const
    {
        if (condition) {
            warn(warning);
        }
    }

    inline bool
    BaseHandle::null() const
    {
        return !obj || type_code() == ::ot_null;
    }

    inline qpdf_offset_t
    BaseHandle::offset() const
    {
        return obj ? obj->parsed_offset : -1;
    }

    inline QPDF*
    BaseHandle::qpdf() const
    {
        return obj ? obj->qpdf : nullptr;
    }

    inline qpdf_object_type_e
    BaseHandle::raw_type_code() const
    {
        return obj ? static_cast<qpdf_object_type_e>(obj->value.index()) : ::ot_uninitialized;
    }

    inline qpdf_object_type_e
    BaseHandle::resolved_type_code() const
    {
        if (!obj) {
            return ::ot_uninitialized;
        }
        if (raw_type_code() == ::ot_unresolved) {
            return QPDF::Doc::Resolver::resolved(obj->qpdf, obj->og)->getTypeCode();
        }
        return raw_type_code();
    }

    inline qpdf_object_type_e
    BaseHandle::type_code() const
    {
        if (!obj) {
            return ::ot_uninitialized;
        }
        if (raw_type_code() == ::ot_unresolved) {
            return QPDF::Doc::Resolver::resolved(obj->qpdf, obj->og)->getTypeCode();
        }
        if (raw_type_code() == ::ot_reference) {
            return std::get<QPDF_Reference>(obj->value).obj->getTypeCode();
        }
        return raw_type_code();
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

inline qpdf_object_type_e
QPDFObject::getResolvedTypeCode() const
{
    if (getTypeCode() == ::ot_unresolved) {
        return QPDF::Doc::Resolver::resolved(qpdf, og)->getTypeCode();
    }
    if (getTypeCode() == ::ot_reference) {
        return std::get<QPDF_Reference>(value).obj->getTypeCode();
    }
    return getTypeCode();
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
