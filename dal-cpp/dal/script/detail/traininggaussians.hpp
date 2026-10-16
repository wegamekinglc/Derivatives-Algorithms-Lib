//
// Created by Codex on 2026-10-16.
//

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>

#include <dal/math/random/base.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::Script::Detail {
    //  One retrained-risk call owns the cache; its first fit writes disjoint path rows.
    class TrainingGaussians_ {
        size_t dimensions_;
        size_t factors_;
        size_t paths_;
        std::optional<uint64_t> key_;
        std::unique_ptr<double[]> values_;
        bool ready_ = false;

    public:
        static constexpr size_t MAX_BYTES = 32 * 1024 * 1024;

        TrainingGaussians_(size_t dimensions, size_t factors, size_t paths, std::optional<uint64_t> key)
            : dimensions_(dimensions), factors_(factors), paths_(paths), key_(key) {
            if (dimensions_ != 0 && dimensions_ <= MAX_BYTES / sizeof(double) && paths_ <= MAX_BYTES / sizeof(double) / dimensions_)
                values_.reset(new (std::nothrow) double[paths_ * dimensions_]);
        }

        [[nodiscard]] bool Matches(size_t dimensions, size_t factors, std::optional<uint64_t> key) const {
            return values_ && dimensions_ == dimensions && factors_ == factors && key_ == key;
        }

        [[nodiscard]] size_t Bytes() const { return values_ ? paths_ * dimensions_ * sizeof(double) : 0; }
        [[nodiscard]] size_t Paths() const { return paths_; }
        void Complete() { ready_ = true; }

        void FillNormal(Random_* random, size_t path, Vector_<>* gauss) {
            REQUIRE(values_ && path < paths_ && gauss->size() == dimensions_, "Invalid LSMC training Gaussian row");
            double* row = values_.get() + path * dimensions_;
            if (ready_)
                std::copy_n(row, dimensions_, gauss->begin());
            else {
                random->FillNormal(gauss);
                std::copy(gauss->begin(), gauss->end(), row);
            }
        }
    };
} // namespace Dal::Script::Detail
