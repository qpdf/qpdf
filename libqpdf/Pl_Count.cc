#include <qpdf/Pl_Count.hh>

#include <qpdf/QIntC.hh>

class Pl_Count::Members
{
  public:
    Members() = default;
    Members(Members const&) = delete;
    ~Members() = default;

    // Must be qpdf_offset_t, not size_t, to handle writing more than size_t can handle.
    qpdf_offset_t count{0};
    unsigned char last_char{'\0'};
};

Pl_Count::Pl_Count(char const* identifier, Pipeline* next) :
    Pipeline(identifier, next),
    m(std::make_unique<Members>())
{
    if (!next) {
        throw std::logic_error("Attempt to create Pl_Count with nullptr as next");
    }
}

Pl_Count::~Pl_Count() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
Pl_Count::write(unsigned char const* buf, size_t len)
{
    if (len) {
        m->count += QIntC::to_offset(len);
        m->last_char = buf[len - 1];
        next()->write(buf, len);
    }
}

void
Pl_Count::finish()
{
    next()->finish();
}

qpdf_offset_t
Pl_Count::getCount() const
{
    return m->count;
}

unsigned char
Pl_Count::getLastChar() const
{
    return m->last_char;
}
