// See doc/linearization.

#include <qpdf/QPDF.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/PCRE.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/BitWriter.hh>
#include <qpdf/BitStream.hh>

#include <iostream>
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <string.h>

template <class T, class int_type>
static void
load_vector_int(BitStream& bit_stream, int nitems, std::vector<T>& vec,
		int bits_wanted, int_type T::*field)
{
    bool append = vec.empty();
    // nitems times, read bits_wanted from the given bit stream,
    // storing results in the ith vector entry.

    for (int i = 0; i < nitems; ++i)
    {
        if (append)
        {
            vec.push_back(T());
        }
	vec.at(i).*field = bit_stream.getBits(bits_wanted);
    }
    if (static_cast<int>(vec.size()) != nitems)
    {
        throw std::logic_error("vector has wrong size in load_vector_int");
    }
    // The PDF spec says that each hint table starts at a byte
    // boundary.  Each "row" actually must start on a byte boundary.
    bit_stream.skipToNextByte();
}

template <class T>
static void
load_vector_vector(BitStream& bit_stream,
		   int nitems1, std::vector<T>& vec1, int T::*nitems2,
		   int bits_wanted, std::vector<int> T::*vec2)
{
    // nitems1 times, read nitems2 (from the ith element of vec1) items
    // into the vec2 vector field of the ith item of vec1.
    for (int i1 = 0; i1 < nitems1; ++i1)
    {
	for (int i2 = 0; i2 < vec1.at(i1).*nitems2; ++i2)
	{
	    (vec1.at(i1).*vec2).push_back(bit_stream.getBits(bits_wanted));
	}
    }
    bit_stream.skipToNextByte();
}

bool
QPDF::checkLinearization()
{
    bool result = false;
    try
    {
	readLinearizationData();
	result = checkLinearizationInternal();
    }
    catch (QPDFExc& e)
    {
	*out_stream << e.what() << std::endl;
    }
    return result;
}

bool
QPDF::isLinearized()
{
    // If the first object in the file is a dictionary with a suitable
    // /Linearized key and has an /L key that accurately indicates the
    // file size, initialize this->lindict and return true.

    // A linearized PDF spec's first object will be contained within
    // the first 1024 bytes of the file and will be a dictionary with
    // a valid /Linearized key.  This routine looks for that and does
    // no additional validation.

    // The PDF spec says the linearization dictionary must be
    // completely contained within the first 1024 bytes of the file.
    // Add a byte for a null terminator.
    static int const tbuf_size = 1025;

    char* buf = new char[tbuf_size];
    this->file->seek(0, SEEK_SET);
    PointerHolder<char> b(true, buf);
    memset(buf, '\0', tbuf_size);
    this->file->read(buf, tbuf_size - 1);

    PCRE lindict_re("(?s:(\\d+)\\s+0\\s+obj\\s*<<)");

    int lindict_obj = -1;
    char* p = buf;
    while (lindict_obj == -1)
    {
	PCRE::Match m(lindict_re.match(p));
	if (m)
	{
	    lindict_obj = atoi(m.getMatch(1).c_str());
	    if (m.getMatch(0).find('\n') != std::string::npos)
	    {
		QTC::TC("qpdf", "QPDF lindict found newline");
	    }
	}
	else
	{
	    p = reinterpret_cast<char*>(memchr(p, '\0', tbuf_size - (p - buf)));
	    assert(p != 0);
	    while ((p - buf < tbuf_size) && (*p == 0))
	    {
		++p;
	    }
	    if ((p - buf) == tbuf_size)
	    {
		break;
	    }
	    QTC::TC("qpdf", "QPDF lindict searching after null");
	}
    }

    if (lindict_obj == 0)
    {
	return false;
    }

    QPDFObjectHandle candidate = QPDFObjectHandle::Factory::newIndirect(
	this, lindict_obj, 0);
    if (! candidate.isDictionary())
    {
	return false;
    }

    QPDFObjectHandle linkey = candidate.getKey("/Linearized");
    if (! (linkey.isNumber() &&
           (static_cast<int>(floor(linkey.getNumericValue())) == 1)))
    {
	return false;
    }

    QPDFObjectHandle L = candidate.getKey("/L");
    if (L.isInteger())
    {
	qpdf_offset_t Li = L.getIntValue();
	this->file->seek(0, SEEK_END);
	if (Li != this->file->tell())
	{
	    QTC::TC("qpdf", "QPDF /L mismatch");
	    return false;
	}
	else
	{
	    this->linp.file_size = Li;
	}
    }

    this->lindict = candidate;

    return true;
}

void
QPDF::readLinearizationData()
{
    // This function throws an exception (which is trapped by
    // checkLinearization()) for any errors that prevent loading.

    // Hint table parsing code needs at least 32 bits in a long.
    assert(sizeof(long) >= 4);

    if (! isLinearized())
    {
	throw std::logic_error("called readLinearizationData for file"
			       " that is not linearized");
    }

    // /L is read and stored in linp by isLinearized()
    QPDFObjectHandle H = lindict.getKey("/H");
    QPDFObjectHandle O = lindict.getKey("/O");
    QPDFObjectHandle E = lindict.getKey("/E");
    QPDFObjectHandle N = lindict.getKey("/N");
    QPDFObjectHandle T = lindict.getKey("/T");
    QPDFObjectHandle P = lindict.getKey("/P");

    if (! (H.isArray() &&
	   O.isInteger() &&
	   E.isInteger() &&
	   N.isInteger() &&
	   T.isInteger() &&
	   (P.isInteger() || P.isNull())))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "linearization dictionary",
		      this->file->getLastOffset(),
		      "some keys in linearization dictionary are of "
		      "the wrong type");
    }

    // Hint table array: offset length [ offset length ]
    unsigned int n_H_items = H.getArrayNItems();
    if (! ((n_H_items == 2) || (n_H_items == 4)))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "linearization dictionary",
		      this->file->getLastOffset(),
		      "H has the wrong number of items");
    }

    std::vector<int> H_items;
    for (unsigned int i = 0; i < n_H_items; ++i)
    {
	QPDFObjectHandle oh(H.getArrayItem(i));
	if (oh.isInteger())
	{
	    H_items.push_back(oh.getIntValue());
	}
	else
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "linearization dictionary",
			  this->file->getLastOffset(),
			  "some H items are of the wrong type");
	}
    }

    // H: hint table offset/length for primary and overflow hint tables
    int H0_offset = H_items.at(0);
    int H0_length = H_items.at(1);
    int H1_offset = 0;
    int H1_length = 0;
    if (H_items.size() == 4)
    {
	// Acrobat doesn't read or write these (as PDF 1.4), so we
	// don't have a way to generate a test case.
	// QTC::TC("qpdf", "QPDF overflow hint table");
	H1_offset = H_items.at(2);
	H1_length = H_items.at(3);
    }

    // P: first page number
    int first_page = 0;
    if (P.isInteger())
    {
	QTC::TC("qpdf", "QPDF P present in lindict");
	first_page = P.getIntValue();
    }
    else
    {
	QTC::TC("qpdf", "QPDF P absent in lindict");
    }

    // Store linearization parameter data

    // Various places in the code use linp.npages, which is
    // initialized from N, to pre-allocate memory, so make sure it's
    // accurate and bail right now if it's not.
    if (N.getIntValue() != static_cast<long long>(getAllPages().size()))
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                      "linearization hint table",
                      this->file->getLastOffset(),
                      "/N does not match number of pages");
    }

    // file_size initialized by isLinearized()
    this->linp.first_page_object = O.getIntValue();
    this->linp.first_page_end = E.getIntValue();
    this->linp.npages = N.getIntValue();
    this->linp.xref_zero_offset = T.getIntValue();
    this->linp.first_page = first_page;
    this->linp.H_offset = H0_offset;
    this->linp.H_length = H0_length;

    // Read hint streams

    Pl_Buffer pb("hint buffer");
    QPDFObjectHandle H0 = readHintStream(pb, H0_offset, H0_length);
    if (H1_offset)
    {
	(void) readHintStream(pb, H1_offset, H1_length);
    }

    // PDF 1.4 hint tables that we ignore:

    //  /T    thumbnail
    //  /A    thread information
    //  /E    named destination
    //  /V    interactive form
    //  /I    information dictionary
    //  /C    logical structure
    //  /L    page label

    // Individual hint table offsets
    QPDFObjectHandle HS = H0.getKey("/S"); // shared object
    QPDFObjectHandle HO = H0.getKey("/O"); // outline

    PointerHolder<Buffer> hbp = pb.getBuffer();
    Buffer* hb = hbp.getPointer();
    unsigned char const* h_buf = hb->getBuffer();
    int h_size = hb->getSize();

    readHPageOffset(BitStream(h_buf, h_size));

    int HSi = HS.getIntValue();
    if ((HSi < 0) || (HSi >= h_size))
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                      "linearization hint table",
                      this->file->getLastOffset(),
                      "/S (shared object) offset is out of bounds");
    }
    readHSharedObject(BitStream(h_buf + HSi, h_size - HSi));

    if (HO.isInteger())
    {
	int HOi = HO.getIntValue();
        if ((HOi < 0) || (HOi >= h_size))
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "linearization hint table",
                          this->file->getLastOffset(),
                          "/O (outline) offset is out of bounds");
        }
	readHGeneric(BitStream(h_buf + HOi, h_size - HOi),
		     this->outline_hints);
    }
}

