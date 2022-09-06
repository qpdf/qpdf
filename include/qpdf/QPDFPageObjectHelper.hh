// Copyright (c) 2005-2022 Jay Berkenbilt
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

#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QPDFObjectHelper.hh>

#include <qpdf/DLL.h>

#include <qpdf/QPDFObjectHandle.hh>
#include <functional>

class QPDFAcroFormDocumentHelper;

class QPDFPageObjectHelper: public QPDFObjectHelper
{
    // This is a helper class for page objects, but as of qpdf 10.1,
    // many of the methods also work for form XObjects. When this is
    // the case, it is noted in the comment.

  public:
    QPDF_DLL
    QPDFPageObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFPageObjectHelper() = default;

    // PAGE ATTRIBUTES

    // The getAttribute method works with pages and form XObjects. It
    // return the value of the requested attribute from the page/form
    // XObject's dictionary, taking inheritance from the pages tree
    // into consideration. For pages, the attributes /MediaBox,
    // /CropBox, /Resources, and /Rotate are inheritable, meaning that
    // if they are not present directly on the page node, they may be
    // inherited from ancestor nodes in the pages tree.
    //
    // There are two ways that an attribute can be "shared":
    //
    // * For inheritable attributes on pages, it may appear in a
    //   higher level node of the pages tree
    //
    // * For any attribute, the attribute may be an indirect object
    //   which may be referenced by more than one page/form XObject.
    //
    // If copy_if_shared is true, then this method will replace the
    // attribute with a shallow copy if it is indirect or inherited
    // and return the copy. You should do this if you are going to
    // modify the returned object and want the modifications to apply
    // to the current page/form XObject only.
    QPDF_DLL
    QPDFObjectHandle getAttribute(std::string const& name, bool copy_if_shared);

    // PAGE BOXES
    //
    // Pages have various types of boundary boxes. These are described
    // in detail in the PDF specification (section 14.11.2 Page
    // boundaries). They are, by key in the page dictionary:
    //
    // * /MediaBox -- boundaries of physical page
    // * /CropBox -- clipping region of what is displayed
    // * /BleedBox -- clipping region for production environments
    // * /TrimBox -- dimensions of final printed page after trimming
    // * /ArtBox -- extent of meaningful content including margins
    //
    // Of these, only /MediaBox is required. If any are absent, the
    // fallback value for /CropBox is /MediaBox, and the fallback
    // values for the other boxes are /CropBox.
    //
    // As noted above (PAGE ATTRIBUTES), /MediaBox and /CropBox can be
    // inherited from parent nodes in the pages tree. The other boxes
    // can't be inherited.
    //
    // When the comments below refer to the "effective value" of an
    // box, this takes into consideration both inheritance through the
    // pages tree (in the case of /MediaBox and /CropBox) and fallback
    // values for missing attributes (for all except /MediaBox).
    //
    // For the methods below, copy_if_shared is passed to getAttribute
    // and therefore refers only to indirect objects and values that
    // are inherited through the pages tree.
    //
    // If copy_if_fallback is true, a copy is made if the object's
    // value was obtained by falling back to a different box.
    //
    // The copy_if_shared and copy_if_fallback parameters carry across
    // multiple layers. This is explained below.
    //
    // You should set copy_if_shared to true if you want to modify a
    // bounding box for the current page without affecting other pages
    // but you don't want to change the fallback behavior. For
    // example, if you want to modify the /TrimBox for the current
    // page only but have it continue to fall back to the value of
    // /CropBox or /MediaBox if they are not defined, you could set
    // copy_if_shared to true.
    //
    // You should set copy_if_fallback to true if you want to modify a
    // specific box as distinct from any other box. For example, if
    // you want to make /TrimBox differ from /CropBox, then you should
    // set copy_if_fallback to true.
    //
    // The copy_if_fallback flags were added in qpdf 11.
    //
    // For example, suppose that neither /CropBox nor /TrimBox is
    // present on a page but /CropBox is present in the page's parent
    // node in the page tree.
    //
    // * getTrimBox(false, false) would return the /CropBox from the
    //   parent node.
    //
    // * getTrimBox(true, false) would make a shallow copy of the
    //   /CropBox from the parent node into the current node and
    //   return it.
    //
    // * getTrimBox(false, true) would make a shallow copy of the
    //   /CropBox from the parent node into /TrimBox of the current
    //   node and return it.
    //
    // * getTrimBox(true, true) would make a shallow copy of the
    //   /CropBox from the parent node into the current node, then
    //   make a shallow copy of the resulting copy to /TrimBox of the
    //   current node, and then return that.
    //
    // To illustrate how these parameters carry across multiple
    // layers, suppose that neither /MediaBox, /CropBox, nor /TrimBox
    // is present on a page but /MediaBox is present on the parent. In
    // this case:
    //
    // * getTrimBox(false, false) would return the value of /MediaBox
    //   from the parent node.
    //
    // * getTrimBox(true, false) would copy /MediaBox to the current
    //   node and return it.
    //
    // * getTrimBox(false, true) would first copy /MediaBox from the
    //   parent to /CropBox, then copy /CropBox to /TrimBox, and then
    //   return the result.
    //
    // * getTrimBox(true, true) would first copy /MediaBox from the
    //   parent to the current page, then copy it to /CropBox, then
    //   copy /CropBox to /TrimBox, and then return the result.
    //
    // If you need different behavior, call getAttribute directly and
    // take care of your own copying.

