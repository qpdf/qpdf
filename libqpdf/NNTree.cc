#include <qpdf/NNTree.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

#include <exception>

static std::string
get_description(QPDFObjectHandle& node)
{
    std::string result("Name/Number tree node");
    if (node.isIndirect())
    {
        result += " (object " + QUtil::int_to_string(node.getObjectID()) + ")";
    }
    return result;
}

static void
warn(QPDF* qpdf, QPDFObjectHandle& node, std::string const& msg)
{
    // ABI: in qpdf 11, change to a reference.

    if (qpdf)
    {
        qpdf->warn(QPDFExc(
                       qpdf_e_damaged_pdf,
                       qpdf->getFilename(), get_description(node), 0, msg));
    }
}

static void
error(QPDF* qpdf, QPDFObjectHandle& node, std::string const& msg)
{
    // ABI: in qpdf 11, change to a reference.

    if (qpdf)
    {
        throw QPDFExc(qpdf_e_damaged_pdf,
                      qpdf->getFilename(), get_description(node), 0, msg);
    }
    else
    {
        throw std::runtime_error(get_description(node) + ": " + msg);
    }
}

NNTreeIterator::NNTreeIterator(NNTreeImpl& impl) :
    impl(impl),
    item_number(-1)
{
}

NNTreeIterator::PathElement::PathElement(
    QPDFObjectHandle const& node, int kid_number) :
    node(node),
    kid_number(kid_number)
{
}

QPDFObjectHandle
NNTreeIterator::getNextKid(PathElement& pe, bool backward)
{
    QPDFObjectHandle result;
    bool found = false;
    while (! found)
    {
        pe.kid_number += backward ? -1 : 1;
        auto kids = pe.node.getKey("/Kids");
        if ((pe.kid_number >= 0) && (pe.kid_number < kids.getArrayNItems()))
        {
            result = kids.getArrayItem(pe.kid_number);
            if (result.isDictionary() &&
                (result.hasKey("/Kids") ||
                 result.hasKey(impl.details.itemsKey())))
            {
                found = true;
            }
            else
            {
                QTC::TC("qpdf", "NNTree skip invalid kid");
                warn(impl.qpdf, pe.node,
                     "skipping over invalid kid at index " +
                     QUtil::int_to_string(pe.kid_number));
            }
        }
        else
        {
            result = QPDFObjectHandle::newNull();
            found = true;
        }
    }
    return result;
}

bool
NNTreeIterator::valid() const
{
    return this->item_number >= 0;
}

void
NNTreeIterator::increment(bool backward)
{
    if (this->item_number < 0)
    {
        QTC::TC("qpdf", "NNTree increment end()");
        deepen(impl.oh, ! backward, true);
        return;
    }
    bool found_valid_key = false;
    while (valid() && (! found_valid_key))
    {
        this->item_number += backward ? -2 : 2;
        auto items = this->node.getKey(impl.details.itemsKey());
        if ((this->item_number < 0) ||
            (this->item_number >= items.getArrayNItems()))
        {
            bool found = false;
            setItemNumber(QPDFObjectHandle(), -1);
            while (! (found || this->path.empty()))
            {
                auto& element = this->path.back();
                auto pe_node = getNextKid(element, backward);
                if (pe_node.isNull())
                {
                    this->path.pop_back();
                }
                else
                {
                    found = deepen(pe_node, ! backward, false);
                }
            }
        }
        if (this->item_number >= 0)
        {
            items = this->node.getKey(impl.details.itemsKey());
            if (this->item_number + 1 >= items.getArrayNItems())
            {
                QTC::TC("qpdf", "NNTree skip item at end of short items");
                warn(impl.qpdf, this->node,
                     "items array doesn't have enough elements");
            }
            else if (! impl.details.keyValid(
                         items.getArrayItem(this->item_number)))
            {
                QTC::TC("qpdf", "NNTree skip invalid key");
                warn(impl.qpdf, this->node,
                     "item " + QUtil::int_to_string(this->item_number) +
                     " has the wrong type");
            }
            else
            {
                found_valid_key = true;
            }
        }
    }
}