QPDFObjectHandle
QPDF::readHintStream(Pipeline& pl, qpdf_offset_t offset, size_t length)
{
    int obj;
    int gen;
    QPDFObjectHandle H = readObjectAtOffset(
	false, offset, "linearization hint stream", -1, 0, obj, gen);
    ObjCache& oc = this->obj_cache[QPDFObjGen(obj, gen)];
    qpdf_offset_t min_end_offset = oc.end_before_space;
    qpdf_offset_t max_end_offset = oc.end_after_space;
    if (! H.isStream())
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "linearization dictionary",
		      this->file->getLastOffset(),
		      "hint table is not a stream");
    }

    QPDFObjectHandle Hdict = H.getDict();

    // Some versions of Acrobat make /Length indirect and place it
    // immediately after the stream, increasing length to cover it,
    // even though the specification says all objects in the
    // linearization parameter dictionary must be direct.  We have to
    // get the file position of the end of length in this case.
    QPDFObjectHandle length_obj = Hdict.getKey("/Length");
    if (length_obj.isIndirect())
    {
	QTC::TC("qpdf", "QPDF hint table length indirect");
	// Force resolution
	(void) length_obj.getIntValue();
	ObjCache& oc = this->obj_cache[length_obj.getObjGen()];
	min_end_offset = oc.end_before_space;
	max_end_offset = oc.end_after_space;
    }
    else
    {
	QTC::TC("qpdf", "QPDF hint table length direct");
    }
    qpdf_offset_t computed_end = offset + length;
    if ((computed_end < min_end_offset) ||
	(computed_end > max_end_offset))
    {
	*out_stream << "expected = " << computed_end
		    << "; actual = " << min_end_offset << ".."
		    << max_end_offset << std::endl;
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "linearization dictionary",
		      this->file->getLastOffset(),
		      "hint table length mismatch");
    }
    H.pipeStreamData(&pl, true, false, false);
    return Hdict;
}

void
QPDF::readHPageOffset(BitStream h)
{
    // All comments referring to the PDF spec refer to the spec for
    // version 1.4.

    HPageOffset& t = this->page_offset_hints;

    t.min_nobjects = h.getBits(32);		  	    // 1
    t.first_page_offset = h.getBits(32);		    // 2
    t.nbits_delta_nobjects = h.getBits(16);		    // 3
    t.min_page_length = h.getBits(32);			    // 4
    t.nbits_delta_page_length = h.getBits(16);		    // 5
    t.min_content_offset = h.getBits(32);		    // 6
    t.nbits_delta_content_offset = h.getBits(16);	    // 7
    t.min_content_length = h.getBits(32);		    // 8
    t.nbits_delta_content_length = h.getBits(16);	    // 9
    t.nbits_nshared_objects = h.getBits(16);		    // 10
    t.nbits_shared_identifier = h.getBits(16);		    // 11
    t.nbits_shared_numerator = h.getBits(16);		    // 12
    t.shared_denominator = h.getBits(16);		    // 13

    std::vector<HPageOffsetEntry>& entries = t.entries;
    entries.clear();
    unsigned int nitems = this->linp.npages;
    load_vector_int(h, nitems, entries,
		    t.nbits_delta_nobjects,
		    &HPageOffsetEntry::delta_nobjects);
    load_vector_int(h, nitems, entries,
		    t.nbits_delta_page_length,
		    &HPageOffsetEntry::delta_page_length);
    load_vector_int(h, nitems, entries,
		    t.nbits_nshared_objects,
		    &HPageOffsetEntry::nshared_objects);
    load_vector_vector(h, nitems, entries,
		       &HPageOffsetEntry::nshared_objects,
		       t.nbits_shared_identifier,
		       &HPageOffsetEntry::shared_identifiers);
    load_vector_vector(h, nitems, entries,
		       &HPageOffsetEntry::nshared_objects,
		       t.nbits_shared_numerator,
		       &HPageOffsetEntry::shared_numerators);
    load_vector_int(h, nitems, entries,
		    t.nbits_delta_content_offset,
		    &HPageOffsetEntry::delta_content_offset);
    load_vector_int(h, nitems, entries,
		    t.nbits_delta_content_length,
		    &HPageOffsetEntry::delta_content_length);
}

void
QPDF::readHSharedObject(BitStream h)
{
    HSharedObject& t = this->shared_object_hints;

    t.first_shared_obj = h.getBits(32);			    // 1
    t.first_shared_offset = h.getBits(32);		    // 2
    t.nshared_first_page = h.getBits(32);		    // 3
    t.nshared_total = h.getBits(32);			    // 4
    t.nbits_nobjects = h.getBits(16);			    // 5
    t.min_group_length = h.getBits(32);			    // 6
    t.nbits_delta_group_length = h.getBits(16);		    // 7

    QTC::TC("qpdf", "QPDF lin nshared_total > nshared_first_page",
	    (t.nshared_total > t.nshared_first_page) ? 1 : 0);

    std::vector<HSharedObjectEntry>& entries = t.entries;
    entries.clear();
    int nitems = t.nshared_total;
    load_vector_int(h, nitems, entries,
		    t.nbits_delta_group_length,
		    &HSharedObjectEntry::delta_group_length);
    load_vector_int(h, nitems, entries,
		    1, &HSharedObjectEntry::signature_present);
    for (int i = 0; i < nitems; ++i)
    {
	if (entries.at(i).signature_present)
	{
	    // Skip 128-bit MD5 hash.  These are not supported by
	    // acrobat, so they should probably never be there.  We
	    // have no test case for this.
	    for (int j = 0; j < 4; ++j)
	    {
		(void) h.getBits(32);
	    }
	}
    }
    load_vector_int(h, nitems, entries,
		    t.nbits_nobjects,
		    &HSharedObjectEntry::nobjects_minus_one);
}

void
QPDF::readHGeneric(BitStream h, HGeneric& t)
{
    t.first_object = h.getBits(32);			    // 1
    t.first_object_offset = h.getBits(32);		    // 2
    t.nobjects = h.getBits(32);				    // 3
    t.group_length = h.getBits(32);			    // 4
}

bool
QPDF::checkLinearizationInternal()
{
    // All comments referring to the PDF spec refer to the spec for
    // version 1.4.

    std::list<std::string> errors;
    std::list<std::string> warnings;

    // Check all values in linearization parameter dictionary

    LinParameters& p = this->linp;

    // L: file size in bytes -- checked by isLinearized

    // O: object number of first page
    std::vector<QPDFObjectHandle> const& pages = getAllPages();
    if (p.first_page_object != pages.at(0).getObjectID())
    {
	QTC::TC("qpdf", "QPDF err /O mismatch");
	errors.push_back("first page object (/O) mismatch");
    }

    // N: number of pages
    int npages = pages.size();
    if (p.npages != npages)
    {
	// Not tested in the test suite
	errors.push_back("page count (/N) mismatch");
    }

    for (int i = 0; i < npages; ++i)
    {
	QPDFObjectHandle const& page = pages.at(i);
	QPDFObjGen og(page.getObjGen());
	if (this->xref_table[og].getType() == 2)
	{
	    errors.push_back("page dictionary for page " +
			     QUtil::int_to_string(i) + " is compressed");
	}
    }

    // T: offset of whitespace character preceding xref entry for object 0
    this->file->seek(p.xref_zero_offset, SEEK_SET);
    while (1)
    {
	char ch;
	this->file->read(&ch, 1);
	if (! ((ch == ' ') || (ch == '\r') || (ch == '\n')))
	{
	    this->file->seek(-1, SEEK_CUR);
	    break;
	}
    }
    if (this->file->tell() != this->first_xref_item_offset)
    {
	QTC::TC("qpdf", "QPDF err /T mismatch");
	errors.push_back("space before first xref item (/T) mismatch "
			 "(computed = " +
			 QUtil::int_to_string(this->first_xref_item_offset) +
			 "; file = " +
			 QUtil::int_to_string(this->file->tell()));
    }

    // P: first page number -- Implementation note 124 says Acrobat
    // ignores this value, so we will too.

    // Check numbering of compressed objects in each xref section.
    // For linearized files, all compressed objects are supposed to be
    // at the end of the containing xref section if any object streams
    // are in use.

    if (this->uncompressed_after_compressed)
    {
	errors.push_back("linearized file contains an uncompressed object"
			 " after a compressed one in a cross-reference stream");
    }

    // Further checking requires optimization and order calculation.
    // Don't allow optimization to make changes.  If it has to, then
    // the file is not properly linearized.  We use the xref table to
    // figure out which objects are compressed and which are
    // uncompressed.
    { // local scope
	std::map<int, int> object_stream_data;
	for (std::map<QPDFObjGen, QPDFXRefEntry>::const_iterator iter =
		 this->xref_table.begin();
	     iter != this->xref_table.end(); ++iter)
	{
	    QPDFObjGen const& og = (*iter).first;
	    QPDFXRefEntry const& entry = (*iter).second;
	    if (entry.getType() == 2)
	    {
		object_stream_data[og.getObj()] = entry.getObjStreamNumber();
	    }
	}
	optimize(object_stream_data, false);
	calculateLinearizationData(object_stream_data);
    }

    // E: offset of end of first page -- Implementation note 123 says
    // Acrobat includes on extra object here by mistake.  pdlin fails
    // to place thumbnail images in section 9, so when thumbnails are
    // present, it also gets the wrong value for /E.  It also doesn't
    // count outlines here when it should even though it places them
    // in part 6.  This code fails to put thread information
    // dictionaries in part 9, so it actually gets the wrong value for
    // E when threads are present.  In that case, it would probably
    // agree with pdlin.  As of this writing, the test suite doesn't
    // contain any files with threads.

    if (this->part6.empty())
    {
        throw std::logic_error("linearization part 6 unexpectedly empty");
    }
    qpdf_offset_t min_E = -1;
    qpdf_offset_t max_E = -1;
    for (std::vector<QPDFObjectHandle>::iterator iter = this->part6.begin();
	 iter != this->part6.end(); ++iter)
    {
	QPDFObjGen og((*iter).getObjGen());
	if (this->obj_cache.count(og) == 0)
        {
            // All objects have to have been dereferenced to be classified.
            throw std::logic_error("linearization part6 object not in cache");
        }
	ObjCache const& oc = this->obj_cache[og];
	min_E = std::max(min_E, oc.end_before_space);
	max_E = std::max(max_E, oc.end_after_space);
    }
    if ((p.first_page_end < min_E) || (p.first_page_end > max_E))
    {
	QTC::TC("qpdf", "QPDF warn /E mismatch");
	warnings.push_back("end of first page section (/E) mismatch: /E = " +
			   QUtil::int_to_string(p.first_page_end) +
			   "; computed = " +
			   QUtil::int_to_string(min_E) + ".." +
			   QUtil::int_to_string(max_E));
    }

    // Check hint tables

    std::map<int, int> shared_idx_to_obj;
    checkHSharedObject(errors, warnings, pages, shared_idx_to_obj);
    checkHPageOffset(errors, warnings, pages, shared_idx_to_obj);
    checkHOutlines(warnings);

    // Report errors

    bool result = true;

    if (! errors.empty())
    {
	result = false;
	for (std::list<std::string>::iterator iter = errors.begin();
	     iter != errors.end(); ++iter)
	{
	    *out_stream << "ERROR: " << (*iter) << std::endl;
	}
    }

    if (! warnings.empty())
    {
	result = false;
	for (std::list<std::string>::iterator iter = warnings.begin();
	     iter != warnings.end(); ++iter)
	{
	    *out_stream << "WARNING: " << (*iter) << std::endl;
	}
    }

    return result;
}

