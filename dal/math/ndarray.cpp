//
// Created by wegam on 2023/1/25.
//

#include <functional>
#include <dal/math/ndarray.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal::ArrayN {
    Vector_<int> Strides(const Vector_<int>& sizes) {
        Vector_<int> ret_val(sizes.size(), 1);
        for (int ii = sizes.size() - 1; ii > 0; --ii)
            ret_val[ii - 1] = ret_val[ii] * sizes[ii];
        return ret_val;
    }

    Vector_<pair<int, int>> Moves(const Vector_<int>& old_sizes, const Vector_<int>& new_sizes) {
        const int nd = old_sizes.size();
        REQUIRE(new_sizes.size() == nd, "Resize() can't change the dimension of an array");
        static const auto func = [](int x, int y) { return Min(x, y);};
        Vector_<int> oldStrides = Strides(old_sizes), newStrides = Strides(new_sizes);
        Vector_<int> mins = Apply(func, old_sizes, new_sizes);
        Vector_<int> loc(nd, 0);
        Vector_<pair<int, int>> ret_val;
        for (;;) {
            int depth;
            for (depth = 0; depth < nd; ++depth) {
                if (++loc[depth] < mins[depth])
                    break;
                loc[depth] = 0;
            }
            if (depth == nd)
                return ret_val;
            ret_val.emplace_back(InnerProduct(loc, oldStrides), InnerProduct(loc, newStrides));
        }
        return ret_val;
    }
} // namespace Dal::ArrayN