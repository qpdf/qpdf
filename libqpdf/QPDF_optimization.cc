// See the "Optimization" section of the manual.

#include <qpdf/QPDF_private.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFWriter_private.hh>
#include <qpdf/QTC.hh>

using Lin = QPDF::Doc::Linearization;
using Pages = QPDF::Doc::Pages;

QPDF::ObjUser::ObjUser(user_e type) :
    ou_type(type)
{
    qpdf_assert_debug(type == ou_root);
}

QPDF::ObjUser::ObjUser(user_e type, size_t pageno) :
    ou_type(type),
    pageno(pageno)
{
    qpdf_assert_debug((type == ou_page) || (type == ou_thumb));
}

QPDF::ObjUser::ObjUser(user_e type, std::string const& key) :
    ou_type(type),
    key(key)
{
    qpdf_assert_debug((type == ou_trailer_key) || (type == ou_root_key));
}

bool
QPDF::ObjUser::operator<(ObjUser const& rhs) const
{
    if (ou_type < rhs.ou_type) {
        return true;
    }
    if (ou_type == rhs.ou_type) {
        if (pageno < rhs.pageno) {
            return true;
        }
        if (pageno == rhs.pageno) {
            return key < rhs.key;
        }
    }
    return false;
}

QPDF::UpdateObjectMapsFrame::UpdateObjectMapsFrame(
    QPDF::ObjUser const& ou, QPDFObjectHandle oh, bool top) :
    ou(ou),
    oh(oh),
    top(top)
{
}

void
QPDF::optimize(
    std::map<int, int> const& object_stream_data,
    bool allow_changes,
    std::function<int(QPDFObjectHandle&)> skip_stream_parameters)
{
    m->lin.optimize_internal(object_stream_data, allow_changes, skip_stream_parameters);
}

void
Lin::optimize(
    QPDFWriter::ObjTable const& obj, std::function<int(QPDFObjectHandle&)> skip_stream_parameters)
{
    optimize_internal(obj, true, skip_stream_parameters);
}

template <typename T>
void
Lin::optimize_internal(
    T const& object_stream_data,
    bool allow_changes,
    std::function<int(QPDFObjectHandle&)> skip_stream_parameters)
{
    if (!m->obj_user_to_objects.empty()) {
        // already optimized
        return;
    }

    // The PDF specification indicates that /Outlines is supposed to be an indirect reference. Force
    // it to be so if it exists and is direct.  (This has been seen in the wild.)
    QPDFObjectHandle root = qpdf.getRoot();
    if (root.getKey("/Outlines").isDictionary()) {
        QPDFObjectHandle outlines = root.getKey("/Outlines");
        if (!outlines.isIndirect()) {
            root.replaceKey("/Outlines", qpdf.makeIndirectObject(outlines));
        }
    }

    // Traverse pages tree pushing all inherited resources down to the page level.  This also
    // initializes m->all_pages.
    m->pages.pushInheritedAttributesToPage(allow_changes, false);
    // Traverse pages

    size_t n = 0;
    for (auto const& page: m->pages.all()) {
        updateObjectMaps(ObjUser(ObjUser::ou_page, n), page, skip_stream_parameters);
        ++n;
    }

    // Traverse document-level items
    for (auto const& [key, value]: m->trailer.as_dictionary()) {
        if (key == "/Root") {
            // handled separately
        } else {
            if (!value.null()) {
                updateObjectMaps(
                    ObjUser(ObjUser::ou_trailer_key, key), value, skip_stream_parameters);
            }
        }
    }

    for (auto const& [key, value]: root.as_dictionary()) {
        // Technically, /I keys from /Thread dictionaries are supposed to be handled separately, but
        // we are going to disregard that specification for now.  There is loads of evidence that
        // pdlin and Acrobat both disregard things like this from time to time, so this is almost
        // certain not to cause any problems.
        if (!value.null()) {
            updateObjectMaps(ObjUser(ObjUser::ou_root_key, key), value, skip_stream_parameters);
        }
    }

    ObjUser root_ou = ObjUser(ObjUser::ou_root);
    auto root_og = QPDFObjGen(root.getObjGen());
    m->obj_user_to_objects[root_ou].insert(root_og);
    m->object_to_obj_users[root_og].insert(root_ou);

    filterCompressedObjects(object_stream_data);
}

