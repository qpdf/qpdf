#include <qpdf/ObjectHandle.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QUtil.hh>
#include <map>
#include <set>
#include <vector>

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static bool
is_image_xobject(QPDFObjectHandle obj)
{
    return obj.isStream() && obj.getDict().getKey("/Subtype").isNameAndEquals("/Image");
}

static void
update_references_iterative(
    std::vector<QPDFObjectHandle>& all_objects,
    const std::map<QPDFObjGen, QPDFObjectHandle>& replacements)
{
    std::set<QPDFObjGen> visited_ids;
    std::vector<QPDFObjectHandle> work_stack = all_objects;

    while (!work_stack.empty()) {
        QPDFObjectHandle current = work_stack.back();
        work_stack.pop_back();

        if (current.isIndirect()) {
            QPDFObjGen id = current.getObjGen();
            if (visited_ids.count(id)) {
                continue;
            }
            visited_ids.insert(id);
        }

        if (current.isDictionary()) {
            for (const auto& key: current.getKeys()) {
                QPDFObjectHandle val = current.getKey(key);
                if (val.isIndirect() && replacements.count(val.getObjGen())) {
                    current.replaceKey(key, replacements.at(val.getObjGen()));
                } else if (val.isDictionary() || val.isArray()) {
                    work_stack.push_back(val);
                }
            }
        } else if (current.isArray()) {
            for (int i = 0; i < current.getArrayNItems(); ++i) {
                QPDFObjectHandle val = current.getArrayItem(i);
                if (val.isIndirect() && replacements.count(val.getObjGen())) {
                    current.setArrayItem(i, replacements.at(val.getObjGen()));
                } else if (val.isDictionary() || val.isArray()) {
                    work_stack.push_back(val);
                }
            }
        } else if (current.isStream()) {
            work_stack.push_back(current.getDict());
        }
    }
}

// ----------------------------------------------------------------------------
// Core Deduplication
// ----------------------------------------------------------------------------

void
QPDF::deduplicateImageXobjects()
{
    std::shared_ptr<QPDFLogger> log = this->getLogger();

    if (this->getTrailer().hasKey("/Encrypt")) {
        log->warn("qpdf: Image deduplication skipped for encrypted files.\n");
        return;
    }

    struct ImageInfo
    {
        QPDFObjectHandle handle;
        double length;
    };

    std::vector<ImageInfo> image_candidates;
    for (auto& obj: this->getAllObjects()) {
        if (is_image_xobject(obj)) {
            auto len = obj.getDict().getKey("/Length").getNumericValue();
            image_candidates.push_back({obj, len});
        }
    }

    if (image_candidates.size() < 2) {
        return;
    }

    std::map<QPDFObjGen, QPDFObjectHandle> replacements;
    std::vector<ImageInfo> masters;

    for (auto& candidate: image_candidates) {
        bool found_master = false;
        for (auto const& master: masters) {
            if (candidate.length == master.length) {
                if (candidate.handle.equivalent_to(master.handle)) {
                    replacements[candidate.handle.getObjGen()] = master.handle;
                    found_master = true;
                    break;
                }
            }
        }
        if (!found_master) {
            masters.push_back(candidate);
        }
    }

    if (!replacements.empty()) {
        if (QUtil::get_env("QPDF_DEDUPE_DEBUG")) {
            log->info(
                "Deduplication: replacing " + std::to_string(replacements.size()) +
                " redundant images.\n");
        }

        std::vector<QPDFObjectHandle> all_objs = this->getAllObjects();
        all_objs.push_back(this->getTrailer());
        update_references_iterative(all_objs, replacements);
    }
}
