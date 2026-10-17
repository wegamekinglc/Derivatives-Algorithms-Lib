
bool operator==(const AADProfilingPhase_& lhs, const AADProfilingPhase_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<AADProfilingPhase_>& TheAADProfilingPhaseList() {
        RETURN_STATIC(Vector_<AADProfilingPhase_>);
    }
}    // leave local

Vector_<AADProfilingPhase_> AADProfilingPhaseListAll() {
   if (TheAADProfilingPhaseList().empty()) {
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::PREPARE);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::CALIBRATE);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::WORKER_INIT);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::PATH_FORWARD);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::PAYOFF);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::REVERSE);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::REVERSE_SUFFIX);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::REVERSE_PREFIX);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::WAIT);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::REDUCE);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::SAMPLE_TAPE);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::QUOTE_MAP);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::LSM_TRAIN);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::LSM_REGRESS);
        TheAADProfilingPhaseList().emplace_back(AADProfilingPhase_::Value_::LSM_REPLAY);
   }
   return TheAADProfilingPhaseList();
}


const char* AADProfilingPhase_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::PREPARE:
        return "PREPARE";
    case Value_::CALIBRATE:
        return "CALIBRATE";
    case Value_::WORKER_INIT:
        return "WORKER_INIT";
    case Value_::PATH_FORWARD:
        return "PATH_FORWARD";
    case Value_::PAYOFF:
        return "PAYOFF";
    case Value_::REVERSE:
        return "REVERSE";
    case Value_::REVERSE_SUFFIX:
        return "REVERSE_SUFFIX";
    case Value_::REVERSE_PREFIX:
        return "REVERSE_PREFIX";
    case Value_::WAIT:
        return "WAIT";
    case Value_::REDUCE:
        return "REDUCE";
    case Value_::SAMPLE_TAPE:
        return "SAMPLE_TAPE";
    case Value_::QUOTE_MAP:
        return "QUOTE_MAP";
    case Value_::LSM_TRAIN:
        return "LSM_TRAIN";
    case Value_::LSM_REGRESS:
        return "LSM_REGRESS";
    case Value_::LSM_REPLAY:
        return "LSM_REPLAY";
        
    }}

struct ReadStringAADProfilingPhase_
{
    ReadStringAADProfilingPhase_() {}

    bool operator()(const String_& src, AADProfilingPhase_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "PREPARE"))
        *val = AADProfilingPhase_::Value_::PREPARE;

	else if (String::Equivalent(src, "CALIBRATE"))
        *val = AADProfilingPhase_::Value_::CALIBRATE;

	else if (String::Equivalent(src, "WORKERINIT"))
        *val = AADProfilingPhase_::Value_::WORKER_INIT;

	else if (String::Equivalent(src, "PATHFORWARD"))
        *val = AADProfilingPhase_::Value_::PATH_FORWARD;

	else if (String::Equivalent(src, "PAYOFF"))
        *val = AADProfilingPhase_::Value_::PAYOFF;

	else if (String::Equivalent(src, "REVERSE"))
        *val = AADProfilingPhase_::Value_::REVERSE;

	else if (String::Equivalent(src, "REVERSESUFFIX"))
        *val = AADProfilingPhase_::Value_::REVERSE_SUFFIX;

	else if (String::Equivalent(src, "REVERSEPREFIX"))
        *val = AADProfilingPhase_::Value_::REVERSE_PREFIX;

	else if (String::Equivalent(src, "WAIT"))
        *val = AADProfilingPhase_::Value_::WAIT;

	else if (String::Equivalent(src, "REDUCE"))
        *val = AADProfilingPhase_::Value_::REDUCE;

	else if (String::Equivalent(src, "SAMPLETAPE"))
        *val = AADProfilingPhase_::Value_::SAMPLE_TAPE;

	else if (String::Equivalent(src, "QUOTEMAP"))
        *val = AADProfilingPhase_::Value_::QUOTE_MAP;

	else if (String::Equivalent(src, "LSMTRAIN"))
        *val = AADProfilingPhase_::Value_::LSM_TRAIN;

	else if (String::Equivalent(src, "LSMREGRESS"))
        *val = AADProfilingPhase_::Value_::LSM_REGRESS;

	else if (String::Equivalent(src, "LSMREPLAY"))
        *val = AADProfilingPhase_::Value_::LSM_REPLAY;
        else
            ret_val = false;
        return ret_val;
    }
};

AADProfilingPhase_::AADProfilingPhase_(const String_& src) {
    static const ReadStringAADProfilingPhase_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable AADProfilingPhase");
}



