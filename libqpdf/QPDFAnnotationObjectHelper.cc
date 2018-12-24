#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <algorithm>

QPDFAnnotationObjectHelper::Members::~Members()
{
}

QPDFAnnotationObjectHelper::Members::Members()
{
}

QPDFAnnotationObjectHelper::QPDFAnnotationObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

std::string
QPDFAnnotationObjectHelper::getSubtype()
{
    return this->oh.getKey("/Subtype").getName();
}

QPDFObjectHandle::Rectangle
QPDFAnnotationObjectHelper::getRect()
{
    return this->oh.getKey("/Rect").getArrayAsRectangle();
}

QPDFObjectHandle
QPDFAnnotationObjectHelper::getAppearanceDictionary()
{
    return this->oh.getKey("/AP");
}

std::string
QPDFAnnotationObjectHelper::getAppearanceState()
{
    if (this->oh.getKey("/AS").isName())
    {
        QTC::TC("qpdf", "QPDFAnnotationObjectHelper AS present");
        return this->oh.getKey("/AS").getName();
    }
    QTC::TC("qpdf", "QPDFAnnotationObjectHelper AS absent");
    return "";
}

QPDFObjectHandle
QPDFAnnotationObjectHelper::getAppearanceStream(
    std::string const& which,
    std::string const& state)
{
    QPDFObjectHandle ap = getAppearanceDictionary();
    std::string desired_state = state.empty() ? getAppearanceState() : state;
    if (ap.isDictionary())
    {
        QPDFObjectHandle ap_sub = ap.getKey(which);
        if (ap_sub.isStream() && desired_state.empty())
        {
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP stream");
            return ap_sub;
        }
        if (ap_sub.isDictionary() && (! desired_state.empty()))
        {
            QTC::TC("qpdf", "QPDFAnnotationObjectHelper AP dictionary");
            QPDFObjectHandle ap_sub_val = ap_sub.getKey(desired_state);
            if (ap_sub_val.isStream())
            {
                QTC::TC("qpdf", "QPDFAnnotationObjectHelper AN sub stream");
                return ap_sub_val;
            }
        }
    }
    QTC::TC("qpdf", "QPDFAnnotationObjectHelper AN null");
    return QPDFObjectHandle::newNull();
}

