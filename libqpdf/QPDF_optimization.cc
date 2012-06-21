// See "Optimization" sectino of manual.

#include <qpdf/QPDF.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Array.hh>
#include <assert.h>

QPDF::ObjUser::ObjUser() :
    ou_type(ou_bad),
    pageno(0)
{
}

QPDF::ObjUser::ObjUser(user_e type) :
    ou_type(type),
    pageno(0)
{
    assert(type == ou_root);
}

QPDF::ObjUser::ObjUser(user_e type, int pageno) :
    ou_type(type),
    pageno(pageno)
{
    assert((type == ou_page) || (type == ou_thumb));
}

QPDF::ObjUser::ObjUser(user_e type, std::string const& key) :
    ou_type(type),
    pageno(0),
    key(key)
{
    assert((type == ou_trailer_key) || (type == ou_root_key));
}

bool
QPDF::ObjUser::operator<(ObjUser const& rhs) const
{
    if (this->ou_type < rhs.ou_type)
    {
	return true;
    }
    else if (this->ou_type == rhs.ou_type)
    {
	if (this->pageno < rhs.pageno)
	{
	    return true;
	}
	else if (this->pageno == rhs.pageno)
	{
	    return (this->key < rhs.key);
	}
    }

    return false;
}

void
QPDF::flattenScalarReferences()
{
    // Do a traversal of the entire PDF file structure replacing all
    // indirect objects that are not arrays, streams, or dictionaries
    // with direct objects.

    std::list<QPDFObjectHandle> queue;
    queue.push_back(this->trailer);
    std::set<ObjGen> visited;

    // Add every object in the xref table to the queue.  This ensures
    // that we flatten scalar references in unreferenced objects.
    // This becomes important if we are preserving object streams in a
    // file that has unreferenced objects in its object streams.  (See
    // QPDF bug 2974522 at SourceForge.)
    for (std::map<ObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	ObjGen const& og = (*iter).first;
	queue.push_back(getObjectByID(og.obj, og.gen));
    }

    while (! queue.empty())
    {
	QPDFObjectHandle node = queue.front();
	queue.pop_front();
	if (node.isIndirect())
	{
	    ObjGen og(node.getObjectID(), node.getGeneration());
	    if (visited.count(og) > 0)
	    {
		continue;
	    }
	    visited.insert(og);
	}

	if (node.isArray())
	{
	    int nitems = node.getArrayNItems();
	    for (int i = 0; i < nitems; ++i)
	    {
		QPDFObjectHandle oh = node.getArrayItem(i);
		if (oh.isScalar())
		{
		    if (oh.isIndirect())
		    {
			QTC::TC("qpdf", "QPDF opt flatten array scalar");
			oh.makeDirect();
			node.setArrayItem(i, oh);
		    }
		}
		else
		{
		    queue.push_back(oh);
		}
	    }
	}
	else if (node.isDictionary() || node.isStream())
	{
	    QPDFObjectHandle dict = node;
	    if (node.isStream())
	    {
		dict = node.getDict();
	    }
	    std::set<std::string> keys = dict.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		std::string const& key = *iter;
		QPDFObjectHandle oh = dict.getKey(key);
		if (oh.isNull())
		{
		    // QPDF_Dictionary.getKeys() never returns null
		    // keys.
		    throw std::logic_error(
			"INTERNAL ERROR: dictionary with null key found");
		}
		else if (oh.isScalar())
		{
		    if (oh.isIndirect())
		    {
			QTC::TC("qpdf", "QPDF opt flatten dict scalar");
			oh.makeDirect();
			dict.replaceKey(key, oh);
		    }
		}
		else
		{
		    queue.push_back(oh);
		}
	    }
	}
    }
}

void
QPDF::optimize(std::map<int, int> const& object_stream_data,
	       bool allow_changes)
{
    if (! this->obj_user_to_objects.empty())
    {
	// already optimized
	return;
    }

    // Traverse pages tree pushing all inherited resources down to the
    // page level.

    // key_ancestors is a mapping of page attribute keys to a stack of
    // Pages nodes that contain values for them.  pageno is the
    // current page sequence number numbered from 0.
    std::map<std::string, std::vector<QPDFObjectHandle> > key_ancestors;
    int pageno = 0;
    optimizePagesTree(this->trailer.getKey("/Root").getKey("/Pages"),
		      key_ancestors, pageno, allow_changes);
    assert(key_ancestors.empty());

    // Traverse document-level items
    std::set<std::string> keys = this->trailer.getKeys();
    for (std::set<std::string>::iterator iter = keys.begin();
	 iter != keys.end(); ++iter)
    {
	std::string const& key = *iter;
	if (key == "/Root")
	{
	    // handled separately
	}
	else
	{
	    updateObjectMaps(ObjUser(ObjUser::ou_trailer_key, key),
			     this->trailer.getKey(key));
	}
    }

    QPDFObjectHandle root = getRoot();
    keys = root.getKeys();
    for (std::set<std::string>::iterator iter = keys.begin();
	 iter != keys.end(); ++iter)
    {
	// Technically, /I keys from /Thread dictionaries are supposed
	// to be handled separately, but we are going to disregard
	// that specification for now.  There is loads of evidence
	// that pdlin and Acrobat both disregard things like this from
	// time to time, so this is almost certain not to cause any
	// problems.

	std::string const& key = *iter;
	updateObjectMaps(ObjUser(ObjUser::ou_root_key, key),
			 root.getKey(key));
    }

    ObjUser root_ou = ObjUser(ObjUser::ou_root);
    ObjGen root_og = ObjGen(root.getObjectID(), root.getGeneration());
    obj_user_to_objects[root_ou].insert(root_og);
    object_to_obj_users[root_og].insert(root_ou);

    filterCompressedObjects(object_stream_data);
}