qpdf_offset_t
QPDF::maxEnd(ObjUser const& ou)
{
    assert(this->obj_user_to_objects.count(ou) > 0);
    std::set<QPDFObjGen> const& ogs = this->obj_user_to_objects[ou];
    qpdf_offset_t end = 0;
    for (std::set<QPDFObjGen>::const_iterator iter = ogs.begin();
	 iter != ogs.end(); ++iter)
    {
	QPDFObjGen const& og = *iter;
	assert(this->obj_cache.count(og) > 0);
	end = std::max(end, this->obj_cache[og].end_after_space);
    }
    return end;
}

qpdf_offset_t
QPDF::getLinearizationOffset(QPDFObjGen const& og)
{
    QPDFXRefEntry entry = this->xref_table[og];
    qpdf_offset_t result = 0;
    switch (entry.getType())
    {
      case 1:
	result = entry.getOffset();
	break;

      case 2:
	// For compressed objects, return the offset of the object
	// stream that contains them.
	result = getLinearizationOffset(
            QPDFObjGen(entry.getObjStreamNumber(), 0));
	break;

      default:
	throw std::logic_error(
	    "getLinearizationOffset called for xref entry not of type 1 or 2");
	break;
    }
    return result;
}

QPDFObjectHandle
QPDF::getUncompressedObject(QPDFObjectHandle& obj,
			    std::map<int, int> const& object_stream_data)
{
    if (obj.isNull() || (object_stream_data.count(obj.getObjectID()) == 0))
    {
	return obj;
    }
    else
    {
	int repl = (*(object_stream_data.find(obj.getObjectID()))).second;
	return objGenToIndirect(QPDFObjGen(repl, 0));
    }
}

int
QPDF::lengthNextN(int first_object, int n,
		  std::list<std::string>& errors)
{
    int length = 0;
    for (int i = 0; i < n; ++i)
    {
	QPDFObjGen og(first_object + i, 0);
	if (this->xref_table.count(og) == 0)
	{
	    errors.push_back(
		"no xref table entry for " +
		QUtil::int_to_string(first_object + i) + " 0");
	}
	else
	{
	    assert(this->obj_cache.count(og) > 0);
	    length += this->obj_cache[og].end_after_space -
		getLinearizationOffset(og);
	}
    }
    return length;
}

void
QPDF::checkHPageOffset(std::list<std::string>& errors,
		       std::list<std::string>& warnings,
		       std::vector<QPDFObjectHandle> const& pages,
		       std::map<int, int>& shared_idx_to_obj)
{
    // Implementation note 126 says Acrobat always sets
    // delta_content_offset and delta_content_length in the page
    // offset header dictionary to 0.  It also states that
    // min_content_offset in the per-page information is always 0,
    // which is an incorrect value.

    // Implementation note 127 explains that Acrobat always sets item
    // 8 (min_content_length) to zero, item 9
    // (nbits_delta_content_length) to the value of item 5
    // (nbits_delta_page_length), and item 7 of each per-page hint
    // table (delta_content_length) to item 2 (delta_page_length) of
    // that entry.  Acrobat ignores these values when reading files.

    // Empirically, it also seems that Acrobat sometimes puts items
    // under a page's /Resources dictionary in with shared objects
    // even when they are private.

    unsigned int npages = pages.size();
    int table_offset = adjusted_offset(
	this->page_offset_hints.first_page_offset);
    QPDFObjGen first_page_og(pages.at(0).getObjGen());
    assert(this->xref_table.count(first_page_og) > 0);
    int offset = getLinearizationOffset(first_page_og);
    if (table_offset != offset)
    {
	warnings.push_back("first page object offset mismatch");
    }

    for (unsigned int pageno = 0; pageno < npages; ++pageno)
    {
	QPDFObjGen page_og(pages.at(pageno).getObjGen());
	int first_object = page_og.getObj();
	assert(this->xref_table.count(page_og) > 0);
	offset = getLinearizationOffset(page_og);

	HPageOffsetEntry& he = this->page_offset_hints.entries.at(pageno);
	CHPageOffsetEntry& ce = this->c_page_offset_data.entries.at(pageno);
	int h_nobjects = he.delta_nobjects +
	    this->page_offset_hints.min_nobjects;
	if (h_nobjects != ce.nobjects)
	{
	    // This happens with pdlin when there are thumbnails.
	    warnings.push_back(
		"object count mismatch for page " +
		QUtil::int_to_string(pageno) + ": hint table = " +
		QUtil::int_to_string(h_nobjects) + "; computed = " +
		QUtil::int_to_string(ce.nobjects));
	}

	// Use value for number of objects in hint table rather than
	// computed value if there is a discrepancy.
	int length = lengthNextN(first_object, h_nobjects, errors);
	int h_length = he.delta_page_length +
	    this->page_offset_hints.min_page_length;
	if (length != h_length)
	{
	    // This condition almost certainly indicates a bad hint
	    // table or a bug in this code.
	    errors.push_back(
		"page length mismatch for page " +
		QUtil::int_to_string(pageno) + ": hint table = " +
		QUtil::int_to_string(h_length) + "; computed length = " +
		QUtil::int_to_string(length) + " (offset = " +
		QUtil::int_to_string(offset) + ")");
	}

	offset += h_length;

	// Translate shared object indexes to object numbers.
	std::set<int> hint_shared;
	std::set<int> computed_shared;

	if ((pageno == 0) && (he.nshared_objects > 0))
	{
	    // pdlin and Acrobat both do this even though the spec
	    // states clearly and unambiguously that they should not.
	    warnings.push_back("page 0 has shared identifier entries");
	}

	for (int i = 0; i < he.nshared_objects; ++i)
	{
	    int idx = he.shared_identifiers.at(i);
	    if (shared_idx_to_obj.count(idx) == 0)
            {
                throw std::logic_error(
                    "unable to get object for item in"
                    " shared objects hint table");
            }
	    hint_shared.insert(shared_idx_to_obj[idx]);
	}

	for (int i = 0; i < ce.nshared_objects; ++i)
	{
	    int idx = ce.shared_identifiers.at(i);
	    if (idx >= this->c_shared_object_data.nshared_total)
            {
                throw std::logic_error(
                    "index out of bounds for shared object hint table");
            }
	    int obj = this->c_shared_object_data.entries.at(idx).object;
	    computed_shared.insert(obj);
	}

	for (std::set<int>::iterator iter = hint_shared.begin();
	     iter != hint_shared.end(); ++iter)
	{
	    if (! computed_shared.count(*iter))
	    {
		// pdlin puts thumbnails here even though it shouldn't
		warnings.push_back(
		    "page " + QUtil::int_to_string(pageno) +
		    ": shared object " + QUtil::int_to_string(*iter) +
		    ": in hint table but not computed list");
	    }
	}

	for (std::set<int>::iterator iter = computed_shared.begin();
	     iter != computed_shared.end(); ++iter)
	{
	    if (! hint_shared.count(*iter))
	    {
		// Acrobat does not put some things including at least
		// built-in fonts and procsets here, at least in some
		// cases.
		warnings.push_back(
		    "page " + QUtil::int_to_string(pageno) +
		    ": shared object " + QUtil::int_to_string(*iter) +
		    ": in computed list but not hint table");
	    }
	}
    }
}

