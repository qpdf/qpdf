#include <qpdf/assert_debug.h>

#include <qpdf/NNTree.hh>

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <bit>
#include <exception>
#include <utility>

using namespace qpdf;

static std::string
get_description(QPDFObjectHandle const& node)
{
    std::string result("Name/Number tree node");
    if (node.indirect()) {
        result += " (object " + std::to_string(node.getObjectID()) + ")";
    }
    return result;
}

void
NNTreeImpl::warn(QPDFObjectHandle const& node, std::string const& msg)
{
    qpdf.warn(qpdf_e_damaged_pdf, get_description(node), 0, msg);
    if (++error_count > 5 && qpdf.reconstructed_xref()) {
        error(node, "too many errors - giving up");
    }
}

void
NNTreeImpl::error(QPDFObjectHandle const& node, std::string const& msg) const
{
    throw QPDFExc(qpdf_e_damaged_pdf, qpdf.getFilename(), get_description(node), 0, msg);
}

NNTreeIterator::NNTreeIterator(NNTreeImpl& impl) :
    impl(impl)
{
}

void
NNTreeIterator::updateIValue(bool allow_invalid)
{
    // ivalue should never be used inside the class since we return a pointer/reference to it. Every
    // bit of code that ever changes what object the iterator points to should take care to call
    // updateIValue. Failure to do this means that any old references to *iter will point to
    // incorrect objects, though the next dereference of the iterator will fix it. This isn't
    // necessarily catastrophic, but it would be confusing. The test suite attempts to exercise
    // various cases to ensure we don't introduce that bug in the future, but sadly it's tricky to
    // verify by reasoning about the code that this constraint is always satisfied. Whenever we
    // update what the iterator points to, we should call setItemNumber, which calls this. If we
    // change what the iterator points to in some other way, such as replacing a value or removing
    // an item and making the iterator point at a different item in potentially the same position,
    // we must call updateIValue as well. These cases are handled, and for good measure, we also
    // call updateIValue in operator* and operator->.

    if (item_number < 0 || !node.isDictionary()) {
        if (!allow_invalid) {
            throw std::logic_error(
                "attempt made to dereference an invalid name/number tree iterator");
        }
        ivalue.first = QPDFObjectHandle();
        ivalue.second = QPDFObjectHandle();
        return;
    }
    Array items = node.getKey(impl.itemsKey());
    if (!std::cmp_less(item_number + 1, items.size())) {
        impl.error(node, "update ivalue: items array is too short");
    }
    ivalue.first = items[item_number];
    ivalue.second = items[1 + item_number];
}

NNTreeIterator::PathElement::PathElement(QPDFObjectHandle const& node, int kid_number) :
    node(node),
    kid_number(kid_number)
{
}

QPDFObjectHandle
NNTreeIterator::getNextKid(PathElement& pe, bool backward)
{
    while (true) {
        pe.kid_number += backward ? -1 : 1;
        Array kids = pe.node.getKey("/Kids");
        if (pe.kid_number >= 0 && std::cmp_less(pe.kid_number, kids.size())) {
            auto result = kids[pe.kid_number];
            if (result.isDictionary() &&
                (result.hasKey("/Kids") || result.hasKey(impl.itemsKey()))) {
                return result;
            } else {
                impl.warn(
                    pe.node, "skipping over invalid kid at index " + std::to_string(pe.kid_number));
            }
        } else {
            return QPDFObjectHandle::newNull();
        }
    }
}

// iterator can be incremented or decremented, or dereferenced. This does not imply that it points
// to a valid item.
bool
NNTreeIterator::valid() const
{
    return item_number >= 0;
}

