#pragma once

class  AADProfilingPhase_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     PREPARE,
     CALIBRATE,
     WORKER_INIT,
     PATH_FORWARD,
     PAYOFF,
     REVERSE,
     REVERSE_SUFFIX,
     REVERSE_PREFIX,
     WAIT,
     REDUCE,
     SAMPLE_TAPE,
     QUOTE_MAP,
     LSM_TRAIN,
     LSM_REGRESS,
     LSM_REPLAY,
     _N_VALUES
    } val_;
      
    AADProfilingPhase_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const AADProfilingPhase_& lhs, const AADProfilingPhase_& rhs);
    friend struct ReadStringAADProfilingPhase_;
    friend Vector_<AADProfilingPhase_> AADProfilingPhaseListAll();
    friend bool operator<(const AADProfilingPhase_& lhs, const AADProfilingPhase_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit AADProfilingPhase_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    AADProfilingPhase_() : val_(Value_::_NOT_SET) {};
};

Vector_<AADProfilingPhase_> AADProfilingPhaseListAll();

bool operator==(const AADProfilingPhase_& lhs, const AADProfilingPhase_& rhs);
inline bool operator!=(const AADProfilingPhase_& lhs, const AADProfilingPhase_& rhs) {return !(lhs == rhs);}
inline bool operator==(const AADProfilingPhase_& lhs, AADProfilingPhase_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const AADProfilingPhase_& lhs, AADProfilingPhase_::Value_ rhs) {return lhs.Switch() != rhs;}
