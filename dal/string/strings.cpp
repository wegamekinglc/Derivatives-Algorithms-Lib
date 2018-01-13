//
// Created by Cheng Li on 2018/1/7.
//

#include <dal/utilities/algorithms.hpp>
#include <dal/math/vector.hpp>
#include <dal/string/strings.hpp>

namespace dal {
    namespace string {
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
                (void)ToDoule(src);
                return true;
            } catch (...) {
                return false;
            }
        }

        double ToDouble(const String_& src) { return std::stod(src.c_str()); }

        int ToInt(const String_& src) { return std::stoi(src.c_str()); }

        String_ FromDouble(double src) { return String_(std::to_string(src)); }

        String_ FromInt(int src) { return String_(std::to_string(src)); }
    } // namespace string
} // namespace dal