void
NNTreeIterator::increment(bool backward)
{
    if (item_number < 0) {
        deepen(impl.oh, !backward, true);
        return;
    }

    while (valid()) {
        item_number += backward ? -2 : 2;
        Array items = node.getKey(impl.itemsKey());
        if (item_number < 0 || std::cmp_greater_equal(item_number, items.size())) {
            bool found = false;
            setItemNumber(QPDFObjectHandle(), -1);
            while (!(found || path.empty())) {
                auto& element = path.back();
                auto pe_node = getNextKid(element, backward);
                if (pe_node.null()) {
                    path.pop_back();
                } else {
                    found = deepen(pe_node, !backward, false);
                }
            }
        }
        if (item_number >= 0) {
            items = node.getKey(impl.itemsKey());
            if (std::cmp_greater_equal(item_number + 1, items.size())) {
                impl.warn(node, "items array doesn't have enough elements");
            } else if (!impl.keyValid(items[item_number])) {
                impl.warn(node, ("item " + std::to_string(item_number) + " has the wrong type"));
            } else if (!impl.value_valid(items[item_number + 1])) {
                impl.warn(node, "item " + std::to_string(item_number + 1) + " is invalid");
            } else {
                return;
            }
        }
    }
}

void
NNTreeIterator::resetLimits(QPDFObjectHandle a_node, std::list<PathElement>::iterator parent)
{
    while (true) {
        if (parent == path.end()) {
            a_node.removeKey("/Limits");
            break;
        }
        Array kids = a_node.getKey("/Kids");
        size_t nkids = kids.size();
        Array items = a_node.getKey(impl.itemsKey());
        size_t nitems = items.size();

        bool changed = true;
        QPDFObjectHandle first;
        QPDFObjectHandle last;
        if (nitems >= 2) {
            first = items[0];
            last = items[(nitems - 1u) & ~1u];
        } else if (nkids > 0) {
            auto first_kid = kids[0];
            auto last_kid = kids[nkids - 1u];
            if (first_kid.isDictionary() && last_kid.isDictionary()) {
                Array first_limits = first_kid.getKey("/Limits");
                Array last_limits = last_kid.getKey("/Limits");
                if (first_limits.size() >= 2 && last_limits.size() >= 2) {
                    first = first_limits[0];
                    last = last_limits[1];
                }
            }
        }
        if (first && last) {
            Array limits({first, last});
            Array olimits = a_node.getKey("/Limits");
            if (olimits.size() == 2) {
                auto ofirst = olimits[0];
                auto olast = olimits[1];
                if (impl.keyValid(ofirst) && impl.keyValid(olast) &&
                    impl.compareKeys(first, ofirst) == 0 && impl.compareKeys(last, olast) == 0) {
                    changed = false;
                }
            }
            if (changed && !a_node.isSameObjectAs(path.begin()->node)) {
                a_node.replaceKey("/Limits", limits);
            }
        } else {
            impl.warn(a_node, "unable to determine limits");
        }

        if (!changed || parent == path.begin()) {
            break;
        } else {
            a_node = parent->node;
            --parent;
        }
    }
}

