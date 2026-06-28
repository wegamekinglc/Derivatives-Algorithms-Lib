
bool operator==(const AuditorMode_& lhs, const AuditorMode_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<AuditorMode_>& TheAuditorModeList() {
        RETURN_STATIC(Vector_<AuditorMode_>);
    }
}    // leave local

Vector_<AuditorMode_> AuditorModeListAll() {
   if (TheAuditorModeList().empty()) {
        TheAuditorModeList().emplace_back(AuditorMode_::Value_::PASSIVE);
        TheAuditorModeList().emplace_back(AuditorMode_::Value_::READING);
        TheAuditorModeList().emplace_back(AuditorMode_::Value_::READING_EXCLUSIVE);
        TheAuditorModeList().emplace_back(AuditorMode_::Value_::SHOWING);
   }
   return TheAuditorModeList();
}


const char* AuditorMode_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::PASSIVE:
        return "PASSIVE";
    case Value_::READING:
        return "READING";
    case Value_::READING_EXCLUSIVE:
        return "READING_EXCLUSIVE";
    case Value_::SHOWING:
        return "SHOWING";
        
    }}

struct ReadStringAuditorMode_
{
    ReadStringAuditorMode_() {}

    bool operator()(const String_& src, AuditorMode_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PASSIVE"))
        *val = AuditorMode_::Value_::PASSIVE;

	else if (String::Equivalent(src, "READING"))
        *val = AuditorMode_::Value_::READING;

	else if (String::Equivalent(src, "READINGEXCLUSIVE"))
        *val = AuditorMode_::Value_::READING_EXCLUSIVE;

	else if (String::Equivalent(src, "SHOWING"))
        *val = AuditorMode_::Value_::SHOWING;
        else
            ret_val = false;
        return ret_val;
    }
};

AuditorMode_::AuditorMode_(const String_& src) {
    static const ReadStringAuditorMode_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable AuditorMode");
}



