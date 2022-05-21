#include <qpdf/QPDFPagesTree.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

static std::string
get_description(QPDFObjectHandle& node)
{
    std::string result("page tree node");
    if (node.isIndirect()) {
        result += " (object " + QUtil::int_to_string(node.getObjectID()) + ")";
    }
    return result;
}

static void
warn(QPDF& qpdf, QPDFObjectHandle& node, std::string const& msg)
{
    qpdf.warn(qpdf_e_damaged_pdf, get_description(node), 0, msg);
}

QPDFPagesTree::iterator::PathElement::PathElement(
    QPDFObjectHandle const& node, int kid_number) :
    node(node),
    kid_number(kid_number)
{
}

QPDFPagesTree::iterator::iterator(QPDFPagesTree const& pages_tree) :
    m(new Members(pages_tree))
{
}

QPDFPagesTree::iterator::Members::Members(QPDFPagesTree const& pages_tree) :
    qpdf(pages_tree.m->qpdf),
    pages_root(pages_tree.getPagesRoot()),
    split_threshold(pages_tree.m->split_threshold)
{
}

QPDFPagesTree::iterator&
QPDFPagesTree::iterator::operator++()
{
    increment(false);
    return *this;
}

QPDFPagesTree::iterator&
QPDFPagesTree::iterator::operator--()
{
    increment(true);
    return *this;
}

QPDFPagesTree::iterator::reference
QPDFPagesTree::iterator::operator*()
{
    updateIValue();
    return this->m->ivalue;
}

QPDFPagesTree::iterator::pointer
QPDFPagesTree::iterator::operator->()
{
    updateIValue();
    return &(this->m->ivalue);
}

void
QPDFPagesTree::iterator::addPathElement(
    QPDFObjectHandle const& node, int kid_number)
{
    this->m->path.push_back(PathElement(node, kid_number));
}

void
QPDFPagesTree::iterator::updateIValue(bool allow_invalid)
{
    // ivalue should never be used inside the class since we return a
    // pointer/reference to it. Every bit of code that ever changes
    // what object the iterator points to should take care to call
    // updateIValue. Failure to do this means that any old references
    // to *iter will point to incorrect objects, though the next
    // dereference of the iterator will fix it. This isn't necessarily
    // catastrophic, but it would be confusing. The test suite
    // attempts to exercise various cases to ensure we don't introduce
    // that bug in the future, but sadly it's tricky to verify by
    // reasoning about the code that this constraint is always
    // satisfied. Whenever we update what the iterator points to, we
    // should call this method. We also call updateIValue in operator*
    // and operator-> just in case.

    if (this->m->path.empty()) {
        if (!allow_invalid) {
            throw std::logic_error(
                "attempt made to dereference an invalid page tree iterator");
        }
        this->m->ivalue = QPDFObjectHandle();
    } else {
        auto& cur = this->m->path.back();
        this->m->ivalue = cur.node.getKey("/Kids").getArrayItem(cur.kid_number);
    }
}

bool
QPDFPagesTree::iterator::valid() const
{
    return (!this->m->path.empty());
}

void
QPDFPagesTree::iterator::increment(bool backward)
{
    if (this->m->path.empty()) {
        QTC::TC("qpdf", "QPDFPagesTree increment end()");
        deepen(this->m->pages_root, !backward);
        return;
    }
    bool found_valid_key = false;
    while (valid() && (!found_valid_key)) {
        auto& cur = this->m->path.back();
        cur.kid_number += backward ? -1 : 1;
        auto kids = cur.node.getKey("/Kids");
        if ((cur.kid_number < 0) || (cur.kid_number >= kids.getArrayNItems())) {
            this->m->path.pop_back();
        } else {
            auto kid = getKidIndirect(cur.node, cur.kid_number);
            if (!kid.isDictionary()) {
                // XXX Maybe remove rather than skip
                QTC::TC("qpdf", "QPDFPagesTree skip invalid node");
                warn(
                    this->m->qpdf,
                    cur.node,
                    "item " + QUtil::int_to_string(cur.kid_number) +
                        " is not a dictionary");
            } else if (kid.getKey("/Kids").isArray()) {
                if (deepen(kid, !backward)) {
                    found_valid_key = true;
                } else {
                    // We don't need a separate warning since deepen
                    // has already given a warning.
                    QTC::TC("qpdf", "QPDFPagesTree can't deepen");
                    // XXX Maybe remove rather than skip
                }
            } else {
                maybeSetType(kid, "/Page");
                found_valid_key = true;
            }
        }
    }
}

