#include <qpdf/JSONHandler.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFUsage.hh>

JSONHandler::JSONHandler() :
    m(new Members())
{
}

JSONHandler::Members::Members()
{
}

void
JSONHandler::usage(std::string const& msg)
{
    throw QPDFUsage(msg);
}

void
JSONHandler::addAnyHandler(json_handler_t fn)
{
    this->m->h.any_handler = fn;
}

void
JSONHandler::addNullHandler(void_handler_t fn)
{
    this->m->h.null_handler = fn;
}

void
JSONHandler::addStringHandler(string_handler_t fn)
{
    this->m->h.string_handler = fn;
}

void
JSONHandler::addNumberHandler(string_handler_t fn)
{
    this->m->h.number_handler = fn;
}

void
JSONHandler::addBoolHandler(bool_handler_t fn)
{
    this->m->h.bool_handler = fn;
}

void
JSONHandler::addDictHandlers(json_handler_t start_fn, void_handler_t end_fn)
{
    this->m->h.dict_start_handler = start_fn;
    this->m->h.dict_end_handler = end_fn;
}

void
JSONHandler::addDictKeyHandler(
    std::string const& key, std::shared_ptr<JSONHandler> dkh)
{
    this->m->h.dict_handlers[key] = dkh;
}

void
JSONHandler::addFallbackDictHandler(std::shared_ptr<JSONHandler> fdh)
{
    this->m->h.fallback_dict_handler = fdh;
}

void
JSONHandler::addArrayHandlers(json_handler_t start_fn,
                              void_handler_t end_fn,
                              std::shared_ptr<JSONHandler> ah)
{
    this->m->h.array_start_handler = start_fn;
    this->m->h.array_end_handler = end_fn;
    this->m->h.array_item_handler = ah;
}

void
JSONHandler::handle(std::string const& path, JSON j)
{
    if (this->m->h.any_handler)
    {
        this->m->h.any_handler(path, j);
        return;
    }
    bool handled = false;
    bool bvalue = false;
    std::string s_value;
    if (this->m->h.null_handler && j.isNull())
    {
        this->m->h.null_handler(path);
        handled = true;
    }
    if (this->m->h.string_handler && j.getString(s_value))
    {
        this->m->h.string_handler(path, s_value);
        handled = true;
    }
    if (this->m->h.number_handler && j.getNumber(s_value))
    {
        this->m->h.number_handler(path, s_value);
        handled = true;
    }
    if (this->m->h.bool_handler && j.getBool(bvalue))
    {
        this->m->h.bool_handler(path, bvalue);
        handled = true;
    }
    if (this->m->h.dict_start_handler && j.isDictionary())
    {
        this->m->h.dict_start_handler(path, j);
        std::string path_base = path;
        if (path_base != ".")
        {
            path_base += ".";
        }
        j.forEachDictItem([&path, &path_base, this](
                              std::string const& k, JSON v) {
            auto i = this->m->h.dict_handlers.find(k);
            if (i == this->m->h.dict_handlers.end())
            {
                if (this->m->h.fallback_dict_handler.get())
                {
                    this->m->h.fallback_dict_handler->handle(
                        path_base + k, v);
                }
                else
                {
                    QTC::TC("libtests", "JSONHandler unexpected key");
                    usage("JSON handler found unexpected key " + k +
                          " in object at " + path);
                }
            }
            else
            {
                i->second->handle(path_base + k, v);
            }
        });
        this->m->h.dict_end_handler(path);
        handled = true;
    }
    if (this->m->h.array_start_handler && j.isArray())
    {
        this->m->h.array_start_handler(path, j);
        size_t i = 0;
        j.forEachArrayItem([&i, &path, this](JSON v) {
            this->m->h.array_item_handler->handle(
                path + "[" + QUtil::uint_to_string(i) + "]", v);
            ++i;
        });
        this->m->h.array_end_handler(path);
        handled = true;
    }

    if (! handled)
    {
        // It would be nice to include information about what type the
        // object was and what types were allowed, but we're relying
        // on schema validation to make sure input is properly
        // structured before calling the handlers. It would be
        // different if this code were trying to be part of a
        // general-purpose JSON package.
        QTC::TC("libtests", "JSONHandler unhandled value");
        usage("JSON handler: value at " + path + " is not of expected type");
    }
}
