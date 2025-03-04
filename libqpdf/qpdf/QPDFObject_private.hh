#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

// NOTE: This file is called QPDFObject_private.hh instead of QPDFObject.hh because of
// include/qpdf/QPDFObject.hh. See comments there for an explanation.

#include <qpdf/Constants.h>
#include <qpdf/JSON.hh>
#include <qpdf/JSON_writer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/Types.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class QPDFObject;
class QPDFObjectHandle;

namespace qpdf
{
    class Array;
    class BaseDictionary;
    class Dictionary;
    class Stream;
} // namespace qpdf

class QPDF_Array final
{
  private:
    struct Sparse
    {
        int size{0};
        std::map<int, QPDFObjectHandle> elements;
    };

  public:
    QPDF_Array() = default;
    QPDF_Array(QPDF_Array const& other) :
        sp(other.sp ? std::make_unique<Sparse>(*other.sp) : nullptr)
    {
    }

    QPDF_Array(QPDF_Array&&) = default;
    QPDF_Array& operator=(QPDF_Array&&) = default;

  private:
    friend class QPDFObject;
    friend class qpdf::BaseHandle;
    friend class qpdf::Array;

    QPDF_Array(std::vector<QPDFObjectHandle> const& items) :
        elements(items)
    {
    }
    QPDF_Array(std::vector<QPDFObjectHandle>&& items, bool sparse);

    QPDF_Array(std::vector<QPDFObjectHandle>&& items) :
        elements(std::move(items))
    {
    }

    int
    size() const
    {
        return sp ? sp->size : int(elements.size());
    }

    std::unique_ptr<Sparse> sp;
    std::vector<QPDFObjectHandle> elements;
};

class QPDF_Bool final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;
    friend class QPDFObjectHandle;

    explicit QPDF_Bool(bool val) :
        val(val)
    {
    }
    bool val;
};

class QPDF_Destroyed final
{
};

class QPDF_Dictionary final
{
    friend class QPDFObject;
    friend class qpdf::BaseDictionary;
    friend class qpdf::BaseHandle;

    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items) :
        items(items)
    {
    }
    inline QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items);

    std::map<std::string, QPDFObjectHandle> items;
};

class QPDF_InlineImage final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

    explicit QPDF_InlineImage(std::string val) :
        val(std::move(val))
    {
    }
    std::string val;
};

class QPDF_Integer final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;
    friend class QPDFObjectHandle;

    QPDF_Integer(long long val) :
        val(val)
    {
    }
    long long val;
};

class QPDF_Name final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

    explicit QPDF_Name(std::string name) :
        name(std::move(name))
    {
    }
    std::string name;
};

class QPDF_Null final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

  public:
    static inline std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr);
};

class QPDF_Operator final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

    QPDF_Operator(std::string val) :
        val(std::move(val))
    {
    }

    std::string val;
};

class QPDF_Real final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

    QPDF_Real(std::string val) :
        val(std::move(val))
    {
    }
    inline QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes);
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

class QPDF_Reference
{
    // This is a minimal implementation to support QPDF::replaceObject. Once we support parsing of
    // objects that are an indirect reference we will need to support multiple levels of
    // indirection, including the possibility of circular references.
    friend class QPDFObject;
    friend class qpdf::BaseHandle;

    QPDF_Reference(std::shared_ptr<QPDFObject> obj) :
        obj(std::move(obj))
    {
    }

    std::shared_ptr<QPDFObject> obj;
};

class QPDF_Reserved final
{
};

class QPDF_Stream final
{
    class Members
    {
        friend class QPDF_Stream;
        friend class QPDFObject;
        friend class qpdf::Stream;
        friend class qpdf::BaseHandle;

      public:
        Members(QPDFObjectHandle stream_dict, size_t length) :
            stream_dict(std::move(stream_dict)),
            length(length)
        {
        }

      private:
        bool filter_on_write{true};
        QPDFObjectHandle stream_dict;
        size_t length{0};
        std::shared_ptr<Buffer> stream_data;
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider;
        std::vector<std::shared_ptr<QPDFObjectHandle::TokenFilter>> token_filters;
    };

    friend class QPDFObject;
    friend class qpdf::BaseHandle;
    friend class qpdf::Stream;

    QPDF_Stream(QPDFObjectHandle stream_dict, size_t length) :
        m(std::make_unique<Members>(stream_dict, length))
    {
        if (!stream_dict.isDictionary()) {
            throw std::logic_error(
                "stream object instantiated with non-dictionary object for dictionary");
        }
    }

    std::unique_ptr<Members> m;
};

// QPDF_Strings may included embedded null characters.
class QPDF_String final
{
    friend class QPDFObject;
    friend class qpdf::BaseHandle;
    friend class QPDFWriter;

  public:
    static std::shared_ptr<QPDFObject> create_utf16(std::string const& utf8_val);
    std::string unparse(bool force_binary = false);
    void writeJSON(int json_version, JSON::Writer& p);
    std::string getUTF8Val() const;

  private:
    QPDF_String(std::string val) :
        val(std::move(val))
    {
    }
    bool useHexString() const;
    std::string val;
};

class QPDF_Unresolved final
{
};

class QPDFObject
{
  public:
    template <typename T>
    QPDFObject(T&& value) :
        value(std::forward<T>(value))
    {
    }