void
QPDF::checkHSharedObject(std::list<std::string>& errors,
			 std::list<std::string>& warnings,
			 std::vector<QPDFObjectHandle> const& pages,
			 std::map<int, int>& idx_to_obj)
{
    // Implementation note 125 says shared object groups always
    // contain only one object.  Implementation note 128 says that
    // Acrobat always nbits_nobjects to zero.  Implementation note 130
    // says that Acrobat does not support more than one shared object
    // per group.  These are all consistent.

    // Implementation note 129 states that MD5 signatures are not
    // implemented in Acrobat, so signature_present must always be
    // zero.

    // Implementation note 131 states that first_shared_obj and
    // first_shared_offset have meaningless values for single-page
    // files.

    // Empirically, Acrobat and pdlin generate incorrect values for
    // these whenever there are no shared objects not referenced by
    // the first page (i.e., nshared_total == nshared_first_page).

    HSharedObject& so = this->shared_object_hints;
    if (so.nshared_total < so.nshared_first_page)
    {
	errors.push_back("shared object hint table: ntotal < nfirst_page");
    }
    else
    {
	// The first nshared_first_page objects are consecutive
	// objects starting with the first page object.  The rest are
	// consecutive starting from the first_shared_obj object.
	int cur_object = pages.at(0).getObjectID();
	for (int i = 0; i < so.nshared_total; ++i)
	{
	    if (i == so.nshared_first_page)
	    {
		QTC::TC("qpdf", "QPDF lin check shared past first page");
		if (this->part8.empty())
		{
		    errors.push_back(
			"part 8 is empty but nshared_total > "
			"nshared_first_page");
		}
		else
		{
		    int obj = this->part8.at(0).getObjectID();
		    if (obj != so.first_shared_obj)
		    {
			errors.push_back(
			    "first shared object number mismatch: "
			    "hint table = " +
			    QUtil::int_to_string(so.first_shared_obj) +
			    "; computed = " +
			    QUtil::int_to_string(obj));
		    }
		}

		cur_object = so.first_shared_obj;

		QPDFObjGen og(cur_object, 0);
		assert(this->xref_table.count(og) > 0);
		int offset = getLinearizationOffset(og);
		int h_offset = adjusted_offset(so.first_shared_offset);
		if (offset != h_offset)
		{
		    errors.push_back(
			"first shared object offset mismatch: hint table = " +
			QUtil::int_to_string(h_offset) + "; computed = " +
			QUtil::int_to_string(offset));
		}
	    }

	    idx_to_obj[i] = cur_object;
	    HSharedObjectEntry& se = so.entries.at(i);
	    int nobjects = se.nobjects_minus_one + 1;
	    int length = lengthNextN(cur_object, nobjects, errors);
	    int h_length = so.min_group_length + se.delta_group_length;
	    if (length != h_length)
	    {
		errors.push_back(
		    "shared object " + QUtil::int_to_string(i) +
		    " length mismatch: hint table = " +
		    QUtil::int_to_string(h_length) + "; computed = " +
		    QUtil::int_to_string(length));
	    }
	    cur_object += nobjects;
	}
    }
}

void
QPDF::checkHOutlines(std::list<std::string>& warnings)
{
    // Empirically, Acrobat generates the correct value for the object
    // number but incorrectly stores the next object number's offset
    // as the offset, at least when outlines appear in part 6.  It
    // also generates an incorrect value for length (specifically, the
    // length that would cover the correct number of objects from the
    // wrong starting place).  pdlin appears to generate correct
    // values in those cases.

    if (this->c_outline_data.nobjects == this->outline_hints.nobjects)
    {
	if (this->c_outline_data.nobjects == 0)
	{
	    return;
	}

	if (this->c_outline_data.first_object ==
	    this->outline_hints.first_object)
	{
	    // Check length and offset.  Acrobat gets these wrong.
	    QPDFObjectHandle outlines = getRoot().getKey("/Outlines");
            if (! outlines.isIndirect())
            {
                // This case is not exercised in test suite since not
                // permitted by the spec, but if this does occur, the
                // code below would fail.
		warnings.push_back(
		    "/Outlines key of root dictionary is not indirect");
                return;
            }
	    QPDFObjGen og(outlines.getObjGen());
	    assert(this->xref_table.count(og) > 0);
	    int offset = getLinearizationOffset(og);
	    ObjUser ou(ObjUser::ou_root_key, "/Outlines");
	    int length = maxEnd(ou) - offset;
	    int table_offset =
		adjusted_offset(this->outline_hints.first_object_offset);
	    if (offset != table_offset)
	    {
		warnings.push_back(
		    "incorrect offset in outlines table: hint table = " +
		    QUtil::int_to_string(table_offset) +
		    "; computed = " + QUtil::int_to_string(offset));
	    }
	    int table_length = this->outline_hints.group_length;
	    if (length != table_length)
	    {
		warnings.push_back(
		    "incorrect length in outlines table: hint table = " +
		    QUtil::int_to_string(table_length) +
		    "; computed = " + QUtil::int_to_string(length));
	    }
	}
	else
	{
	    warnings.push_back("incorrect first object number in outline "
			       "hints table.");
	}
    }
    else
    {
	warnings.push_back("incorrect object count in outline hint table");
    }
}

void
QPDF::showLinearizationData()
{
    try
    {
	readLinearizationData();
	checkLinearizationInternal();
	dumpLinearizationDataInternal();
    }
    catch (QPDFExc& e)
    {
	*out_stream << e.what() << std::endl;
    }
}

void
QPDF::dumpLinearizationDataInternal()
{
    *out_stream << this->file->getName() << ": linearization data:" << std::endl
		<< std::endl;

    *out_stream
	<< "file_size: " << this->linp.file_size << std::endl
	<< "first_page_object: " << this->linp.first_page_object << std::endl
	<< "first_page_end: " << this->linp.first_page_end << std::endl
	<< "npages: " << this->linp.npages << std::endl
	<< "xref_zero_offset: " << this->linp.xref_zero_offset << std::endl
	<< "first_page: " << this->linp.first_page << std::endl
	<< "H_offset: " << this->linp.H_offset << std::endl
	<< "H_length: " << this->linp.H_length << std::endl
	<< std::endl;

    *out_stream << "Page Offsets Hint Table" << std::endl
		<< std::endl;
    dumpHPageOffset();
    *out_stream << std::endl
		<< "Shared Objects Hint Table" << std::endl
		<< std::endl;
    dumpHSharedObject();

    if (this->outline_hints.nobjects > 0)
    {
	*out_stream << std::endl
		    << "Outlines Hint Table" << std::endl
		    << std::endl;
	dumpHGeneric(this->outline_hints);
    }
}

int
QPDF::adjusted_offset(int offset)
{
    // All offsets >= H_offset have to be increased by H_length
    // since all hint table location values disregard the hint table
    // itself.
    if (offset >= this->linp.H_offset)
    {
	return offset + this->linp.H_length;
    }
    return offset;
}


void
QPDF::dumpHPageOffset()
{
    HPageOffset& t = this->page_offset_hints;
    *out_stream
	<< "min_nobjects: " << t.min_nobjects
	<< std::endl
	<< "first_page_offset: " << adjusted_offset(t.first_page_offset)
	<< std::endl
	<< "nbits_delta_nobjects: " << t.nbits_delta_nobjects
	<< std::endl
	<< "min_page_length: " << t.min_page_length
	<< std::endl
	<< "nbits_delta_page_length: " << t.nbits_delta_page_length
	<< std::endl
	<< "min_content_offset: " << t.min_content_offset
	<< std::endl
	<< "nbits_delta_content_offset: " << t.nbits_delta_content_offset
	<< std::endl
	<< "min_content_length: " << t.min_content_length
	<< std::endl
	<< "nbits_delta_content_length: " << t.nbits_delta_content_length
	<< std::endl
	<< "nbits_nshared_objects: " << t.nbits_nshared_objects
	<< std::endl
	<< "nbits_shared_identifier: " << t.nbits_shared_identifier
	<< std::endl
	<< "nbits_shared_numerator: " << t.nbits_shared_numerator
	<< std::endl
	<< "shared_denominator: " << t.shared_denominator
	<< std::endl;

    for (int i1 = 0; i1 < this->linp.npages; ++i1)
    {
	HPageOffsetEntry& pe = t.entries.at(i1);
	*out_stream
	    << "Page " << i1 << ":" << std::endl
	    << "  nobjects: " << pe.delta_nobjects + t.min_nobjects
	    << std::endl
	    << "  length: " << pe.delta_page_length + t.min_page_length
	    << std::endl
	    // content offset is relative to page, not file
	    << "  content_offset: "
	    << pe.delta_content_offset + t.min_content_offset << std::endl
	    << "  content_length: "
	    << pe.delta_content_length + t.min_content_length << std::endl
	    << "  nshared_objects: " << pe.nshared_objects << std::endl;
	for (int i2 = 0; i2 < pe.nshared_objects; ++i2)
	{
	    *out_stream << "    identifier " << i2 << ": "
			<< pe.shared_identifiers.at(i2) << std::endl;
	    *out_stream << "    numerator " << i2 << ": "
			<< pe.shared_numerators.at(i2) << std::endl;
	}
    }
}

