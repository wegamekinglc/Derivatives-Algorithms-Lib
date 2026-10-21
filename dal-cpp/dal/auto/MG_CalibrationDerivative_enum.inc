
bool operator==(const CalibrationDerivative_& lhs, const CalibrationDerivative_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<CalibrationDerivative_>& TheCalibrationDerivativeList() {
        RETURN_STATIC(Vector_<CalibrationDerivative_>);
    }
}    // leave local

Vector_<CalibrationDerivative_> CalibrationDerivativeListAll() {
   if (TheCalibrationDerivativeList().empty()) {
        TheCalibrationDerivativeList().emplace_back(CalibrationDerivative_::Value_::Auto);
        TheCalibrationDerivativeList().emplace_back(CalibrationDerivative_::Value_::Analytic);
        TheCalibrationDerivativeList().emplace_back(CalibrationDerivative_::Value_::AAD);
        TheCalibrationDerivativeList().emplace_back(CalibrationDerivative_::Value_::FiniteDifference);
        TheCalibrationDerivativeList().emplace_back(CalibrationDerivative_::Value_::None);
   }
   return TheCalibrationDerivativeList();
}


const char* CalibrationDerivative_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::Auto:
        return "Auto";
    case Value_::Analytic:
        return "Analytic";
    case Value_::AAD:
        return "AAD";
    case Value_::FiniteDifference:
        return "FiniteDifference";
    case Value_::None:
        return "None";
        
    }}

struct ReadStringCalibrationDerivative_
{
    ReadStringCalibrationDerivative_() {}

    bool operator()(const String_& src, CalibrationDerivative_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "AUTO"))
        *val = CalibrationDerivative_::Value_::Auto;

	else if (String::Equivalent(src, "ANALYTIC"))
        *val = CalibrationDerivative_::Value_::Analytic;

	else if (String::Equivalent(src, "AAD"))
        *val = CalibrationDerivative_::Value_::AAD;

	else if (String::Equivalent(src, "FINITEDIFFERENCE"))
        *val = CalibrationDerivative_::Value_::FiniteDifference;

	else if (String::Equivalent(src, "NONE"))
        *val = CalibrationDerivative_::Value_::None;
        else
            ret_val = false;
        return ret_val;
    }
};

CalibrationDerivative_::CalibrationDerivative_(const String_& src) {
    static const ReadStringCalibrationDerivative_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable CalibrationDerivative");
}