void
QPDFPagesTree::iterator::maybeSetType(
    QPDFObjectHandle node, std::string const& type)
{
    auto type_oh = node.getKey("/Type");
    if (!type_oh.isNameAndEquals(type)) {
        QTC::TC("qpdf", "QPDFPagesTree set /Type");
        warn(
            this->m->qpdf,
            node,
            "setting /Type to " + type + " in page tree node");
        node.replaceKey("/Type", QPDFObjectHandle::newName(type));
    }
}

QPDFObjectHandle
QPDFPagesTree::iterator::getKidIndirect(QPDFObjectHandle node, int kid_number)
{
    auto kids = node.getKey("/Kids");
    auto kid = kids.getArrayItem(kid_number);
    if (!kid.isIndirect()) {
        QTC::TC("qpdf", "QPDFPagesTree fix direct kid");
        // No need to warn for this. It's safe, and there's nothing
        // the user can/should do about it. Besides, qpdf has been
        // fixing direct kids for a while without issuing warnings.
        kid = this->m->qpdf.makeIndirectObject(kid);
        kids.setArrayItem(kid_number, kid);
    }
    return kid;
}

bool
QPDFPagesTree::iterator::deepen(QPDFObjectHandle node, bool first)
{
    // Starting at this node, descend through the first or last kid
    // until we reach a page node. If we succeed, return true;
    // otherwise return false and leave path alone.

    auto opath = this->m->path;
    bool failed = false;

    if (node.isDictionary() && node.getKey("/Kids").isArray() &&
        (node.getKey("/Kids").getArrayNItems() == 0)) {
        QTC::TC("qpdf", "QPDFPagesTree empty");
        return false;
    }

    std::set<QPDFObjGen> seen;
    for (auto i: this->m->path) {
        seen.insert(i.node.getObjGen());
    }
    while (!failed) {
        if (node.isIndirect()) {
            auto og = node.getObjGen();
            if (seen.count(og)) {
                QTC::TC("qpdf", "QPDFPagesTree deepen: loop");
                warn(
                    this->m->qpdf,
                    node,
                    "loop detected while traversing pages tree");
                failed = true;
                break;
            }
            seen.insert(og);
        }
        if (!node.isDictionary()) {
            QTC::TC("qpdf", "QPDFPagesTree node is not a dictionary");
            warn(
                this->m->qpdf,
                node,
                "non-dictionary node while traversing pages tree");
            failed = true;
            break;
        }
        auto kids = node.getKey("/Kids");
        if (kids.isArray()) {
            int nkids = kids.getArrayNItems();
            int kid_number = first ? 0 : nkids - 1;
            addPathElement(node, kid_number);
            maybeSetType(node, "/Pages");
            node = getKidIndirect(node, kid_number);
        } else {
            maybeSetType(node, "/Page");
            break;
        }
    }
    if (failed) {
        this->m->path = opath;
        return false;
    }
    return true;
}

void
QPDFPagesTree::iterator::updateCounts(QPDFObjectHandle node)
{
    auto kids = node.getKey("/Kids");
    if (!kids.isArray()) {
        return;
    }
    int count = 0;
    for (auto& kid: kids.aitems()) {
        if (!kid.isDictionary()) {
            // This will be caught elsewhere
            continue;
        }
        auto kid_count = kid.getKey("/Count");
        if (kid_count.isInteger()) {
            count += kid_count.getIntValueAsInt();
        }
    }
    node.replaceKey("/Count", QPDFObjectHandle::newInteger(count));
}