std::string
QPDFAnnotationObjectHelper::getAnnotationAppearanceMatrix(int rotate)
{
    // The appearance matrix is the transformation in effect when
    // rendering an appearance stream's content. The appearance stream
    // itself is a form XObject, which has a /BBox and an optional
    // /Matrix. The /BBox describes the bounding box of the annotation
    // in unrotated page coordinates. /Matrix may be applied to the
    // bounding box to transform the bounding box. The effect of this
    // is that the transformed box is still fit within the area the
    // annotation designates in its /Rect field.

    // The algorithm for computing the appearance matrix described in
    // section 12.5.5 of the ISO-32000 PDF spec. It is as follows:

    // 1. Transform the four corners of /BBox by applying /Matrix to
    //    them, creating an arbitrarily transformed quadrilateral.

    // 2. Find the minimum upright rectangle that encompasses the
    //    resulting quadrilateral. This is the "transformed appearance
    //    box", T.

    // 3. Compute matrix A that maps the lower left and upper right
    //    corners of T to the annotation's /Rect. This can be done by
    //    translating the lower left corner and then scaling so that
    //    the upper right corner also matches.

    // 4. Concatenate /Matrix to A to get matrix AA. This matrix
    //    translates from appearance stream coordinates to page
    //    coordinates.

    // If the annotation's /F flag has bit 4 set, we modify the matrix
    // to also rotate the annotation in the opposite direction, and we
    // adjust the destination rectangle by rotating it about the upper
    // left hand corner so that the annotation will appear upright on
    // the rotated page.

    // You can see that the above algorithm works by imagining the
    // following:

    // * In the simple case of where /BBox = /Rect and /Matrix is the
    //   identity matrix, the transformed quadrilateral in step 1 will
    //   be the bounding box. Since the bounding box is upright, T
    //   will be the bounding box. Since /BBox = /Rect, matrix A is
    //   the identity matrix, and matrix AA in step 4 is also the
    //   identity matrix.
    //
    // * Imagine that the rectangle is different from the bounding
    //   box. In this case, matrix A just transforms the bounding box
    //   to the rectangle by scaling and translating, effectively
    //   squeezing or stretching it into /Rect.
    //
    // * Imagine that /Matrix rotates the annotation by 30 degrees.
    //   The transformed bounding box would stick out, and T would be
    //   too big. In this case, matrix A shrinks T down until it fits
    //   in /Rect.

    QPDFObjectHandle rect_obj = this->oh.getKey("/Rect");
    QPDFObjectHandle flags = this->oh.getKey("/F");
    QPDFObjectHandle as = getAppearanceStream("/N").getDict();
    QPDFObjectHandle bbox_obj = as.getKey("/BBox");
    QPDFObjectHandle matrix_obj = as.getKey("/Matrix");

    if (! (bbox_obj.isRectangle() && rect_obj.isRectangle()))
    {
        return "";
    }
    QPDFMatrix matrix;
    if (matrix_obj.isMatrix())
    {
///        QTC::TC("qpdf", "QPDFAnnotationObjectHelper explicit matrix");
        matrix = QPDFMatrix(matrix_obj.getArrayAsMatrix());
    }
    else
    {
///        QTC::TC("qpdf", "QPDFAnnotationObjectHelper default matrix");
    }
    QPDFObjectHandle::Rectangle rect = rect_obj.getArrayAsRectangle();
    if (rotate && flags.isInteger() && (flags.getIntValue() & 16))
    {
        // If the the annotation flags include the NoRotate bit and
        // the page is rotated, we have to rotate the annotation about
        // its upper left corner by the same amount in the opposite
        // direction so that it will remain upright in absolute
        // coordinates. Since the semantics of /Rotate for a page are
        // to rotate the page, while the effect of rotating using a
        // transformation matrix is to rotate the coordinate system,
        // the opposite directionality is explicit in the code.
        QPDFMatrix mr;
        mr.rotatex90(rotate);
        mr.concat(matrix);
        matrix = mr;
        double rect_w = rect.urx - rect.llx;
        double rect_h = rect.ury - rect.lly;
        switch (rotate)
        {
          case 90:
///            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 90");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx,
                rect.ury,
                rect.llx + rect_h,
                rect.ury + rect_w);
            break;
          case 180:
///            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 180");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx - rect_w,
                rect.ury,
                rect.llx,
                rect.ury + rect_h);
            break;
          case 270:
///            QTC::TC("qpdf", "QPDFAnnotationObjectHelper rotate 270");
            rect = QPDFObjectHandle::Rectangle(
                rect.llx - rect_h,
                rect.ury - rect_w,
                rect.llx,
                rect.ury);
            break;
          default:
            // ignore
            break;
        }
    }

    // Transform bounding box by matrix to get T
    QPDFObjectHandle::Rectangle bbox = bbox_obj.getArrayAsRectangle();
    std::vector<double> bx(4);
    std::vector<double> by(4);
    matrix.transform(bbox.llx, bbox.lly, bx.at(0), by.at(0));
    matrix.transform(bbox.llx, bbox.ury, bx.at(1), by.at(1));
    matrix.transform(bbox.urx, bbox.lly, bx.at(2), by.at(2));
    matrix.transform(bbox.urx, bbox.ury, bx.at(3), by.at(3));
    // Find the transformed appearance box
    double t_llx = *std::min_element(bx.begin(), bx.end());
    double t_urx = *std::max_element(bx.begin(), bx.end());
    double t_lly = *std::min_element(by.begin(), by.end());
    double t_ury = *std::max_element(by.begin(), by.end());
    if ((t_urx == t_llx) || (t_ury == t_lly))
    {
        // avoid division by zero
        return "";
    }
    // Compute a matrix to transform the appearance box to the rectangle
    QPDFMatrix AA;
    AA.translate(rect.llx, rect.lly);
    AA.scale((rect.urx - rect.llx) / (t_urx - t_llx),
             (rect.ury - rect.lly) / (t_ury - t_lly));
    AA.translate(-t_llx, -t_lly);
    // Concatenate the user-specified matrix
    AA.concat(matrix);
    return AA.unparse();
}

std::string
QPDFAnnotationObjectHelper::getPageContentForAppearance(int rotate)
{
    QPDFObjectHandle as = getAppearanceStream("/N");
    if (! (as.isStream() && as.getDict().getKey("/BBox").isRectangle()))
    {
        return "";
    }

    QPDFObjectHandle::Rectangle rect =
        as.getDict().getKey("/BBox").getArrayAsRectangle();
    std::string cm = getAnnotationAppearanceMatrix(rotate);
    if (cm.empty())
    {
        return "";
    }
    std::string as_content = (
        "q\n" +
        cm + " cm\n" +
        QUtil::double_to_string(rect.llx, 5) + " " +
        QUtil::double_to_string(rect.lly, 5) + " " +
        QUtil::double_to_string(rect.urx - rect.llx, 5) + " " +
        QUtil::double_to_string(rect.ury - rect.lly, 5) + " " +
        "re W n\n");
    PointerHolder<Buffer> buf = as.getStreamData(qpdf_dl_all);
    as_content += std::string(
        reinterpret_cast<char *>(buf->getBuffer()),
        buf->getSize());
    as_content += "\nQ\n";
    return as_content;
}
