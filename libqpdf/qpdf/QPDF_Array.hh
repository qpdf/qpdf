#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

#include <qpdf/QPDFObject.hh>

#include <qpdf/SparseOHArray.hh>
#include <list>
#include <vector>

class QPDF_Array: public QPDFObject
{
  public:
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    QPDF_Array(SparseOHArray const& items);
    virtual ~QPDF_Array() = default;
    virtual std::string unparse();
    virtual JSON getJSON();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;
    virtual void setDescription(QPDF*, std::string const&);

    int getNItems() const;
    QPDFObjectHandle getItem(int n) const;
    void getAsVector(std::vector<QPDFObjectHandle>&) const;

    void setItem(int, QPDFObjectHandle const&);
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    void insertItem(int at, QPDFObjectHandle const& item);
    void appendItem(QPDFObjectHandle const& item);
    void eraseItem(int at);

    // Helper methods for QPDF and QPDFObjectHandle -- these are
    // public methods since the whole class is not part of the public
    // API. Otherwise, these would be wrapped in accessor classes.
    SparseOHArray const& getElementsForShallowCopy() const;
    void addExplicitElementsToList(std::list<QPDFObjectHandle>&) const;

  protected:
    virtual void releaseResolved();

  private:
    SparseOHArray elements;
};

#endif // QPDF_ARRAY_HH
