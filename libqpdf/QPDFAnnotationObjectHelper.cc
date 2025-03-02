#include <qpdf/QPDFAnnotationObjectHelper.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

QPDFAnnotationObjectHelper::QPDFAnnotationObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

std::string
QPDFAnnotationObjectHelper::getSubtype()
{
    return oh().getKey("/Subtype").getName();
}

QPDFObjectHandle::Rectangle
QPDFAnnotationObjectHelper::getRect()
{
    return oh().getKey("/Rect").getArrayAsRectangle();
}

QPDFObjectHandle
QPDFAnnotationObjectHelper::getAppearanceDictionary()
{
    return oh().getKey("/AP");
}

std::string
QPDFAnnotationObjectHelper::getAppearanceState()
{
    if (oh().getKey("/AS").isName()) {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper AS present");
        return oh().getKey("/AS").getName();
    }
    QTC::TC("qpdf", "QPDFAnnotationObjectHelper AS absent");
    return "";
}

int
QPDFAnnotationObjectHelper::getFlags()
{
    QPDFObjectHandle flags_obj = oh().getKey("/F");
    return flags_obj.isInteger() ? flags_obj.getIntValueAsInt() : 0;
}

QPDFObjectHandle
QPDFAnnotationObjectHelper::getAppearanceStream(std::string const& which, std::string const& state)
{
    QPDFObjectHandle ap = getAppearanceDictionary();
    std::string desired_state = state.empty() ? getAppearanceState() : state;
    if (ap.isDictionary()) {
        QPDFObjectHandle ap_sub = ap.getKey(which);
        if (ap_sub.isStream()) {
            // According to the spec, Appearance State is supposed to refer to a subkey of the
            // appearance stream when /AP is a dictionary, but files have been seen in the wild
            // where Appearance State is `/N` and `/AP` is a stream. Therefore, if `which` points to
            // a stream, disregard state and just use the stream. See qpdf issue #949 for details.
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP stream");
            return ap_sub;
        }
        if (ap_sub.isDictionary() && (!desired_state.empty())) {
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP dictionary");
            QPDFObjectHandle ap_sub_val = ap_sub.getKey(desired_state);
            if (ap_sub_val.isStream()) {
                QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP sub stream");
                return ap_sub_val;
            }
        }
    }
    QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP null");
    return QPDFObjectHandle::newNull();
}

