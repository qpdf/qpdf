// Copyright (c) 2005-2020 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFPAGEOBJECTHELPER_HH
#define QPDFPAGEOBJECTHELPER_HH

#include <qpdf/QPDFObjectHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>

#include <qpdf/DLL.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <functional>

class QPDFPageObjectHelper: public QPDFObjectHelper
{
    // This is a helper class for page objects, but as of qpdf 10.1,
    // many of the methods also work for form XObjects. When this is
    // the case, it is noted in the comment.

  public:
    QPDF_DLL
    QPDFPageObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFPageObjectHelper()
    {
    }

    // Works with pages and form XObjects. Return the effective value
    // of this attribute for the page/form XObject. For pages, if the
    // requested attribute is not present on the page but is
    // inheritable, look up through the page's ancestors in the page
    // tree. If copy_if_shared is true, then this method will replace
    // the attribute with a shallow copy if it is in indirect or
    // inherited and return the copy. You should do this if you are
    // going to modify the returned object and want the modifications
    // to apply to the current page/form XObject only.
    QPDF_DLL
    QPDFObjectHandle
    getAttribute(std::string const& name, bool copy_if_shared);

    // Return the TrimBox. If not defined, fall back to CropBox
    QPDF_DLL
    QPDFObjectHandle
    getTrimBox(bool copy_if_shared = false);

    // Return the CropBox. If not defined, fall back to MediaBox
    QPDF_DLL
    QPDFObjectHandle
    getCropBox(bool copy_if_shared = false);

    // Return the MediaBox
    QPDF_DLL
    QPDFObjectHandle
    getMediaBox(bool copy_if_shared = false);

    // Returns an empty map if there are no images or no resources.
    // Prior to qpdf 8.4.0, this function did not support inherited
    // resources, but it does now. Return value is a map from XObject
    // name to the image object, which is always a stream. Works with
    // form XObjects as well as pages.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getImages();

    // Old name -- calls getImages()
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getPageImages();

    // Convert each inline image to an external (normal) image if the
    // size is at least the specified number of bytes.
    QPDF_DLL
    void externalizeInlineImages(size_t min_size = 0);

    // Return the annotations in the page's "/Annots" list, if any. If
    // only_subtype is non-empty, only include annotations of the
    // given subtype.
    QPDF_DLL
    std::vector<QPDFAnnotationObjectHelper> getAnnotations(
        std::string const& only_subtype = "");

    // Returns a vector of stream objects representing the content
    // streams for the given page.  This routine allows the caller to
    // not care whether there are one or more than one content streams
    // for a page.
    QPDF_DLL
    std::vector<QPDFObjectHandle> getPageContents();

    // Add the given object as a new content stream for this page. If
    // parameter 'first' is true, add to the beginning. Otherwise, add
    // to the end. This routine automatically converts the page
    // contents to an array if it is a scalar, allowing the caller not
    // to care what the initial structure is. You can call
    // coalesceContentStreams() afterwards if you want to force it to
    // be a single stream.
    QPDF_DLL
    void addPageContents(QPDFObjectHandle contents, bool first);

    // Rotate a page. If relative is false, set the rotation of the
    // page to angle. Otherwise, add angle to the rotation of the
    // page. Angle must be a multiple of 90. Adding 90 to the rotation
    // rotates clockwise by 90 degrees.
    QPDF_DLL
    void rotatePage(int angle, bool relative);

    // Coalesce a page's content streams. A page's content may be a
    // stream or an array of streams. If this page's content is an
    // array, concatenate the streams into a single stream. This can
    // be useful when working with files that split content streams in
    // arbitrary spots, such as in the middle of a token, as that can
    // confuse some software. You could also call this after calling
    // addPageContents.
    QPDF_DLL
    void coalesceContentStreams();

    //
    // Content stream handling
    //

    // Parse a page's contents through ParserCallbacks, described
    // above. This method works whether the contents are a single
    // stream or an array of streams. Call on a page object.
    QPDF_DLL
    void parsePageContents(QPDFObjectHandle::ParserCallbacks* callbacks);

    // Pass a page's or form XObject's contents through the given
    // TokenFilter. If a pipeline is also provided, it will be the
    // target of the write methods from the token filter. If a
    // pipeline is not specified, any output generated by the token
    // filter will be discarded. Use this interface if you need to
    // pass a page's contents through filter for work purposes without
    // having that filter automatically applied to the page's
    // contents, as happens with addContentTokenFilter. See
    // examples/pdf-count-strings.cc for an example.
    QPDF_DLL
    void filterContents(QPDFObjectHandle::TokenFilter* filter,
                        Pipeline* next = 0);

    // Old name -- calls filterContents()
    QPDF_DLL
    void filterPageContents(QPDFObjectHandle::TokenFilter* filter,
                            Pipeline* next = 0);

