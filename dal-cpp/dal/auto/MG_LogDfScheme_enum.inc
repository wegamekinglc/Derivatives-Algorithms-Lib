
bool operator==(const LogDfScheme_& lhs, const LogDfScheme_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<LogDfScheme_>& TheLogDfSchemeList() {
        RETURN_STATIC(Vector_<LogDfScheme_>);
    }
}    // leave local

Vector_<LogDfScheme_> LogDfSchemeListAll() {
   if (TheLogDfSchemeList().empty()) {
        TheLogDfSchemeList().emplace_back(LogDfScheme_::Value_::LOG_LINEAR);
        TheLogDfSchemeList().emplace_back(LogDfScheme_::Value_::LOG_CUBIC_NATURAL);
        TheLogDfSchemeList().emplace_back(LogDfScheme_::Value_::MIXED);
   }
   return TheLogDfSchemeList();
}


const char* LogDfScheme_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::LOG_LINEAR:
        return "LOG_LINEAR";
    case Value_::LOG_CUBIC_NATURAL:
        return "LOG_CUBIC_NATURAL";
    case Value_::MIXED:
        return "MIXED";
        
    }}

struct ReadStringLogDfScheme_
{
    ReadStringLogDfScheme_() {}

    bool operator()(const String_& src, LogDfScheme_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "LOGLINEAR"))
        *val = LogDfScheme_::Value_::LOG_LINEAR;

	else if (String::Equivalent(src, "LOGCUBICNATURAL"))
        *val = LogDfScheme_::Value_::LOG_CUBIC_NATURAL;

	else if (String::Equivalent(src, "MIXED"))
        *val = LogDfScheme_::Value_::MIXED;
        else
            ret_val = false;
        return ret_val;
    }
};

LogDfScheme_::LogDfScheme_(const String_& src) {
    static const ReadStringLogDfScheme_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable LogDfScheme");
}



