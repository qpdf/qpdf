#include <qpdf/QPDFLogger.hh>

#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_OStream.hh>
#include <qpdf/QUtil.hh>
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

        void
        write(unsigned char const* data, size_t len) override
        {
            this->used = true;
            getNext()->write(data, len);
        }

        void
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
QPDFLogger::create()
{
    return std::shared_ptr<QPDFLogger>(new QPDFLogger);
}

std::shared_ptr<QPDFLogger>
QPDFLogger::defaultLogger()
{
    static auto l = create();
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
    return throwIfNull(m->p_info, null_okay);
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
    if (m->p_warn) {
        return m->p_warn;
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
    return throwIfNull(m->p_error, null_okay);
}

std::shared_ptr<Pipeline>
QPDFLogger::getSave(bool null_okay)
{
    return throwIfNull(m->p_save, null_okay);
}

std::shared_ptr<Pipeline>
QPDFLogger::standardOutput()
{
    return m->p_stdout;
}

std::shared_ptr<Pipeline>
QPDFLogger::standardError()
{
    return m->p_stderr;
}

std::shared_ptr<Pipeline>
QPDFLogger::discard()
{
    return m->p_discard;
}

void
QPDFLogger::setInfo(std::shared_ptr<Pipeline> p)
{
    if (p == nullptr) {
        if (m->p_save == m->p_stdout) {
            p = m->p_stderr;
        } else {
            p = m->p_stdout;
        }
    }
    m->p_info = p;
}

void
QPDFLogger::setWarn(std::shared_ptr<Pipeline> p)
{
    m->p_warn = p;
}

void
QPDFLogger::setError(std::shared_ptr<Pipeline> p)
{
    if (p == nullptr) {
        p = m->p_stderr;
    }
    m->p_error = p;
}

void
QPDFLogger::setSave(std::shared_ptr<Pipeline> p, bool only_if_not_set)
{
    if (only_if_not_set && (m->p_save != nullptr)) {
        return;
    }
    if (m->p_save == p) {
        return;
    }
    if (p == m->p_stdout) {
        auto pt = dynamic_cast<Pl_Track*>(p.get());
        if (pt->getUsed()) {
            throw std::logic_error("QPDFLogger: called setSave on standard output after standard"
                                   " output has already been used");
        }
        if (m->p_info == m->p_stdout) {
            m->p_info = m->p_stderr;
        }
        QUtil::binary_stdout();
    }
    m->p_save = p;
}

void
QPDFLogger::saveToStandardOutput(bool only_if_not_set)
{
    setSave(standardOutput(), only_if_not_set);
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
        if (m->p_save == m->p_stdout) {
            new_out = m->p_stderr;
        } else {
            new_out = m->p_stdout;
        }
    } else {
        new_out = std::make_shared<Pl_OStream>("output", *out_stream);
    }
    if (err_stream == nullptr) {
        new_err = m->p_stderr;
    } else {
        new_err = std::make_shared<Pl_OStream>("error output", *err_stream);
    }
    m->p_info = new_out;
    m->p_warn = nullptr;
    m->p_error = new_err;
}

std::shared_ptr<Pipeline>
QPDFLogger::throwIfNull(std::shared_ptr<Pipeline> p, bool null_okay)
{
    if (!(null_okay || p)) {
        throw std::logic_error("QPDFLogger: requested a null pipeline without null_okay == true");
    }
    return p;
}
