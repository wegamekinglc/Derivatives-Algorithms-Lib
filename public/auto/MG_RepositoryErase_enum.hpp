#pragma once

class __declspec(dllexport) RepositoryErase_
{
public:
    enum class Value_ : char
    {
     _NOT_SET=-1,
     NONE,
     NAME_NONEMPTY,
     NAME,
     TYPE,
     _N_VALUES
    } val_;
      
    RepositoryErase_(Value_ val) : val_(val) {
        REQUIRE(val < Value_::_N_VALUES, "val is not valid");
    }
private:
    friend __declspec(dllexport)bool operator==(const RepositoryErase_& lhs, const RepositoryErase_& rhs);
    friend struct ReadStringRepositoryErase_;
    friend Vector_<RepositoryErase_> RepositoryEraseListAll();
    friend bool operator<(const RepositoryErase_& lhs, const RepositoryErase_& rhs) {
        return lhs.val_ < rhs.val_;
    }
public:
    explicit RepositoryErase_(const String_& src);
    const char* String() const;
    Value_ Switch() const {return val_;}
    RepositoryErase_() : val_(Value_::_NOT_SET) {};
};

Vector_<RepositoryErase_> RepositoryEraseListAll();

__declspec(dllexport)bool operator==(const RepositoryErase_& lhs, const RepositoryErase_& rhs);
inline bool operator!=(const RepositoryErase_& lhs, const RepositoryErase_& rhs) {return !(lhs == rhs);}
inline bool operator==(const RepositoryErase_& lhs, RepositoryErase_::Value_ rhs) {return lhs.Switch() == rhs;}
inline bool operator!=(const RepositoryErase_& lhs, RepositoryErase_::Value_ rhs) {return lhs.Switch() != rhs;}