void
NNTreeIterator::resetLimits(QPDFObjectHandle node,
                            std::list<PathElement>::iterator parent)
{
    bool done = false;
    while (! done)
    {
        if (parent == this->path.end())
        {
            QTC::TC("qpdf", "NNTree remove limits from root");
            node.removeKey("/Limits");
            done = true;
            break;
        }
        auto kids = node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = node.getKey(impl.details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;

        bool changed = true;
        QPDFObjectHandle first;
        QPDFObjectHandle last;
        if (nitems >= 2)
        {
            first = items.getArrayItem(0);
            last = items.getArrayItem((nitems - 1) & ~1);
        }
        else if (nkids > 0)
        {
            auto first_kid = kids.getArrayItem(0);
            auto last_kid = kids.getArrayItem(nkids - 1);
            if (first_kid.isDictionary() && last_kid.isDictionary())
            {
                auto first_limits = first_kid.getKey("/Limits");
                auto last_limits = last_kid.getKey("/Limits");
                if (first_limits.isArray() &&
                    (first_limits.getArrayNItems() >= 2) &&
                    last_limits.isArray() &&
                    (last_limits.getArrayNItems() >= 2))
                {
                    first = first_limits.getArrayItem(0);
                    last = last_limits.getArrayItem(1);
                }
            }
        }
        if (first.isInitialized() && last.isInitialized())
        {
            auto limits = QPDFObjectHandle::newArray();
            limits.appendItem(first);
            limits.appendItem(last);
            auto olimits = node.getKey("/Limits");
            if (olimits.isArray() && (olimits.getArrayNItems() == 2))
            {
                auto ofirst = olimits.getArrayItem(0);
                auto olast = olimits.getArrayItem(1);
                if (impl.details.keyValid(ofirst) &&
                    impl.details.keyValid(olast) &&
                    (impl.details.compareKeys(first, ofirst) == 0) &&
                    (impl.details.compareKeys(last, olast) == 0))
                {
                    QTC::TC("qpdf", "NNTree limits didn't change");
                    changed = false;
                }
            }
            if (changed)
            {
                node.replaceKey("/Limits", limits);
            }
        }
        else
        {
            QTC::TC("qpdf", "NNTree unable to determine limits");
            warn(impl.qpdf, node, "unable to determine limits");
        }

        if ((! changed) || (parent == this->path.begin()))
        {
            done = true;
        }
        else
        {
            node = parent->node;
            --parent;
        }
    }
}

void
NNTreeIterator::split(QPDFObjectHandle to_split,
                      std::list<PathElement>::iterator parent)
{
    // Split some node along the path to the item pointed to by this
    // iterator, and adjust the iterator so it points to the same
    // item.

    // In examples, for simplicity, /Nums is show to just contain
    // numbers instead of pairs. Imagine this tre:
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

    if (! this->impl.qpdf)
    {
        throw std::logic_error(
            "NNTreeIterator::split called with null qpdf");
    }
    if (! valid())
    {
        throw std::logic_error(
            "NNTreeIterator::split called an invalid iterator");
    }

    // Find the array we actually need to split, which is either this
    // node's kids or items.
    auto kids = to_split.getKey("/Kids");
    int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
    auto items = to_split.getKey(impl.details.itemsKey());
    int nitems = items.isArray() ? items.getArrayNItems() : 0;

    QPDFObjectHandle first_half;
    int n = 0;
    std::string key;
    int threshold = 0;
    if (nkids > 0)
    {
        QTC::TC("qpdf", "NNTree split kids");
        first_half = kids;
        n = nkids;
        threshold = impl.split_threshold;
        key = "/Kids";
    }
    else if (nitems > 0)
    {
        QTC::TC("qpdf", "NNTree split items");
        first_half = items;
        n = nitems;
        threshold = 2 * impl.split_threshold;
        key = impl.details.itemsKey();
    }
    else
    {
        throw std::logic_error("NNTreeIterator::split called on invalid node");
    }

    if (n <= threshold)
    {
        return;
    }

    bool is_root = (parent == this->path.end());
    bool is_leaf = (nitems > 0);

    // CURRENT STATE: tree is in original state; iterator is valid and
    // unchanged.

    if (is_root)
    {
        // What we want to do is to create a new node for the second
        // half of the items and put it in the parent's /Kids array
        // right after the element that points to the current to_split
        // node, but if we're splitting root, there is no parent, so
        // handle that first.

        // In the non-root case, parent points to the path element
        // whose /Kids contains the first half node, and the first
        // half node is to_split. If we are splitting the root, we
        // need to push everything down a level, but we want to keep
        // the actual root object the same so that indirect references
        // to it remain intact (and also in case it might be a direct
        // object, which it shouldn't be but that case probably exists
        // in the wild). To achieve this, we create a new node for the
        // first half and then replace /Kids in the root to contain
        // it. Then we adjust the path so that the first element is
        // root and the second element, if any, is the new first half.
        // In this way, we make the root case identical to the
        // non-root case so remaining logic can handle them in the
        // same way.

        auto first_node = impl.qpdf->makeIndirectObject(
            QPDFObjectHandle::newDictionary());
        first_node.replaceKey(key, first_half);
        QPDFObjectHandle new_kids = QPDFObjectHandle::newArray();
        new_kids.appendItem(first_node);
        to_split.removeKey("/Limits"); // already shouldn't be there for root
        to_split.removeKey(impl.details.itemsKey());
        to_split.replaceKey("/Kids", new_kids);
        if (is_leaf)
        {
            QTC::TC("qpdf", "NNTree split root + leaf");
            this->node = first_node;
        }
        else
        {
            QTC::TC("qpdf", "NNTree split root + !leaf");
            auto next = this->path.begin();
            next->node = first_node;
        }
        this->path.push_front(PathElement(to_split, 0));
        parent = this->path.begin();
        to_split = first_node;
    }

    // CURRENT STATE: parent is guaranteed to be defined, and we have
    // the invariants that parent[/Kids][kid_number] == to_split and
    // (++parent).node == to_split.

    // Create a second half array, and transfer the second half of the
    // items into the second half array.
    QPDFObjectHandle second_half = QPDFObjectHandle::newArray();
    int start_idx = ((n / 2) & ~1);
    while (first_half.getArrayNItems() > start_idx)
    {
        second_half.appendItem(first_half.getArrayItem(start_idx));
        first_half.eraseItem(start_idx);
    }
    resetLimits(to_split, parent);

    // Create a new node to contain the second half
    QPDFObjectHandle second_node = impl.qpdf->makeIndirectObject(
        QPDFObjectHandle::newDictionary());
    second_node.replaceKey(key, second_half);
    resetLimits(second_node, parent);

    // CURRENT STATE: half the items from the kids or items array in
    // the node being split have been moved into a new node. The new
    // node is not yet attached to the tree. The iterator have a path
    // element or leaf node that is out of bounds.

    // We need to adjust the parent to add the second node to /Kids
    // and, if needed, update kid_number to traverse through it. We
    // need to update to_split's path element, or the node if this is
    // a leaf, so that the kid/item number points to the right place.

    auto parent_kids = parent->node.getKey("/Kids");
    parent_kids.insertItem(parent->kid_number + 1, second_node);
    auto cur_elem = parent;
    ++cur_elem; // points to end() for leaf nodes
    int old_idx = (is_leaf ? this->item_number : cur_elem->kid_number);
    if (old_idx >= start_idx)
    {
        ++parent->kid_number;
        if (is_leaf)
        {
            QTC::TC("qpdf", "NNTree split second half item");
            setItemNumber(second_node, this->item_number - start_idx);
        }
        else
        {
            QTC::TC("qpdf", "NNTree split second half kid");
            cur_elem->node = second_node;
            cur_elem->kid_number -= start_idx;
        }
    }
    if (! is_root)
    {
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
    auto result = this->path.end();
    if (! this->path.empty())
    {
        --result;
    }
    return result;
}

void
NNTreeIterator::insertAfter(QPDFObjectHandle key, QPDFObjectHandle value)
{
    if (! valid())
    {
        QTC::TC("qpdf", "NNTree insertAfter inserts first");
        impl.insertFirst(key, value);
        deepen(impl.oh, true, false);
        return;
    }

    auto items = this->node.getKey(impl.details.itemsKey());
    if (! items.isArray())
    {
        error(impl.qpdf, node, "node contains no items array");
    }
    if (items.getArrayNItems() < this->item_number + 2)
    {
        error(impl.qpdf, node, "insert: items array is too short");
    }
    items.insertItem(this->item_number + 2, key);
    items.insertItem(this->item_number + 3, value);
    resetLimits(this->node, lastPathElement());
    split(this->node, lastPathElement());
    increment(false);
}

void
NNTreeIterator::remove()
{
    // Remove this item, leaving the tree valid and this iterator
    // pointing to the next item.

    if (! valid())
    {
        throw std::logic_error("attempt made to remove an invalid iterator");
    }
    auto items = this->node.getKey(impl.details.itemsKey());
    int nitems = items.getArrayNItems();
    if (this->item_number + 2 > nitems)
    {
        error(impl.qpdf, this->node,
              "found short items array while removing an item");
    }

    items.eraseItem(this->item_number);
    items.eraseItem(this->item_number);
    nitems -= 2;

    if (nitems > 0)
    {
        // There are still items left

        if ((this->item_number == 0) || (this->item_number == nitems))
        {
            // We removed either the first or last item of an items array
            // that remains non-empty, so we have to adjust limits.
            QTC::TC("qpdf", "NNTree remove reset limits");
            resetLimits(this->node, lastPathElement());
        }

        if (this->item_number == nitems)
        {
            // We removed the last item of a non-empty items array, so
            // advance to the successor of the previous item.
            QTC::TC("qpdf", "NNTree erased last item");
            this->item_number -= 2;
            increment(false);
        }
        else if (this->item_number < nitems)
        {
            // We don't have to do anything since the removed item's
            // successor now occupies its former location.
            QTC::TC("qpdf", "NNTree erased non-last item");
        }
        else
        {
            // We already checked to ensure this condition would not
            // happen.
            throw std::logic_error(
                "NNTreeIterator::remove: item_number > nitems after erase");
        }
        return;
    }

    if (this->path.empty())
    {
        // Special case: if this is the root node, we can leave it
        // empty.
        QTC::TC("qpdf", "NNTree erased all items on leaf/root");
        setItemNumber(impl.oh, -1);
        return;
    }

    QTC::TC("qpdf", "NNTree items is empty after remove");

    // We removed the last item from this items array, so we need to
    // remove this node from the parent on up the tree. Then we need
    // to position ourselves at the removed item's successor.
    bool done = false;
    while (! done)
    {
        auto element = lastPathElement();
        auto parent = element;
        --parent;
        auto kids = element->node.getKey("/Kids");
        kids.eraseItem(element->kid_number);
        auto nkids = kids.getArrayNItems();
        if (nkids > 0)
        {
            // The logic here is similar to the items case.
            if ((element->kid_number == 0) || (element->kid_number == nkids))
            {
                QTC::TC("qpdf", "NNTree erased first or last kid");
                resetLimits(element->node, parent);
            }
            if (element->kid_number == nkids)
            {
                // Move to the successor of the last child of the
                // previous kid.
                setItemNumber(QPDFObjectHandle(), -1);
                --element->kid_number;
                deepen(kids.getArrayItem(element->kid_number), false, true);
                if (valid())
                {
                    increment(false);
                    if (! valid())
                    {
                        QTC::TC("qpdf", "NNTree erased last item in tree");
                    }
                    else
                    {
                        QTC::TC("qpdf", "NNTree erased last kid");
                    }
                }
            }
            else
            {
                // Next kid is in deleted kid's position
                QTC::TC("qpdf", "NNTree erased non-last kid");
                deepen(kids.getArrayItem(element->kid_number), true, true);
            }
            done = true;
        }
        else if (parent == this->path.end())
        {
            // We erased the very last item. Convert the root to an
            // empty items array.
            QTC::TC("qpdf", "NNTree non-flat tree is empty after remove");
            element->node.removeKey("/Kids");
            element->node.replaceKey(impl.details.itemsKey(),
                                     QPDFObjectHandle::newArray());
            this->path.clear();
            setItemNumber(impl.oh, -1);
            done = true;
        }
        else
        {
            // Walk up the tree and continue
            QTC::TC("qpdf", "NNTree remove walking up tree");
            this->path.pop_back();
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
    if (this->item_number < 0)
    {
        throw std::logic_error(
            "attempt made to dereference an invalid"
            " name/number tree iterator");
    }
    auto items = this->node.getKey(impl.details.itemsKey());
    if (items.getArrayNItems() < this->item_number + 2)
    {
        error(impl.qpdf, node, "operator*: items array is too short");
    }
    return std::make_pair(items.getArrayItem(this->item_number),
                          items.getArrayItem(1+this->item_number));
}

bool
NNTreeIterator::operator==(NNTreeIterator const& other) const
{
    if ((this->item_number == -1) && (other.item_number == -1))
    {
        return true;
    }
    if (this->path.size() != other.path.size())
    {
        return false;
    }
    auto tpi = this->path.begin();
    auto opi = other.path.begin();
    while (tpi != this->path.end())
    {
        if ((*tpi).kid_number != (*opi).kid_number)
        {
            return false;
        }
        ++tpi;
        ++opi;
    }
    if (this->item_number != other.item_number)
    {
        return false;
    }
    return true;
}

void
NNTreeIterator::setItemNumber(QPDFObjectHandle const& node, int n)
{
    this->node = node;
    this->item_number = n;
}

void
NNTreeIterator::addPathElement(QPDFObjectHandle const& node,
                               int kid_number)
{
    this->path.push_back(PathElement(node, kid_number));
}

bool
NNTreeIterator::deepen(QPDFObjectHandle node, bool first, bool allow_empty)
{
    // Starting at this node, descend through the first or last kid
    // until we reach a node with items. If we succeed, return true;
    // otherwise return false and leave path alone.

    auto opath = this->path;
    bool failed = false;

    std::set<QPDFObjGen> seen;
    while (! failed)
    {
        if (node.isIndirect())
        {
            auto og = node.getObjGen();
            if (seen.count(og))
            {
                QTC::TC("qpdf", "NNTree deepen: loop");
                warn(impl.qpdf, node,
                     "loop detected while traversing name/number tree");
                failed = true;
                break;
            }
            seen.insert(og);
        }
        if (! node.isDictionary())
        {
            QTC::TC("qpdf", "NNTree node is not a dictionary");
            warn(impl.qpdf, node,
                 "non-dictionary node while traversing name/number tree");
            failed = true;
            break;
        }

        auto kids = node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = node.getKey(impl.details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;
        if (nitems > 0)
        {
            setItemNumber(node, first ? 0 : nitems - 2);
            break;
        }
        else if (nkids > 0)
        {
            int kid_number = first ? 0 : nkids - 1;
            addPathElement(node, kid_number);
            auto next = kids.getArrayItem(kid_number);
            if (! next.isIndirect())
            {
                if (impl.qpdf && impl.auto_repair)
                {
                    QTC::TC("qpdf", "NNTree fix indirect kid");
                    warn(impl.qpdf, node,
                         "converting kid number " +
                         QUtil::int_to_string(kid_number) +
                         " to an indirect object");
                    next = impl.qpdf->makeIndirectObject(next);
                    kids.setArrayItem(kid_number, next);
                }
                else
                {
                    QTC::TC("qpdf", "NNTree warn indirect kid");
                    warn(impl.qpdf, node,
                         "kid number " + QUtil::int_to_string(kid_number) +
                         " is not an indirect object");
                }
            }
            node = next;
        }
        else if (allow_empty && items.isArray())
        {
            QTC::TC("qpdf", "NNTree deepen found empty");
            setItemNumber(node, -1);
            break;
        }
        else
        {
            QTC::TC("qpdf", "NNTree deepen: invalid node");
            warn(impl.qpdf, node,
                 "name/number tree node has neither non-empty " +
                 impl.details.itemsKey() + " nor /Kids");
            failed = true;
            break;
        }
    }
    if (failed)
    {
        this->path = opath;
        return false;
    }
    return true;
}

NNTreeImpl::NNTreeImpl(NNTreeDetails const& details,
                       QPDF* qpdf,
                       QPDFObjectHandle& oh,
                       bool auto_repair) :
    details(details),
    qpdf(qpdf),
    split_threshold(32),
    oh(oh),
    auto_repair(auto_repair)
{
}

void
NNTreeImpl::setSplitThreshold(int split_threshold)
{
    this->split_threshold = split_threshold;
}

NNTreeImpl::iterator
NNTreeImpl::begin()
{
    iterator result(*this);
    result.deepen(this->oh, true, true);
    return result;
}

NNTreeImpl::iterator
NNTreeImpl::end()
{
    return iterator(*this);
}

NNTreeImpl::iterator
NNTreeImpl::last()
{
    iterator result(*this);
    result.deepen(this->oh, false, true);
    return result;
}

int
NNTreeImpl::withinLimits(QPDFObjectHandle key, QPDFObjectHandle node)
{
    int result = 0;
    auto limits = node.getKey("/Limits");
    if (limits.isArray() && (limits.getArrayNItems() >= 2) &&
        details.keyValid(limits.getArrayItem(0)) &&
        details.keyValid(limits.getArrayItem(1)))
    {
        if (details.compareKeys(key, limits.getArrayItem(0)) < 0)
        {
            result = -1;
        }
        else if (details.compareKeys(key, limits.getArrayItem(1)) > 0)
        {
            result = 1;
        }
    }
    else
    {
        QTC::TC("qpdf", "NNTree missing limits");
        error(qpdf, node, "node is missing /Limits");
    }
    return result;
}

int
NNTreeImpl::binarySearch(
    QPDFObjectHandle key, QPDFObjectHandle items,
    int num_items, bool return_prev_if_not_found,
    int (NNTreeImpl::*compare)(QPDFObjectHandle& key,
                             QPDFObjectHandle& arr,
                             int item))
{
    int max_idx = 1;
    while (max_idx < num_items)
    {
        max_idx <<= 1;
    }

    int step = max_idx / 2;
    int checks = max_idx;
    int idx = step;
    int found_idx = -1;
    bool found = false;
    bool found_leq = false;
    int status = 0;

    while ((! found) && (checks > 0))
    {
        if (idx < num_items)
        {
            status = (this->*compare)(key, items, idx);
            if (status >= 0)
            {
                found_leq = true;
                found_idx = idx;
            }
        }
        else
        {
            // consider item to be below anything after the top
            status = -1;
        }

        if (status == 0)
        {
            found = true;
        }
        else
        {
            checks >>= 1;
            if (checks > 0)
            {
                step >>= 1;
                if (step == 0)
                {
                    step = 1;
                }

                if (status < 0)
                {
                    idx -= step;
                }
                else
                {
                    idx += step;
                }
            }
        }
    }

    if (found || (found_leq && return_prev_if_not_found))
    {
        return found_idx;
    }
    else
    {
        return -1;
    }
}

int
NNTreeImpl::compareKeyItem(
    QPDFObjectHandle& key, QPDFObjectHandle& items, int idx)
{
    if (! ((items.isArray() && (items.getArrayNItems() > (2 * idx)) &&
            details.keyValid(items.getArrayItem(2 * idx)))))
    {
        QTC::TC("qpdf", "NNTree item is wrong type");
        error(qpdf, this->oh,
              "item at index " + QUtil::int_to_string(2 * idx) +
              " is not the right type");
    }
    return details.compareKeys(key, items.getArrayItem(2 * idx));
}

int
NNTreeImpl::compareKeyKid(
    QPDFObjectHandle& key, QPDFObjectHandle& kids, int idx)
{
    if (! (kids.isArray() && (idx < kids.getArrayNItems()) &&
           kids.getArrayItem(idx).isDictionary()))
    {
        QTC::TC("qpdf", "NNTree kid is invalid");
        error(qpdf, this->oh,
              "invalid kid at index " + QUtil::int_to_string(idx));
    }
    return withinLimits(key, kids.getArrayItem(idx));
}


void
NNTreeImpl::repair()
{
    auto new_node = QPDFObjectHandle::newDictionary();
    new_node.replaceKey(details.itemsKey(), QPDFObjectHandle::newArray());
    NNTreeImpl repl(details, qpdf, new_node, false);
    for (auto i: *this)
    {
        repl.insert(i.first, i.second);
    }
    this->oh.replaceKey("/Kids", new_node.getKey("/Kids"));
    this->oh.replaceKey(
        details.itemsKey(), new_node.getKey(details.itemsKey()));
}

NNTreeImpl::iterator
NNTreeImpl::find(QPDFObjectHandle key, bool return_prev_if_not_found)
{
    try
    {
        return findInternal(key, return_prev_if_not_found);
    }
    catch (QPDFExc& e)
    {
        if (this->auto_repair)
        {
            QTC::TC("qpdf", "NNTree repair");
            warn(qpdf, this->oh,
                 std::string("attempting to repair after error: ") + e.what());
            repair();
            return findInternal(key, return_prev_if_not_found);
        }
        else
        {
            throw e;
        }
    }
}

NNTreeImpl::iterator
NNTreeImpl::findInternal(QPDFObjectHandle key, bool return_prev_if_not_found)
{
    auto first_item = begin();
    auto last_item = end();
    if (first_item == end())
    {
        // Empty
        return end();
    }
    else if (first_item.valid() &&
        details.keyValid((*first_item).first) &&
        details.compareKeys(key, (*first_item).first) < 0)
    {
        // Before the first key
        return end();
    }
    else if (last_item.valid() &&
             details.keyValid((*last_item).first) &&
             details.compareKeys(key, (*last_item).first) > 0)
    {
        // After the last key
        if (return_prev_if_not_found)
        {
            return last_item;
        }
        else
        {
            return end();
        }
    }

    std::set<QPDFObjGen> seen;
    auto node = this->oh;
    iterator result(*this);

    while (true)
    {
        auto og = node.getObjGen();
        if (seen.count(og))
        {
            QTC::TC("qpdf", "NNTree loop in find");
            error(qpdf, node, "loop detected in find");
        }
        seen.insert(og);

        auto kids = node.getKey("/Kids");
        int nkids = kids.isArray() ? kids.getArrayNItems() : 0;
        auto items = node.getKey(details.itemsKey());
        int nitems = items.isArray() ? items.getArrayNItems() : 0;
        if (nitems > 0)
        {
            int idx = binarySearch(
                key, items, nitems / 2, return_prev_if_not_found,
                &NNTreeImpl::compareKeyItem);
            if (idx >= 0)
            {
                result.setItemNumber(node, 2 * idx);
            }
            break;
        }
        else if (nkids > 0)
        {
            int idx = binarySearch(
                key, kids, nkids, true,
                &NNTreeImpl::compareKeyKid);
            if (idx == -1)
            {
                QTC::TC("qpdf", "NNTree -1 in binary search");
                error(qpdf, node,
                      "unexpected -1 from binary search of kids;"
                      " limits may by wrong");
            }
            result.addPathElement(node, idx);
            node = kids.getArrayItem(idx);
        }
        else
        {
            QTC::TC("qpdf", "NNTree bad node during find");
            error(qpdf, node, "bad node during find");
        }
    }

    return result;
}

NNTreeImpl::iterator
NNTreeImpl::insertFirst(QPDFObjectHandle key, QPDFObjectHandle value)
{
    auto iter = begin();
    QPDFObjectHandle items;
    if (iter.node.isInitialized() &&
        iter.node.isDictionary())
    {
        items = iter.node.getKey(details.itemsKey());
    }
    if (! (items.isInitialized() && items.isArray()))
    {
        QTC::TC("qpdf", "NNTree no valid items node in insertFirst");
        error(qpdf, this->oh, "unable to find a valid items node");
    }
    items.insertItem(0, key);
    items.insertItem(1, value);
    iter.item_number = 0;
    iter.resetLimits(iter.node, iter.lastPathElement());
    iter.split(iter.node, iter.lastPathElement());
    return begin();
}

NNTreeImpl::iterator
NNTreeImpl::insert(QPDFObjectHandle key, QPDFObjectHandle value)
{
    auto iter = find(key, true);
    if (! iter.valid())
    {
        QTC::TC("qpdf", "NNTree insert inserts first");
        return insertFirst(key, value);
    }
    else if (details.compareKeys(key, (*iter).first) == 0)
    {
        QTC::TC("qpdf", "NNTree insert replaces");
        auto items = iter.node.getKey(details.itemsKey());
        items.setArrayItem(iter.item_number + 1, value);
    }
    else
    {
        QTC::TC("qpdf", "NNTree insert inserts after");
        iter.insertAfter(key, value);
    }
    return iter;
}

bool
NNTreeImpl::remove(QPDFObjectHandle key, QPDFObjectHandle* value)
{
    auto iter = find(key, false);
    if (! iter.valid())
    {
        QTC::TC("qpdf", "NNTree remove not found");
        return false;
    }
    if (value)
    {
        *value = (*iter).second;
    }
    iter.remove();
    return true;
}
