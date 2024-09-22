#ifndef OBJECTHANDLE_PRIVATE_HH
#define OBJECTHANDLE_PRIVATE_HH

#include <qpdf/QPDFObjectHandle.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>

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

    inline qpdf_object_type_e
    BaseHandle::type_code() const
    {
        return obj ? obj->getResolvedTypeCode() : ::ot_uninitialized;
    }

} // namespace qpdf

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
    return qpdf::Array({});
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

#endif // OBJECTHANDLE_PRIVATE_HH