void
NNTreeIterator::split(QPDFObjectHandle to_split, std::list<PathElement>::iterator parent)
{
    // Split some node along the path to the item pointed to by this iterator, and adjust the
    // iterator so it points to the same item.

    // In examples, for simplicity, /Nums is shown to just contain numbers instead of pairs. Imagine
    // this tree:
    //
    // root: << /Kids [ A B C D ] >>
    // A: << /Nums [ 1 2 3 4 ] >>
    // B: << /Nums [ 5 6 7 8 ] >>
    // C: << /Nums [ 9 10 11 12 ] >>
    // D: << /Kids [ E F ]
    // E: << /Nums [ 13 14 15 16 ] >>
    // F: << /Nums [ 17 18 19 20 ] >>

    // iter1 (points to 19)
    //   path:
    //   - { node: root: kid_number: 3 }
    //   - { node: D, kid_number: 1 }
    //   node: F
    //   item_number: 2

    // iter2 (points to 1)
    //   path:
    //   - { node: root, kid_number: 0}
    //   node: A
    //   item_number: 0

    if (!valid()) {
        throw std::logic_error("NNTreeIterator::split called an invalid iterator");
    }

    // Find the array we actually need to split, which is either this node's kids or items.
    Array kids = to_split.getKey("/Kids");
    size_t nkids = kids.size();
    Array items = to_split.getKey(impl.itemsKey());
    size_t nitems = items.size();

    Array first_half;
    size_t n = 0;
    std::string key;
    size_t threshold = static_cast<size_t>(impl.split_threshold);
    if (nkids > 0) {
        first_half = kids;
        n = nkids;
        key = "/Kids";
    } else if (nitems > 0) {
        first_half = items;
        n = nitems;
        threshold *= 2;
        key = impl.itemsKey();
    } else {
        throw std::logic_error("NNTreeIterator::split called on invalid node");
    }

    if (n <= threshold) {
        return;
    }

    bool is_root = parent == path.end();
    bool is_leaf = nitems > 0;

    // CURRENT STATE: tree is in original state; iterator is valid and unchanged.

    if (is_root) {
        // What we want to do is to create a new node for the second half of the items and put it in
        // the parent's /Kids array right after the element that points to the current to_split
        // node, but if we're splitting root, there is no parent, so handle that first.

        // In the non-root case, parent points to the path element whose /Kids contains the first
        // half node, and the first half node is to_split. If we are splitting the root, we need to
        // push everything down a level, but we want to keep the actual root object the same so that
        // indirect references to it remain intact (and also in case it might be a direct object,
        // which it shouldn't be but that case probably exists in the wild). To achieve this, we
        // create a new node for the first half and then replace /Kids in the root to contain it.
        // Then we adjust the path so that the first element is root and the second element, if any,
        // is the new first half. In this way, we make the root case identical to the non-root case
        // so remaining logic can handle them in the same way.

        auto first_node = impl.qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        first_node.replaceKey(key, first_half);
        Array new_kids;
        new_kids.push_back(first_node);
        to_split.removeKey("/Limits"); // already shouldn't be there for root
        to_split.removeKey(impl.itemsKey());
        to_split.replaceKey("/Kids", new_kids);
        if (is_leaf) {
            node = first_node;
        } else {
            auto next = path.begin();
            next->node = first_node;
        }
        this->path.emplace_front(to_split, 0);
        parent = path.begin();
        to_split = first_node;
    }

    // CURRENT STATE: parent is guaranteed to be defined, and we have the invariants that
    // parent[/Kids][kid_number] == to_split and (++parent).node == to_split.

    // Create a second half array, and transfer the second half of the items into the second half
    // array.
    Array second_half;
    auto start_idx = static_cast<int>((n / 2) & ~1u);
    while (std::cmp_greater(first_half.size(), start_idx)) {
        second_half.push_back(first_half[start_idx]);
        first_half.erase(start_idx);
    }
    resetLimits(to_split, parent);

    // Create a new node to contain the second half
    QPDFObjectHandle second_node = impl.qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    second_node.replaceKey(key, second_half);
    resetLimits(second_node, parent);

    // CURRENT STATE: half the items from the kids or items array in the node being split have been
    // moved into a new node. The new node is not yet attached to the tree. The iterator may have a
    // path element or leaf node that is out of bounds.

    // We need to adjust the parent to add the second node to /Kids and, if needed, update
    // kid_number to traverse through it. We need to update to_split's path element, or the node if
    // this is a leaf, so that the kid/item number points to the right place.

    Array parent_kids = parent->node.getKey("/Kids");
    parent_kids.insert(parent->kid_number + 1, second_node);
    auto cur_elem = parent;
    ++cur_elem; // points to end() for leaf nodes
    int old_idx = (is_leaf ? item_number : cur_elem->kid_number);
    if (old_idx >= start_idx) {
        ++parent->kid_number;
        if (is_leaf) {
            setItemNumber(second_node, item_number - start_idx);
        } else {
            cur_elem->node = second_node;
            cur_elem->kid_number -= start_idx;
        }
    }
    if (!is_root) {
        auto next = parent->node;
        resetLimits(next, parent);
        --parent;
        split(next, parent);
    }
}

