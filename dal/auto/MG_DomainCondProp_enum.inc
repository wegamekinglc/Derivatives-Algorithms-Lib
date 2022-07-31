
bool operator==(const DomainCondProp_& lhs, const DomainCondProp_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<DomainCondProp_>& TheDomainCondPropList() {
        RETURN_STATIC(Vector_<DomainCondProp_>);
    }
}    // leave local

Vector_<DomainCondProp_> DomainCondPropListAll() {
   if (TheDomainCondPropList().empty()) {
        TheDomainCondPropList().emplace_back(DomainCondProp_::Value_::AlwaysTrue);
        TheDomainCondPropList().emplace_back(DomainCondProp_::Value_::AlwaysFalse);
        TheDomainCondPropList().emplace_back(DomainCondProp_::Value_::TrueOrFalse);
   }
   return TheDomainCondPropList();
}


const char* DomainCondProp_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
        case Value_::AlwaysTrue:
        return "AlwaysTrue";
    case Value_::AlwaysFalse:
        return "AlwaysFalse";
    case Value_::TrueOrFalse:
        return "TrueOrFalse";
        
    }}

struct ReadStringDomainCondProp_
{
    ReadStringDomainCondProp_() {}

    bool operator()(const String_& src, DomainCondProp_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
        
	else if (String::Equivalent(src, "ALWAYSTRUE"))
        *val = DomainCondProp_::Value_::AlwaysTrue;

	else if (String::Equivalent(src, "ALWAYSFALSE"))
        *val = DomainCondProp_::Value_::AlwaysFalse;

	else if (String::Equivalent(src, "TRUEORFALSE"))
        *val = DomainCondProp_::Value_::TrueOrFalse;
        else
            ret_val = false;
        return ret_val;
    }
};

DomainCondProp_::DomainCondProp_(const String_& src) {
    static const ReadStringDomainCondProp_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable DomainCondProp");
}



