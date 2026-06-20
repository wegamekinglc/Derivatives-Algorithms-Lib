//
// Created by wegam on 2022/4/3.
//

#pragma once

#include <dal/utilities/environment.hpp>

namespace Dal {
    class Date_;
    class Storable_;

    class UIRow_ : noncopyable {
    public:
        virtual bool Exists(int iCol) const = 0;
        virtual double ExtractDouble(int iCol) const = 0;
        virtual double ExtractDouble(int iCol, double def_val) const = 0;
        virtual int ExtractInt(int iCol) const = 0;
        virtual int ExtractInt(int iCol, int def_val) const = 0;
        virtual bool ExtractBool(int iCol) const = 0;
        virtual String_ ExtractString(int iCol) const = 0;
        String_ ExtractString(int iCol, const String_& def_val) const {
            return Exists(iCol) ? ExtractString(iCol) : def_val;
        }
        virtual Date_ ExtractDate(int iCol) const = 0;
        virtual Handle_<Storable_> ExtractHandleBase(_ENV, int iCol) const = 0;

        template <class T_> T_ ExtractEnum(int iCol) const { return T_(ExtractString(iCol)); }
        template <class T_> T_ ExtractEnum(int iCol, const T_& defval) const {
            return Exists(iCol) ? ExtractEnum<T_>(iCol) : defval;
        }
        template <class T_> Handle_<T_> ExtractHandle(_ENV, int iCol) const {
            auto base = ExtractHandleBase(_env, iCol);
            REQUIRE(base, "Missing handle in record");
            auto retval = handle_cast<T_>(base);
            REQUIRE(retval, "Handle has wrong type in record");
            return retval;
        }
        template <class T_> Handle_<T_> ExtractHandle(_ENV, int iCol, const Handle_<T_>& defval) const {
            // if anything is provided, it must be a handle
            return Exists(iCol) ? ExtractHandle<T_>(_env, iCol) : defval;
        }
    };
} // namespace Dal
