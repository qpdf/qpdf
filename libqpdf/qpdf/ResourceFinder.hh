#ifndef RESOURCEFINDER_HH
#define RESOURCEFINDER_HH

#include <qpdf/QPDFObjectHandle.hh>

class ResourceFinder final: public QPDFObjectHandle::ParserCallbacks
{
  public:
    ResourceFinder() = default;
    ~ResourceFinder() final = default;
    void handleObject(QPDFObjectHandle, size_t, size_t) final;
    void handleEOF() final;
    std::set<std::string> const&
    getNames() const
    {
        return names;
    }
    std::map<std::string, std::map<std::string, std::vector<size_t>>> const&
    getNamesByResourceType() const
    {
        return names_by_resource_type;
    }

  private:
    std::string last_name;
    size_t last_name_offset{0};
    std::set<std::string> names;
    std::map<std::string, std::map<std::string, std::vector<size_t>>> names_by_resource_type;
};

#endif // RESOURCEFINDER_HH
