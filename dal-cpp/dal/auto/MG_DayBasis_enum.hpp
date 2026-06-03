#pragma once

class  DayBasis_
{
    enum class Value_ : char
    {
     _NOT_SET=-1,
     ACT_365F,
     ACT_365L,
     ACT_360,
     ACT_ACT,
     BOND,
 _EXTENSION,
     _N_VALUES
    } val_;

public:
    // This will be an extensible enum
    class Extension_
    {
    public:
        virtual ~Extension_();
        // Must implement DayBasis_ interface
        virtual const char* String() const = 0;
		virtual double operator()(const Date_& start_date, const Date_& end_date, const DayBasis::Context_* context) const = 0;
    };
private:
   
    Handle_<Extension_> other_;
    const Extension_& Extension() const;
    DayBasis_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_EXTENSION, "val is not valid");
    }
    DayBasis_(const Handle_<Extension_>& imp) : val_(Value_::_EXTENSION), other_(imp) {assert(imp.get());}
    friend void DayBasis_RejectDuplicate(const String_&);
       friend bool operator==(const DayBasis_& lhs, const DayBasis_& rhs);
    friend struct ReadStringDayBasis_;
    friend Vector_<DayBasis_> DayBasisListAll();
    friend bool operator<(const DayBasis_& lhs, const DayBasis_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit DayBasis_(const String_& src);
    const char* String() const;
	// idiosyncratic (hand-written) members:
    double operator()(const Date_& start_date, const Date_& end_date, const DayBasis::Context_* context) const;
    DayBasis_() : val_(Value_::_NOT_SET) {};
};

Vector_<DayBasis_> DayBasisListAll();

bool operator==(const DayBasis_& lhs, const DayBasis_& rhs);
inline bool operator!=(const DayBasis_& lhs, const DayBasis_& rhs) {return !(lhs == rhs);}

namespace DayBasis
{
    void RegisterExtension
       (const Vector_<String_>& names,
        const Handle_<DayBasis_::Extension_>& imp);
}