    template <typename T>
    QPDFObject(QPDF* qpdf, QPDFObjGen og, T&& value) :
        value(std::forward<T>(value)),
        qpdf(qpdf),
        og(og)
    {
    }

    template <typename T, typename... Args>
    inline static std::shared_ptr<QPDFObject> create(Args&&... args);

    template <typename T, typename... Args>
    inline static std::shared_ptr<QPDFObject>
    create(QPDF* qpdf, QPDFObjGen og, Args&&... args)
    {
        return std::make_shared<QPDFObject>(
            qpdf, og, std::forward<T>(T(std::forward<Args>(args)...)));
    }

    void disconnect();
    std::string getStringValue() const;

    // Return a unique type code for the resolved object
    qpdf_object_type_e
    getResolvedTypeCode() const
    {
        if (getTypeCode() == ::ot_unresolved) {
            return QPDF::Resolver::resolved(qpdf, og)->getTypeCode();
        }
        if (getTypeCode() == ::ot_reference) {
            return std::get<QPDF_Reference>(value).obj->getResolvedTypeCode();
        }
        return getTypeCode();
    }
    // Return a unique type code for the object
    qpdf_object_type_e
    getTypeCode() const
    {
        return static_cast<qpdf_object_type_e>(value.index());
    }
    void
    assign_null()
    {
        value = QPDF_Null();
        qpdf = nullptr;
        og = QPDFObjGen();
        object_description = nullptr;
        parsed_offset = -1;
    }
    void
    move_to(std::shared_ptr<QPDFObject>& o, bool destroy)
    {
        o->value = std::move(value);
        o->qpdf = qpdf;
        o->og = og;
        o->object_description = object_description;
        o->parsed_offset = parsed_offset;
        if (!destroy) {
            value = QPDF_Reference(o);
        }
    }
    void
    swapWith(std::shared_ptr<QPDFObject> o)
    {
        std::swap(value, o->value);
        std::swap(qpdf, o->qpdf);
        std::swap(object_description, o->object_description);
        std::swap(parsed_offset, o->parsed_offset);
    }

    void
    setObjGen(QPDF* a_qpdf, QPDFObjGen a_og)
    {
        qpdf = a_qpdf;
        og = a_og;
    }
    // Mark an object as destroyed. Used by QPDF's destructor for its indirect objects.
    void
    destroy()
    {
        value = QPDF_Destroyed();
    }

    bool
    isUnresolved() const
    {
        return getTypeCode() == ::ot_unresolved;
    }
    const QPDFObject*
    resolved_object() const
    {
        return isUnresolved() ? QPDF::Resolver::resolved(qpdf, og).get() : this;
    }

    struct JSON_Descr
    {
        JSON_Descr(std::shared_ptr<std::string> input, std::string const& object) :
            input(input),
            object(object)
        {
        }

        std::shared_ptr<std::string> input;
        std::string object;
    };

    struct ChildDescr
    {
        ChildDescr(
            std::shared_ptr<QPDFObject> parent,
            std::string_view const& static_descr,
            std::string var_descr) :
            parent(parent),
            static_descr(static_descr),
            var_descr(var_descr)
        {
        }

        std::weak_ptr<QPDFObject> parent;
        std::string_view const& static_descr;
        std::string var_descr;
    };

    using Description = std::variant<std::string, JSON_Descr, ChildDescr>;

    void
    setDescription(
        QPDF* qpdf_p, std::shared_ptr<Description>& description, qpdf_offset_t offset = -1)
    {
        qpdf = qpdf_p;
        object_description = description;
        setParsedOffset(offset);
    }
    void
    setDefaultDescription(QPDF* a_qpdf, QPDFObjGen const& a_og)
    {
        qpdf = a_qpdf;
        og = a_og;
    }
    void
    setChildDescription(
        QPDF* a_qpdf,
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        object_description =
            std::make_shared<Description>(ChildDescr(parent, static_descr, var_descr));
        qpdf = a_qpdf;
    }
    std::string getDescription();
    bool
    hasDescription()
    {
        return object_description || og.isIndirect();
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        if (parsed_offset < 0) {
            parsed_offset = offset;
        }
    }
    bool
    getDescription(QPDF*& a_qpdf, std::string& description)
    {
        a_qpdf = qpdf;
        description = getDescription();
        return qpdf != nullptr;
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return parsed_offset;
    }
    QPDF*
    getQPDF()
    {
        return qpdf;
    }
    QPDFObjGen
    getObjGen()
    {
        return og;
    }

  private:
    friend class QPDF_Stream;
    friend class qpdf::BaseHandle;

    typedef std::variant<
        std::monostate,
        QPDF_Reserved,
        QPDF_Null,
        QPDF_Bool,
        QPDF_Integer,
        QPDF_Real,
        QPDF_String,
        QPDF_Name,
        QPDF_Array,
        QPDF_Dictionary,
        QPDF_Stream,
        QPDF_Operator,
        QPDF_InlineImage,
        QPDF_Unresolved,
        QPDF_Destroyed,
        QPDF_Reference>
        Value;
    Value value;

    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;

    std::shared_ptr<Description> object_description;

    QPDF* qpdf{nullptr};
    QPDFObjGen og{};
    qpdf_offset_t parsed_offset{-1};
};

#endif // QPDFOBJECT_HH
