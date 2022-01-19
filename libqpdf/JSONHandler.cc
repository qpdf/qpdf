#include <qpdf/JSONHandler.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

JSONHandler::Error::Error(std::string const& msg) :
    std::runtime_error(msg)
{
}

JSONHandler::JSONHandler() :
    m(new Members())
{
}

JSONHandler::Members::Members()
{
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

std::map<std::string, std::shared_ptr<JSONHandler>>&
JSONHandler::addDictHandlers()
{
    return this->m->h.dict_handlers;
}

void
JSONHandler::addFallbackDictHandler(std::shared_ptr<JSONHandler> fdh)
{
    this->m->h.fallback_dict_handler = fdh;
}

void
JSONHandler::addArrayHandler(std::shared_ptr<JSONHandler> ah)
{
    this->m->h.array_handler = ah;
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
    std::string svalue;
    if (this->m->h.null_handler && j.isNull())
    {
        this->m->h.null_handler(path);
        handled = true;
    }
    if (this->m->h.string_handler && j.getString(svalue))
    {
        this->m->h.string_handler(path, svalue);
        handled = true;
    }
    if (this->m->h.number_handler && j.getNumber(svalue))
    {
        this->m->h.number_handler(path, svalue);
        handled = true;
    }
    if (this->m->h.bool_handler && j.getBool(bvalue))
    {
        this->m->h.bool_handler(path, bvalue);
        handled = true;
    }
    if ((this->m->h.fallback_dict_handler.get() ||
         (! this->m->h.dict_handlers.empty())) && j.isDictionary())
    {
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
                    throw Error(
                        "JSON handler found unexpected key " + k +
                        " in object at " + path);
                }
            }
            else
            {
                i->second->handle(path_base + k, v);
            }
        });

        // Set handled = true even if we didn't call any handlers.
        // This dictionary could have been empty, but it's okay since
        // it's a dictionary like it's supposed to be.
        handled = true;
    }
    if (this->m->h.array_handler.get())
    {
        size_t i = 0;
        j.forEachArrayItem([&i, &path, this](JSON v) {
            this->m->h.array_handler->handle(
                path + "[" + QUtil::uint_to_string(i) + "]", v);
            ++i;
        });
        // Set handled = true even if we didn't call any handlers.
        // This could have been an empty array.
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
        throw Error("JSON handler: value at " + path +
                    " is not of expected type");
    }
}
