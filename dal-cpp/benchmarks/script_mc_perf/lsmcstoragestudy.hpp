//
// Created by Codex on 2026/10/14.
//

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    struct StudyRowCounters_ {
        std::atomic<size_t> current_{0};
        std::atomic<size_t> peak_{0};
    };

    inline StudyRowCounters_& StudyRowCounters() {
        static StudyRowCounters_ counters;
        return counters;
    }

    inline size_t StudyCurrentRowBytes() { return StudyRowCounters().current_.load(std::memory_order_relaxed); }
    inline size_t StudyPeakRowBytes() { return StudyRowCounters().peak_.load(std::memory_order_relaxed); }

    class StudyRowAllocation_ {
        size_t bytes_ = 0;

    public:
        explicit StudyRowAllocation_(size_t bytes) : bytes_(bytes) {
            auto& counters = StudyRowCounters();
            const size_t current = counters.current_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
            size_t peak = counters.peak_.load(std::memory_order_relaxed);
            while (peak < current && !counters.peak_.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {
            }
        }
        StudyRowAllocation_(StudyRowAllocation_&& other) noexcept : bytes_(std::exchange(other.bytes_, 0)) {}
        StudyRowAllocation_& operator=(StudyRowAllocation_&& other) noexcept {
            if (this != &other) {
                StudyRowCounters().current_.fetch_sub(bytes_, std::memory_order_relaxed);
                bytes_ = std::exchange(other.bytes_, 0);
            }
            return *this;
        }
        ~StudyRowAllocation_() { StudyRowCounters().current_.fetch_sub(bytes_, std::memory_order_relaxed); }
    };

    class StudyReadRow_ {
        const double* doubles_ = nullptr;
        const float* singles_ = nullptr;

    public:
        StudyReadRow_(std::nullptr_t = nullptr) {}
        // Native regression aggregates accept both pointer types in the overlay.
        StudyReadRow_(const double* values) : doubles_(values) {}
        StudyReadRow_(const float* values) : singles_(values) {}

        [[nodiscard]] double operator[](size_t path) const { return doubles_ ? doubles_[path] : static_cast<double>(singles_[path]); }
        [[nodiscard]] explicit operator bool() const { return doubles_ || singles_; }
        [[nodiscard]] StudyReadRow_ operator+(size_t paths) const {
            if (doubles_)
                return StudyReadRow_(doubles_ + paths);
            if (singles_)
                return StudyReadRow_(singles_ + paths);
            REQUIRE(paths == 0, "InvalidStudyStorage: cannot advance a null row");
            return {};
        }
    };

    template <class T_> class StudyStoredValue_ {
        T_* value_;

    public:
        explicit StudyStoredValue_(T_* value) : value_(value) {}
        [[nodiscard]] operator double() const { return static_cast<double>(*value_); }
        StudyStoredValue_& operator=(double value) {
            REQUIRE(std::isfinite(value) && std::fabs(value) <= static_cast<double>(std::numeric_limits<T_>::max()),
                    "InvalidStudyStorage: value must be finite and representable in the storage type");
            *value_ = static_cast<T_>(value);
            return *this;
        }
        StudyStoredValue_& operator=(const StudyStoredValue_& value) { return *this = static_cast<double>(value); }
        StudyStoredValue_& operator+=(double value) { return *this = static_cast<double>(*value_) + value; }
    };

    template <class T_> class StudyWriteRow_ {
        T_* values_;

    public:
        explicit StudyWriteRow_(T_* values) : values_(values) {}
        [[nodiscard]] StudyStoredValue_<T_> operator[](size_t path) const { return StudyStoredValue_<T_>(values_ + path); }
    };

    template <class T_> class StudyRows_ {
        static_assert(std::is_same_v<T_, float> || std::is_same_v<T_, double>);
        std::unique_ptr<T_[]> data_;
        StudyRowAllocation_ allocation_{0};
        size_t nRows_ = 0;
        size_t nPaths_ = 0;

        static size_t Count(size_t rows, size_t paths) {
            REQUIRE(paths == 0 || rows <= std::numeric_limits<size_t>::max() / paths / sizeof(T_),
                    "InvalidStudyStorage: row allocation exceeds addressable bytes");
            return rows * paths;
        }

    public:
        StudyRows_() = default;
        StudyRows_(size_t rows, size_t paths)
            : data_(Count(rows, paths) ? new T_[Count(rows, paths)] : nullptr), allocation_(Count(rows, paths) * sizeof(T_)), nRows_(rows),
              nPaths_(paths) {}
        [[nodiscard]] size_t Rows() const { return nRows_; }
        [[nodiscard]] size_t Paths() const { return nPaths_; }
        [[nodiscard]] size_t AllocatedBytes() const { return nRows_ * nPaths_ * sizeof(T_); }
        [[nodiscard]] StudyWriteRow_<T_> operator[](size_t row) { return StudyWriteRow_<T_>(data_.get() + row * nPaths_); }
        [[nodiscard]] StudyReadRow_ operator[](size_t row) const { return StudyReadRow_(data_.get() + row * nPaths_); }

        void ZeroPaths(size_t firstPath, size_t pathCount) {
            if (nRows_ == 0)
                return;
            REQUIRE(firstPath <= nPaths_ && pathCount <= nPaths_ - firstPath, "InvalidStudyStorage: path range exceeds row length");
            if (pathCount == 0)
                return;
            for (size_t row = 0; row < nRows_; ++row)
                std::fill_n(data_.get() + row * nPaths_ + firstPath, pathCount, T_(0.0));
        }
    };
} // namespace Dal::Script