    // Return the effective MediaBox
    QPDF_DLL
    QPDFObjectHandle getMediaBox(bool copy_if_shared = false);

    // Return the effective CropBox. If not defined, fall back to
    // MediaBox
    QPDF_DLL
    QPDFObjectHandle
    getCropBox(bool copy_if_shared = false, bool copy_if_fallback = false);

    // Return the effective BleedBox. If not defined, fall back to
    // CropBox.
    QPDF_DLL
    QPDFObjectHandle
    getBleedBox(bool copy_if_shared = false, bool copy_if_fallback = false);

    // Return the effective TrimBox. If not defined, fall back to
    // CropBox.
    QPDF_DLL
    QPDFObjectHandle
    getTrimBox(bool copy_if_shared = false, bool copy_if_fallback = false);

    // Return the effective ArtBox. If not defined, fall back to
    // CropBox.
    QPDF_DLL
    QPDFObjectHandle
    getArtBox(bool copy_if_shared = false, bool copy_if_fallback = false);

    // Iterate through XObjects, possibly recursing into form
    // XObjects. This works with pages or form XObjects. Call action
    // on each XObject for which selector, if specified, returns true.
    // With no selector, calls action for every object. In addition to
    // the object being passed to action, the containing XObject
    // dictionary and key are passed in. Remember that the XObject
    // dictionary may be shared, and the object may appear in multiple
    // XObject dictionaries.
    QPDF_DLL
    void forEachXObject(
        bool recursive,
        std::function<void(
            QPDFObjectHandle& obj,
            QPDFObjectHandle& xobj_dict,
            std::string const& key)> action,
        std::function<bool(QPDFObjectHandle)> selector = nullptr);
    // Only call action for images
    QPDF_DLL
    void forEachImage(
        bool recursive,
        std::function<void(
            QPDFObjectHandle& obj,
            QPDFObjectHandle& xobj_dict,
            std::string const& key)> action);
    // Only call action for form XObjects
    QPDF_DLL
    void forEachFormXObject(
        bool recursive,
        std::function<void(
            QPDFObjectHandle& obj,
            QPDFObjectHandle& xobj_dict,
            std::string const& key)> action);

    // Returns an empty map if there are no images or no resources.
    // Prior to qpdf 8.4.0, this function did not support inherited
    // resources, but it does now. Return value is a map from XObject
    // name to the image object, which is always a stream. Works with
    // form XObjects as well as pages. This method does not recurse
    // into nested form XObjects. For that, use forEachImage.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getImages();

    // Old name -- calls getImages()
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getPageImages();

    // Returns an empty map if there are no form XObjects or no
    // resources. Otherwise, returns a map of keys to form XObjects
    // directly referenced from this page or form XObjects. This does
    // not recurse into nested form XObjects. For that, use
    // forEachFormXObject.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getFormXObjects();

    // Converts each inline image to an external (normal) image if the
    // size is at least the specified number of bytes. This method
    // works with pages or form XObjects. By default, it recursively
    // processes nested form XObjects. Pass true as shallow to avoid
    // this behavior. Prior to qpdf 10.1, form XObjects were ignored,
    // but this was considered a bug.
    QPDF_DLL
    void externalizeInlineImages(size_t min_size = 0, bool shallow = false);

    // Return the annotations in the page's "/Annots" list, if any. If
    // only_subtype is non-empty, only include annotations of the
    // given subtype.
    QPDF_DLL
    std::vector<QPDFAnnotationObjectHelper>
    getAnnotations(std::string const& only_subtype = "");

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
    // stream or an array of streams. Call on a page object. Also
    // works for form XObjects.
    QPDF_DLL
    void parseContents(QPDFObjectHandle::ParserCallbacks* callbacks);
    // Old name
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
    void
    filterContents(QPDFObjectHandle::TokenFilter* filter, Pipeline* next = 0);

