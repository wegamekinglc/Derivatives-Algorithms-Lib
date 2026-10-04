
bool operator==(const AADRecordingState_& lhs, const AADRecordingState_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<AADRecordingState_>& TheAADRecordingStateList() {
        RETURN_STATIC(Vector_<AADRecordingState_>);
    }
}    // leave local

Vector_<AADRecordingState_> AADRecordingStateListAll() {
   if (TheAADRecordingStateList().empty()) {
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::REGISTERING);
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::RECORDING);
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::READY);
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::REVERSING);
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::FAILED);
        TheAADRecordingStateList().emplace_back(AADRecordingState_::Value_::CLOSED);
   }
   return TheAADRecordingStateList();
}


const char* AADRecordingState_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::REGISTERING:
        return "REGISTERING";
    case Value_::RECORDING:
        return "RECORDING";
    case Value_::READY:
        return "READY";
    case Value_::REVERSING:
        return "REVERSING";
    case Value_::FAILED:
        return "FAILED";
    case Value_::CLOSED:
        return "CLOSED";
        
    }}

struct ReadStringAADRecordingState_
{
    ReadStringAADRecordingState_() {}

    bool operator()(const String_& src, AADRecordingState_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "REGISTERING"))
        *val = AADRecordingState_::Value_::REGISTERING;

	else if (String::Equivalent(src, "RECORDING"))
        *val = AADRecordingState_::Value_::RECORDING;

	else if (String::Equivalent(src, "READY"))
        *val = AADRecordingState_::Value_::READY;

	else if (String::Equivalent(src, "REVERSING"))
        *val = AADRecordingState_::Value_::REVERSING;

	else if (String::Equivalent(src, "FAILED"))
        *val = AADRecordingState_::Value_::FAILED;

	else if (String::Equivalent(src, "CLOSED"))
        *val = AADRecordingState_::Value_::CLOSED;
        else
            ret_val = false;
        return ret_val;
    }
};

AADRecordingState_::AADRecordingState_(const String_& src) {
    static const ReadStringAADRecordingState_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable AADRecordingState");
}



