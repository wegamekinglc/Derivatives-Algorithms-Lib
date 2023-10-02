//
// Created by wegam on 2022/9/10.
//

#pragma once

#include <algorithm>

namespace Dal::AAD {
    template <class CONT_, class T_, class IT_ = T_*>
    CONT_ FillData(const CONT_& original, const T_& maxDx, const T_& minDx = T_(0.0), IT_ addBegin = nullptr, IT_ addEnd = nullptr) {
        CONT_ filled;
        CONT_ added;
        const size_t add_points = addBegin != addEnd ? std::distance(addBegin, addEnd) : 0;

        if (add_points > 0) {
            std::set_union(original.begin(), original.end(), addBegin, addEnd, back_inserter(added),
                      [minDx](const T_ x, const T_ y) { return x < y - minDx; });
        }
        const CONT_& sequence = add_points > 0 ? added : original;

        //  Position on the start, add it
        auto it = sequence.begin();
        filled.push_back(*it);
        ++it;

        while (it != sequence.end()) {
            auto current = filled.back();
            auto next = *it;
            //  Must supplement?
            if (next - current > maxDx) {
                //  Number of points to add
                int addPoints = int((next - current) / maxDx - EPSILON) + 1;
                //  Spacing between supplementary points
                auto spacing = (next - current) / addPoints;
                //  Add the steps
                auto t = current + spacing;
                while (t < next - minDx) {
                    filled.push_back(t);
                    t += spacing;
                }
            }
            //  Push the next step on the product timeline and advance
            filled.push_back(*it);
            ++it;
        }

        return filled;
    }

} // namespace Dal::AAD
