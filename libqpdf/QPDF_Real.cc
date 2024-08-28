#include <qpdf/QPDF_Real.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QUtil.hh>

QPDF_Real::QPDF_Real(std::string const& val) :
    QPDFValue(::ot_real),
    val(val)
{
}

QPDF_Real::QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes) :
    QPDFValue(::ot_real),
    val(QUtil::double_to_string(value, decimal_places, trim_trailing_zeroes))
{
}

std::shared_ptr<QPDFObject>
QPDF_Real::create(std::string const& val)
{
    return do_create(new QPDF_Real(val));
}

std::shared_ptr<QPDFObject>
QPDF_Real::create(double value, int decimal_places, bool trim_trailing_zeroes)
{
    return do_create(new QPDF_Real(value, decimal_places, trim_trailing_zeroes));
}

std::shared_ptr<QPDFObject>
QPDF_Real::copy(bool shallow)
{
    return create(val);
}

std::string
QPDF_Real::unparse()
{
    return this->val;
}

void
QPDF_Real::writeJSON(int json_version, JSON::Writer& p)
{
    if (this->val.length() == 0) {
        // Can't really happen...
        p << "0";
    } else if (this->val.at(0) == '.') {
        p << "0" << this->val;
    } else if (this->val.length() >= 2 && this->val.at(0) == '-' && this->val.at(1) == '.') {
        p << "-0." << this->val.substr(2);
    } else {
        p << this->val;
    }
    if (val.back() == '.') {
        p << "0";
    }
}
