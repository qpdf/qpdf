#include <qpdf/assert_debug.h>

#include <qpdf/NNTree.hh>

#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDF_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <bit>
#include <exception>
#include <utility>

static std::string
get_description(QPDFObjectHandle& node)
{
    std::string result("Name/Number tree node");
    if (node.isIndirect()) {
        result += " (object " + std::to_string(node.getObjectID()) + ")";
    }
    return result;
}

void
NNTreeImpl::warn(QPDFObjectHandle& node, std::string const& msg)
{
    qpdf.warn(qpdf_e_damaged_pdf, get_description(node), 0, msg);
    if (++error_count > 5 && qpdf.reconstructed_xref()) {
        error(node, "too many errors - giving up");
    }
}

void
NNTreeImpl::error(QPDFObjectHandle& node, std::string const& msg)
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

    bool okay = false;
    if (item_number >= 0 && node.isDictionary()) {
        auto items = node.getKey(impl.details.itemsKey());
        if (item_number + 1 < items.getArrayNItems()) {
            okay = true;
            ivalue.first = items.getArrayItem(item_number);
            ivalue.second = items.getArrayItem(1 + item_number);
        } else {
            impl.error(node, "update ivalue: items array is too short");
        }
    }
    if (!okay) {
        if (!allow_invalid) {
            throw std::logic_error(
                "attempt made to dereference an invalid name/number tree iterator");
        }
        ivalue.first = QPDFObjectHandle();
        ivalue.second = QPDFObjectHandle();
    }
}

NNTreeIterator::PathElement::PathElement(QPDFObjectHandle const& node, int kid_number) :
    node(node),
    kid_number(kid_number)
{
}

QPDFObjectHandle
NNTreeIterator::getNextKid(PathElement& pe, bool backward)
{
    QPDFObjectHandle result;
    bool found = false;
    while (!found) {
        pe.kid_number += backward ? -1 : 1;
        auto kids = pe.node.getKey("/Kids");
        if ((pe.kid_number >= 0) && (pe.kid_number < kids.getArrayNItems())) {
            result = kids.getArrayItem(pe.kid_number);
            if (result.isDictionary() &&
                (result.hasKey("/Kids") || result.hasKey(impl.details.itemsKey()))) {
                found = true;
            } else {
                QTC::TC("qpdf", "NNTree skip invalid kid");
                impl.warn(
                    pe.node,
                    ("skipping over invalid kid at index " + std::to_string(pe.kid_number)));
            }
        } else {
            result = QPDFObjectHandle::newNull();
            found = true;
        }
    }
    return result;
}

bool
NNTreeIterator::valid() const
{
    return item_number >= 0;
}

void
NNTreeIterator::increment(bool backward)
{
    if (item_number < 0) {
        QTC::TC("qpdf", "NNTree increment end()");
        deepen(impl.oh, !backward, true);
        return;
    }
    bool found_valid_key = false;
    while (valid() && !found_valid_key) {
        item_number += backward ? -2 : 2;
        auto items = node.getKey(impl.details.itemsKey());
        if (item_number < 0 || item_number >= items.getArrayNItems()) {
            bool found = false;
            setItemNumber(QPDFObjectHandle(), -1);
            while (!(found || path.empty())) {
                auto& element = path.back();
                auto pe_node = getNextKid(element, backward);
                if (pe_node.isNull()) {
                    path.pop_back();
                } else {
                    found = deepen(pe_node, !backward, false);
                }
            }
        }
        if (item_number >= 0) {
            items = node.getKey(impl.details.itemsKey());
            if (item_number + 1 >= items.getArrayNItems()) {
                QTC::TC("qpdf", "NNTree skip item at end of short items");
                impl.warn(node, "items array doesn't have enough elements");
            } else if (!impl.details.keyValid(items.getArrayItem(item_number))) {
                QTC::TC("qpdf", "NNTree skip invalid key");
                impl.warn(node, ("item " + std::to_string(item_number) + " has the wrong type"));
            } else {
                found_valid_key = true;
            }
        }
    }
}