std::list<NNTreeIterator::PathElement>::iterator
NNTreeIterator::lastPathElement()
{
    auto result = path.end();
    if (!path.empty()) {
        --result;
    }
    return result;
}

void
NNTreeIterator::insertAfter(QPDFObjectHandle const& key, QPDFObjectHandle const& value)
{
    if (!valid()) {
        impl.insertFirst(key, value);
        deepen(impl.oh, true, false);
        return;
    }

    Array items = node.getKey(impl.itemsKey());
    if (!items) {
        impl.error(node, "node contains no items array");
    }

    if (std::cmp_less(items.size(), item_number + 2)) {
        impl.error(node, "insert: items array is too short");
    }
    if (!(key && value)) {
        impl.error(node, "insert: key or value is null");
    }
    if (!impl.value_valid(value)) {
        impl.error(node, "insert: value is invalid");
    }
    items.insert(item_number + 2, key);
    items.insert(item_number + 3, value);
    resetLimits(node, lastPathElement());
    split(node, lastPathElement());
    increment(false);
}

void
NNTreeIterator::remove()
{
    // Remove this item, leaving the tree valid and this iterator pointing to the next item.

    if (!valid()) {
        throw std::logic_error("attempt made to remove an invalid iterator");
    }
    Array items = node.getKey(impl.itemsKey());
    int nitems = static_cast<int>(items.size());
    if (std::cmp_greater(item_number + 2, nitems)) {
        impl.error(node, "found short items array while removing an item");
    }

    items.erase(item_number);
    items.erase(item_number);
    nitems -= 2;

    if (nitems > 0) {
        // There are still items left

        if (item_number == 0 || item_number == nitems) {
            // We removed either the first or last item of an items array that remains non-empty, so
            // we have to adjust limits.
            resetLimits(node, lastPathElement());
        }

        if (item_number == nitems) {
            // We removed the last item of a non-empty items array, so advance to the successor of
            // the previous item.
            item_number -= 2;
            increment(false);
        } else if (item_number < nitems) {
            // We don't have to do anything since the removed item's successor now occupies its
            // former location.
            updateIValue();
        } else {
            // We already checked to ensure this condition would not happen.
            throw std::logic_error("NNTreeIterator::remove: item_number > nitems after erase");
        }
        return;
    }

    if (path.empty()) {
        // Special case: if this is the root node, we can leave it empty.
        setItemNumber(impl.oh, -1);
        return;
    }

    // We removed the last item from this items array, so we need to remove this node from the
    // parent on up the tree. Then we need to position ourselves at the removed item's successor.
    while (true) {
        auto element = lastPathElement();
        auto parent = element;
        --parent;
        Array kids = element->node.getKey("/Kids");
        kids.erase(element->kid_number);
        auto nkids = kids.size();
        if (nkids > 0) {
            // The logic here is similar to the items case.
            if (element->kid_number == 0 || std::cmp_equal(element->kid_number, nkids)) {
                resetLimits(element->node, parent);
            }
            if (std::cmp_equal(element->kid_number, nkids)) {
                // Move to the successor of the last child of the previous kid.
                setItemNumber({}, -1);
                --element->kid_number;
                deepen(kids[element->kid_number], false, true);
                if (valid()) {
                    increment(false);
                    QTC::TC("qpdf", "NNTree erased last kid/item in tree", valid() ? 0 : 1);
                }
            } else {
                // Next kid is in deleted kid's position
                deepen(kids.get(element->kid_number), true, true);
            }
            return;
        }

        if (parent == path.end()) {
            // We erased the very last item. Convert the root to an empty items array.
            element->node.removeKey("/Kids");
            element->node.replaceKey(impl.itemsKey(), Array());
            path.clear();
            setItemNumber(impl.oh, -1);
            return;
        }

        // Walk up the tree and continue
        path.pop_back();
    }
}

NNTreeIterator&
NNTreeIterator::operator++()
{
    increment(false);
    return *this;
}