    // Old name -- calls filterContents()
    QPDF_DLL
    void filterPageContents(
        QPDFObjectHandle::TokenFilter* filter, Pipeline* next = 0);

    // Pipe a page's contents through the given pipeline. This method
    // works whether the contents are a single stream or an array of
    // streams. Also works on form XObjects.
    QPDF_DLL
    void pipeContents(Pipeline* p);
    // Old name
    QPDF_DLL
    void pipePageContents(Pipeline* p);

    // Attach a token filter to a page's contents. If the page's
    // contents is an array of streams, it is automatically coalesced.
    // The token filter is applied to the page's contents as a single
    // stream. Also works on form XObjects.
    QPDF_DLL
    void addContentTokenFilter(
        std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter);

    // A page's resources dictionary maps names to objects elsewhere
    // in the file. This method walks through a page's contents and
    // keeps tracks of which resources are referenced somewhere in the
    // contents. Then it removes from the resources dictionary any
    // object that is not referenced in the contents. This operation
    // is most useful after calling
    // QPDFPageDocumentHelper::pushInheritedAttributesToPage(). This
    // method is used by page splitting code to avoid copying unused
    // objects in files that used shared resource dictionaries across
    // multiple pages. This method recurses into form XObjects and can
    // be called with a form XObject as well as a page.
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
        QPDFObjectHandle fo,
        std::string const& name,
        QPDFObjectHandle::Rectangle rect,
        bool invert_transformations = true,
        bool allow_shrink = true,
        bool allow_expand = false);

    // Alternative version that also fills in the transformation
    // matrix that was used.
    QPDF_DLL
    std::string placeFormXObject(
        QPDFObjectHandle fo,
        std::string const& name,
        QPDFObjectHandle::Rectangle rect,
        QPDFMatrix& cm,
        bool invert_transformations = true,
        bool allow_shrink = true,
        bool allow_expand = false);

    // Return the transformation matrix that translates from the given
    // form XObject's coordinate system into the given rectangular
    // region on the page. The parameters have the same meaning as for
    // placeFormXObject.
    QPDF_DLL
    QPDFMatrix getMatrixForFormXObjectPlacement(
        QPDFObjectHandle fo,
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
    // rotated pages. If a QPDFAcroFormDocumentHelper is provided, it
    // will be used for resolving any form fields that have to be
    // rotated. If not, one will be created inside the function, which
    // is less efficient.
    QPDF_DLL
    void flattenRotation(QPDFAcroFormDocumentHelper* afdh = nullptr);

    // Copy annotations from another page into this page. The other
    // page may be from the same QPDF or from a different QPDF. Each
    // annotation's rectangle is transformed by the given matrix. If
    // the annotation is a widget annotation that is associated with a
    // form field, the form field is copied into this document's
    // AcroForm dictionary as well. You can use this to copy
    // annotations from a page that was converted to a form XObject
    // and added to another page. For example of this, see
    // examples/pdf-overlay-page.cc. This method calls
    // QPDFAcroFormDocumentHelper::transformAnnotations, which will
    // copy annotations and form fields so that you can copy
    // annotations from a source page to any number of other pages,
    // even with different matrices, and maintain independence from
    // the original annotations. See also
    // QPDFAcroFormDocumentHelper::fixCopiedAnnotations, which can be
    // used if you copy a page and want to repair the annotations on
    // the destination page to make them independent from the original
    // page's annotations.
    //
    // If you pass in a QPDFAcroFormDocumentHelper*, the method will
    // use that instead of creating one in the function. Creating
    // QPDFAcroFormDocumentHelper objects is expensive, so if you're
    // doing a lot of copying, it can be more efficient to create
    // these outside and pass them in.
    QPDF_DLL
    void copyAnnotations(
        QPDFPageObjectHelper from_page,
        QPDFMatrix const& cm = QPDFMatrix(),
        QPDFAcroFormDocumentHelper* afdh = nullptr,
        QPDFAcroFormDocumentHelper* from_afdh = nullptr);

  private:
    QPDFObjectHandle getAttribute(
        std::string const& name,
        bool copy_if_shared,
        std::function<QPDFObjectHandle()> get_fallback,
        bool copy_if_fallback);
    static bool removeUnreferencedResourcesHelper(
        QPDFPageObjectHelper ph, std::set<std::string>& unresolved);

    class Members
    {
        friend class QPDFPageObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members() = default;
        Members(Members const&) = delete;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFPAGEOBJECTHELPER_HH
