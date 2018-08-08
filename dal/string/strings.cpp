//
// Created by Cheng Li on 2018/1/7.
//

#include <bitset>
#include <dal/utilities/algorithms.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    namespace String {
        Vector_<String_> Split(const String_& src, char sep, bool keep_empties) {
            Vector_<String_> ret_val;
            auto start = src.begin();
            while (start != src.end()) {
                auto loc = std::find(start, src.end(), sep);
                if (loc == src.end()) {
                    ret_val.emplace_back(start, src.end());
                    start = src.end();
                } else {
                    ret_val.emplace_back(start, loc);
                    start = ++loc;
                }
                if (ret_val.back().empty() && !keep_empties)
                    ret_val.pop_back();
            }
            return ret_val;
        }

        bool IsNumber(const String_& src) {
            try {
                ToDouble(src);
                return true;
            } catch (...) {
                return false;
            }
        }

        int ToInt(const String_& src) { return std::stoi(src.c_str()); }

        double ToDouble(const String_& src) { return std::stod(src.c_str()); }

        String_ FromDouble(double src) { return String_(std::to_string(src)); }

        String_ FromInt(int src) { return String_(std::to_string(src)); }

        namespace
        {
            bool IsFluff(char c)
            {
                switch (c)
                {
                    case ' ':
                    case '\t':
                    case '_':
                        return true;
                    default:
                        return false;
                }
            }
        }

        String_ Condensed(const String_& src) {
            String_ ret_val;
            for(const auto& c : src) {
                if (!IsFluff(c))
                    ret_val.push_back(static_cast<char>(toupper(static_cast<int>(c))));
            }
            return ret_val;
        }

        // compares compressed version of a String with already-compressed rhs
        bool Equivalent(const String_& lhs, const char* rhs) {
            struct Otiose_ : std::bitset<256> {
                Otiose_() { set(' '); set('\t'); set('_'); }
            };
            static const Otiose_ SKIP;

            auto p = lhs.begin();
            auto q = rhs;
            while (true) {
                while (p != lhs.end() && SKIP[*p])
                    ++p;
                if (!*q || p == lhs.end())
                    return !*q && p == lhs.end();
                if (!ci_traits::eq(*p, *q))
                    return false;
                ++p, ++q;

            }
        }

        String_ NextName(const String_& name) {
            if (name.empty())
                return String_("0");
            String_ retval(name);
            switch (retval.back())
            {
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
                    ++retval.back();
                    return retval;
                case '9':
                    retval.pop_back();
                    return String_(NextName(retval) + '0');
                default:
                    return String_(retval + '1');
            }
        }
    } // namespace String
} // namespace Dal