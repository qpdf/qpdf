#include <qpdf/Pl_MD5.hh>  // MD5 for fast hashing only, not security-sensitive
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Types & Forward Declarations
// ----------------------------------------------------------------------------

// Tracks pairs {ID1, ID2} that are currently being compared or have already
// been verified as equivalent.
//
// PROOF OF CORRECTNESS:
// 1. Cycle Detection: If we see a pair currently on the recursion stack, we
//    assume equivalence (Co-induction) to break the cycle.
// 2. Memoization: If we see a pair verified earlier in this traversal, we
//    return true immediately (Diamond problem optimization).
// 3. Safety: This cache is local to a single `are_equivalent` check. If any
//    branch fails, the entire check returns false and this cache is discarded.
//    Therefore, optimistic assumptions from cycle detection never persist
//    as "false positives" in the final deduplication map.
using EquivalenceCache = std::set<std::pair<QPDFObjGen, QPDFObjGen>>;

static bool
are_equivalent(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth);

static bool
check_dict_equivalence(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth);

static bool
check_array_equivalence(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth);

// ----------------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------------

static void
debug(std::string const& msg, std::shared_ptr<QPDFLogger> log)
{
    // Use a specific env var to avoid polluting global debug logs
    bool debug_mode = QUtil::get_env("QPDF_DEDUPE_DEBUG");
    if (debug_mode && log) {
        log->info("DEBUG: " + msg + "\n");
    }
}

static std::string
get_stream_data_hash(QPDFObjectHandle stream)
{
    if (!stream.isStream()) {
        return "";
    }

    Pl_MD5 md5("stream_hash");
    try {
        // qpdf_dl_none ensures we get the raw filtered data, ignoring specific
        // decode parameters that might change representation but not content.
        stream.pipeStreamData(&md5, nullptr, 0, qpdf_dl_none);
    } catch (std::exception const& e) {
        // Return a unique sentinel so broken streams never merge.
        return "HASH_ERROR_" + stream.getObjGen().unparse('_');
    }
    return md5.getHexDigest();
}

// Check if an XObject is a candidate for deduplication.
// We exclude objects with complex rendering or state-dependent behaviors
// where "structural equivalence" is insufficient to guarantee "visual identity."
static bool
is_safe_to_dedupe(QPDFObjectHandle obj)
{
    if (!obj.isStream()) {
        return false;
    }
    QPDFObjectHandle dict = obj.getDict();

    // 1. Must be an XObject
    if (!dict.hasKey("/Type") || !dict.getKey("/Type").isName() ||
        dict.getKey("/Type").getName() != "/XObject") {
        return false;
    }

    // 2. Exclude Transparency Groups (/Group)
    // Merging these requires matching color spaces/blending modes perfectly.
    if (dict.hasKey("/Group")) {
        return false;
    }

    // 3. Exclude Soft Masks (/SMask)
    // Context-sensitive transparency; high risk of visual corruption if merged.
    if (dict.hasKey("/SMask")) {
        return false;
    }

    // 4. Exclude Optional Content / Layers (/OC)
    // Merging layers can break visibility toggles in the viewer.
    if (dict.hasKey("/OC")) {
        return false;
    }

    // 5. Exclude Tagged PDF Structure (/StructParent, /StructParents)
    // These link content to the logical structure tree (accessibility).
    // Merging them can break the reading order or accessibility tags.
    if (dict.hasKey("/StructParent") || dict.hasKey("/StructParents")) {
        return false;
    }

    // NOTE:
    // We intentionally do NOT exclude XObjects with /Resources.
    // Structural equivalence is checked deeply, but resource *identity*
    // (e.g., fonts or color spaces that are separate but identical objects)
    // is not preserved. Some viewers/producers may treat resource identity
    // as significant, but this tradeoff is accepted in favor of deduplication.
    // The more dangerous, stateful cases (transparency, OC, structure)
    // are explicitly excluded above.

    return true;
}

// ----------------------------------------------------------------------------
// Structural equivalence (Graph Isomorphism)
// ----------------------------------------------------------------------------

