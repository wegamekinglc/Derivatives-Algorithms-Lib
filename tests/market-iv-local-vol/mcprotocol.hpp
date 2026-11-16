#pragma once

#include <array>
#include <cmath>
#include <limits>

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

namespace DalTest {
    constexpr size_t BATCHES = 16, MAX_PATHS = size_t{1} << 20;

    inline size_t CheckedProduct(size_t left, size_t right) {
        REQUIRE(right == 0 || left <= std::numeric_limits<size_t>::max() / right, "Qualification scalar offset overflow");
        return left * right;
    }

    inline size_t ScalarOffset(size_t batch, size_t path, size_t dimension) {
        static_assert(sizeof(size_t) >= 8, "Qualification requires checked 64-bit scalar addresses");
        REQUIRE(batch < BATCHES && path < MAX_PATHS && dimension > 0, "Invalid qualification stream coordinate");
        return CheckedProduct(batch * MAX_PATHS + path, dimension);
    }

    inline Dal::Vector_<> NestedTimeline(const Dal::Vector_<>& anchors, size_t level) {
        REQUIRE(level <= 3 && anchors.size() > 1 && anchors.front() == 0., "Invalid qualification refinement");
        Dal::Vector_<> result{0.};
        for (size_t i = 1; i < anchors.size(); ++i) {
            const double left = anchors[i - 1], right = anchors[i];
            REQUIRE(right > left, "Qualification anchors must increase");
            const size_t count = static_cast<size_t>(std::ceil((right - left) * 48.)) * (size_t{1} << level);
            for (size_t part = 1; part < count; ++part)
                result.push_back(left + (right - left) * part / count);
            result.push_back(right);
        }
        return result;
    }

    class BrownianMap_ {
        size_t stride_, size_;
        Dal::Vector_<> weights_;

    public:
        BrownianMap_(const Dal::Vector_<>& fine, const Dal::Vector_<>& coarse) {
            REQUIRE(coarse.size() > 1 && fine.size() >= coarse.size() && (fine.size() - 1) % (coarse.size() - 1) == 0,
                    "Non-nested Brownian dimensions");
            stride_ = (fine.size() - 1) / (coarse.size() - 1);
            size_ = coarse.size() - 1;
            weights_.Resize(fine.size() - 1);
            for (size_t step = 0; step < size_; ++step) {
                REQUIRE(fine[step * stride_] == coarse[step] && fine[(step + 1) * stride_] == coarse[step + 1], "Non-nested Brownian anchors");
                for (size_t j = step * stride_; j < (step + 1) * stride_; ++j)
                    weights_[j] = std::sqrt((fine[j + 1] - fine[j]) / (coarse[step + 1] - coarse[step]));
            }
        }
        void Apply(const Dal::Vector_<>& normals, Dal::Vector_<>* result) const {
            REQUIRE(normals.size() == weights_.size(), "Brownian coordinates do not match frozen fine slots");
            result->Resize(size_);
            for (size_t step = 0; step < size_; ++step) {
                (*result)[step] = 0.;
                for (size_t j = step * stride_; j < (step + 1) * stride_; ++j)
                    (*result)[step] += weights_[j] * normals[j];
            }
        }
    };

    inline Dal::Vector_<> Aggregate(const Dal::Vector_<>& fine, const Dal::Vector_<>& coarse, const Dal::Vector_<>& normals) {
        Dal::Vector_<> result;
        BrownianMap_(fine, coarse).Apply(normals, &result);
        return result;
    }

    inline std::pair<double, double> Estimate(const std::array<double, BATCHES>& means) {
        double mean = 0., squares = 0.;
        for (const double value : means)
            mean += value / BATCHES;
        for (const double value : means)
            squares += (value - mean) * (value - mean);
        return {mean, std::sqrt(squares / (BATCHES * (BATCHES - 1)))};
    }
} // namespace DalTest
