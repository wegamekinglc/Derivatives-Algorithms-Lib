
bool operator==(const SecondOrderStage_& lhs, const SecondOrderStage_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<SecondOrderStage_>& TheSecondOrderStageList() {
        RETURN_STATIC(Vector_<SecondOrderStage_>);
    }
}    // leave local

Vector_<SecondOrderStage_> SecondOrderStageListAll() {
   if (TheSecondOrderStageList().empty()) {
        TheSecondOrderStageList().emplace_back(SecondOrderStage_::Value_::VALIDATION);
        TheSecondOrderStageList().emplace_back(SecondOrderStage_::Value_::RECORDING);
        TheSecondOrderStageList().emplace_back(SecondOrderStage_::Value_::REVERSE);
        TheSecondOrderStageList().emplace_back(SecondOrderStage_::Value_::RESULT);
        TheSecondOrderStageList().emplace_back(SecondOrderStage_::Value_::CLEANUP);
   }
   return TheSecondOrderStageList();
}


const char* SecondOrderStage_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::VALIDATION:
        return "VALIDATION";
    case Value_::RECORDING:
        return "RECORDING";
    case Value_::REVERSE:
        return "REVERSE";
    case Value_::RESULT:
        return "RESULT";
    case Value_::CLEANUP:
        return "CLEANUP";
        
    }}

struct ReadStringSecondOrderStage_
{
    ReadStringSecondOrderStage_() {}

    bool operator()(const String_& src, SecondOrderStage_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "VALIDATION"))
        *val = SecondOrderStage_::Value_::VALIDATION;

	else if (String::Equivalent(src, "RECORDING"))
        *val = SecondOrderStage_::Value_::RECORDING;

	else if (String::Equivalent(src, "REVERSE"))
        *val = SecondOrderStage_::Value_::REVERSE;

	else if (String::Equivalent(src, "RESULT"))
        *val = SecondOrderStage_::Value_::RESULT;

	else if (String::Equivalent(src, "CLEANUP"))
        *val = SecondOrderStage_::Value_::CLEANUP;
        else
            ret_val = false;
        return ret_val;
    }
};

SecondOrderStage_::SecondOrderStage_(const String_& src) {
    static const ReadStringSecondOrderStage_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable SecondOrderStage");
}