void
QPDF::dumpHSharedObject()
{
    HSharedObject& t = this->shared_object_hints;
    *out_stream
	<< "first_shared_obj: " << t.first_shared_obj
	<< std::endl
	<< "first_shared_offset: " << adjusted_offset(t.first_shared_offset)
	<< std::endl
	<< "nshared_first_page: " << t.nshared_first_page
	<< std::endl
	<< "nshared_total: " << t.nshared_total
	<< std::endl
	<< "nbits_nobjects: " << t.nbits_nobjects
	<< std::endl
	<< "min_group_length: " << t.min_group_length
	<< std::endl
	<< "nbits_delta_group_length: " << t.nbits_delta_group_length
	<< std::endl;

    for (int i = 0; i < t.nshared_total; ++i)
    {
	HSharedObjectEntry& se = t.entries.at(i);
	*out_stream << "Shared Object " << i << ":" << std::endl;
	*out_stream << "  group length: "
		    << se.delta_group_length + t.min_group_length << std::endl;
	// PDF spec says signature present nobjects_minus_one are
	// always 0, so print them only if they have a non-zero value.
	if (se.signature_present)
	{
	    *out_stream << "  signature present" << std::endl;
	}
	if (se.nobjects_minus_one != 0)
	{
	    *out_stream << "  nobjects: "
			<< se.nobjects_minus_one + 1 << std::endl;
	}
    }
}

void
QPDF::dumpHGeneric(HGeneric& t)
{
    *out_stream
	<< "first_object: " << t.first_object
	<< std::endl
	<< "first_object_offset: " << adjusted_offset(t.first_object_offset)
	<< std::endl
	<< "nobjects: " << t.nobjects
	<< std::endl
	<< "group_length: " << t.group_length
	<< std::endl;
}

QPDFObjectHandle
QPDF::objGenToIndirect(QPDFObjGen const& og)
{
    return getObjectByID(og.getObj(), og.getGen());
}

