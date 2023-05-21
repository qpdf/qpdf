#include <qpdf/Pl_Function.hh>

#include <stdexcept>

Pl_Function::Members::Members(writer_t fn) :
    fn(fn)
{
}

Pl_Function::Pl_Function(char const* identifier, Pipeline* next, writer_t fn) :
    Pipeline(identifier, next),
    m(new Members(fn))
{
}

Pl_Function::Pl_Function(
    char const* identifier, Pipeline* next, writer_c_t fn, void* udata) :
    Pipeline(identifier, next),
    m(new Members(nullptr))
{
    m->fn = [identifier, fn, udata](unsigned char const* data, size_t len) {
        int code = fn(data, len, udata);
        if (code != 0) {
            throw std::runtime_error(
                std::string(identifier) + " function returned code " +
                std::to_string(code));
        }
    };
}

Pl_Function::Pl_Function(
    char const* identifier, Pipeline* next, writer_c_char_t fn, void* udata) :
    Pipeline(identifier, next),
    m(new Members(nullptr))
{
    m->fn = [identifier, fn, udata](unsigned char const* data, size_t len) {
        int code = fn(reinterpret_cast<char const*>(data), len, udata);
        if (code != 0) {
            throw std::runtime_error(
                std::string(identifier) + " function returned code " +
                std::to_string(code));
        }
    };
}

Pl_Function::~Pl_Function()
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in
    // README-maintainer
}

void
Pl_Function::write(unsigned char const* buf, size_t len)
{
    m->fn(buf, len);
    if (getNext(true)) {
        getNext()->write(buf, len);
    }
}

void
Pl_Function::finish()
{
    if (getNext(true)) {
        getNext()->finish();
    }
}
