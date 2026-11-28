
bool operator==(const SecondOrderErrorReason_& lhs, const SecondOrderErrorReason_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<SecondOrderErrorReason_>& TheSecondOrderErrorReasonList() {
        RETURN_STATIC(Vector_<SecondOrderErrorReason_>);
    }
}    // leave local

Vector_<SecondOrderErrorReason_> SecondOrderErrorReasonListAll() {
   if (TheSecondOrderErrorReasonList().empty()) {
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::INVALID_INPUT);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::OUTSIDE_DOMAIN);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::UNSUPPORTED_OPERATION);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::BUDGET_EXCEEDED);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::CAPACITY_EXCEEDED);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::LIFECYCLE);
        TheSecondOrderErrorReasonList().emplace_back(SecondOrderErrorReason_::Value_::NUMERICAL_FAILURE);
   }
   return TheSecondOrderErrorReasonList();
}


const char* SecondOrderErrorReason_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::INVALID_INPUT:
        return "INVALID_INPUT";
    case Value_::OUTSIDE_DOMAIN:
        return "OUTSIDE_DOMAIN";
    case Value_::UNSUPPORTED_OPERATION:
        return "UNSUPPORTED_OPERATION";
    case Value_::BUDGET_EXCEEDED:
        return "BUDGET_EXCEEDED";
    case Value_::CAPACITY_EXCEEDED:
        return "CAPACITY_EXCEEDED";
    case Value_::LIFECYCLE:
        return "LIFECYCLE";
    case Value_::NUMERICAL_FAILURE:
        return "NUMERICAL_FAILURE";
        
    }}

struct ReadStringSecondOrderErrorReason_
{
    ReadStringSecondOrderErrorReason_() {}

    bool operator()(const String_& src, SecondOrderErrorReason_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "INVALIDINPUT"))
        *val = SecondOrderErrorReason_::Value_::INVALID_INPUT;

	else if (String::Equivalent(src, "OUTSIDEDOMAIN"))
        *val = SecondOrderErrorReason_::Value_::OUTSIDE_DOMAIN;

	else if (String::Equivalent(src, "UNSUPPORTEDOPERATION"))
        *val = SecondOrderErrorReason_::Value_::UNSUPPORTED_OPERATION;

	else if (String::Equivalent(src, "BUDGETEXCEEDED"))
        *val = SecondOrderErrorReason_::Value_::BUDGET_EXCEEDED;

	else if (String::Equivalent(src, "CAPACITYEXCEEDED"))
        *val = SecondOrderErrorReason_::Value_::CAPACITY_EXCEEDED;

	else if (String::Equivalent(src, "LIFECYCLE"))
        *val = SecondOrderErrorReason_::Value_::LIFECYCLE;

	else if (String::Equivalent(src, "NUMERICALFAILURE"))
        *val = SecondOrderErrorReason_::Value_::NUMERICAL_FAILURE;
        else
            ret_val = false;
        return ret_val;
    }
};

SecondOrderErrorReason_::SecondOrderErrorReason_(const String_& src) {
    static const ReadStringSecondOrderErrorReason_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable SecondOrderErrorReason");
}