void
QPDF::calculateLinearizationData(std::map<int, int> const& object_stream_data)
{
    // This function calculates the ordering of objects, divides them
    // into the appropriate parts, and computes some values for the
    // linearization parameter dictionary and hint tables.  The file
    // must be optimized (via calling optimize()) prior to calling
    // this function.  Note that actual offsets and lengths are not
    // computed here, but anything related to object ordering is.

    if (this->object_to_obj_users.empty())
    {
	// Note that we can't call optimize here because we don't know
	// whether it should be called with or without allow changes.
	throw std::logic_error(
	    "INTERNAL ERROR: QPDF::calculateLinearizationData "
	    "called before optimize()");
    }

    // Separate objects into the categories sufficient for us to
    // determine which part of the linearized file should contain the
    // object.  This categorization is useful for other purposes as
    // well.  Part numbers refer to version 1.4 of the PDF spec.

    // Parts 1, 3, 5, 10, and 11 don't contain any objects from the
    // original file (except the trailer dictionary in part 11).

    // Part 4 is the document catalog (root) and the following root
    // keys: /ViewerPreferences, /PageMode, /Threads, /OpenAction,
    // /AcroForm, /Encrypt.  Note that Thread information dictionaries
    // are supposed to appear in part 9, but we are disregarding that
    // recommendation for now.

    // Part 6 is the first page section.  It includes all remaining
    // objects referenced by the first page including shared objects
    // but not including thumbnails.  Additionally, if /PageMode is
    // /Outlines, then information from /Outlines also appears here.

    // Part 7 contains remaining objects private to pages other than
    // the first page.

    // Part 8 contains all remaining shared objects except those that
    // are shared only within thumbnails.

    // Part 9 contains all remaining objects.

    // We sort objects into the following categories:

    //   * open_document: part 4

    //   * first_page_private: part 6

    //   * first_page_shared: part 6

    //   * other_page_private: part 7

    //   * other_page_shared: part 8

    //   * thumbnail_private: part 9

    //   * thumbnail_shared: part 9

    //   * other: part 9

    //   * outlines: part 6 or 9

    this->part4.clear();
    this->part6.clear();
    this->part7.clear();
    this->part8.clear();
    this->part9.clear();
    this->c_linp = LinParameters();
    this->c_page_offset_data = CHPageOffset();
    this->c_shared_object_data = CHSharedObject();
    this->c_outline_data = HGeneric();

    QPDFObjectHandle root = getRoot();
    bool outlines_in_first_page = false;
    QPDFObjectHandle pagemode = root.getKey("/PageMode");
    QTC::TC("qpdf", "QPDF categorize pagemode present",
	    pagemode.isName() ? 1 : 0);
    if (pagemode.isName())
    {
	if (pagemode.getName() == "/UseOutlines")
	{
	    if (root.hasKey("/Outlines"))
	    {
		outlines_in_first_page = true;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF UseOutlines but no Outlines");
	    }
	}
	QTC::TC("qpdf", "QPDF categorize pagemode outlines",
		outlines_in_first_page ? 1 : 0);
    }

    std::set<std::string> open_document_keys;
    open_document_keys.insert("/ViewerPreferences");
    open_document_keys.insert("/PageMode");
    open_document_keys.insert("/Threads");
    open_document_keys.insert("/OpenAction");
    open_document_keys.insert("/AcroForm");

    std::set<QPDFObjGen> lc_open_document;
    std::set<QPDFObjGen> lc_first_page_private;
    std::set<QPDFObjGen> lc_first_page_shared;
    std::set<QPDFObjGen> lc_other_page_private;
    std::set<QPDFObjGen> lc_other_page_shared;
    std::set<QPDFObjGen> lc_thumbnail_private;
    std::set<QPDFObjGen> lc_thumbnail_shared;
    std::set<QPDFObjGen> lc_other;
    std::set<QPDFObjGen> lc_outlines;
    std::set<QPDFObjGen> lc_root;

    for (std::map<QPDFObjGen, std::set<ObjUser> >::iterator oiter =
	     this->object_to_obj_users.begin();
	 oiter != this->object_to_obj_users.end(); ++oiter)
    {
	QPDFObjGen const& og = (*oiter).first;

	std::set<ObjUser>& ous = (*oiter).second;

	bool in_open_document = false;
	bool in_first_page = false;
	int other_pages = 0;
	int thumbs = 0;
	int others = 0;
	bool in_outlines = false;
	bool is_root = false;

	for (std::set<ObjUser>::iterator uiter = ous.begin();
	     uiter != ous.end(); ++uiter)
	{
	    ObjUser const& ou = *uiter;
	    switch (ou.ou_type)
	    {
	      case ObjUser::ou_trailer_key:
		if (ou.key == "/Encrypt")
		{
		    in_open_document = true;
		}
		else
		{
		    ++others;
		}
		break;

	      case ObjUser::ou_thumb:
		++thumbs;
		break;

	      case ObjUser::ou_root_key:
		if (open_document_keys.count(ou.key) > 0)
		{
		    in_open_document = true;
		}
		else if (ou.key == "/Outlines")
		{
		    in_outlines = true;
		}
		else
		{
		    ++others;
		}
		break;

	      case ObjUser::ou_page:
		if (ou.pageno == 0)
		{
		    in_first_page = true;
		}
		else
		{
		    ++other_pages;
		}
		break;

	      case ObjUser::ou_root:
		is_root = true;
		break;

	      case ObjUser::ou_bad:
		throw std::logic_error(
		    "INTERNAL ERROR: QPDF::calculateLinearizationData: "
		    "invalid user type");
		break;
	    }
	}

	if (is_root)
	{
	    lc_root.insert(og);
	}
	else if (in_outlines)
	{
	    lc_outlines.insert(og);
	}
	else if (in_open_document)
	{
	    lc_open_document.insert(og);
	}
	else if ((in_first_page) &&
		 (others == 0) && (other_pages == 0) && (thumbs == 0))
	{
	    lc_first_page_private.insert(og);
	}
	else if (in_first_page)
	{
	    lc_first_page_shared.insert(og);
	}
	else if ((other_pages == 1) && (others == 0) && (thumbs == 0))
	{
	    lc_other_page_private.insert(og);
	}
	else if (other_pages > 1)
	{
	    lc_other_page_shared.insert(og);
	}
	else if ((thumbs == 1) && (others == 0))
	{
	    lc_thumbnail_private.insert(og);
	}
	else if (thumbs > 1)
	{
	    lc_thumbnail_shared.insert(og);
	}
	else
	{
	    lc_other.insert(og);
	}
    }

    // Generate ordering for objects in the output file.  Sometimes we
    // just dump right from a set into a vector.  Rather than
    // optimizing this by going straight into the vector, we'll leave
    // these phases separate for now.  That way, this section can be
    // concerned only with ordering, and the above section can be
    // considered only with categorization.  Note that sets of
    // QPDFObjGens are sorted by QPDFObjGen.  In a linearized file,
    // objects appear in sequence with the possible exception of hints
    // tables which we won't see here anyway.  That means that running
    // calculateLinearizationData() on a linearized file should give
    // results identical to the original file ordering.

    // We seem to traverse the page tree a lot in this code, but we
    // can address this for a future code optimization if necessary.
    // Premature optimization is the root of all evil.
    std::vector<QPDFObjectHandle> pages;
    { // local scope
	// Map all page objects to the containing object stream.  This
	// should be a no-op in a properly linearized file.
	std::vector<QPDFObjectHandle> t = getAllPages();
	for (std::vector<QPDFObjectHandle>::iterator iter = t.begin();
	     iter != t.end(); ++iter)
	{
	    pages.push_back(getUncompressedObject(*iter, object_stream_data));
	}
    }
    unsigned int npages = pages.size();

    // We will be initializing some values of the computed hint
    // tables.  Specifically, we can initialize any items that deal
    // with object numbers or counts but not any items that deal with
    // lengths or offsets.  The code that writes linearized files will
    // have to fill in these values during the first pass.  The
    // validation code can compute them relatively easily given the
    // rest of the information.

    // npages is the size of the existing pages vector, which has been
    // created by traversing the pages tree, and as such is a
    // reasonable size.
    this->c_linp.npages = npages;
    this->c_page_offset_data.entries = std::vector<CHPageOffsetEntry>(npages);

    // Part 4: open document objects.  We don't care about the order.

    assert(lc_root.size() == 1);
    this->part4.push_back(objGenToIndirect(*(lc_root.begin())));
    for (std::set<QPDFObjGen>::iterator iter = lc_open_document.begin();
	 iter != lc_open_document.end(); ++iter)
    {
	this->part4.push_back(objGenToIndirect(*iter));
    }

    // Part 6: first page objects.  Note: implementation note 124
    // states that Acrobat always treats page 0 as the first page for
    // linearization regardless of /OpenAction.  pdlin doesn't provide
    // any option to set this and also disregards /OpenAction.  We
    // will do the same.

    // First, place the actual first page object itself.
    QPDFObjGen first_page_og(pages.at(0).getObjGen());
    if (! lc_first_page_private.count(first_page_og))
    {
	throw std::logic_error(
	    "INTERNAL ERROR: QPDF::calculateLinearizationData: first page "
	    "object not in lc_first_page_private");
    }
    lc_first_page_private.erase(first_page_og);
    this->c_linp.first_page_object = pages.at(0).getObjectID();
    this->part6.push_back(pages.at(0));

    // The PDF spec "recommends" an order for the rest of the objects,
    // but we are going to disregard it except to the extent that it
    // groups private and shared objects contiguously for the sake of
    // hint tables.

    for (std::set<QPDFObjGen>::iterator iter = lc_first_page_private.begin();
	 iter != lc_first_page_private.end(); ++iter)
    {
	this->part6.push_back(objGenToIndirect(*iter));
    }

    for (std::set<QPDFObjGen>::iterator iter = lc_first_page_shared.begin();
	 iter != lc_first_page_shared.end(); ++iter)
    {
	this->part6.push_back(objGenToIndirect(*iter));
    }

    // Place the outline dictionary if it goes in the first page section.
    if (outlines_in_first_page)
    {
	pushOutlinesToPart(this->part6, lc_outlines, object_stream_data);
    }

    // Fill in page offset hint table information for the first page.
    // The PDF spec says that nshared_objects should be zero for the
    // first page.  pdlin does not appear to obey this, but it fills
    // in garbage values for all the shared object identifiers on the
    // first page.

    this->c_page_offset_data.entries.at(0).nobjects = this->part6.size();

    // Part 7: other pages' private objects

    // For each page in order:
    for (unsigned int i = 1; i < npages; ++i)
    {
	// Place this page's page object

	QPDFObjGen page_og(pages.at(i).getObjGen());
	if (! lc_other_page_private.count(page_og))
	{
	    throw std::logic_error(
		"INTERNAL ERROR: "
		"QPDF::calculateLinearizationData: page object for page " +
		QUtil::int_to_string(i) + " not in lc_other_page_private");
	}
	lc_other_page_private.erase(page_og);
	this->part7.push_back(pages.at(i));

	// Place all non-shared objects referenced by this page,
	// updating the page object count for the hint table.

	this->c_page_offset_data.entries.at(i).nobjects = 1;

	ObjUser ou(ObjUser::ou_page, i);
	assert(this->obj_user_to_objects.count(ou) > 0);
	std::set<QPDFObjGen> ogs = this->obj_user_to_objects[ou];
	for (std::set<QPDFObjGen>::iterator iter = ogs.begin();
	     iter != ogs.end(); ++iter)
	{
	    QPDFObjGen const& og = (*iter);
	    if (lc_other_page_private.count(og))
	    {
		lc_other_page_private.erase(og);
		this->part7.push_back(objGenToIndirect(og));
		++this->c_page_offset_data.entries.at(i).nobjects;
	    }
	}
    }
    // That should have covered all part7 objects.
    if (! lc_other_page_private.empty())
    {
	throw std::logic_error(
	    "INTERNAL ERROR:"
	    " QPDF::calculateLinearizationData: lc_other_page_private is "
	    "not empty after generation of part7");
    }

    // Part 8: other pages' shared objects

    // Order is unimportant.
    for (std::set<QPDFObjGen>::iterator iter = lc_other_page_shared.begin();
	 iter != lc_other_page_shared.end(); ++iter)
    {
	this->part8.push_back(objGenToIndirect(*iter));
    }

    // Part 9: other objects

    // The PDF specification makes recommendations on ordering here.
    // We follow them only to a limited extent.  Specifically, we put
    // the pages tree first, then private thumbnail objects in page
    // order, then shared thumbnail objects, and then outlines (unless
    // in part 6).  After that, we throw all remaining objects in
    // arbitrary order.

    // Place the pages tree.
    std::set<QPDFObjGen> pages_ogs =
	this->obj_user_to_objects[ObjUser(ObjUser::ou_root_key, "/Pages")];
    assert(! pages_ogs.empty());
    for (std::set<QPDFObjGen>::iterator iter = pages_ogs.begin();
	 iter != pages_ogs.end(); ++iter)
    {
	QPDFObjGen const& og = *iter;
	if (lc_other.count(og))
	{
	    lc_other.erase(og);
	    this->part9.push_back(objGenToIndirect(og));
	}
    }

    // Place private thumbnail images in page order.  Slightly more
    // information would be required if we were going to bother with
    // thumbnail hint tables.
    for (unsigned int i = 0; i < npages; ++i)
    {
	QPDFObjectHandle thumb = pages.at(i).getKey("/Thumb");
	thumb = getUncompressedObject(thumb, object_stream_data);
	if (! thumb.isNull())
	{
	    // Output the thumbnail itself
	    QPDFObjGen thumb_og(thumb.getObjGen());
	    if (lc_thumbnail_private.count(thumb_og))
	    {
		lc_thumbnail_private.erase(thumb_og);
		this->part9.push_back(thumb);
	    }
	    else
	    {
		// No internal error this time...there's nothing to
		// stop this object from having been referred to
		// somewhere else outside of a page's /Thumb, and if
		// it had been, there's nothing to prevent it from
		// having been in some set other than
		// lc_thumbnail_private.
	    }
	    std::set<QPDFObjGen>& ogs =
		this->obj_user_to_objects[ObjUser(ObjUser::ou_thumb, i)];
	    for (std::set<QPDFObjGen>::iterator iter = ogs.begin();
		 iter != ogs.end(); ++iter)
	    {
		QPDFObjGen const& og = *iter;
		if (lc_thumbnail_private.count(og))
		{
		    lc_thumbnail_private.erase(og);
		    this->part9.push_back(objGenToIndirect(og));
		}
	    }
	}
    }
    if (! lc_thumbnail_private.empty())
    {
	throw std::logic_error(
	    "INTERNAL ERROR: "
	    "QPDF::calculateLinearizationData: lc_thumbnail_private "
	    "not empty after placing thumbnails");
    }

    // Place shared thumbnail objects
    for (std::set<QPDFObjGen>::iterator iter = lc_thumbnail_shared.begin();
	 iter != lc_thumbnail_shared.end(); ++iter)
    {
	this->part9.push_back(objGenToIndirect(*iter));
    }

    // Place outlines unless in first page
    if (! outlines_in_first_page)
    {
	pushOutlinesToPart(this->part9, lc_outlines, object_stream_data);
    }

    // Place all remaining objects
    for (std::set<QPDFObjGen>::iterator iter = lc_other.begin();
	 iter != lc_other.end(); ++iter)
    {
	this->part9.push_back(objGenToIndirect(*iter));
    }

    // Make sure we got everything exactly once.

    unsigned int num_placed =
        this->part4.size() + this->part6.size() + this->part7.size() +
        this->part8.size() + this->part9.size();
    unsigned int num_wanted = this->object_to_obj_users.size();
    if (num_placed != num_wanted)
    {
	throw std::logic_error(
	    "INTERNAL ERROR: QPDF::calculateLinearizationData: wrong "
	    "number of objects placed (num_placed = " +
	    QUtil::int_to_string(num_placed) +
	    "; number of objects: " +
	    QUtil::int_to_string(num_wanted));
    }

    // Calculate shared object hint table information including
    // references to shared objects from page offset hint data.

    // The shared object hint table consists of all part 6 (whether
    // shared or not) in order followed by all part 8 objects in
    // order.  Add the objects to shared object data keeping a map of
    // object number to index.  Then populate the shared object
    // information for the pages.

    // Note that two objects never have the same object number, so we
    // can map from object number only without regards to generation.
    std::map<int, int> obj_to_index;

    this->c_shared_object_data.nshared_first_page = this->part6.size();
    this->c_shared_object_data.nshared_total =
	this->c_shared_object_data.nshared_first_page + this->part8.size();

    std::vector<CHSharedObjectEntry>& shared =
	this->c_shared_object_data.entries;
    for (std::vector<QPDFObjectHandle>::iterator iter = this->part6.begin();
	 iter != this->part6.end(); ++iter)
    {
	QPDFObjectHandle& oh = *iter;
	int obj = oh.getObjectID();
	obj_to_index[obj] = shared.size();
	shared.push_back(CHSharedObjectEntry(obj));
    }
    QTC::TC("qpdf", "QPDF lin part 8 empty", this->part8.empty() ? 1 : 0);
    if (! this->part8.empty())
    {
	this->c_shared_object_data.first_shared_obj =
	    this->part8.at(0).getObjectID();
	for (std::vector<QPDFObjectHandle>::iterator iter =
		 this->part8.begin();
	     iter != this->part8.end(); ++iter)
	{
	    QPDFObjectHandle& oh = *iter;
	    int obj = oh.getObjectID();
	    obj_to_index[obj] = shared.size();
	    shared.push_back(CHSharedObjectEntry(obj));
	}
    }
    if (static_cast<size_t>(this->c_shared_object_data.nshared_total) !=
        this->c_shared_object_data.entries.size())
    {
        throw std::logic_error(
            "shared object hint table has wrong number of entries");
    }

    // Now compute the list of shared objects for each page after the
    // first page.

    for (unsigned int i = 1; i < npages; ++i)
    {
	CHPageOffsetEntry& pe = this->c_page_offset_data.entries.at(i);
	ObjUser ou(ObjUser::ou_page, i);
	assert(this->obj_user_to_objects.count(ou) > 0);
	std::set<QPDFObjGen> const& ogs = this->obj_user_to_objects[ou];
	for (std::set<QPDFObjGen>::const_iterator iter = ogs.begin();
	     iter != ogs.end(); ++iter)
	{
	    QPDFObjGen const& og = *iter;
	    if ((this->object_to_obj_users[og].size() > 1) &&
		(obj_to_index.count(og.getObj()) > 0))
	    {
		int idx = obj_to_index[og.getObj()];
		++pe.nshared_objects;
		pe.shared_identifiers.push_back(idx);
	    }
	}
    }
}

