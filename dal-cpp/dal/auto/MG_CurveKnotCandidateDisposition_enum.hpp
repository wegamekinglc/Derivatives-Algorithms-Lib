#pragma once

class  CurveKnotCandidateDisposition_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     ADDED,
     DUPLICATE,
     FILTERED_NOT_AFTER_TODAY,
     _N_VALUES
    } val_;
      
    CurveKnotCandidateDisposition_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend bool operator==(const CurveKnotCandidateDisposition_& lhs, const CurveKnotCandidateDisposition_& rhs);
    friend struct ReadStringCurveKnotCandidateDisposition_;
    friend Vector_<CurveKnotCandidateDisposition_> CurveKnotCandidateDispositionListAll();
    friend bool operator<(const CurveKnotCandidateDisposition_& lhs, const CurveKnotCandidateDisposition_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit CurveKnotCandidateDisposition_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    CurveKnotCandidateDisposition_() : val_(Value_::_NOT_SET) {};
};

Vector_<CurveKnotCandidateDisposition_> CurveKnotCandidateDispositionListAll();

bool operator==(const CurveKnotCandidateDisposition_& lhs, const CurveKnotCandidateDisposition_& rhs);
inline bool operator!=(const CurveKnotCandidateDisposition_& lhs, const CurveKnotCandidateDisposition_& rhs) {return !(lhs == rhs);}
inline bool operator==(const CurveKnotCandidateDisposition_& lhs, CurveKnotCandidateDisposition_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const CurveKnotCandidateDisposition_& lhs, CurveKnotCandidateDisposition_::Value_ rhs) {return lhs.Switch() != rhs;}