NNTreeIterator&
NNTreeIterator::operator--()
{
    increment(true);
    return *this;
}

NNTreeIterator::reference
NNTreeIterator::operator*()
{
    updateIValue(false);
    return ivalue;
}

NNTreeIterator::pointer
NNTreeIterator::operator->()
{
    updateIValue(false);
    return &ivalue;
}

bool
NNTreeIterator::operator==(NNTreeIterator const& other) const
{
    if (item_number == -1 && other.item_number == -1) {
        return true;
    }
    if (path.size() != other.path.size()) {
        return false;
    }
    auto tpi = path.begin();
    auto opi = other.path.begin();
    while (tpi != path.end()) {
        if (tpi->kid_number != opi->kid_number) {
            return false;
        }
        ++tpi;
        ++opi;
    }
    return item_number == other.item_number;
}

void
NNTreeIterator::setItemNumber(QPDFObjectHandle const& a_node, int n)
{
    node = a_node;
    item_number = n;
    updateIValue();
}

void
NNTreeIterator::addPathElement(QPDFObjectHandle const& a_node, int kid_number)
{
    path.emplace_back(a_node, kid_number);
}

bool
NNTreeIterator::deepen(QPDFObjectHandle a_node, bool first, bool allow_empty)
{
    // Starting at this node, descend through the first or last kid until we reach a node with
    // items. If we succeed, return true; otherwise return false and leave path alone.

    auto opath = path;

    auto fail = [this, &opath](QPDFObjectHandle const& failed_node, std::string const& msg) {
        impl.warn(failed_node, msg);
        path = opath;
        return false;
    };

    QPDFObjGen::set seen;
    for (auto const& i: path) {
        seen.add(i.node);
    }
    while (true) {
        if (!seen.add(a_node)) {
            return fail(a_node, "loop detected while traversing name/number tree");
        }

        if (!a_node.isDictionary()) {
            return fail(a_node, "non-dictionary node while traversing name/number tree");
        }

        Array items = a_node.getKey(impl.itemsKey());
        int nitems = static_cast<int>(items.size());
        if (nitems > 1) {
            setItemNumber(a_node, first ? 0 : nitems - 2);
            break;
        }

        Array kids = a_node.getKey("/Kids");
        int nkids = static_cast<int>(kids.size());
        if (nkids > 0) {
            int kid_number = first ? 0 : nkids - 1;
            addPathElement(a_node, kid_number);
            auto next = kids[kid_number];
            if (!next) {
                return fail(a_node, "kid number " + std::to_string(kid_number) + " is invalid");
            }
            if (!next.indirect()) {
                if (impl.auto_repair) {
                    impl.warn(
                        a_node,
                        "converting kid number " + std::to_string(kid_number) +
                            " to an indirect object");
                    next = impl.qpdf.makeIndirectObject(next);
                    kids.set(kid_number, next);
                } else {
                    impl.warn(
                        a_node,
                        "kid number " + std::to_string(kid_number) + " is not an indirect object");
                }
            }
            a_node = next;
        } else if (allow_empty && items) {
            setItemNumber(a_node, -1);
            break;
        } else {
            return fail(
                a_node,
                "name/number tree node has neither non-empty " + impl.itemsKey() + " nor /Kids");
        }
    }
    return true;
}

NNTreeImpl::NNTreeImpl(
    QPDF& qpdf,
    QPDFObjectHandle& oh,
    qpdf_object_type_e key_type,
    std::function<bool(QPDFObjectHandle const&)> value_validator,
    bool auto_repair) :
    qpdf(qpdf),
    oh(oh),
    key_type(key_type),
    items_key(key_type == ::ot_string ? "/Names" : "/Nums"),
    value_valid(value_validator),
    auto_repair(auto_repair)
{
}

void
NNTreeImpl::setSplitThreshold(int threshold)
{
    split_threshold = threshold;
}

NNTreeImpl::iterator
NNTreeImpl::begin()
{
    iterator result(*this);
    result.deepen(oh, true, true);
    return result;
}

