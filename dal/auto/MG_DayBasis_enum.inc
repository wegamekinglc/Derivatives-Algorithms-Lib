
bool operator==(const DayBasis_& lhs, const DayBasis_& rhs) {return lhs.val_ == rhs.val_;}
DayBasis_::Extension_::~Extension_() {}
namespace {
    Vector_<DayBasis_>& TheDayBasisList() {
        RETURN_STATIC(Vector_<DayBasis_>);
    }

    std::map<String_, Handle_<DayBasis_::Extension_>>& TheDayBasisExtensions()
    {
        RETURN_STATIC(std::map<String_, Handle_<DayBasis_::Extension_>>);
    }

}    // leave local

void DayBasis_RejectDuplicate(const String_&);
void DayBasis::RegisterExtension
    (const Vector_<String_>& names,
     const Handle_<DayBasis_::Extension_>& imp) 
{
    REQUIRE(TheDayBasisList().empty(), "Can't register a new DayBasis after enumerating all values");
    assert(!imp.Empty());
    DayBasis_RejectDuplicate(imp->String());
    for (auto pn = names.begin(); pn != names.end(); ++pn)
    {
        DayBasis_RejectDuplicate(*pn);    // check this name does not already have a meaning
        TheDayBasisExtensions()[String::Condensed(*pn)] = imp;
    }
    // check that two-way string conversion works
    DayBasis_ check(imp->String());
    REQUIRE(check.String() == imp->String(), "String representation reconstructs the wrong DayBasis -- presumably a name clash"); 
}

Vector_<DayBasis_> DayBasisListAll()
{
    if (TheDayBasisList().empty())
    {
        Vector_<DayBasis_> retval;
        std::set<String_> exists;
		retval.emplace_back("ACT_365F");
		retval.emplace_back("ACT_365L");
		retval.emplace_back("ACT_360");
		retval.emplace_back("ACT_ACT");
		retval.emplace_back("BOND");
		for (const auto& r : retval)
            exists.insert(r.String());
        auto more = TheDayBasisExtensions();
        for (auto e : more)
        {
            if (!exists.count(e.second->String()))
            {
                retval.emplace_back(e.second->String());
                exists.insert(e.second->String());
            }
        }
        // store these forever
        TheDayBasisList().Swap(&retval);
    }
    return TheDayBasisList();
}



const char* DayBasis_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::ACT_365F:
        return "ACT_365F";
	case Value_::ACT_365L:
        return "ACT_365L";
	case Value_::ACT_360:
        return "ACT_360";
	case Value_::ACT_ACT:
        return "ACT_ACT";
	case Value_::BOND:
        return "BOND";
	    case Value_::_EXTENSION:
        return other_->String();
    }}

struct ReadStringDayBasis_
{
    ReadStringDayBasis_() {}

    bool operator()(const String_& src, DayBasis_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ACT365F"))
		*val = DayBasis_::Value_::ACT_365F;
	else if (String::Equivalent(src, "ACT/365F"))
		*val = DayBasis_::Value_::ACT_365F;
	else if (String::Equivalent(src, "ACT365FIXED"))
		*val = DayBasis_::Value_::ACT_365F;
	else if (String::Equivalent(src, "ACT/365FIXED"))
		*val = DayBasis_::Value_::ACT_365F;

	else if (String::Equivalent(src, "ACT365L"))
		*val = DayBasis_::Value_::ACT_365L;
	else if (String::Equivalent(src, "ACT/365L"))
		*val = DayBasis_::Value_::ACT_365L;
	else if (String::Equivalent(src, "ISMAYEAR"))
		*val = DayBasis_::Value_::ACT_365L;

	else if (String::Equivalent(src, "ACT360"))
		*val = DayBasis_::Value_::ACT_360;
	else if (String::Equivalent(src, "ACT/360"))
		*val = DayBasis_::Value_::ACT_360;
	else if (String::Equivalent(src, "MONEY"))
		*val = DayBasis_::Value_::ACT_360;
	else if (String::Equivalent(src, "ACTUAL/360"))
		*val = DayBasis_::Value_::ACT_360;

	else if (String::Equivalent(src, "ACTACT"))
		*val = DayBasis_::Value_::ACT_ACT;
	else if (String::Equivalent(src, "ACT/ACT"))
		*val = DayBasis_::Value_::ACT_ACT;
	else if (String::Equivalent(src, "ACTUAL/ACTUAL"))
		*val = DayBasis_::Value_::ACT_ACT;

	else if (String::Equivalent(src, "BOND"))
		*val = DayBasis_::Value_::BOND;
	else if (String::Equivalent(src, "30360"))
		*val = DayBasis_::Value_::BOND;
	else if (String::Equivalent(src, "30/360"))
		*val = DayBasis_::Value_::BOND;
	else if (String::Equivalent(src, "30360US"))
		*val = DayBasis_::Value_::BOND;
        else
            ret_val = false;
        return ret_val;
    }
};

DayBasis_::DayBasis_(const String_& src) {
    static const ReadStringDayBasis_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    else
    {
        // check for extensions
        auto pe = TheDayBasisExtensions().find(String::Condensed(src));
        if (pe != TheDayBasisExtensions().end())
        {
            val_ = Value_::_EXTENSION;
            other_ = pe->second;
			   return;
        }
    }

    THROW("'" + src + "' is not a recognizable DayBasis");
}

void DayBasis_RejectDuplicate(const String_& test)	// check added extensions to ensure they are not already used
{
    static const ReadStringDayBasis_ READ_FIXED;    // allows precomputation for speed, in constructor
	DayBasis_::Value_ val;
	NOTICE(test);
	REQUIRE(!READ_FIXED(test, &val), "Attempt to change meaning of fixed DayBasis string");
	REQUIRE(!TheDayBasisExtensions().count(String::Condensed(test)), "Attempt to change meaning of DayBasis string");
}