void
NNTreeIterator::resetLimits(QPDFObjectHandle a_node, std::list<PathElement>::iterator parent)
{
    bool done = false;
    while (!done) {
        if (parent == path.end()) {
            QTC::TC("qpdf", "NNTree remove limits from root");
            a_node.removeKey("/Limits");
            done = true;
            break;
        }
        auto kids = a_node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = a_node.getKey(impl.details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;

        bool changed = true;
        QPDFObjectHandle first;
        QPDFObjectHandle last;
        if (nitems >= 2) {
            first = items.getArrayItem(0);
            last = items.getArrayItem((nitems - 1) & ~1);
        } else if (nkids > 0) {
            auto first_kid = kids.getArrayItem(0);
            auto last_kid = kids.getArrayItem(nkids - 1);
            if (first_kid.isDictionary() && last_kid.isDictionary()) {
                auto first_limits = first_kid.getKey("/Limits");
                auto last_limits = last_kid.getKey("/Limits");
                if (first_limits.isArray() && (first_limits.getArrayNItems() >= 2) &&
                    last_limits.isArray() && (last_limits.getArrayNItems() >= 2)) {
                    first = first_limits.getArrayItem(0);
                    last = last_limits.getArrayItem(1);
                }
            }
        }
        if (first && last) {
            auto limits = QPDFObjectHandle::newArray();
            limits.appendItem(first);
            limits.appendItem(last);
            auto olimits = a_node.getKey("/Limits");
            if (olimits.isArray() && (olimits.getArrayNItems() == 2)) {
                auto ofirst = olimits.getArrayItem(0);
                auto olast = olimits.getArrayItem(1);
                if (impl.details.keyValid(ofirst) && impl.details.keyValid(olast) &&
                    (impl.details.compareKeys(first, ofirst) == 0) &&
                    (impl.details.compareKeys(last, olast) == 0)) {
                    QTC::TC("qpdf", "NNTree limits didn't change");
                    changed = false;
                }
            }
            if (changed && !a_node.isSameObjectAs(path.begin()->node)) {
                a_node.replaceKey("/Limits", limits);
            }
        } else {
            QTC::TC("qpdf", "NNTree unable to determine limits");
            impl.warn(a_node, "unable to determine limits");
        }

        if (!changed || parent == path.begin()) {
            done = true;
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
    auto kids = to_split.getKey("/Kids");
    int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
    auto items = to_split.getKey(impl.details.itemsKey());
    int nitems = items.isArray() ? items.getArrayNItems() : 0;

    QPDFObjectHandle first_half;
    int n = 0;
    std::string key;
    int threshold = 0;
    if (nkids > 0) {
        QTC::TC("qpdf", "NNTree split kids");
        first_half = kids;
        n = nkids;
        threshold = impl.split_threshold;
        key = "/Kids";
    } else if (nitems > 0) {
        QTC::TC("qpdf", "NNTree split items");
        first_half = items;
        n = nitems;
        threshold = 2 * impl.split_threshold;
        key = impl.details.itemsKey();
    } else {
        throw std::logic_error("NNTreeIterator::split called on invalid node");
    }

    if (n <= threshold) {
        return;
    }

    bool is_root = (parent == path.end());
    bool is_leaf = (nitems > 0);

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
        QPDFObjectHandle new_kids = QPDFObjectHandle::newArray();
        new_kids.appendItem(first_node);
        to_split.removeKey("/Limits"); // already shouldn't be there for root
        to_split.removeKey(impl.details.itemsKey());
        to_split.replaceKey("/Kids", new_kids);
        if (is_leaf) {
            QTC::TC("qpdf", "NNTree split root + leaf");
            node = first_node;
        } else {
            QTC::TC("qpdf", "NNTree split root + !leaf");
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
    QPDFObjectHandle second_half = QPDFObjectHandle::newArray();
    int start_idx = ((n / 2) & ~1);
    while (first_half.getArrayNItems() > start_idx) {
        second_half.appendItem(first_half.getArrayItem(start_idx));
        first_half.eraseItem(start_idx);
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

    auto parent_kids = parent->node.getKey("/Kids");
    parent_kids.insertItem(parent->kid_number + 1, second_node);
    auto cur_elem = parent;
    ++cur_elem; // points to end() for leaf nodes
    int old_idx = (is_leaf ? item_number : cur_elem->kid_number);
    if (old_idx >= start_idx) {
        ++parent->kid_number;
        if (is_leaf) {
            QTC::TC("qpdf", "NNTree split second half item");
            setItemNumber(second_node, item_number - start_idx);
        } else {
            QTC::TC("qpdf", "NNTree split second half kid");
            cur_elem->node = second_node;
            cur_elem->kid_number -= start_idx;
        }
    }
    if (!is_root) {
        QTC::TC("qpdf", "NNTree split parent");
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
NNTreeIterator::insertAfter(QPDFObjectHandle key, QPDFObjectHandle value)
{
    if (!valid()) {
        QTC::TC("qpdf", "NNTree insertAfter inserts first");
        impl.insertFirst(key, value);
        deepen(impl.oh, true, false);
        return;
    }

    auto items = node.getKey(impl.details.itemsKey());
    if (!items.isArray()) {
        impl.error(node, "node contains no items array");
    }
    if (items.getArrayNItems() < item_number + 2) {
        impl.error(node, "insert: items array is too short");
    }
    items.insertItem(item_number + 2, key);
    items.insertItem(item_number + 3, value);
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
    auto items = node.getKey(impl.details.itemsKey());
    int nitems = items.getArrayNItems();
    if (item_number + 2 > nitems) {
        impl.error(node, "found short items array while removing an item");
    }

    items.eraseItem(item_number);
    items.eraseItem(item_number);
    nitems -= 2;

    if (nitems > 0) {
        // There are still items left

        if (item_number == 0 || item_number == nitems) {
            // We removed either the first or last item of an items array that remains non-empty, so
            // we have to adjust limits.
            QTC::TC("qpdf", "NNTree remove reset limits");
            resetLimits(node, lastPathElement());
        }

        if (item_number == nitems) {
            // We removed the last item of a non-empty items array, so advance to the successor of
            // the previous item.
            QTC::TC("qpdf", "NNTree erased last item");
            item_number -= 2;
            increment(false);
        } else if (item_number < nitems) {
            // We don't have to do anything since the removed item's successor now occupies its
            // former location.
            QTC::TC("qpdf", "NNTree erased non-last item");
            updateIValue();
        } else {
            // We already checked to ensure this condition would not happen.
            throw std::logic_error("NNTreeIterator::remove: item_number > nitems after erase");
        }
        return;
    }

    if (path.empty()) {
        // Special case: if this is the root node, we can leave it empty.
        QTC::TC("qpdf", "NNTree erased all items on leaf/root");
        setItemNumber(impl.oh, -1);
        return;
    }

    QTC::TC("qpdf", "NNTree items is empty after remove");

    // We removed the last item from this items array, so we need to remove this node from the
    // parent on up the tree. Then we need to position ourselves at the removed item's successor.
    bool done = false;
    while (!done) {
        auto element = lastPathElement();
        auto parent = element;
        --parent;
        auto kids = element->node.getKey("/Kids");
        kids.eraseItem(element->kid_number);
        auto nkids = kids.getArrayNItems();
        if (nkids > 0) {
            // The logic here is similar to the items case.
            if ((element->kid_number == 0) || (element->kid_number == nkids)) {
                QTC::TC("qpdf", "NNTree erased first or last kid");
                resetLimits(element->node, parent);
            }
            if (element->kid_number == nkids) {
                // Move to the successor of the last child of the previous kid.
                setItemNumber(QPDFObjectHandle(), -1);
                --element->kid_number;
                deepen(kids.getArrayItem(element->kid_number), false, true);
                if (valid()) {
                    increment(false);
                    if (!valid()) {
                        QTC::TC("qpdf", "NNTree erased last item in tree");
                    } else {
                        QTC::TC("qpdf", "NNTree erased last kid");
                    }
                }
            } else {
                // Next kid is in deleted kid's position
                QTC::TC("qpdf", "NNTree erased non-last kid");
                deepen(kids.getArrayItem(element->kid_number), true, true);
            }
            done = true;
        } else if (parent == path.end()) {
            // We erased the very last item. Convert the root to an empty items array.
            QTC::TC("qpdf", "NNTree non-flat tree is empty after remove");
            element->node.removeKey("/Kids");
            element->node.replaceKey(impl.details.itemsKey(), QPDFObjectHandle::newArray());
            path.clear();
            setItemNumber(impl.oh, -1);
            done = true;
        } else {
            // Walk up the tree and continue
            QTC::TC("qpdf", "NNTree remove walking up tree");
            path.pop_back();
        }
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
    if (item_number != other.item_number) {
        return false;
    }
    return true;
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
    bool failed = false;

    QPDFObjGen::set seen;
    for (auto const& i: path) {
        seen.add(i.node);
    }
    while (!failed) {
        if (!seen.add(a_node)) {
            QTC::TC("qpdf", "NNTree deepen: loop");
            impl.warn(a_node, "loop detected while traversing name/number tree");
            failed = true;
            break;
        }

        if (!a_node.isDictionary()) {
            QTC::TC("qpdf", "NNTree node is not a dictionary");
            impl.warn(a_node, "non-dictionary node while traversing name/number tree");
            failed = true;
            break;
        }

        auto kids = a_node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = a_node.getKey(impl.details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;
        if (nitems > 0) {
            setItemNumber(a_node, first ? 0 : nitems - 2);
            break;
        } else if (nkids > 0) {
            int kid_number = first ? 0 : nkids - 1;
            addPathElement(a_node, kid_number);
            auto next = kids.getArrayItem(kid_number);
            if (!next.isIndirect()) {
                if (impl.auto_repair) {
                    QTC::TC("qpdf", "NNTree fix indirect kid");
                    impl.warn(
                        a_node,
                        ("converting kid number " + std::to_string(kid_number) +
                         " to an indirect object"));
                    next = impl.qpdf.makeIndirectObject(next);
                    kids.setArrayItem(kid_number, next);
                } else {
                    QTC::TC("qpdf", "NNTree warn indirect kid");
                    impl.warn(
                        a_node,
                        ("kid number " + std::to_string(kid_number) +
                         " is not an indirect object"));
                }
            }
            a_node = next;
        } else if (allow_empty && items.isArray()) {
            QTC::TC("qpdf", "NNTree deepen found empty");
            setItemNumber(a_node, -1);
            break;
        } else {
            QTC::TC("qpdf", "NNTree deepen: invalid node");
            impl.warn(
                a_node,
                ("name/number tree node has neither non-empty " + impl.details.itemsKey() +
                 " nor /Kids"));
            failed = true;
            break;
        }
    }
    if (failed) {
        path = opath;
        return false;
    }
    return true;
}

NNTreeImpl::NNTreeImpl(
    NNTreeDetails const& details, QPDF& qpdf, QPDFObjectHandle& oh, bool auto_repair) :
    details(details),
    qpdf(qpdf),
    oh(oh),
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
NNTreeImpl::withinLimits(QPDFObjectHandle key, QPDFObjectHandle node)
{
    int result = 0;
    auto limits = node.getKey("/Limits");
    if (limits.isArray() && (limits.getArrayNItems() >= 2) &&
        details.keyValid(limits.getArrayItem(0)) && details.keyValid(limits.getArrayItem(1))) {
        if (details.compareKeys(key, limits.getArrayItem(0)) < 0) {
            result = -1;
        } else if (details.compareKeys(key, limits.getArrayItem(1)) > 0) {
            result = 1;
        }
    } else {
        QTC::TC("qpdf", "NNTree missing limits");
        error(node, "node is missing /Limits");
    }
    return result;
}

int
NNTreeImpl::binarySearch(
    QPDFObjectHandle key,
    QPDFObjectHandle items,
    size_t num_items,
    bool return_prev_if_not_found,
    int (NNTreeImpl::*compare)(QPDFObjectHandle& key, QPDFObjectHandle& arr, int item))
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
NNTreeImpl::compareKeyItem(QPDFObjectHandle& key, QPDFObjectHandle& items, int idx)
{
    if (!((items.isArray() && (items.getArrayNItems() > (2 * idx)) &&
           details.keyValid(items.getArrayItem(2 * idx))))) {
        QTC::TC("qpdf", "NNTree item is wrong type");
        error(oh, ("item at index " + std::to_string(2 * idx) + " is not the right type"));
    }
    return details.compareKeys(key, items.getArrayItem(2 * idx));
}

int
NNTreeImpl::compareKeyKid(QPDFObjectHandle& key, QPDFObjectHandle& kids, int idx)
{
    if (!(kids.isArray() && (idx < kids.getArrayNItems()) &&
          kids.getArrayItem(idx).isDictionary())) {
        QTC::TC("qpdf", "NNTree kid is invalid");
        error(oh, "invalid kid at index " + std::to_string(idx));
    }
    return withinLimits(key, kids.getArrayItem(idx));
}

void
NNTreeImpl::repair()
{
    auto new_node = QPDFObjectHandle::newDictionary();
    new_node.replaceKey(details.itemsKey(), QPDFObjectHandle::newArray());
    NNTreeImpl repl(details, qpdf, new_node, false);
    for (auto const& i: *this) {
        repl.insert(i.first, i.second);
    }
    oh.replaceKey("/Kids", new_node.getKey("/Kids"));
    oh.replaceKey(details.itemsKey(), new_node.getKey(details.itemsKey()));
}

NNTreeImpl::iterator
NNTreeImpl::find(QPDFObjectHandle key, bool return_prev_if_not_found)
{
    try {
        return findInternal(key, return_prev_if_not_found);
    } catch (QPDFExc& e) {
        if (auto_repair) {
            QTC::TC("qpdf", "NNTree repair");
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
    if (first_item.valid() && details.keyValid(first_item->first) &&
        details.compareKeys(key, first_item->first) < 0) {
        // Before the first key
        return end();
    }
    qpdf_assert_debug(!last_item.valid());

    QPDFObjGen::set seen;
    auto node = oh;
    iterator result(*this);

    while (true) {
        if (!seen.add(node)) {
            error(node, "loop detected in find");
        }

        auto items = node.getKey(details.itemsKey());
        size_t nitems = items.size();
        if (nitems > 1) {
            int idx = binarySearch(
                key, items, nitems / 2, return_prev_if_not_found, &NNTreeImpl::compareKeyItem);
            if (idx >= 0) {
                result.setItemNumber(node, 2 * idx);
            }
            return result;
        }

        auto kids = node.getKey("/Kids");
        size_t nkids = kids.isArray() ? kids.size() : 0;
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
NNTreeImpl::insertFirst(QPDFObjectHandle key, QPDFObjectHandle value)
{
    auto iter = begin();
    QPDFObjectHandle items;
    if (iter.node.isDictionary()) {
        items = iter.node.getKey(details.itemsKey());
    }
    if (!(items.isArray())) {
        QTC::TC("qpdf", "NNTree no valid items node in insertFirst");
        error(oh, "unable to find a valid items node");
    }
    items.insertItem(0, key);
    items.insertItem(1, value);
    iter.setItemNumber(iter.node, 0);
    iter.resetLimits(iter.node, iter.lastPathElement());
    iter.split(iter.node, iter.lastPathElement());
    return iter;
}

NNTreeImpl::iterator
NNTreeImpl::insert(QPDFObjectHandle key, QPDFObjectHandle value)
{
    auto iter = find(key, true);
    if (!iter.valid()) {
        QTC::TC("qpdf", "NNTree insert inserts first");
        return insertFirst(key, value);
    } else if (details.compareKeys(key, iter->first) == 0) {
        QTC::TC("qpdf", "NNTree insert replaces");
        auto items = iter.node.getKey(details.itemsKey());
        items.setArrayItem(iter.item_number + 1, value);
        iter.updateIValue();
    } else {
        QTC::TC("qpdf", "NNTree insert inserts after");
        iter.insertAfter(key, value);
    }
    return iter;
}

bool
NNTreeImpl::remove(QPDFObjectHandle key, QPDFObjectHandle* value)
{
    auto iter = find(key, false);
    if (!iter.valid()) {
        QTC::TC("qpdf", "NNTree remove not found");
        return false;
    }
    if (value) {
        *value = iter->second;
    }
    iter.remove();
    return true;
}
