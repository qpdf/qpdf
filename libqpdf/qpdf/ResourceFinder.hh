#ifndef RESOURCEFINDER_HH
#define RESOURCEFINDER_HH

#include <qpdf/QPDFObjectHandle.hh>

class ResourceFinder: public QPDFObjectHandle::ParserCallbacks
{
  public:
    ResourceFinder();
    virtual ~ResourceFinder() = default;
    virtual void handleObject(QPDFObjectHandle, size_t, size_t) override;
    virtual void handleEOF() override;
    std::set<std::string> const& getNames() const;
    std::map<std::string,
             std::map<std::string,
                      std::set<size_t>>> const& getNamesByResourceType() const;

  private:
    std::string last_name;
    size_t last_name_offset;
    std::set<std::string> names;
    std::map<std::string,
             std::map<std::string,
                      std::set<size_t>>> names_by_resource_type;
};

#endif // RESOURCEFINDER_HH
