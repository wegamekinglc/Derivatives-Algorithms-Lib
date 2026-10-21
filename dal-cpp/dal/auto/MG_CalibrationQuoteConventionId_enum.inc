
bool operator==(const CalibrationQuoteConventionId_& lhs, const CalibrationQuoteConventionId_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationQuoteConventionId_>& TheCalibrationQuoteConventionIdList() {
        RETURN_STATIC(Vector_<CalibrationQuoteConventionId_>);
    }
}    // leave local

Vector_<CalibrationQuoteConventionId_> CalibrationQuoteConventionIdListAll() {
   if (TheCalibrationQuoteConventionIdList().empty()) {
        TheCalibrationQuoteConventionIdList().emplace_back(CalibrationQuoteConventionId_::Value_::DecimalRate);
        TheCalibrationQuoteConventionIdList().emplace_back(CalibrationQuoteConventionId_::Value_::DecimalSpread);
        TheCalibrationQuoteConventionIdList().emplace_back(CalibrationQuoteConventionId_::Value_::PresentValueAmount);
        TheCalibrationQuoteConventionIdList().emplace_back(CalibrationQuoteConventionId_::Value_::QuotePerBase);
   }
   return TheCalibrationQuoteConventionIdList();
}


const char* CalibrationQuoteConventionId_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::DecimalRate:
        return "DecimalRate";
    case Value_::DecimalSpread:
        return "DecimalSpread";
    case Value_::PresentValueAmount:
        return "PresentValueAmount";
    case Value_::QuotePerBase:
        return "QuotePerBase";
        
    }}

struct ReadStringCalibrationQuoteConventionId_
{
    ReadStringCalibrationQuoteConventionId_() {}

    bool operator()(const String_& src, CalibrationQuoteConventionId_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "DECIMALRATE"))
        *val = CalibrationQuoteConventionId_::Value_::DecimalRate;

	else if (String::Equivalent(src, "DECIMALSPREAD"))
        *val = CalibrationQuoteConventionId_::Value_::DecimalSpread;

	else if (String::Equivalent(src, "PRESENTVALUEAMOUNT"))
        *val = CalibrationQuoteConventionId_::Value_::PresentValueAmount;

	else if (String::Equivalent(src, "QUOTEPERBASE"))
        *val = CalibrationQuoteConventionId_::Value_::QuotePerBase;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationQuoteConventionId_::CalibrationQuoteConventionId_(const String_& src) {
    static const ReadStringCalibrationQuoteConventionId_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationQuoteConventionId");
}



