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
        void beginArray(std::string const& key,
                        json_handler_t start_fn,
                        bare_handler_t end_fn);
        void endContainer();

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
Handlers::beginArray(std::string const& key,
                     json_handler_t start_fn,
                     bare_handler_t end_fn)
{
    auto new_jh = std::make_shared<JSONHandler>();
    auto item_jh = std::make_shared<JSONHandler>();
    new_jh->addArrayHandlers(
        [start_fn](std::string const&, JSON j){ start_fn(j); },
        [end_fn](std::string const&){ end_fn(); },
        item_jh);
    this->jh->addDictKeyHandler(key, new_jh);
    this->json_handlers.push_back(item_jh);
    this->jh = item_jh.get();
}

void
Handlers::endContainer()
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
    addParameter(key, [this](char const* p) {
        c_main->password(p);
    });
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
Handlers::beginOutputOptionsEncrypt(JSON j)
{
    // This method is only called if the overall JSON structure
    // matches the schema, so we already know that keys that are
    // present have the right types.
    int key_len = 0;
    std::string user_password;
    std::string owner_password;
    bool user_password_seen = false;
    bool owner_password_seen = false;
    j.forEachDictItem([&](std::string const& key, JSON value){
        if ((key == "40bit") || (key == "128bit") || (key == "256bit"))
        {
            if (key_len != 0)
            {
                usage("exactly one of 40bit, 128bit, or 256bit must be given");
            }
            key_len = QUtil::string_to_int(key.c_str());
        }
        else if (key == "userPassword")
        {
            user_password_seen = value.getString(user_password);
        }
        else if (key == "ownerPassword")
        {
            owner_password_seen = value.getString(owner_password);
        }
    });
    if (key_len == 0)
    {
        usage("exactly one of 40bit, 128bit, or 256bit must be given;"
              " an empty dictionary may be supplied for one of them"
              " to set the key length without imposing any restrictions");
    }
    if (! (user_password_seen && owner_password_seen))
    {
        usage("the user and owner password are both required; use the empty"
              " string for the user password if you don't want a password");
    }
    this->c_enc = c_main->encrypt(key_len, user_password, owner_password);
}

void
Handlers::endOutputOptionsEncrypt()
{
    this->c_enc->endEncrypt();
    this->c_enc = nullptr;
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
Handlers::beginOptionsAddAttachmentArray(JSON)
{
    // nothing needed
}

void
Handlers::endOptionsAddAttachmentArray()
{
    // nothing needed
}

void
Handlers::beginOptionsAddAttachment(JSON)
{
    this->c_att = c_main->addAttachment();
}

void
Handlers::endOptionsAddAttachment()
{
    this->c_att->endAddAttachment();
    this->c_att = nullptr;
}

void
Handlers::setupOptionsAddAttachmentPath(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_att->path(p);
    });
}

void
Handlers::beginOptionsCopyAttachmentsFromArray(JSON)
{
    // nothing needed
}

void
Handlers::endOptionsCopyAttachmentsFromArray()
{
    // nothing needed
}

void
Handlers::beginOptionsCopyAttachmentsFrom(JSON)
{
    this->c_copy_att = c_main->copyAttachmentsFrom();
}

void
Handlers::endOptionsCopyAttachmentsFrom()
{
    this->c_copy_att->endCopyAttachmentsFrom();
    this->c_copy_att = nullptr;
}

void
Handlers::setupOptionsCopyAttachmentsFromPath(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_copy_att->path(p);
    });
}

void
Handlers::setupOptionsCopyAttachmentsFromPassword(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_copy_att->password(p);
    });
}

void
Handlers::beginOptionsPagesArray(JSON)
{
    this->c_pages = c_main->pages();
}

void
Handlers::endOptionsPagesArray()
{
    c_pages->endPages();
    c_pages = nullptr;
}

void
Handlers::beginOptionsPages(JSON j)
{
    std::string file;
    std::string range("1-z");
    std::string password;
    bool file_seen = false;
    bool password_seen = false;
    j.forEachDictItem([&](std::string const& key, JSON value){
        if (key == "file")
        {
            file_seen = value.getString(file);
        }
        else if (key == "range")
        {
            value.getString(range);
        }
        else if (key == "password")
        {
            password_seen = value.getString(password);
        }
    });
    if (! file_seen)
    {
        usage("file is required in page specification");
    }
    this->c_pages->pageSpec(
        file, range, password_seen ? password.c_str() : nullptr);
}

void
Handlers::endOptionsPages()
{
    // nothing needed
}

void
Handlers::setupOptionsPagesFile(std::string const& key)
{
    // handled in beginOptionsPages
}

void
Handlers::setupOptionsPagesPassword(std::string const& key)
{
    // handled in beginOptionsPages
}

void
Handlers::setupOptionsPagesRange(std::string const& key)
{
    // handled in beginOptionsPages
}

void
Handlers::beginOptionsOverlay(JSON)
{
    this->c_uo = c_main->overlay();
}

void
Handlers::endOptionsOverlay()
{
    c_uo->endUnderlayOverlay();
    c_uo = nullptr;
}

void
Handlers::setupOptionsOverlayFile(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_uo->path(p);
    });
}

void
Handlers::setupOptionsOverlayPassword(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_uo->password(p);
    });
}

void
Handlers::beginOptionsUnderlay(JSON)
{
    this->c_uo = c_main->underlay();
}

void
Handlers::endOptionsUnderlay()
{
    c_uo->endUnderlayOverlay();
    c_uo = nullptr;
}

void
Handlers::setupOptionsUnderlayFile(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_uo->path(p);
    });
}

void
Handlers::setupOptionsUnderlayPassword(std::string const& key)
{
    addParameter(key, [this](char const* p) {
        c_uo->password(p);
    });
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