void
QPDF::optimizePagesTree(
    QPDFObjectHandle cur_pages,
    std::map<std::string, std::vector<QPDFObjectHandle> >& key_ancestors,
    int& pageno, bool allow_changes)
{
    // Extract the underlying dictionary object
    std::string type = cur_pages.getKey("/Type").getName();

    if (type == "/Pages")
    {
	// Make a list of inheritable keys.  Any key other than /Type,
	// /Parent, Kids, or /Count is an inheritable attribute.  Push
	// this object onto the stack of pages nodes that have values
	// for this attribute.

	std::set<std::string> inheritable_keys;
	std::set<std::string> keys = cur_pages.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& key = *iter;
	    if (! ((key == "/Type") || (key == "/Parent") ||
		   (key == "/Kids") || (key == "/Count")))
	    {
		if (! allow_changes)
		{
		    throw QPDFExc(qpdf_e_internal, this->file->getName(),
				  this->last_object_description,
				  this->file->getLastOffset(),
				  "optimize detected an "
				  "inheritable resource when called "
				  "in no-change mode");
		}

		// This is an inheritable resource
		inheritable_keys.insert(key);
		QPDFObjectHandle oh = cur_pages.getKey(key);
		QTC::TC("qpdf", "QPDF opt direct pages resource",
			oh.isIndirect() ? 0 : 1);
		if (! oh.isIndirect())
		{
		    if (! oh.isScalar())
		    {
			// Replace shared direct object non-scalar
			// resources with indirect objects to avoid
			// copying large structures around.
			cur_pages.replaceKey(key, makeIndirectObject(oh));
			oh = cur_pages.getKey(key);
		    }
		    else
		    {
			// Don't defeat flattenScalarReferences which
			// would have already been called by this
			// time.
			QTC::TC("qpdf", "QPDF opt inherited scalar");
		    }
		}
		key_ancestors[key].push_back(oh);
		if (key_ancestors[key].size() > 1)
		{
		    QTC::TC("qpdf", "QPDF opt key ancestors depth > 1");
		}
		// Remove this resource from this node.  It will be
		// reattached at the page level.
		cur_pages.removeKey(key);
	    }
	}

	// Visit descendant nodes.
	QPDFObjectHandle kids = cur_pages.getKey("/Kids");
	int n = kids.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    optimizePagesTree(kids.getArrayItem(i), key_ancestors, pageno,
			      allow_changes);
	}

	// For each inheritable key, pop the stack.  If the stack
	// becomes empty, remove it from the map.  That way, the
	// invariant that the list of keys in key_ancestors is exactly
	// those keys for which inheritable attributes are available.

	if (! inheritable_keys.empty())
	{
	    QTC::TC("qpdf", "QPDF opt inheritable keys");
	    for (std::set<std::string>::iterator iter =
		     inheritable_keys.begin();
		 iter != inheritable_keys.end(); ++iter)
	    {
		std::string const& key = (*iter);
		key_ancestors[key].pop_back();
		if (key_ancestors[key].empty())
		{
		    QTC::TC("qpdf", "QPDF opt erase empty key ancestor");
		    key_ancestors.erase(key);
		}
	    }
	}
	else
	{
	    QTC::TC("qpdf", "QPDF opt no inheritable keys");
	}
    }
    else if (type == "/Page")
    {
	// Add all available inheritable attributes not present in
	// this object to this object.
	for (std::map<std::string, std::vector<QPDFObjectHandle> >::iterator
		 iter = key_ancestors.begin();
	     iter != key_ancestors.end(); ++iter)
	{
	    std::string const& key = (*iter).first;
	    if (! cur_pages.hasKey(key))
	    {
		QTC::TC("qpdf", "QPDF opt resource inherited");
		cur_pages.replaceKey(key, (*iter).second.back());
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF opt page resource hides ancestor");
	    }
	}

	// Traverse from this point, updating the mappings of object
	// users to objects and objects to object users.

	updateObjectMaps(ObjUser(ObjUser::ou_page, pageno), cur_pages);

	// Increment pageno so that its value will be correct for the
	// next page.
	++pageno;
    }
    else
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "invalid Type in page tree");
    }
}

