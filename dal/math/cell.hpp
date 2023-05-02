//
// Created by wegamekinglc on 2020/5/2.
//

#pragma once

#include <variant>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    struct Cell_ {
        std::variant<bool, double, Date_, DateTime_, String_, std::monostate> val_;

        Cell_() : val_(std::monostate()) {}
        explicit Cell_(bool b) : val_(b) {}
        explicit Cell_(double d) : val_(d) {}
        explicit Cell_(const Date_& dt) : val_(dt) {}
        explicit Cell_(const DateTime_& dt) : val_(dt) {}
        explicit Cell_(const String_& s) : val_(s) {}
        explicit Cell_(const char* s) : val_(String_(s)) {}

        // unrecognized pointer type, hides ptr-to-bool conversion
        template <class T_> Cell_(const T_* p) = delete;
        template <class V_> auto Visit(V_ func) const { return std::visit(func, val_); }

        bool operator==(const Cell_& rhs) const {
            return val_ == rhs.val_;
        }

        void Clear() { val_ = std::monostate(); }
        Cell_& operator=(bool b) {
            val_ = b;
            return *this;
        }
        Cell_& operator=(int i) {
            val_ = static_cast<double>(i);
            return *this;
        }
        Cell_& operator=(double d) {
            val_ = d;
            return *this;
        }
        Cell_& operator=(const Date_& dt) {
            val_ = dt;
            return *this;
        }
        Cell_& operator=(const DateTime_& dt) {
            val_ = dt;
            return *this;
        }
        Cell_& operator=(const String_& s) {
            val_ = s;
            return *this;
        }
        Cell_& operator=(const char* s) {
            val_ = String_(s);
            return *this;
        }

        Cell_& operator=(const Cell_& rhs) = default;
    };

    bool operator==(const Cell_& lhs, const String_& rhs);
    inline bool operator==(const String_& lhs, const Cell_& rhs) { return rhs == lhs; }

    namespace Cell {

        inline bool IsEmpty(const Cell_& cell) {
            if (auto p = std::get_if<String_>(&cell.val_))
                return p->empty();
            return static_cast<bool>(std::get_if<std::monostate>(&cell.val_));
        }

        inline bool IsString(const Cell_& src) { return static_cast<bool>(std::get_if<String_>(&src.val_)); }
        String_ ToString(const Cell_& src);
        inline Cell_ FromString(const String_& src) { return Cell_(src); }

        template<class T_> T_ ToEnum(const Cell_& src) {
            return T_(ToString(src));
        }
        template<class T_> T_ ToEnum(const Cell_& src, const T_& empty_val) {
            return IsEmpty(src) ? empty_val : ToEnum<T_>(src);
        }
        template<class T_> Cell_ FromEnum(const T_& src) {
            return Cell_(String_(src.String()));
        }

        inline bool IsDouble(const Cell_& src) { return static_cast<bool>(std::get_if<double>(&src.val_)); }
        double ToDouble(const Cell_& src);
        inline double ToDouble(const Cell_& src, double empty_val) {
            return IsEmpty(src) ? empty_val : ToDouble(src);
        }
        inline Cell_ FromDouble(double src) { return Cell_(src); }

        bool IsInt(const Cell_& src);
        int ToInt(const Cell_& src);
        inline int ToInt(const Cell_& src, int empty_val) {
            return IsEmpty(src) ? empty_val : ToInt(src);
        }
        inline Cell_ FromInt(int src) { return Cell_(double(src)); }

        inline bool IsBool(const Cell_& src) { return static_cast<bool>(std::get_if<bool>(&src.val_)); }
        bool ToBool(const Cell_& src);
        inline Cell_ FromBool(bool src) { return Cell_(src); }

        inline bool IsDate(const Cell_& src) { return static_cast<bool>(std::get_if<Date_>(&src.val_)); }
        Date_ ToDate(const Cell_& src);
        inline Cell_ FromDate(const Date_& src) { return Cell_(src); }

        inline bool IsDateTime(const Cell_& src) { return static_cast<bool>(std::get_if<DateTime_>(&src.val_)); }
        DateTime_ ToDateTime(const Cell_& src);
        inline Cell_ FromDateTime(const DateTime_& src) { return Cell_(src); }

        Vector_<bool> ToBoolVector(const Cell_& src);
        Cell_ FromBoolVector(const Vector_<bool>& src);
    } // namespace Cell
} // namespace Dal
