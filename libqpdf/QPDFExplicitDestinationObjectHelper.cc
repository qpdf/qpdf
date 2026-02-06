#include <qpdf/QPDFExplicitDestinationObjectHelper.hh>

QPDFExplicitDestinationObjectHelper::QPDFExplicitDestinationObjectHelper(QPDFObjectHandle oh) :
    QPDFObjectHelper(oh)
{
}

QPDFObjectHandle
QPDFExplicitDestinationObjectHelper::get_explicit_array() const
{
    auto obj = oh();
    return obj.isArray() ? obj : QPDFObjectHandle::newNull();
}

bool
QPDFExplicitDestinationObjectHelper::is_remote() const
{
    if (!strictly_compliant()) {
        return false;
    }
    auto page = oh().getArrayItem(0);
    return page.isInteger() && page.getIntValue() >= 0;
}

bool
QPDFExplicitDestinationObjectHelper::strictly_compliant() const
{
    auto obj = oh();
    if (!obj.isArray()) {
        return false;
    }
    auto n = obj.getArrayNItems();
    if (n < 2) {
        return false;
    }

    auto view = obj.getArrayItem(1);
    if (!view.isName()) {
        return false;
    }

    auto page = obj.getArrayItem(0);
    if (!page.isPageObject() && !(page.isInteger() && page.getIntValue() >= 0)) {
        return false;
    }

    std::string view_str = view.getName();
    if (view_str == "/XYZ") {
        return n == 5;
    }
    if (view_str == "/Fit") {
        return n == 2;
    }
    if (view_str == "/FitH" || view_str == "/FitV" || view_str == "/FitBH" ||
        view_str == "/FitBV") {
        return n == 3;
    }
    if (view_str == "/FitR") {
        return n == 6;
    }
    if (view_str == "/FitB") {
        return n == 2;
    }

    // Disallow unknown views
    return false;
}
