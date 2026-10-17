//
// Created by Codex on 2026/10/04.
//

#pragma once

#include <limits>

#include <dal/platform/platform.hpp>

#include <dal/math/aad/profiling.hpp>
#include <dal/math/aad/sample.hpp>

namespace Dal::Script::Detail {
    inline AAD::ProfilingArrayStatistics_ ProfileElements(size_t live, size_t capacity, size_t width) {
        constexpr auto MAX = std::numeric_limits<std::uint64_t>::max();
        if (live > capacity || (width != 0 && capacity > MAX / width))
            return {0, 0, false};
        return {live * width, capacity * width, true};
    }

    inline AAD::ProfilingArrayStatistics_ JoinProfileArrays(const AAD::ProfilingArrayStatistics_& lhs, const AAD::ProfilingArrayStatistics_& rhs) {
        constexpr auto MAX = std::numeric_limits<std::uint64_t>::max();
        if (!lhs.valid_ || !rhs.valid_ || rhs.liveBytes_ > MAX - lhs.liveBytes_ || rhs.capacityBytes_ > MAX - lhs.capacityBytes_)
            return {0, 0, false};
        return {lhs.liveBytes_ + rhs.liveBytes_, lhs.capacityBytes_ + rhs.capacityBytes_, true};
    }

    template <class C_> AAD::ProfilingArrayStatistics_ ProfileArrays(const C_& values) {
        return ProfileElements(values.size(), values.capacity(), sizeof(typename C_::value_type));
    }

    template <class C_, class... R_> AAD::ProfilingArrayStatistics_ ProfileArrays(const C_& first, const R_&... rest) {
        return JoinProfileArrays(ProfileArrays(first), ProfileArrays(rest...));
    }

    template <class C_> AAD::ProfilingArrayStatistics_ ProfileNestedArrays(const C_& values) {
        auto result = ProfileArrays(values);
        for (const auto& row : values)
            result = JoinProfileArrays(result, ProfileArrays(row));
        return result;
    }

    template <class T_> AAD::ProfilingArrayStatistics_ ProfilePathArrays(const AAD::Scenario_<T_>& path) {
        auto result = ProfileArrays(path);
        for (const auto& sample : path) {
            result = JoinProfileArrays(
                result, ProfileArrays(sample.observations_, sample.discounts_, sample.libors_, sample.modelScratch_, sample.modelFactorScratch_));
            result = JoinProfileArrays(result, ProfileNestedArrays(sample.forwards_));
        }
        return result;
    }

    template <class E_> AAD::ProfilingArrayStatistics_ ProfileEvaluatorArrays(const E_& evaluator) {
        return JoinProfileArrays(ProfileArrays(evaluator.VarVals(), evaluator.ConstVarVals()), ProfileNestedArrays(evaluator.VectorVals()));
    }
} // namespace Dal::Script::Detail