void
QPDFPagesTree::iterator::split(
    QPDFObjectHandle to_split, std::list<PathElement>::iterator parent)
{
    // Split some node along the path to the item pointed to by this
    // iterator, and adjust the iterator so it points to the same
    // item.

    if (!valid()) {
        throw std::logic_error(
            "QPDFPagesTreeIterator::split called an invalid iterator");
    }

    auto kids = to_split.getKey("/Kids");
    auto nkids = kids.getArrayNItems();
    if (nkids <= this->m->split_threshold) {
        return;
    }

    bool is_root = (parent == this->m->path.end());
    QPDFObjectHandle first_half = kids;

    // CURRENT STATE: tree is in original state; iterator is valid and
    // unchanged.

    if (is_root) {
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

        QTC::TC("qpdf", "QPDFPagesTree split root");
        auto first_node =
            this->m->qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        first_node.replaceKey("/Kids", first_half);
        QPDFObjectHandle new_kids = QPDFObjectHandle::newArray();
        new_kids.appendItem(first_node);
        to_split.replaceKey("/Kids", new_kids);
        auto next = this->m->path.begin();
        next->node = first_node;
        this->m->path.push_front(PathElement(to_split, 0));
        parent = this->m->path.begin();
        to_split = first_node;
    }

    // CURRENT STATE: parent is guaranteed to be defined, and we have
    // the invariants that parent[/Kids][kid_number] == to_split and
    // (++parent).node == to_split.

    // Create a second half array, and transfer the second half of the
    // items into the second half array.
    QPDFObjectHandle second_half = QPDFObjectHandle::newArray();
    int start_idx = nkids / 2;
    while (first_half.getArrayNItems() > start_idx) {
        second_half.appendItem(first_half.getArrayItem(start_idx));
        first_half.eraseItem(start_idx);
    }

    // Create a new node to contain the second half
    QPDFObjectHandle second_node =
        this->m->qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    second_node.replaceKey("/Kids", second_half);

    // CURRENT STATE: half the items from the kids or items array in
    // the node being split have been moved into a new node. The new
    // node is not yet attached to the tree. The iterator may have a
    // path element whose kid_number is out of bounds.

    // We need to adjust the parent to add the second node to /Kids
    // and, if needed, update kid_number to traverse through it. We
    // need to update to_split's path element so that the kid number
    // points to the right place.

    auto parent_kids = parent->node.getKey("/Kids");
    parent_kids.insertItem(parent->kid_number + 1, second_node);
    auto cur_elem = parent;
    ++cur_elem;
    int old_idx = cur_elem->kid_number;
    if (old_idx >= start_idx) {
        ++parent->kid_number;
        cur_elem->node = second_node;
        cur_elem->kid_number -= start_idx;
    }

    updateCounts(to_split);
    updateCounts(second_node);

    if (!is_root) {
        QTC::TC("qpdf", "QPDFPagesTree split parent");
        auto next = parent->node;
        --parent;
        split(next, parent);
    }
}

bool
QPDFPagesTree::iterator::operator==(iterator const& other) const
{
    if (this->m->path.size() != other.m->path.size()) {
        return false;
    }
    auto tpi = this->m->path.begin();
    auto opi = other.m->path.begin();
    while (tpi != this->m->path.end()) {
        if (tpi->kid_number != opi->kid_number) {
            return false;
        }
        ++tpi;
        ++opi;
    }
    return true;
}

void
QPDFPagesTree::iterator::insert(QPDFObjectHandle page)
{
    // XXX
}

void
QPDFPagesTree::iterator::remove()
{
    // XXX
}

QPDFPagesTree::QPDFPagesTree(QPDF& qpdf) :
    m(new Members(qpdf))
{
}

QPDFPagesTree::Members::Members(QPDF& qpdf) :
    qpdf(qpdf),
    pages_root(qpdf.getRoot().getKey("/Pages")),
    split_threshold(100)
{
}

size_t
QPDFPagesTree::size()
{
    // XXX
    return 0;
}

QPDFObjectHandle
QPDFPagesTree::at(size_t)
{
    // XXX
    return QPDFObjectHandle(); // XXX
}

QPDFObjectHandle
QPDFPagesTree::operator[](size_t n)
{
    return at(n);
}

QPDFObjectHandle
QPDFPagesTree::getPagesRoot() const
{
    return this->m->pages_root;
}

QPDFPagesTree::iterator
QPDFPagesTree::begin() const
{
    iterator result(*this);
    result.deepen(this->m->pages_root, true);
    return result;
}

QPDFPagesTree::iterator
QPDFPagesTree::end() const
{
    return iterator(*this);
}

void
QPDFPagesTree::setSplitThreshold(int n)
{
    this->m->split_threshold = n;
}
