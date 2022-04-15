#include <qpdf/QPDF_Real.hh>

#include <qpdf/QUtil.hh>

QPDF_Real::QPDF_Real(std::string const& val) :
    val(val)
{
}

QPDF_Real::QPDF_Real(
    double value, int decimal_places, bool trim_trailing_zeroes) :
    val(QUtil::double_to_string(value, decimal_places, trim_trailing_zeroes))
{
}

std::string
QPDF_Real::unparse()
{
    return this->val;
}

JSON
QPDF_Real::getJSON()
{
    // While PDF allows .x or -.x, JSON does not. Rather than
    // converting from string to double and back, just handle this as a
    // special case for JSON.
    std::string result;
    if (this->val.length() == 0) {
        // Can't really happen...
        result = "0";
    } else if (this->val.at(0) == '.') {
        result = "0" + this->val;
    } else if (
        (this->val.length() >= 2) && (this->val.at(0) == '-') &&
        (this->val.at(1) == '.')) {
        result = "-0." + this->val.substr(2);
    } else {
        result = this->val;
    }
    return JSON::makeNumber(result);
}

QPDFObject::object_type_e
QPDF_Real::getTypeCode() const
{
    return QPDFObject::ot_real;
}

char const*
QPDF_Real::getTypeName() const
{
    return "real";
}

std::string
QPDF_Real::getVal()
{
    return this->val;
}