NNTreeImpl::iterator
NNTreeImpl::end()
{
    return {*this};
}

NNTreeImpl::iterator
NNTreeImpl::last()
{
    iterator result(*this);
    result.deepen(oh, false, true);
    return result;
}

int
NNTreeImpl::compareKeys(QPDFObjectHandle a, QPDFObjectHandle b) const
{
    // We don't call this without calling keyValid first
    qpdf_assert_debug(keyValid(a));
    qpdf_assert_debug(keyValid(b));
    if (key_type == ::ot_string) {
        auto as = a.getUTF8Value();
        auto bs = b.getUTF8Value();
        return as < bs ? -1 : (as > bs ? 1 : 0);
    }
    auto as = a.getIntValue();
    auto bs = b.getIntValue();
    return as < bs ? -1 : (as > bs ? 1 : 0);
}

int
NNTreeImpl::binarySearch(
    QPDFObjectHandle const& key,
    Array const& items,
    size_t num_items,
    bool return_prev_if_not_found,
    int (NNTreeImpl::*compare)(QPDFObjectHandle const& key, Array const& arr, int item) const) const
{
    size_t max_idx = std::bit_ceil(num_items);

    int step = static_cast<int>(max_idx / 2);
    size_t checks = max_idx;
    int idx = step;
    int found_idx = -1;

    while (checks > 0) {
        int status = -1;
        if (std::cmp_less(idx, num_items)) {
            status = (this->*compare)(key, items, idx);
            if (status == 0) {
                return idx;
            }
            if (status > 0) {
                found_idx = idx;
            }
        }
        checks >>= 1;
        if (checks > 0) {
            step >>= 1;
            if (step == 0) {
                step = 1;
            }

            if (status < 0) {
                idx -= step;
            } else {
                idx += step;
            }
        }
    }

    return return_prev_if_not_found ? found_idx : -1;
}

int
NNTreeImpl::compareKeyItem(QPDFObjectHandle const& key, Array const& items, int idx) const
{
    if (!keyValid(items[2 * idx])) {
        error(oh, ("item at index " + std::to_string(2 * idx) + " is not the right type"));
    }
    return compareKeys(key, items[2 * idx]);
}

int
NNTreeImpl::compareKeyKid(QPDFObjectHandle const& key, Array const& kids, int idx) const
{
    if (!kids[idx].isDictionary()) {
        error(oh, "invalid kid at index " + std::to_string(idx));
    }
    Array limits = kids[idx].getKey("/Limits");
    if (!(keyValid(limits[0]) && keyValid(limits[1]))) {
        error(kids[idx], "node is missing /Limits");
    }
    if (compareKeys(key, limits[0]) < 0) {
        return -1;
    }
    if (compareKeys(key, limits[1]) > 0) {
        return 1;
    }
    return 0;
}

void
NNTreeImpl::repair()
{
    auto new_node = QPDFObjectHandle::newDictionary();
    new_node.replaceKey(itemsKey(), Array());
    NNTreeImpl repl(qpdf, new_node, key_type, value_valid, false);
    for (auto const& [key, value]: *this) {
        if (key && value) {
            repl.insert(key, value);
        }
    }
    oh.replaceKey("/Kids", new_node.getKey("/Kids"));
    oh.replaceKey(itemsKey(), new_node.getKey(itemsKey()));
}

NNTreeImpl::iterator
NNTreeImpl::find(QPDFObjectHandle key, bool return_prev_if_not_found)
{
    try {
        return findInternal(key, return_prev_if_not_found);
    } catch (QPDFExc& e) {
        if (auto_repair) {
            warn(oh, std::string("attempting to repair after error: ") + e.what());
            repair();
            return findInternal(key, return_prev_if_not_found);
        } else {
            throw;
        }
    }
}