static bool
are_equivalent(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth)
{
    static const int MAX_DEPTH = 500;

    // Guard against stack exhaustion on deep graphs
    if (depth >= MAX_DEPTH) {
        return false;
    }

    // 1. Topology Check
    // Merging a direct dictionary into an indirect reference changes semantics.
    if (o1.isIndirect() != o2.isIndirect()) {
        return false;
    }

    // 2. Cycle Detection & Memoization
    if (o1.isIndirect()) {
        QPDFObjGen id1 = o1.getObjGen();
        QPDFObjGen id2 = o2.getObjGen();

        if (id1 == id2) {
            return true; // Identity optimization
        }

        // Normalize pair order to ensure symmetry: (A,B) == (B,A)
        auto pair_key = std::minmax(id1, id2);

        if (cache.count(pair_key)) {
            return true; // Cycle detected OR already verified.
        }
        cache.insert(pair_key);
    }

    // 3. Type Check
    if (o1.getTypeCode() != o2.getTypeCode()) {
        return false;
    }

    // 4. Value Check
    if (o1.isBool()) {
        return o1.getBoolValue() == o2.getBoolValue();
    }
    if (o1.isInteger()) {
        return o1.getIntValue() == o2.getIntValue();
    }
    if (o1.isName()) {
        return o1.getName() == o2.getName();
    }
    if (o1.isString()) {
        return o1.getStringValue() == o2.getStringValue();
    }
    if (o1.isNull()) {
        return true;
    }

    // 5. Container Check
    if (o1.isDictionary()) {
        return check_dict_equivalence(o1, o2, cache, depth);
    }
    if (o1.isArray()) {
        return check_array_equivalence(o1, o2, cache, depth);
    }

    // 6. Stream Check
    if (o1.isStream()) {
        QPDFObjectHandle d1 = o1.getDict();
        QPDFObjectHandle d2 = o2.getDict();

        // Fail fast on mismatching Subtypes (e.g. /Image vs /Form)
        if (d1.hasKey("/Subtype") && d2.hasKey("/Subtype")) {
             if (d1.getKey("/Subtype").getName() !=
                 d2.getKey("/Subtype").getName()) {
                 return false;
             }
        } else if (d1.hasKey("/Subtype") != d2.hasKey("/Subtype")) {
            return false;
        }

        // Nested streams must verify data hash matches.
        if (get_stream_data_hash(o1) != get_stream_data_hash(o2)) {
            return false;
        }
        return check_dict_equivalence(d1, d2, cache, depth);
    }

    return o1.unparse() == o2.unparse();
}

static bool
check_dict_equivalence(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth)
{
    std::set<std::string> k1 = o1.getKeys();
    std::set<std::string> k2 = o2.getKeys();

    if (k1 != k2) {
        return false;
    }

    for (const auto& key: k1) {
        if (!are_equivalent(
                o1.getKey(key), o2.getKey(key), cache, depth + 1)) {
            return false;
        }
    }
    return true;
}