std::string
QPDFAnnotationObjectHelper::getPageContentForAppearance(
    std::string const& name, int rotate, int required_flags, int forbidden_flags)
{
    if (!getAppearanceStream("/N").isStream()) {
        return "";
    }

    // The appearance matrix computed by this method is the transformation matrix that needs to be
    // in effect when drawing this annotation's appearance stream on the page. The algorithm for
    // computing the appearance matrix described in section 12.5.5 of the ISO-32000 PDF spec is
    // similar but not identical to what we are doing here.

    // When rendering an appearance stream associated with an annotation, there are four relevant
    // components:
    //
    // * The appearance stream's bounding box (/BBox)
    // * The appearance stream's matrix (/Matrix)
    // * The annotation's rectangle (/Rect)
    // * In the case of form fields with the NoRotate flag, the page's rotation

    // When rendering a form xobject in isolation, just drawn with a /Do operator, there is no form
    // field, so page rotation is not relevant, and there is no annotation, so /Rect is not
    // relevant, so only /BBox and /Matrix are relevant. The effect of these are as follows:

    // * /BBox is treated as a clipping region
    // * /Matrix is applied as a transformation prior to rendering the appearance stream.

    // There is no relationship between /BBox and /Matrix in this case.

    // When rendering a form xobject in the context of an annotation, things are a little different.
    // In particular, a matrix is established such that /BBox, when transformed by /Matrix, would
    // fit completely inside of /Rect. /BBox is no longer a clipping region. To illustrate the
    // difference, consider a /Matrix of [2 0 0 2 0 0], which is scaling by a factor of two along
    // both axes. If the appearance stream drew a rectangle equal to /BBox, in the case of the form
    // xobject in isolation, this matrix would cause only the lower-left quadrant of the rectangle
    // to be visible since the scaling would cause the rest of it to fall outside of the clipping
    // region. In the case of the form xobject displayed in the context of an annotation, such a
    // matrix would have no effect at all because it would be applied to the bounding box first, and
    // then when the resulting enclosing quadrilateral was transformed to fit into /Rect, the effect
    // of the scaling would be undone.

    // Our job is to create a transformation matrix that compensates for these differences so that
    // the appearance stream of an annotation can be drawn as a regular form xobject.

    // To do this, we perform the following steps, which overlap significantly with the algorithm
    // in 12.5.5:

    // 1. Transform the four corners of /BBox by applying /Matrix to them, creating an arbitrarily
    //    transformed quadrilateral.

    // 2. Find the minimum upright rectangle that encompasses the resulting quadrilateral. This is
    //    the "transformed appearance box", T.

    // 3. Compute matrix A that maps the lower left and upper right corners of T to the annotation's
    //    /Rect. This can be done by scaling so that the sizes match and translating so that the
    //    scaled T exactly overlaps /Rect.

    // If the annotation's /F flag has bit 4 set, this means that annotation is to be rotated about
    // its upper left corner to counteract any rotation of the page so it remains upright. To
    // achieve this effect, we do the following extra steps:

    // 1. Perform the rotation on /BBox box prior to transforming it with /Matrix (by replacing
    //    matrix with concatenation of matrix onto the rotation)

    // 2. Rotate the destination rectangle by the specified amount

    // 3. Apply the rotation to A as computed above to get the final appearance matrix.

    QPDFObjectHandle rect_obj = oh().getKey("/Rect");
    QPDFObjectHandle as = getAppearanceStream("/N").getDict();
    QPDFObjectHandle bbox_obj = as.getKey("/BBox");
    QPDFObjectHandle matrix_obj = as.getKey("/Matrix");

    int flags = getFlags();
    if (flags & forbidden_flags) {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper forbidden flags");
        return "";
    }
    if ((flags & required_flags) != required_flags) {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper missing required flags");
        return "";
    }

    if (!(bbox_obj.isRectangle() && rect_obj.isRectangle())) {
        return "";
    }
    QPDFMatrix matrix;
    if (matrix_obj.isMatrix()) {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper explicit matrix");
        matrix = QPDFMatrix(matrix_obj.getArrayAsMatrix());
    } else {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper default matrix");
    }
    QPDFObjectHandle::Rectangle rect = rect_obj.getArrayAsRectangle();
    bool do_rotate = (rotate && (flags & an_no_rotate));
    if (do_rotate) {
        // If the annotation flags include the NoRotate bit and the page is rotated, we have to
        // rotate the annotation about its upper left corner by the same amount in the opposite
        // direction so that it will remain upright in absolute coordinates. Since the semantics of
        // /Rotate for a page are to rotate the page, while the effect of rotating using a
        // transformation matrix is to rotate the coordinate system, the opposite directionality is
        // explicit in the code.
        QPDFMatrix mr;
        mr.rotatex90(rotate);
        mr.concat(matrix);
        matrix = mr;
        double rect_w = rect.urx - rect.llx;
        double rect_h = rect.ury - rect.lly;
        switch (rotate) {
        case 90:
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 90");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx, rect.ury, rect.llx + rect_h, rect.ury + rect_w);
            break;
        case 180:
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 180");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx - rect_w, rect.ury, rect.llx, rect.ury + rect_h);
            break;
        case 270:
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 270");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx - rect_h, rect.ury - rect_w, rect.llx, rect.ury);
            break;
        default:
            // ignore
            break;
        }
    }

    // Transform bounding box by matrix to get T
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    QPDFObjectHandle::Rectangle T = matrix.transformRectangle(bbox);
    if ((T.urx == T.llx) || (T.ury == T.lly)) {
        // avoid division by zero
        return "";
    }
    // Compute a matrix to transform the appearance box to the rectangle
    QPDFMatrix AA;
    AA.translate(rect.llx, rect.lly);
    AA.scale((rect.urx - rect.llx) / (T.urx - T.llx), (rect.ury - rect.lly) / (T.ury - T.lly));
    AA.translate(-T.llx, -T.lly);
    if (do_rotate) {
        AA.rotatex90(rotate);
    }

    as.replaceKey("/Subtype", QPDFObjectHandle::newName("/Form"));
    return ("q\n" + AA.unparse() + " cm\n" + name + " Do\n" + "Q\n");
}