void
QPDF::pushOutlinesToPart(
    std::vector<QPDFObjectHandle>& part,
    std::set<QPDFObjGen>& lc_outlines,
    std::map<int, int> const& object_stream_data)
{
    QPDFObjectHandle root = getRoot();
    QPDFObjectHandle outlines = root.getKey("/Outlines");
    if (outlines.isNull())
    {
	return;
    }
    outlines = getUncompressedObject(outlines, object_stream_data);
    QPDFObjGen outlines_og(outlines.getObjGen());
    QTC::TC("qpdf", "QPDF lin outlines in part",
	    ((&part == (&this->part6)) ? 0
	     : (&part == (&this->part9)) ? 1
	     : 9999));		// can't happen
    this->c_outline_data.first_object = outlines_og.getObj();
    this->c_outline_data.nobjects = 1;
    lc_outlines.erase(outlines_og);
    part.push_back(outlines);
    for (std::set<QPDFObjGen>::iterator iter = lc_outlines.begin();
	 iter != lc_outlines.end(); ++iter)
    {
	part.push_back(objGenToIndirect(*iter));
	++this->c_outline_data.nobjects;
    }
}

void
QPDF::getLinearizedParts(
    std::map<int, int> const& object_stream_data,
    std::vector<QPDFObjectHandle>& part4,
    std::vector<QPDFObjectHandle>& part6,
    std::vector<QPDFObjectHandle>& part7,
    std::vector<QPDFObjectHandle>& part8,
    std::vector<QPDFObjectHandle>& part9)
{
    calculateLinearizationData(object_stream_data);
    part4 = this->part4;
    part6 = this->part6;
    part7 = this->part7;
    part8 = this->part8;
    part9 = this->part9;
}

static inline int nbits(int val)
{
    return (val == 0 ? 0 : (1 + nbits(val >> 1)));
}

int
QPDF::outputLengthNextN(
    int in_object, int n,
    std::map<int, qpdf_offset_t> const& lengths,
    std::map<int, int> const& obj_renumber)
{
    // Figure out the length of a series of n consecutive objects in
    // the output file starting with whatever object in_object from
    // the input file mapped to.

    assert(obj_renumber.count(in_object) > 0);
    int first = (*(obj_renumber.find(in_object))).second;
    int length = 0;
    for (int i = 0; i < n; ++i)
    {
	assert(lengths.count(first + i) > 0);
	length += (*(lengths.find(first + i))).second;
    }
    return length;
}

void
QPDF::calculateHPageOffset(
    std::map<int, QPDFXRefEntry> const& xref,
    std::map<int, qpdf_offset_t> const& lengths,
    std::map<int, int> const& obj_renumber)
{
    // Page Offset Hint Table

    // We are purposely leaving some values set to their initial zero
    // values.

    std::vector<QPDFObjectHandle> const& pages = getAllPages();
    unsigned int npages = pages.size();
    CHPageOffset& cph = this->c_page_offset_data;
    std::vector<CHPageOffsetEntry>& cphe = cph.entries;

    // Calculate minimum and maximum values for number of objects per
    // page and page length.

    int min_nobjects = cphe.at(0).nobjects;
    int max_nobjects = min_nobjects;
    int min_length = outputLengthNextN(
	pages.at(0).getObjectID(), min_nobjects, lengths, obj_renumber);
    int max_length = min_length;
    int max_shared = cphe.at(0).nshared_objects;

    HPageOffset& ph = this->page_offset_hints;
    std::vector<HPageOffsetEntry>& phe = ph.entries;
    // npages is the size of the existing pages array.
    phe = std::vector<HPageOffsetEntry>(npages);

    for (unsigned int i = 0; i < npages; ++i)
    {
	// Calculate values for each page, assigning full values to
	// the delta items.  They will be adjusted later.

	// Repeat calculations for page 0 so we can assign to phe[i]
	// without duplicating those assignments.

	int nobjects = cphe.at(i).nobjects;
	int length = outputLengthNextN(
	    pages.at(i).getObjectID(), nobjects, lengths, obj_renumber);
	int nshared = cphe.at(i).nshared_objects;

	min_nobjects = std::min(min_nobjects, nobjects);
	max_nobjects = std::max(max_nobjects, nobjects);
	min_length = std::min(min_length, length);
	max_length = std::max(max_length, length);
	max_shared = std::max(max_shared, nshared);

	phe.at(i).delta_nobjects = nobjects;
	phe.at(i).delta_page_length = length;
	phe.at(i).nshared_objects = nshared;
    }

    ph.min_nobjects = min_nobjects;
    int in_page0_id = pages.at(0).getObjectID();
    int out_page0_id = (*(obj_renumber.find(in_page0_id))).second;
    ph.first_page_offset = (*(xref.find(out_page0_id))).second.getOffset();
    ph.nbits_delta_nobjects = nbits(max_nobjects - min_nobjects);
    ph.min_page_length = min_length;
    ph.nbits_delta_page_length = nbits(max_length - min_length);
    ph.nbits_nshared_objects = nbits(max_shared);
    ph.nbits_shared_identifier =
	nbits(this->c_shared_object_data.nshared_total);
    ph.shared_denominator = 4;	// doesn't matter

    // It isn't clear how to compute content offset and content
    // length.  Since we are not interleaving page objects with the
    // content stream, we'll use the same values for content length as
    // page length.  We will use 0 as content offset because this is
    // what Adobe does (implementation note 127) and pdlin as well.
    ph.nbits_delta_content_length = ph.nbits_delta_page_length;
    ph.min_content_length = ph.min_page_length;

    for (unsigned int i = 0; i < npages; ++i)
    {
	// Adjust delta entries
	assert(phe.at(i).delta_nobjects >= min_nobjects);
	assert(phe.at(i).delta_page_length >= min_length);
	phe.at(i).delta_nobjects -= min_nobjects;
	phe.at(i).delta_page_length -= min_length;
	phe.at(i).delta_content_length = phe.at(i).delta_page_length;

	for (int j = 0; j < cphe.at(i).nshared_objects; ++j)
	{
	    phe.at(i).shared_identifiers.push_back(
		cphe.at(i).shared_identifiers.at(j));
	    phe.at(i).shared_numerators.push_back(0);
	}
    }
}

