#include <qpdf/QPDFLogger.hh>

#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_OStream.hh>
#include <iostream>
#include <stdexcept>

namespace
{
    class Pl_Track: public Pipeline
    {
      public:
        Pl_Track(char const* identifier, Pipeline* next) :
            Pipeline(identifier, next),
            used(false)
        {
        }

        virtual void
        write(unsigned char const* data, size_t len) override
        {
            this->used = true;
            getNext()->write(data, len);
        }

        virtual void
        finish() override
        {
            getNext()->finish();
        }

        bool
        getUsed() const
        {
            return used;
        }

      private:
        bool used;
    };
}; // namespace

QPDFLogger::Members::Members() :
    p_discard(new Pl_Discard()),
    p_real_stdout(new Pl_OStream("standard output", std::cout)),
    p_stdout(new Pl_Track("track stdout", p_real_stdout.get())),
    p_stderr(new Pl_OStream("standard error", std::cerr)),
    p_info(p_stdout),
    p_warn(nullptr),
    p_error(p_stderr),
    p_save(nullptr)
{
}

QPDFLogger::Members::~Members()
{
    p_stdout->finish();
    p_stderr->finish();
}

QPDFLogger::QPDFLogger() :
    m(new Members())
{
}

std::shared_ptr<QPDFLogger>
QPDFLogger::defaultLogger()
{
    static auto l = std::make_shared<QPDFLogger>();
    return l;
}

void
QPDFLogger::info(char const* s)
{
    getInfo(false)->writeCStr(s);
}

void
QPDFLogger::info(std::string const& s)
{
    getInfo(false)->writeString(s);
}

std::shared_ptr<Pipeline>
QPDFLogger::getInfo(bool null_okay)
{
    return throwIfNull(this->m->p_info, null_okay);
}

void
QPDFLogger::warn(char const* s)
{
    getWarn(false)->writeCStr(s);
}

void
QPDFLogger::warn(std::string const& s)
{
    getWarn(false)->writeString(s);
}

std::shared_ptr<Pipeline>
QPDFLogger::getWarn(bool null_okay)
{
    if (this->m->p_warn) {
        return this->m->p_warn;
    }
    return getError(null_okay);
}

void
QPDFLogger::error(char const* s)
{
    getError(false)->writeCStr(s);
}

void
QPDFLogger::error(std::string const& s)
{
    getError(false)->writeString(s);
}

std::shared_ptr<Pipeline>
QPDFLogger::getError(bool null_okay)
{
    return throwIfNull(this->m->p_error, null_okay);
}

std::shared_ptr<Pipeline>
QPDFLogger::getSave(bool null_okay)
{
    return throwIfNull(this->m->p_save, null_okay);
}

std::shared_ptr<Pipeline>
QPDFLogger::standardOutput()
{
    return this->m->p_stdout;
}

std::shared_ptr<Pipeline>
QPDFLogger::standardError()
{
    return this->m->p_stderr;
}

std::shared_ptr<Pipeline>
QPDFLogger::discard()
{
    return this->m->p_discard;
}

void
QPDFLogger::setInfo(std::shared_ptr<Pipeline> p)
{
    if (p == nullptr) {
        if (this->m->p_save == this->m->p_stdout) {
            p = this->m->p_stderr;
        } else {
            p = this->m->p_stdout;
        }
    }
    this->m->p_info = p;
}

void
QPDFLogger::setWarn(std::shared_ptr<Pipeline> p)
{
    this->m->p_warn = p;
}

void
QPDFLogger::setError(std::shared_ptr<Pipeline> p)
{
    if (p == nullptr) {
        p = this->m->p_stderr;
    }
    this->m->p_error = p;
}

void
QPDFLogger::setSave(std::shared_ptr<Pipeline> p)
{
    if (p == this->m->p_stdout) {
        auto pt = dynamic_cast<Pl_Track*>(p.get());
        if (pt->getUsed()) {
            throw std::logic_error(
                "QPDFLogger: called setSave on standard output after standard"
                " output has already been used");
        }
        if (this->m->p_info == this->m->p_stdout) {
            this->m->p_info = this->m->p_stderr;
        }
    }
    this->m->p_save = p;
}

void
QPDFLogger::saveToStandardOutput()
{
    setSave(standardOutput());
}

void
QPDFLogger::setOutputStreams(std::ostream* out_stream, std::ostream* err_stream)
{
    if (out_stream == &std::cout) {
        out_stream = nullptr;
    }
    if (err_stream == &std::cerr) {
        err_stream = nullptr;
    }
    std::shared_ptr<Pipeline> new_out;
    std::shared_ptr<Pipeline> new_err;

    if (out_stream == nullptr) {
        if (this->m->p_save == this->m->p_stdout) {
            new_out = this->m->p_stderr;
        } else {
            new_out = this->m->p_stdout;
        }
    } else {
        new_out = std::make_shared<Pl_OStream>("output", *out_stream);
    }
    if (err_stream == nullptr) {
        new_err = this->m->p_stderr;
    } else {
        new_err = std::make_shared<Pl_OStream>("error output", *err_stream);
    }
    this->m->p_info = new_out;
    this->m->p_warn = nullptr;
    this->m->p_error = new_err;
}

std::shared_ptr<Pipeline>
QPDFLogger::throwIfNull(std::shared_ptr<Pipeline> p, bool null_okay)
{
    if (!(null_okay || p)) {
        throw std::logic_error(
            "QPDFLogger: requested a null pipeline without null_okay == true");
    }
    return p;
}