void
Lin::updateObjectMaps(
    ObjUser const& first_ou,
    QPDFObjectHandle first_oh,
    std::function<int(QPDFObjectHandle&)> skip_stream_parameters)
{
    QPDFObjGen::set visited;
    std::vector<UpdateObjectMapsFrame> pending;
    pending.emplace_back(first_ou, first_oh, true);
    // Traverse the object tree from this point taking care to avoid crossing page boundaries.
    std::unique_ptr<ObjUser> thumb_ou;
    while (!pending.empty()) {
        auto cur = pending.back();
        pending.pop_back();

        bool is_page_node = false;

        if (cur.oh.isDictionaryOfType("/Page")) {
            is_page_node = true;
            if (!cur.top) {
                continue;
            }
        }

        if (cur.oh.isIndirect()) {
            QPDFObjGen og(cur.oh.getObjGen());
            if (!visited.add(og)) {
                QTC::TC("qpdf", "QPDF opt loop detected");
                continue;
            }
            m->obj_user_to_objects[cur.ou].insert(og);
            m->object_to_obj_users[og].insert(cur.ou);
        }

        if (cur.oh.isArray()) {
            for (auto const& item: cur.oh.as_array()) {
                pending.emplace_back(cur.ou, item, false);
            }
        } else if (cur.oh.isDictionary() || cur.oh.isStream()) {
            QPDFObjectHandle dict = cur.oh;
            bool is_stream = cur.oh.isStream();
            int ssp = 0;
            if (is_stream) {
                dict = cur.oh.getDict();
                if (skip_stream_parameters) {
                    ssp = skip_stream_parameters(cur.oh);
                }
            }

            for (auto& [key, value]: dict.as_dictionary()) {
                if (value.null()) {
                    continue;
                }

                if (is_page_node && (key == "/Thumb")) {
                    // Traverse page thumbnail dictionaries as a special case. There can only ever
                    // be one /Thumb key on a page, and we see at most one page node per call.
                    thumb_ou = std::make_unique<ObjUser>(ObjUser::ou_thumb, cur.ou.pageno);
                    pending.emplace_back(*thumb_ou, dict.getKey(key), false);
                } else if (is_page_node && (key == "/Parent")) {
                    // Don't traverse back up the page tree
                } else if (
                    ((ssp >= 1) && (key == "/Length")) ||
                    ((ssp >= 2) && ((key == "/Filter") || (key == "/DecodeParms")))) {
                    // Don't traverse into stream parameters that we are not going to write.
                } else {
                    pending.emplace_back(cur.ou, value, false);
                }
            }
        }
    }
}

void
Lin::filterCompressedObjects(std::map<int, int> const& object_stream_data)
{
    if (object_stream_data.empty()) {
        return;
    }

    // Transform object_to_obj_users and obj_user_to_objects so that they refer only to uncompressed
    // objects.  If something is a user of a compressed object, then it is really a user of the
    // object stream that contains it.

    std::map<ObjUser, std::set<QPDFObjGen>> t_obj_user_to_objects;
    std::map<QPDFObjGen, std::set<ObjUser>> t_object_to_obj_users;

    for (auto const& i1: m->obj_user_to_objects) {
        ObjUser const& ou = i1.first;
        // Loop over objects.
        for (auto const& og: i1.second) {
            auto i2 = object_stream_data.find(og.getObj());
            if (i2 == object_stream_data.end()) {
                t_obj_user_to_objects[ou].insert(og);
            } else {
                t_obj_user_to_objects[ou].insert(QPDFObjGen(i2->second, 0));
            }
        }
    }

    for (auto const& i1: m->object_to_obj_users) {
        QPDFObjGen const& og = i1.first;
        // Loop over obj_users.
        for (auto const& ou: i1.second) {
            auto i2 = object_stream_data.find(og.getObj());
            if (i2 == object_stream_data.end()) {
                t_object_to_obj_users[og].insert(ou);
            } else {
                t_object_to_obj_users[QPDFObjGen(i2->second, 0)].insert(ou);
            }
        }
    }

    m->obj_user_to_objects = t_obj_user_to_objects;
    m->object_to_obj_users = t_object_to_obj_users;
}

void
Lin::filterCompressedObjects(QPDFWriter::ObjTable const& obj)
{
    if (obj.getStreamsEmpty()) {
        return;
    }

    // Transform object_to_obj_users and obj_user_to_objects so that they refer only to uncompressed
    // objects.  If something is a user of a compressed object, then it is really a user of the
    // object stream that contains it.

    std::map<ObjUser, std::set<QPDFObjGen>> t_obj_user_to_objects;
    std::map<QPDFObjGen, std::set<ObjUser>> t_object_to_obj_users;

    for (auto const& i1: m->obj_user_to_objects) {
        ObjUser const& ou = i1.first;
        // Loop over objects.
        for (auto const& og: i1.second) {
            if (obj.contains(og)) {
                if (auto const& i2 = obj[og].object_stream; i2 <= 0) {
                    t_obj_user_to_objects[ou].insert(og);
                } else {
                    t_obj_user_to_objects[ou].insert(QPDFObjGen(i2, 0));
                }
            }
        }
    }

    for (auto const& i1: m->object_to_obj_users) {
        QPDFObjGen const& og = i1.first;
        if (obj.contains(og)) {
            // Loop over obj_users.
            for (auto const& ou: i1.second) {
                if (auto i2 = obj[og].object_stream; i2 <= 0) {
                    t_object_to_obj_users[og].insert(ou);
                } else {
                    t_object_to_obj_users[QPDFObjGen(i2, 0)].insert(ou);
                }
            }
        }
    }

    m->obj_user_to_objects = t_obj_user_to_objects;
    m->object_to_obj_users = t_object_to_obj_users;
}