void
QPDF::updateObjectMaps(ObjUser const& ou, QPDFObjectHandle oh)
{
    std::set<ObjGen> visited;
    updateObjectMapsInternal(ou, oh, visited, true);
}

void
QPDF::updateObjectMapsInternal(ObjUser const& ou, QPDFObjectHandle oh,
			       std::set<ObjGen>& visited, bool top)
{
    // Traverse the object tree from this point taking care to avoid
    // crossing page boundaries.

    bool is_page_node = false;

    if (oh.isDictionary() && oh.hasKey("/Type"))
    {
	std::string type = oh.getKey("/Type").getName();
	if (type == "/Page")
	{
	    is_page_node = true;
	    if (! top)
	    {
		return;
	    }
	}
    }

    if (oh.isIndirect())
    {
	ObjGen og(oh.getObjectID(), oh.getGeneration());
	if (visited.count(og))
	{
	    QTC::TC("qpdf", "QPDF opt loop detected");
	    return;
	}
	this->obj_user_to_objects[ou].insert(og);
	this->object_to_obj_users[og].insert(ou);
	visited.insert(og);
    }

    if (oh.isArray())
    {
	int n = oh.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    updateObjectMapsInternal(ou, oh.getArrayItem(i), visited, false);
	}
    }
    else if (oh.isDictionary() || oh.isStream())
    {
	QPDFObjectHandle dict = oh;
	if (oh.isStream())
	{
	    dict = oh.getDict();
	}

	std::set<std::string> keys = dict.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& key = *iter;
	    if (is_page_node && (key == "/Thumb"))
	    {
		// Traverse page thumbnail dictionaries as a special
		// case.
		updateObjectMaps(ObjUser(ObjUser::ou_thumb, ou.pageno),
				 dict.getKey(key));
	    }
	    else if (is_page_node && (key == "/Parent"))
	    {
		// Don't traverse back up the page tree
	    }
	    else
	    {
		updateObjectMapsInternal(ou, dict.getKey(key),
					 visited, false);
	    }
	}
    }
}

void
QPDF::filterCompressedObjects(std::map<int, int> const& object_stream_data)
{
    if (object_stream_data.empty())
    {
	return;
    }

    // Transform object_to_obj_users and obj_user_to_objects so that
    // they refer only to uncompressed objects.  If something is a
    // user of a compressed object, then it is really a user of the
    // object stream that contains it.

    std::map<ObjUser, std::set<ObjGen> > t_obj_user_to_objects;
    std::map<ObjGen, std::set<ObjUser> > t_object_to_obj_users;

    for (std::map<ObjUser, std::set<ObjGen> >::iterator i1 =
	     this->obj_user_to_objects.begin();
	 i1 != this->obj_user_to_objects.end(); ++i1)
    {
	ObjUser const& ou = (*i1).first;
	std::set<ObjGen> const& objects = (*i1).second;
	for (std::set<ObjGen>::const_iterator i2 = objects.begin();
	     i2 != objects.end(); ++i2)
	{
	    ObjGen const& og = (*i2);
	    std::map<int, int>::const_iterator i3 =
		object_stream_data.find(og.obj);
	    if (i3 == object_stream_data.end())
	    {
		t_obj_user_to_objects[ou].insert(og);
	    }
	    else
	    {
		t_obj_user_to_objects[ou].insert(ObjGen((*i3).second, 0));
	    }
	}
    }

    for (std::map<ObjGen, std::set<ObjUser> >::iterator i1 =
	     this->object_to_obj_users.begin();
	 i1 != this->object_to_obj_users.end(); ++i1)
    {
	ObjGen const& og = (*i1).first;
	std::set<ObjUser> const& objusers = (*i1).second;
	for (std::set<ObjUser>::const_iterator i2 = objusers.begin();
	     i2 != objusers.end(); ++i2)
	{
	    ObjUser const& ou = (*i2);
	    std::map<int, int>::const_iterator i3 =
		object_stream_data.find(og.obj);
	    if (i3 == object_stream_data.end())
	    {
		t_object_to_obj_users[og].insert(ou);
	    }
	    else
	    {
		t_object_to_obj_users[ObjGen((*i3).second, 0)].insert(ou);
	    }
	}
    }

    this->obj_user_to_objects = t_obj_user_to_objects;
    this->object_to_obj_users = t_object_to_obj_users;
}
