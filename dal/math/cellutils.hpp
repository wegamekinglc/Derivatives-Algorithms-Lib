//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <bitset>
#include <dal/math/cell.hpp>
#include <dal/platform/optionals.hpp>

namespace Dal {
    namespace Cell {
        typedef std::bitset<static_cast<int>(Type_::N_TYPES)> types_t;
        bool CanConvert(const Cell_& c, const types_t& allowed);
        struct TypeCheck_ {
            types_t ret_;

            TypeCheck_ Add(const Type_& bit) const {
                TypeCheck_ ret(*this);
                ret.ret_.set(static_cast<int>(bit));
                return ret;
            }
            TypeCheck_ String() const { return Add(Type_::STRING); }
            TypeCheck_ Number() const { return Add(Type_::NUMBER); }
            TypeCheck_ Date() const { return Add(Type_::DATE); }
            TypeCheck_ DateTime() const { return Add(Type_::DATETIME); }
            TypeCheck_ Boolean() const { return Add(Type_::BOOLEAN); }
            TypeCheck_ Empty() const { return Add(Type_::EMPTY); }

            bool operator()(const Cell_& c) const {
                return ret_[static_cast<int>(c.type_)] // already the right type
                       || CanConvert(c, ret_);
            }
        };

        Cell_ ConvertString(const String_& src);
        String_ CoerceToString(const Cell_& src);
        Cell_ FromOptionalDouble(const boost::optional<double>& src);

        template <class T_> T_ ToEnum(const Cell_& src) {
            return T_(CoerceToString(src));
        } // this implementation means we accept non-string values, converting to strings
        template <class T_> Cell_ FromEnum(const T_& src) { return Cell_(String_(src.String())); }
    }
}