static bool
check_array_equivalence(
    QPDFObjectHandle o1,
    QPDFObjectHandle o2,
    EquivalenceCache& cache,
    int depth)
{
    int n1 = o1.getArrayNItems();
    int n2 = o2.getArrayNItems();
    if (n1 != n2) {
        return false;
    }

    for (int i = 0; i < n1; ++i) {
        if (!are_equivalent(
                o1.getArrayItem(i),
                o2.getArrayItem(i),
                cache,
                depth + 1)) {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// Reference update traversal
// ----------------------------------------------------------------------------

static void
update_references_iterative(
    std::vector<QPDFObjectHandle>& all_objects,
    const std::map<QPDFObjGen, QPDFObjectHandle>& replacements)
{
    std::set<QPDFObjGen> visited_ids;
    std::vector<QPDFObjectHandle> work_stack;

    // Initialize stack with all reachable objects
    work_stack.reserve(all_objects.size());
    for (const auto& obj: all_objects) {
        work_stack.push_back(obj);
    }

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
            std::set<std::string> keys = current.getKeys();
            for (const auto& key: keys) {
                QPDFObjectHandle val = current.getKey(key);

                if (val.isIndirect() &&
                    replacements.count(val.getObjGen())) {
                    // Update reference to the deduplicated "master" object
                    current.replaceKey(
                        key, replacements.at(val.getObjGen()));
                } else if (val.isDictionary() || val.isArray()) {
                    work_stack.push_back(val);
                }
            }
        } else if (current.isArray()) {
            int n = current.getArrayNItems();
            for (int i = 0; i < n; ++i) {
                QPDFObjectHandle val = current.getArrayItem(i);

                if (val.isIndirect() &&
                    replacements.count(val.getObjGen())) {
                    // Update reference to the deduplicated "master" object
                    current.setArrayItem(
                        i, replacements.at(val.getObjGen()));
                } else if (val.isDictionary() || val.isArray()) {
                    work_stack.push_back(val);
                }
            }
        } else if (current.isStream()) {
            // Streams in PDF cannot contain arrays/dicts outside their
            // own dictionary, so we only need to push the dict.
            work_stack.push_back(current.getDict());
        }
    }
}

// ----------------------------------------------------------------------------
// Core Deduplication Logic
// ----------------------------------------------------------------------------

static bool
deduplicate_pass(
    QPDF& pdf,
    std::vector<QPDFObjectHandle>& all_objects,
    std::map<QPDFObjGen, std::string> const& stream_data_hashes,
    std::set<QPDFObjGen>& dropped_ids,
    std::shared_ptr<QPDFLogger> log)
{
    std::map<std::string, std::vector<QPDFObjectHandle>> groups;
    std::set<QPDFObjGen> seen_in_this_pass;

    // 1. Group by content hash (Bucketing)
    for (auto& obj: all_objects) {
        if (!obj.isIndirect() || !obj.isStream()) {
            continue;
        }

        QPDFObjGen id = obj.getObjGen();
        if (seen_in_this_pass.count(id)) {
            continue;
        }

        if (dropped_ids.count(id)) {
            continue;
        }

        auto it = stream_data_hashes.find(id);
        if (it != stream_data_hashes.end()) {
            groups[it->second].push_back(obj);
            seen_in_this_pass.insert(id);
        }
    }

    std::map<QPDFObjGen, QPDFObjectHandle> replacements;
    bool any_change = false;

    // 2. Verify structural equivalence within groups
    for (auto const& group_pair: groups) {
        const auto& objs = group_pair.second;

        if (objs.size() < 2) {
            continue;
        }

        std::vector<QPDFObjectHandle> masters;

        for (const auto& candidate: objs) {
            bool merged = false;
            for (auto& master: masters) {
                // Optimization: Skip stream hashing. We know the data matches.
                // Go straight to checking the dictionaries.

                EquivalenceCache cache;
                bool match = check_dict_equivalence(
                    candidate.getDict(),
                    master.getDict(),
                    cache,
                    0);

                if (match) {
                    replacements[candidate.getObjGen()] = master;
                    dropped_ids.insert(candidate.getObjGen());
                    merged = true;
                    any_change = true;
                    break;
                }
            }
            if (!merged) {
                masters.push_back(candidate);
            }
        }
    }

    if (!any_change) {
        debug("deduplicate_pass, no change", log);
        return false;
    }

    debug(
        "Consolidated " + std::to_string(replacements.size()) +
            " duplicate streams in this pass.",
        log);

    // 3. Apply Replacements Globally
    // We add the trailer to ensure we update references in the trailer too.
    all_objects.push_back(pdf.getTrailer());
    update_references_iterative(all_objects, replacements);
    all_objects.pop_back();

    return true;
}

// ----------------------------------------------------------------------------
// Public API Implementation
// ----------------------------------------------------------------------------

// NOTE: Named deduplicateXobjects (not XObjects) to avoid issues with
// qpdf's name normalization and API binding scripts.
void
QPDF::deduplicateXobjects()
{
    std::shared_ptr<QPDFLogger> log = this->getLogger();

    if (this->getTrailer().hasKey("/Encrypt")) {
        log->warn(
            "qpdf: XObject deduplication not implemented for "
            "encrypted files; skipping.\n");
        return;
    }

    std::set<QPDFObjGen> dropped_ids;
    std::map<QPDFObjGen, std::string> stream_data_hashes;

    debug("Pre-Pass: Calculating XObject stream hashes...", log);
    std::vector<QPDFObjectHandle> all_objects = this->getAllObjects();

    unsigned int hashed_stream_count = 0;
    unsigned int stream_count = 0;

    for (auto& obj: all_objects) {
        if (obj.isStream()) {
            stream_count++;
        }

        // Use the safe_to_dedupe helper to exclude context-sensitive objects
        if (is_safe_to_dedupe(obj)) {
            QPDFObjGen id = obj.getObjGen();
            if (!stream_data_hashes.count(id)) {
                debug("Pre-pass, hashing XObject " + id.unparse(' '), log);
                stream_data_hashes[id] = get_stream_data_hash(obj);
                hashed_stream_count++;
            }
        }
    }

    debug(
        "Pre-pass, hashed " + std::to_string(hashed_stream_count) +
            " XObjects out of " + std::to_string(stream_count),
        log);

    int pass = 0;

    // Safety limit to prevent infinite loops
    // In practice, the loop runs at most twice before convergence
    const int MAX_PASSES = 10;

    while (pass < MAX_PASSES) {
        std::vector<QPDFObjectHandle> all_objects_raw =
            this->getAllObjects();
        std::vector<QPDFObjectHandle> unique_objects;
        std::set<QPDFObjGen> seen_in_pass;

        for (auto& obj: all_objects_raw) {
            QPDFObjGen id = obj.getObjGen();
            if (seen_in_pass.count(id)) {
                continue;
            }
            seen_in_pass.insert(id);
            unique_objects.push_back(obj);
        }

        if (!deduplicate_pass(
                *this,
                unique_objects,
                stream_data_hashes,
                dropped_ids,
                log)) {
            break;
        }
        pass++;
    }
}
