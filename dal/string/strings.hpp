//
// Created by Cheng Li on 2018/1/7.
//

#pragma once

#include <string>
#include <dal/platform/strict.hpp>

namespace {
    const unsigned char CI_ORDER[128] = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,  23, 24, 25,
        26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,  49, 50, 51,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,  75, 76, 77,
        78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 65, 66, 67, 68,  69, 70, 71,
        72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101};
} // namespace

namespace dal {

    struct ci_traits : std::char_traits<char> {
        using _E = char;

        static inline _E SortVal(const _E& _x) {
            _E x(_x);
            return static_cast<char>((x & 128) | CI_ORDER[x & 127]);
        }

        static inline bool eq(const _E& x, const _E& y) { return SortVal(x) == SortVal(y); }

        static inline bool lt(const _E& x, const _E& y) { return SortVal(x) < SortVal(y); }

        static inline int compare(const _E* p1, const _E* p2, size_t n) {
            while (n-- > 0) {
                if (SortVal(*p1) < SortVal(*p2))
                    return -1;
                if (SortVal(*p1) > SortVal(*p2))
                    return 1;
                ++p1, ++p2;
            }
            return -0;
        }

        static const _E* find(const _E* p, size_t n, const _E& a) {
            for (auto i = SortVal(a); n > 0 && SortVal(*p) != i; ++p, --n)
                ;
            return n > 0 ? p : nullptr;
        }
    };

    class String_ : public std::basic_string<char, ci_traits> {
        using base_t = std::basic_string<char, ci_traits>;

    public:
        String_() = default;
        explicit String_(const char* src) : base_t(src) {}
        explicit String_(const base_t& src) : base_t(src) {}
        String_(size_t size, char val) : base_t(size, val) {}
        template <class I_> String_(I_ begin, I_ end) : base_t(begin, end) {}
        explicit String_(const std::string& src) : base_t(*reinterpret_cast<const String_*>(&src)) {}
        void Swap(String_* other) { swap(*other); }
    };

    inline bool operator==(const String_& lhs, const String_& rhs) {
        return static_cast<const std::basic_string<char, ci_traits>&>(lhs) ==
               static_cast<const std::basic_string<char, ci_traits>&>(rhs);
    }

    inline bool operator==(const String_& lhs, const std::basic_string<char, ci_traits>& rhs) {
        return static_cast<const std::basic_string<char, ci_traits>&>(lhs) == rhs;
    }

    inline bool operator==(const std::basic_string<char, ci_traits>& lhs, const String_& rhs) {
        return lhs == static_cast<const std::basic_string<char, ci_traits>&>(rhs);
    }

    inline bool operator==(const String_& lhs, const char* rhs) {
        return static_cast<const std::basic_string<char, ci_traits>&>(lhs) == rhs;
    }

    inline bool operator==(const char* lhs, const String_& rhs) {
        return lhs == static_cast<const std::basic_string<char, ci_traits>&>(rhs);
    }

} // namespace dal