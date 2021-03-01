#ifndef RESOURCEFINDER_HH
#define RESOURCEFINDER_HH

#include <qpdf/QPDFObjectHandle.hh>

class ResourceFinder: public QPDFObjectHandle::TokenFilter
{
  public:
    ResourceFinder();
    virtual ~ResourceFinder() = default;
    virtual void handleToken(QPDFTokenizer::Token const&) override;
    std::set<std::string> const& getNames() const;
    bool sawBad() const;

  private:
    std::string last_name;
    std::set<std::string> names;
    std::map<std::string, std::set<std::string>> names_by_resource_type;
    bool saw_bad;
};

#endif // RESOURCEFINDER_HH
