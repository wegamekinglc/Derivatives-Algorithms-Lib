//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal {
    /*
     * Scenarios
     */

    struct SampleDef_ {
        struct RateDef_ {
            Time_ start_;
            Time_ end_;
            String_ curve_;

            RateDef_(const Time_& s, const Time_& e, const String_& c) : start_(s), end_(e), curve_(c) {}
        };

        bool numeraire_ = true;
        Vector_<Time_> discountMats_;
        Vector_<RateDef_> liborDefs_;
        Vector_<Vector_<Time_>> forwardMats_;
    };

    template <class T_ = double> struct Sample_ {
        T_ spot_;
        T_ numeraire_;
        Vector_<T_> discounts_;
        Vector_<T_> libors_;
        Vector_<Vector_<T_>> forwards_;

        void Allocate(const SampleDef_& data) {
            discounts_.Resize(data.discountMats_.size());
            libors_.Resize(data.liborDefs_.size());
            forwards_.Resize(data.forwardMats_.size());
            for (size_t i = 0; i < forwards_.size(); ++i)
                forwards_[i].Resize(data.forwardMats_[i].size());
        }

        void Initialize() {
            spot_ = T_(0.0);
            numeraire_ = T_(1.0);
            std::fill(discounts_.begin(), discounts_.end(), T_(1.0));
            std::fill(libors_.begin(), libors_.end(), T_(1.0));
            for (auto& forward : forwards_)
                std::fill(forward.begin(), forward.end(), T_(1.0));
        }
    };

    template <class T_ = double> using Scenario_ = Vector_<Sample_<T_>>;

    template <class T_> inline void AllocatePath(const Vector_<SampleDef_>& defLine, Scenario_<T_>& path) {
        path.Resize(defLine.size());
        for (size_t i = 0; i < defLine.size(); ++i)
            path[i].Allocate(defLine[i]);
    }

    template <class T_> inline void InitializePath(Scenario_<T_>& path) {
        for (auto& s : path)
            s.Initialize();
    }
} // namespace Dal