    // Pipe a page's contents through the given pipeline. This method
    // works whether the contents are a single stream or an array of
    // streams. Call on a page object. Please note that if there is an
    // array of content streams, p->finish() is called after each
    // stream. If you pass a pipeline that doesn't allow write() to be
    // called after finish(), you can wrap it in an instance of
    // Pl_Concatenate and then call manualFinish() on the
    // Pl_Concatenate pipeline at the end.
    QPDF_DLL
    void pipePageContents(Pipeline* p);

    // Attach a token filter to a page's contents. If the page's
    // contents is an array of streams, it is automatically coalesced.
    // The token filter is applied to the page's contents as a single
    // stream.
    QPDF_DLL
    void addContentTokenFilter(
        PointerHolder<QPDFObjectHandle::TokenFilter> token_filter);

    // A page's resources dictionary maps names to objects elsewhere
    // in the file. This method walks through a page's contents and
    // keeps tracks of which resources are referenced somewhere in the
    // contents. Then it removes from the resources dictionary any
    // object that is not referenced in the contents. This operation
    // is most useful after calling
    // QPDFPageDocumentHelper::pushInheritedAttributesToPage(). This
    // method is used by page splitting code to avoid copying unused
    // objects in files that used shared resource dictionaries across
    // multiple pages.
    QPDF_DLL
    void removeUnreferencedResources();

    // Return a new QPDFPageObjectHelper that is a duplicate of the
    // page. The returned object is an indirect object that is ready
    // to be inserted into the same or a different QPDF object using
    // any of the addPage methods in QPDFPageDocumentHelper or QPDF.
    // Without calling one of those methods, the page will not be
    // added anywhere. The new page object shares all content streams
    // and indirect object resources with the original page, so if you
    // are going to modify the contents or other aspects of the page,
    // you will need to handling copying of the component parts
    // separately.
    QPDF_DLL
    QPDFPageObjectHelper shallowCopyPage();

    // Return a transformation matrix whose effect is the same as the
    // page's /Rotate and /UserUnit parameters. If invert is true,
    // return a matrix whose effect is the opposite. The regular
    // matrix is suitable for taking something from this page to put
    // elsewhere, and the second one is suitable for putting something
    // else onto this page. The page's TrimBox is used as the bounding
    // box for purposes of computing the matrix.
    QPDF_DLL
    QPDFObjectHandle::Matrix getMatrixForTransformations(bool invert = false);

    // Return a form XObject that draws this page. This is useful for
    // n-up operations, underlay, overlay, thumbnail generation, or
    // any other case in which it is useful to replicate the contents
    // of a page in some other context. The dictionaries are shallow
    // copies of the original page dictionary, and the contents are
    // coalesced from the page's contents. The resulting object handle
    // is not referenced anywhere. If handle_transformations is true,
    // the resulting form XObject's /Matrix will be set to replicate
    // rotation (/Rotate) and scaling (/UserUnit) in the page's
    // dictionary. In this way, the page's transformations will be
    // preserved when placing this object on another page.
    QPDF_DLL
    QPDFObjectHandle getFormXObjectForPage(bool handle_transformations = true);

    // Return content stream text that will place the given form
    // XObject (fo) using the resource name "name" on this page
    // centered within the given rectangle. If invert_transformations
    // is true, the effect of any rotation (/Rotate) and scaling
    // (/UserUnit) applied to the current page will be inverted in the
    // form XObject placement. This will cause the form XObject's
    // absolute orientation to be preserved. You could overlay one
    // page on another by calling getFormXObjectForPage on the
    // original page, QPDFObjectHandle::getUniqueResourceName on the
    // destination page's Resources dictionary to generate a name for
    // the resulting object, and calling placeFormXObject on the
    // destination page. Then insert the new fo (or, if it comes from
    // a different file, the result of calling copyForeignObject on
    // it) into the resources dictionary using name, and append or
    // prepend the content to the page's content streams. See the
    // overlay/underlay code in qpdf.cc or
    // examples/pdf-overlay-page.cc for an example. From qpdf 10.0.0,
    // the allow_shrink and allow_expand parameters control whether
    // the form XObject is allowed to be shrunk or expanded to stay
    // within or maximally fill the destination rectangle. The default
    // values are for backward compatibility with the pre-10.0.0
    // behavior.
    QPDF_DLL
    std::string placeFormXObject(
        QPDFObjectHandle fo, std::string const& name,
        QPDFObjectHandle::Rectangle rect,
        bool invert_transformations = true,
        bool allow_shrink = true,
        bool allow_expand = false);

    // If a page is rotated using /Rotate in the page's dictionary,
    // instead rotate the page by the same amount by altering the
    // contents and removing the /Rotate key. This method adjusts the
    // various page bounding boxes (/MediaBox, etc.) so that the page
    // will have the same semantics. This can be useful to work around
    // problems with PDF applications that can't properly handle
    // rotated pages.
    QPDF_DLL
    void flattenRotation();

  private:
    static void
    removeUnreferencedResourcesHelper(
        QPDFPageObjectHelper ph, std::set<QPDFObjGen>& seen);

    class Members
    {
        friend class QPDFPageObjectHelper;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&);
    };

    PointerHolder<Members> m;
};

#endif // QPDFPAGEOBJECTHELPER_HH