void
QPDF::calculateHSharedObject(
    std::map<int, QPDFXRefEntry> const& xref,
    std::map<int, qpdf_offset_t> const& lengths,
    std::map<int, int> const& obj_renumber)
{
    CHSharedObject& cso = this->c_shared_object_data;
    std::vector<CHSharedObjectEntry>& csoe = cso.entries;
    HSharedObject& so = this->shared_object_hints;
    std::vector<HSharedObjectEntry>& soe = so.entries;
    soe.clear();

    int min_length = outputLengthNextN(
	csoe.at(0).object, 1, lengths, obj_renumber);
    int max_length = min_length;

    for (int i = 0; i < cso.nshared_total; ++i)
    {
	// Assign absolute numbers to deltas; adjust later
	int length = outputLengthNextN(
	    csoe.at(i).object, 1, lengths, obj_renumber);
	min_length = std::min(min_length, length);
	max_length = std::max(max_length, length);
        soe.push_back(HSharedObjectEntry());
	soe.at(i).delta_group_length = length;
    }
    if (soe.size() != static_cast<size_t>(cso.nshared_total))
    {
        throw std::logic_error("soe has wrong size after initialization");
    }

    so.nshared_total = cso.nshared_total;
    so.nshared_first_page = cso.nshared_first_page;
    if (so.nshared_total > so.nshared_first_page)
    {
	so.first_shared_obj =
	    (*(obj_renumber.find(cso.first_shared_obj))).second;
	so.first_shared_offset =
	    (*(xref.find(so.first_shared_obj))).second.getOffset();
    }
    so.min_group_length = min_length;
    so.nbits_delta_group_length = nbits(max_length - min_length);

    for (int i = 0; i < cso.nshared_total; ++i)
    {
	// Adjust deltas
	assert(soe.at(i).delta_group_length >= min_length);
	soe.at(i).delta_group_length -= min_length;
    }
}

void
QPDF::calculateHOutline(
    std::map<int, QPDFXRefEntry> const& xref,
    std::map<int, qpdf_offset_t> const& lengths,
    std::map<int, int> const& obj_renumber)
{
    HGeneric& cho = this->c_outline_data;

    if (cho.nobjects == 0)
    {
	return;
    }

    HGeneric& ho = this->outline_hints;

    ho.first_object =
	(*(obj_renumber.find(cho.first_object))).second;
    ho.first_object_offset =
	(*(xref.find(ho.first_object))).second.getOffset();
    ho.nobjects = cho.nobjects;
    ho.group_length = outputLengthNextN(
	cho.first_object, ho.nobjects, lengths, obj_renumber);
}

template <class T, class int_type>
static void
write_vector_int(BitWriter& w, int nitems, std::vector<T>& vec,
		 int bits, int_type T::*field)
{
    // nitems times, write bits bits from the given field of the ith
    // vector to the given bit writer.

    for (int i = 0; i < nitems; ++i)
    {
	w.writeBits(vec.at(i).*field, bits);
    }
    // The PDF spec says that each hint table starts at a byte
    // boundary.  Each "row" actually must start on a byte boundary.
    w.flush();
}

template <class T>
static void
write_vector_vector(BitWriter& w,
		    int nitems1, std::vector<T>& vec1, int T::*nitems2,
		    int bits, std::vector<int> T::*vec2)
{
    // nitems1 times, write nitems2 (from the ith element of vec1) items
    // from the vec2 vector field of the ith item of vec1.
    for (int i1 = 0; i1 < nitems1; ++i1)
    {
	for (int i2 = 0; i2 < vec1.at(i1).*nitems2; ++i2)
	{
	    w.writeBits((vec1.at(i1).*vec2).at(i2), bits);
	}
    }
    w.flush();
}


void
QPDF::writeHPageOffset(BitWriter& w)
{
    HPageOffset& t = this->page_offset_hints;

    w.writeBits(t.min_nobjects, 32);			    // 1
    w.writeBits(t.first_page_offset, 32);		    // 2
    w.writeBits(t.nbits_delta_nobjects, 16);		    // 3
    w.writeBits(t.min_page_length, 32);			    // 4
    w.writeBits(t.nbits_delta_page_length, 16);		    // 5
    w.writeBits(t.min_content_offset, 32);		    // 6
    w.writeBits(t.nbits_delta_content_offset, 16);	    // 7
    w.writeBits(t.min_content_length, 32);		    // 8
    w.writeBits(t.nbits_delta_content_length, 16);	    // 9
    w.writeBits(t.nbits_nshared_objects, 16);		    // 10
    w.writeBits(t.nbits_shared_identifier, 16);		    // 11
    w.writeBits(t.nbits_shared_numerator, 16);		    // 12
    w.writeBits(t.shared_denominator, 16);		    // 13

    unsigned int nitems = getAllPages().size();
    std::vector<HPageOffsetEntry>& entries = t.entries;

    write_vector_int(w, nitems, entries,
		     t.nbits_delta_nobjects,
		     &HPageOffsetEntry::delta_nobjects);
    write_vector_int(w, nitems, entries,
		     t.nbits_delta_page_length,
		     &HPageOffsetEntry::delta_page_length);
    write_vector_int(w, nitems, entries,
		     t.nbits_nshared_objects,
		     &HPageOffsetEntry::nshared_objects);
    write_vector_vector(w, nitems, entries,
			&HPageOffsetEntry::nshared_objects,
			t.nbits_shared_identifier,
			&HPageOffsetEntry::shared_identifiers);
    write_vector_vector(w, nitems, entries,
			&HPageOffsetEntry::nshared_objects,
			t.nbits_shared_numerator,
			&HPageOffsetEntry::shared_numerators);
    write_vector_int(w, nitems, entries,
		     t.nbits_delta_content_offset,
		     &HPageOffsetEntry::delta_content_offset);
    write_vector_int(w, nitems, entries,
		     t.nbits_delta_content_length,
		     &HPageOffsetEntry::delta_content_length);
}

void
QPDF::writeHSharedObject(BitWriter& w)
{
    HSharedObject& t = this->shared_object_hints;

    w.writeBits(t.first_shared_obj, 32);		    // 1
    w.writeBits(t.first_shared_offset, 32);		    // 2
    w.writeBits(t.nshared_first_page, 32);		    // 3
    w.writeBits(t.nshared_total, 32);			    // 4
    w.writeBits(t.nbits_nobjects, 16);			    // 5
    w.writeBits(t.min_group_length, 32);		    // 6
    w.writeBits(t.nbits_delta_group_length, 16);	    // 7

    QTC::TC("qpdf", "QPDF lin write nshared_total > nshared_first_page",
	    (t.nshared_total > t.nshared_first_page) ? 1 : 0);

    int nitems = t.nshared_total;
    std::vector<HSharedObjectEntry>& entries = t.entries;

    write_vector_int(w, nitems, entries,
		     t.nbits_delta_group_length,
		     &HSharedObjectEntry::delta_group_length);
    write_vector_int(w, nitems, entries,
		     1, &HSharedObjectEntry::signature_present);
    for (int i = 0; i < nitems; ++i)
    {
	// If signature were present, we'd have to write a 128-bit hash.
	assert(entries.at(i).signature_present == 0);
    }
    write_vector_int(w, nitems, entries,
		     t.nbits_nobjects,
		     &HSharedObjectEntry::nobjects_minus_one);
}

void
QPDF::writeHGeneric(BitWriter& w, HGeneric& t)
{
    w.writeBits(t.first_object, 32);			    // 1
    w.writeBits(t.first_object_offset, 32);		    // 2
    w.writeBits(t.nobjects, 32);			    // 3
    w.writeBits(t.group_length, 32);			    // 4
}

void
QPDF::generateHintStream(std::map<int, QPDFXRefEntry> const& xref,
			 std::map<int, qpdf_offset_t> const& lengths,
			 std::map<int, int> const& obj_renumber,
			 PointerHolder<Buffer>& hint_buffer,
			 int& S, int& O)
{
    // Populate actual hint table values
    calculateHPageOffset(xref, lengths, obj_renumber);
    calculateHSharedObject(xref, lengths, obj_renumber);
    calculateHOutline(xref, lengths, obj_renumber);

    // Write the hint stream itself into a compressed memory buffer.
    // Write through a counter so we can get offsets.
    Pl_Buffer hint_stream("hint stream");
    Pl_Flate f("compress hint stream", &hint_stream, Pl_Flate::a_deflate);
    Pl_Count c("count", &f);
    BitWriter w(&c);

    writeHPageOffset(w);
    S = c.getCount();
    writeHSharedObject(w);
    O = 0;
    if (this->outline_hints.nobjects > 0)
    {
	O = c.getCount();
	writeHGeneric(w, this->outline_hints);
    }
    c.finish();

    hint_buffer = hint_stream.getBuffer();
}
