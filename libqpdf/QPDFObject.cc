#include <qpdf/QPDFObject_private.hh>

std::string
QPDFObject::getDescription()
{
    qpdf_offset_t shift = (getTypeCode() == ::ot_dictionary) ? 2
        : (getTypeCode() == ::ot_array)                      ? 1
                                                             : 0;

    if (object_description) {
        switch (object_description->index()) {
        case 0:
            {
                // Simple template string
                auto description = std::get<0>(*object_description);

                if (auto pos = description.find("$OG"); pos != std::string::npos) {
                    description.replace(pos, 3, og.unparse(' '));
                }
                if (auto pos = description.find("$PO"); pos != std::string::npos) {
                    description.replace(pos, 3, std::to_string(parsed_offset + shift));
                }
                return description;
            }
        case 1:
            {
                // QPDF::JSONReactor generated description
                auto j_descr = std::get<1>(*object_description);
                return (
                    *j_descr.input + (j_descr.object.empty() ? "" : ", " + j_descr.object) +
                    " at offset " + std::to_string(parsed_offset));
            }
        case 2:
            {
                // Child object description
                auto j_descr = std::get<2>(*object_description);
                std::string result;
                if (auto p = j_descr.parent.lock()) {
                    result = p->getDescription();
                }
                result += j_descr.static_descr;
                if (auto pos = result.find("$VD"); pos != std::string::npos) {
                    result.replace(pos, 3, j_descr.var_descr);
                }
                return result;
            }
        case 3:
            auto [stream_id, obj_id] = std::get<3>(*object_description);
            std::string result = qpdf ? qpdf->getFilename() : "";
            result += " object stream " + std::to_string(stream_id) + ", object " +
                std::to_string(obj_id) + " 0 at offset " + std::to_string(parsed_offset + shift);
            return result;
        }

    } else if (og.isIndirect()) {
        return "object " + og.unparse(' ');
    }
    return {};
}