NNTreeImpl::iterator
NNTreeImpl::findInternal(QPDFObjectHandle const& key, bool return_prev_if_not_found)
{
    auto first_item = begin();
    auto last_item = end();
    if (first_item == end()) {
        return end();
    }
    if (first_item.valid()) {
        if (!keyValid(first_item->first)) {
            error(oh, "encountered invalid key in find");
        }
        if (!value_valid(first_item->second)) {
            error(oh, "encountered invalid value in find");
        }
        if (compareKeys(key, first_item->first) < 0) {
            // Before the first key
            return end();
        }
    }
    qpdf_assert_debug(!last_item.valid());

    QPDFObjGen::set seen;
    auto node = oh;
    iterator result(*this);

    while (true) {
        if (!seen.add(node)) {
            error(node, "loop detected in find");
        }

        Array items = node.getKey(itemsKey());
        size_t nitems = items.size();
        if (nitems > 1) {
            int idx = binarySearch(
                key, items, nitems / 2, return_prev_if_not_found, &NNTreeImpl::compareKeyItem);
            if (idx >= 0) {
                result.setItemNumber(node, 2 * idx);
                if (!result.impl.keyValid(result.ivalue.first)) {
                    error(node, "encountered invalid key in find");
                }
                if (!result.impl.value_valid(result.ivalue.second)) {
                    error(oh, "encountered invalid value in find");
                }
            }
            return result;
        }

        Array kids = node.getKey("/Kids");
        size_t nkids = kids.size();
        if (nkids > 0) {
            int idx = binarySearch(key, kids, nkids, true, &NNTreeImpl::compareKeyKid);
            if (idx == -1) {
                error(node, "unexpected -1 from binary search of kids; limits may by wrong");
            }
            result.addPathElement(node, idx);
            node = kids[idx];
        } else {
            error(node, "bad node during find");
        }
    }
}

NNTreeImpl::iterator
NNTreeImpl::insertFirst(QPDFObjectHandle const& key, QPDFObjectHandle const& value)
{
    auto iter = begin();
    Array items(nullptr);
    if (iter.node.isDictionary()) {
        items = iter.node.getKey(itemsKey());
    }
    if (!items) {
        error(oh, "unable to find a valid items node");
    }
    if (!(key && value)) {
        error(oh, "unable to insert null key or value");
    }
    if (!value_valid(value)) {
        error(oh, "attempting to insert an invalid value");
    }
    items.insert(0, key);
    items.insert(1, value);
    iter.setItemNumber(iter.node, 0);
    iter.resetLimits(iter.node, iter.lastPathElement());
    iter.split(iter.node, iter.lastPathElement());
    return iter;
}

NNTreeImpl::iterator
NNTreeImpl::insert(QPDFObjectHandle const& key, QPDFObjectHandle const& value)
{
    auto iter = find(key, true);
    if (!iter.valid()) {
        return insertFirst(key, value);
    } else if (compareKeys(key, iter->first) == 0) {
        Array items = iter.node.getKey(itemsKey());
        items.set(iter.item_number + 1, value);
        iter.updateIValue();
    } else {
        iter.insertAfter(key, value);
    }
    return iter;
}

bool
NNTreeImpl::remove(QPDFObjectHandle const& key, QPDFObjectHandle* value)
{
    auto iter = find(key, false);
    if (!iter.valid()) {
        return false;
    }
    if (value) {
        *value = iter->second;
    }
    iter.remove();
    return true;
}

bool
NNTreeImpl::validate(bool a_repair)
{
    bool first = true;
    QPDFObjectHandle last_key;
    try {
        for (auto const& [key, value]: *this) {
            if (!keyValid(key)) {
                error(oh, "invalid key in validate");
            }
            if (!value_valid(value)) {
                error(oh, "invalid value in validate");
            }
            if (first) {
                first = false;
            } else if (last_key && compareKeys(last_key, key) != -1) {
                error(oh, "keys are not sorted in validate");
            }
            last_key = key;
        }
    } catch (QPDFExc& e) {
        if (a_repair) {
            warn(oh, std::string("attempting to repair after error: ") + e.what());
            repair();
        }
        return false;
    }
    return true;
}
