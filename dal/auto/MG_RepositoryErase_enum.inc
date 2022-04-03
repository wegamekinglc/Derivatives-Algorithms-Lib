
bool operator==(const RepositoryErase_& lhs, const RepositoryErase_& rhs) {return lhs.val_ == rhs.val_;}
namespace {
    Vector_<RepositoryErase_>& TheRepositoryEraseList() {
        RETURN_STATIC(Vector_<RepositoryErase_>);
    }
}    // leave local

Vector_<RepositoryErase_> RepositoryEraseListAll() {
   if (TheRepositoryEraseList().empty()) {
   }
   return TheRepositoryEraseList();
}


const char* RepositoryErase_::String() const {
    switch (val_)
    {
    default:
    case Value_::_NOT_SET:
        return 0;
            
    }}

struct ReadStringRepositoryErase_
{
    ReadStringRepositoryErase_() {}

    bool operator()(const String_& src, RepositoryErase_::Value_* val) const    // returns true iff recognized input
    {
        bool ret_val = true;
        if (0);	// otiose code to allow regular else-if structure
        else if (src.empty())
        { ret_val = false; }
                else
            ret_val = false;
        return ret_val;
    }
};

RepositoryErase_::RepositoryErase_(const String_& src) {
    static const ReadStringRepositoryErase_ READ_FIXED;    // allows precomputation for speed, in constructor
    if (READ_FIXED(src, &val_))
        return;
    THROW("'" + src + "' is not a recognizable RepositoryErase");
}



