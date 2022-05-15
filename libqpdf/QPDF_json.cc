#include <qpdf/QPDF.hh>

#include <qpdf/FileInputSource.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <regex>

namespace
{
    class JSONExc: public std::runtime_error
    {
      public:
        JSONExc(JSON const& value, std::string const& msg) :
            std::runtime_error(
                "offset " + QUtil::uint_to_string(value.getStart()) + ": " +
                msg)
        {
        }
    };
} // namespace

static std::regex PDF_VERSION_RE("^\\d+\\.\\d+$");
static std::regex OBJ_KEY_RE("^obj:(\\d+) (\\d+) R$");

QPDF::JSONReactor::JSONReactor(QPDF& pdf, bool must_be_complete) :
    pdf(pdf),
    must_be_complete(must_be_complete),
    saw_qpdf(false),
    saw_json_version(false),
    saw_pdf_version(false),
    saw_trailer(false),
    state(st_initial),
    next_state(st_top)
{
    state_stack.push_back(st_initial);
}

void
QPDF::JSONReactor::containerStart()
{
    state_stack.push_back(state);
    state = next_state;
}

void
QPDF::JSONReactor::dictionaryStart()
{
    containerStart();
    // QXXXQ
}

void
QPDF::JSONReactor::arrayStart()
{
    containerStart();
    if (state == st_top) {
        QTC::TC("qpdf", "QPDF_json top-level array");
        throw std::runtime_error("QPDF JSON must be a dictionary");
    }
    // QXXXQ
}

void
QPDF::JSONReactor::containerEnd(JSON const& value)
{
    state = state_stack.back();
    state_stack.pop_back();
    if (state == st_initial) {
        if (!this->saw_qpdf) {
            QTC::TC("qpdf", "QPDF_json missing qpdf");
            throw std::runtime_error("\"qpdf\" object was not seen");
        }
        if (!this->saw_json_version) {
            QTC::TC("qpdf", "QPDF_json missing json version");
            throw std::runtime_error("\"qpdf.jsonversion\" was not seen");
        }
        if (must_be_complete && !this->saw_pdf_version) {
            QTC::TC("qpdf", "QPDF_json missing pdf version");
            throw std::runtime_error("\"qpdf.pdfversion\" was not seen");
        }
        if (must_be_complete && !this->saw_trailer) {
            /// QTC::TC("qpdf", "QPDF_json missing trailer");
            throw std::runtime_error("\"qpdf.objects.trailer\" was not seen");
        }
    }

    // QXXXQ
}

void
QPDF::JSONReactor::topLevelScalar()
{
    QTC::TC("qpdf", "QPDF_json top-level scalar");
    throw std::runtime_error("QPDF JSON must be a dictionary");
}

void
QPDF::JSONReactor::nestedState(
    std::string const& key, JSON const& value, state_e next)
{
    // Use this method when the next state is for processing a nested
    // dictionary.
    if (!value.isDictionary()) {
        throw JSONExc(value, "\"" + key + "\" must be a dictionary");
    }
    this->next_state = next;
}

bool
QPDF::JSONReactor::dictionaryItem(std::string const& key, JSON const& value)
{
    if (state == st_ignore) {
        // ignore
    } else if (state == st_top) {
        if (key == "qpdf") {
            this->saw_qpdf = true;
            nestedState(key, value, st_qpdf);
        } else {
            // Ignore all other fields for forward compatibility.
            // Don't use nestedState since this can be any type.
            next_state = st_ignore;
        }
    } else if (state == st_qpdf) {
        if (key == "jsonversion") {
            this->saw_json_version = true;
            std::string v;
            if (!(value.getNumber(v) && (v == "2"))) {
                QTC::TC("qpdf", "QPDF_json bad json version");
                throw JSONExc(value, "only JSON version 2 is supported");
            }
        } else if (key == "pdfversion") {
            this->saw_pdf_version = true;
            bool version_okay = false;
            std::string v;
            if (value.getString(v)) {
                std::smatch m;
                if (std::regex_match(v, m, PDF_VERSION_RE)) {
                    version_okay = true;
                    this->pdf.m->pdf_version = v;
                }
            }
            if (!version_okay) {
                QTC::TC("qpdf", "QPDF_json bad pdf version");
                throw JSONExc(value, "invalid PDF version (must be x.y)");
            }
        } else if (key == "objects") {
            nestedState(key, value, st_objects_top);
        } else {
            // ignore unknown keys for forward compatibility
        }
    } else if (state == st_objects_top) {
        std::smatch m;
        if (key == "trailer") {
            this->saw_trailer = true;
            nestedState(key, value, st_trailer_top);
            // QXXXQ
        } else if (std::regex_match(key, m, OBJ_KEY_RE)) {
            nestedState(key, value, st_object_top);
            // QXXXQ
        } else {
            QTC::TC("qpdf", "QPDF_json bad object key");
            throw JSONExc(
                value, "object key should be \"trailer\" or \"obj:n n R\"");
        }
    } else if (state == st_object_top) {
        if (key == "value") {
            // Don't use nestedState since this can have any type.
            next_state = st_object;
            // QXXXQ
        } else if (key == "stream") {
            nestedState(key, value, st_stream);
            // QXXXQ
        } else {
            // Ignore unknown keys for forward compatibility
        }
    } else if (state == st_trailer_top) {
        if (key == "value") {
            // The trailer must be a dictionary, so we can use nestedState.
            nestedState("trailer.value", value, st_object);
            // QXXXQ
        } else if (key == "stream") {
            QTC::TC("qpdf", "QPDF_json trailer stream");
            throw JSONExc(value, "the trailer may not be a stream");
        } else {
            // Ignore unknown keys for forward compatibility
        }
    } else if (state == st_stream) {
        if (key == "dict") {
            // Since a stream dictionary must be a dictionary, we can
            // use nestedState to transition to st_value.
            nestedState("stream.dict", value, st_object);
            // QXXXQ
        } else if (key == "data") {
            // QXXXQ
        } else if (key == "datafile") {
            // QXXXQ
        } else {
            // Ignore unknown keys for forward compatibility.
            next_state = st_ignore;
        }
    } else if (state == st_object) {
        // QXXXQ
    } else {
        throw std::logic_error(
            "QPDF_json: unknown state " + QUtil::int_to_string(state));
    }

    // QXXXQ
    return true;
}

bool
QPDF::JSONReactor::arrayItem(JSON const& value)
{
    // QXXXQ
    return true;
}

void
QPDF::createFromJSON(std::string const& json_file)
{
    createFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::createFromJSON(std::shared_ptr<InputSource> is)
{
    importJSON(is, true);
}

void
QPDF::updateFromJSON(std::string const& json_file)
{
    updateFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::updateFromJSON(std::shared_ptr<InputSource> is)
{
    importJSON(is, false);
}

void
QPDF::importJSON(std::shared_ptr<InputSource> is, bool must_be_complete)
{
    JSONReactor reactor(*this, must_be_complete);
    try {
        JSON::parse(*is, &reactor);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(is->getName() + ": " + e.what());
    }
}
