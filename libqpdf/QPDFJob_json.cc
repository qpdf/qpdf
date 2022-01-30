#include <qpdf/QPDFJob.hh>
#include <qpdf/JSONHandler.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QUtil.hh>

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

        typedef std::function<void()> bare_handler_t;
        typedef std::function<void(char const*)> param_handler_t;
        typedef std::function<void(JSON)> json_handler_t;
        typedef std::function<void(std::string const& key)> setup_handler_t;

        void addBare(std::string const& key, bare_handler_t);
        void addParameter(std::string const& key, param_handler_t);
        void addChoices(std::string const& key, char const** choices,
                        param_handler_t);
        void doSetup(std::string const& key, setup_handler_t);
        void beginDict(std::string const& key,
                       json_handler_t start_fn,
                       bare_handler_t end_fn);
        void endDict();

        bare_handler_t bindBare(void (Handlers::*f)());
        json_handler_t bindJSON(void (Handlers::*f)(JSON));
        setup_handler_t bindSetup(void (Handlers::*f)(std::string const&));

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

Handlers::bare_handler_t
Handlers::bindBare(void (Handlers::*f)())
{
    return std::bind(std::mem_fn(f), this);
}

Handlers::json_handler_t
Handlers::bindJSON(void (Handlers::*f)(JSON))
{
    return std::bind(std::mem_fn(f), this, std::placeholders::_1);
}

Handlers::setup_handler_t
Handlers::bindSetup(void (Handlers::*f)(std::string const&))
{
    return std::bind(std::mem_fn(f), this, std::placeholders::_1);
}

void
Handlers::initHandlers()
{
    this->json_handlers.push_back(std::make_shared<JSONHandler>());
    this->jh = this->json_handlers.back().get();
    jh->addDictHandlers(
        [](std::string const&, JSON){},
        [this](std::string const&){c_main->checkConfiguration();});

#   include <qpdf/auto_job_json_init.hh>

    if (this->json_handlers.size() != 1)
    {
        throw std::logic_error("QPDFJob_json: json_handlers size != 1 at end");
    }
}

void
Handlers::addBare(std::string const& key, bare_handler_t fn)
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
Handlers::addParameter(std::string const& key, param_handler_t fn)
{
    auto h = std::make_shared<JSONHandler>();
    h->addStringHandler(
        [fn](std::string const& path, std::string const& parameter){
            fn(parameter.c_str());
        });
    jh->addDictKeyHandler(key, h);
}

void
Handlers::addChoices(std::string const& key, char const** choices,
                     param_handler_t fn)
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
    jh->addDictKeyHandler(key, h);
}

void
Handlers::doSetup(std::string const& key, setup_handler_t fn)
{
    fn(key);
}

void
Handlers::beginDict(std::string const& key,
                    json_handler_t start_fn,
                    bare_handler_t end_fn)
{
    auto new_jh = std::make_shared<JSONHandler>();
    new_jh->addDictHandlers(
        [start_fn](std::string const&, JSON j){ start_fn(j); },
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
Handlers::beginInput(JSON)
{
    // nothing needed
}

void
Handlers::endInput()
{
    // nothing needed
}

void
Handlers::setupInputFilename(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_main->inputFile(p);
    });
}

void
Handlers::setupInputPassword(std::string const& key)
{
    // QXXXQ
}

void
Handlers::setupInputEmpty(std::string const& key)
{
    addBare(key, [this]() {
        c_main->emptyInput();
    });
}

void
Handlers::beginOutput(JSON)
{
    // nothing needed
}

void
Handlers::endOutput()
{
    // nothing needed
}

void
Handlers::setupOutputFilename(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_main->outputFile(p);
    });
}

void
Handlers::setupOutputReplaceInput(std::string const& key)
{
    addBare(key, [this]() {
        c_main->replaceInput();
    });
}

void
Handlers::beginOutputOptions(JSON)
{
    // nothing needed
}

void
Handlers::endOutputOptions()
{
    // nothing needed
}

void
Handlers::beginOutputOptionsEncrypt(JSON)
{
    // QXXXQ
//    if (this->keylen_seen == 0)
//    {
//        usage("exactly one of 40bit, 128bit, or 256bit must be given;"
//              " an empty dictionary may be supplied for one of them"
//              " to set the key length without imposing any restrictions");
//    }
}

void
Handlers::endOutputOptionsEncrypt()
{
    // QXXXQ
}

void
Handlers::setupOutputOptionsEncryptUserPassword(std::string const& key)
{
    // Key handled in beginOutputOptionsEncrypt
}

void
Handlers::setupOutputOptionsEncryptOwnerPassword(std::string const& key)
{
    // Key handled in beginOutputOptionsEncrypt
}

void
Handlers::beginOutputOptionsEncrypt40bit(JSON)
{
    // nothing needed
}

void
Handlers::endOutputOptionsEncrypt40bit()
{
    // nothing needed
}

void
Handlers::beginOutputOptionsEncrypt128bit(JSON)
{
    // nothing needed
}

void
Handlers::endOutputOptionsEncrypt128bit()
{
    // nothing needed
}

void
Handlers::beginOutputOptionsEncrypt256bit(JSON)
{
    // nothing needed
}

void
Handlers::endOutputOptionsEncrypt256bit()
{
    // nothing needed
}

void
Handlers::beginOptions(JSON)
{
    // nothing needed
}

void
Handlers::endOptions()
{
    // nothing needed
}

void
Handlers::beginInspect(JSON)
{
    // nothing needed
}

void
Handlers::endInspect()
{
    // nothing needed
}

void
Handlers::beginTransform(JSON)
{
    // nothing needed
}

void
Handlers::endTransform()
{
    // nothing needed
}

void
Handlers::beginModify(JSON)
{
    // nothing needed
}

void
Handlers::endModify()
{
    // nothing needed
}

void
Handlers::beginModifyAddAttachment(JSON)
{
    // QXXXQ
}

void
Handlers::endModifyAddAttachment()
{
    // QXXXQ
}

void
Handlers::setupModifyAddAttachmentPath(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::beginModifyCopyAttachmentsFrom(JSON)
{
    // QXXXQ
}

void
Handlers::endModifyCopyAttachmentsFrom()
{
    // QXXXQ
}

void
Handlers::setupModifyCopyAttachmentsFromPath(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::setupModifyCopyAttachmentsFromPassword(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::beginModifyPages(JSON)
{
    // QXXXQ
}

void
Handlers::endModifyPages()
{
    // QXXXQ
}

void
Handlers::setupModifyPagesFile(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::setupModifyPagesPassword(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::setupModifyPagesRange(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::beginModifyOverlay(JSON)
{
    // QXXXQ
}

void
Handlers::endModifyOverlay()
{
    // QXXXQ
}

void
Handlers::setupModifyOverlayFile(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::setupModifyOverlayPassword(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::beginModifyUnderlay(JSON)
{
    // QXXXQ
}

void
Handlers::endModifyUnderlay()
{
    // QXXXQ
}

void
Handlers::setupModifyUnderlayFile(std::string const& key)
{
    // QXXXQ setup
}

void
Handlers::setupModifyUnderlayPassword(std::string const& key)
{
    // QXXXQ setup
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
