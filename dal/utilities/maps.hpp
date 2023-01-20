//
// Created by wegam on 2023/1/21.
//

#pragma once
#include <map>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    template <class C1_, class C2_>
    std::map<typename C1_::value_type, typename C2_::value_type> ZipToMap(const C1_& v1, const C2_& v2) {
        REQUIRE(v1.size() == v2.size(), "v1 and v2 must have same size");
        std::map<typename C1_::value_type, typename C2_::value_type> ret_val;
        for (int ii = 0; ii < v1.size(); ++ii)
            ret_val.insert(std::make_pair(v1[ii], v2[ii]));
        return ret_val;
    }

    template <class C1_, class C2_>
    std::multimap<typename C1_::value_type, typename C2_::value_type> ZipToMultimap(const C1_& v1, const C2_& v2) {
        REQUIRE(v1.size() == v2.size(), "v1 and v2 must have same size");
        std::multimap<typename C1_::value_type, typename C2_::value_type> ret_val;
        for (int ii = 0; ii < v1.size(); ++ii)
            ret_val.insert(std::make_pair(v1[ii], v2[ii]));
        return ret_val;
    }
}
