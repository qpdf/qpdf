#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QTC.hh>

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
