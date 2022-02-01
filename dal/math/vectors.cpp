//
// Created by Cheng Li on 2017/12/21.
//

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    namespace Vector {
        Vector_<int> UpTo(int n) {
            Vector_<int> ret_val(static_cast<size_t>(n));
            for (auto i = 0; i != n; ++i)
                ret_val[i] = i;
            return ret_val;
        }
    } // namespace Vector
} // namespace Dal
