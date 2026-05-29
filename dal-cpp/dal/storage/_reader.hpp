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
        virtual bool Exists(int i_col) const = 0;
        virtual double ExtractDouble(int i_col) const = 0;
        virtual double ExtractDouble(int i_col, double def_val) const = 0;
        virtual int ExtractInt(int i_col) const = 0;
        virtual int ExtractInt(int i_col, int def_val) const = 0;
        virtual bool ExtractBool(int i_col) const = 0;
        virtual String_ ExtractString(int i_col) const = 0;
        String_ ExtractString(int i_col, const String_& def_val) const {
            return Exists(i_col) ? ExtractString(i_col) : def_val;
        }
        virtual Date_ ExtractDate(int i_col) const = 0;
        virtual Handle_<Storable_> ExtractHandleBase(_ENV, int i_col) const = 0;

        template <class T_> T_ ExtractEnum(int i_col) const { return T_(ExtractString(i_col)); }
        template <class T_> T_ ExtractEnum(int i_col, const T_& defval) const {
            return Exists(i_col) ? ExtractEnum<T_>(i_col) : defval;
        }
        template <class T_> Handle_<T_> ExtractHandle(_ENV, int i_col) const {
            auto base = ExtractHandleBase(_env, i_col);
            REQUIRE(base, "Missing handle in record");
            auto retval = handle_cast<T_>(base);
            REQUIRE(retval, "Handle has wrong type in record");
            return retval;
        }
        template <class T_> Handle_<T_> ExtractHandle(_ENV, int i_col, const Handle_<T_>& defval) const {
            // if anything is provided, it must be a handle
            return Exists(i_col) ? ExtractHandle<T_>(_env, i_col) : defval;
        }
    };
} // namespace Dal
