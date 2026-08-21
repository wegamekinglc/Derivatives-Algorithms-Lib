//
// Created by Cheng Li on 2017/12/21.
//

#include <dal/platform/strict.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Vector {
    void RequireSameSize(size_t lhs, size_t rhs) {
        REQUIRE(lhs == rhs, "Vector_ size mismatch");
    }

    void RequireAtLeastTwoPoints(size_t points) {
        REQUIRE(points >= 2, "XRange requires at least 2 points");
    }

    Vector_<int> UpTo(int n) {
        Vector_<int> ret_val(static_cast<size_t>(n));
        for (auto i = 0; i != n; ++i)
            ret_val[i] = i;
        return ret_val;
    }
} // namespace Dal
