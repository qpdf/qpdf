#include <qpdf/QPDFJob.hh>
#include <qpdf/JSONHandler.hh>
#include <qpdf/QPDFUsage.hh>

#include <memory>
#include <stdexcept>
#include <sstream>
#include <cstring>

static JSON JOB_SCHEMA = JSON::parse(QPDFJob::json_job_schema_v1().c_str());

namespace
{
    class Handlers
    {
      public:
        Handlers(std::shared_ptr<QPDFJob::Config> c_main);
        void handle(JSON&);

      private:
#       include <qpdf/auto_job_json_decl.hh>

        void usage(std::string const& message);
        void initHandlers();

        typedef std::function<void()> bare_arg_handler_t;
        typedef std::function<void(char const*)> param_arg_handler_t;

        void addBare(std::string const& key, bare_arg_handler_t);
        void addParameter(std::string const& key, bool required,
                          param_arg_handler_t);
        void addChoices(std::string const& key,
                        bool required, char const** choices,
                        param_arg_handler_t);
        void beginDict(std::string const& key,
                       bare_arg_handler_t start_fn,
                       bare_arg_handler_t end_fn);
        void endDict();

        std::list<std::shared_ptr<JSONHandler>> json_handlers;
        JSONHandler* jh; // points to last of json_handlers
        std::shared_ptr<QPDFJob::Config> c_main;
        std::shared_ptr<QPDFJob::CopyAttConfig> c_copy_att;
        std::shared_ptr<QPDFJob::AttConfig> c_att;
        std::shared_ptr<QPDFJob::PagesConfig> c_pages;
        std::shared_ptr<QPDFJob::UOConfig> c_uo;
        std::shared_ptr<QPDFJob::EncConfig> c_enc;
    };
}

Handlers::Handlers(std::shared_ptr<QPDFJob::Config> c_main) :
    jh(nullptr),
    c_main(c_main)
{
    initHandlers();
}

void
Handlers::usage(std::string const& message)
{
    throw QPDFUsage(message);
}

void
Handlers::initHandlers()
{
    this->json_handlers.push_back(std::make_shared<JSONHandler>());
    this->jh = this->json_handlers.back().get();
    jh->addDictHandlers(
        [](std::string const&){},
        [this](std::string const&){c_main->checkConfiguration();});

#   include <qpdf/auto_job_json_init.hh>

    // QXXXQ
    auto empty = [](){};
    beginDict("input", empty, empty);
    beginDict("file", empty, empty);
    addParameter("name", true, [this](char const* p) {
        c_main->inputFile(p);
    });
    endDict(); // input.file
    endDict(); // input
    beginDict("output", empty, empty);
    beginDict("file", empty, empty);
    addParameter("name", true, [this](char const* p) {
        c_main->outputFile(p);
    });
    endDict(); // output.file
    beginDict("options", empty, empty);
    addBare("qdf", [this]() {
        c_main->qdf();
    });
    char const* choices[] = {"disable", "preserve", "generate", 0};
    addChoices("objectStreams", true, choices, [this](char const* p) {
        c_main->objectStreams(p);
    });
    endDict(); // output.options
    endDict(); // output
    // /QXXXQ

    if (this->json_handlers.size() != 1)
    {
        throw std::logic_error("QPDFJob_json: json_handlers size != 1 at end");
    }
}

void
Handlers::addBare(std::string const& key, bare_arg_handler_t fn)
{
    auto h = std::make_shared<JSONHandler>();
    h->addBoolHandler([this, fn](std::string const& path, bool v){
        if (! v)
        {
            usage(path + ": value must be true");
        }
        else
        {
            fn();
        }
    });
    jh->addDictKeyHandler(key, h);
}

void
Handlers::addParameter(std::string const& key,
                       bool required,
                       param_arg_handler_t fn)
{
    auto h = std::make_shared<JSONHandler>();
    h->addStringHandler(
        [fn](std::string const& path, std::string const& parameter){
            fn(parameter.c_str());
        });
    if (! required)
    {
        h->addNullHandler(
            [fn](std::string const& path){
                fn(nullptr);
            });
    }
    jh->addDictKeyHandler(key, h);
}

void
Handlers::addChoices(std::string const& key,
                     bool required, char const** choices,
                     param_arg_handler_t fn)
{
    auto h = std::make_shared<JSONHandler>();
    h->addStringHandler(
        [fn, choices, this](
            std::string const& path, std::string const& parameter){

            char const* p = parameter.c_str();
            bool matches = false;
            for (char const** i = choices; *i; ++i)
            {
                if (strcmp(*i, p) == 0)
                {
                    matches = true;
                    break;
                }
            }
            if (! matches)
            {
                std::ostringstream msg;
                msg << path + ": unexpected value; expected one of ";
                bool first = true;
                for (char const** i = choices; *i; ++i)
                {
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        msg << ", ";
                    }
                    msg << *i;
                }
                usage(msg.str());
            }
            fn(parameter.c_str());
        });
    if (! required)
    {
        h->addNullHandler(
            [fn](std::string const& path){
                fn(nullptr);
            });
    }
    jh->addDictKeyHandler(key, h);
}

void
Handlers::beginDict(std::string const& key,
                    bare_arg_handler_t start_fn,
                    bare_arg_handler_t end_fn)
{
    auto new_jh = std::make_shared<JSONHandler>();
    new_jh->addDictHandlers(
        [start_fn](std::string const&){ start_fn(); },
        [end_fn](std::string const&){ end_fn(); });
    this->jh->addDictKeyHandler(key, new_jh);
    this->json_handlers.push_back(new_jh);
    this->jh = new_jh.get();
}

void
Handlers::endDict()
{
    this->json_handlers.pop_back();
    this->jh = this->json_handlers.back().get();
}

void
Handlers::handle(JSON& j)
{
    this->json_handlers.back()->handle(".", j);
}

void
QPDFJob::initializeFromJson(std::string const& json)
{
    std::list<std::string> errors;
    JSON j = JSON::parse(json);
    if (! j.checkSchema(JOB_SCHEMA, JSON::f_optional, errors))
    {
        std::ostringstream msg;
        msg << this->m->message_prefix
            << ": job json has errors:";
        for (auto const& error: errors)
        {
            msg << std::endl << "  " << error;
        }
        throw std::runtime_error(msg.str());
    }

    Handlers(config()).handle(j);
}
