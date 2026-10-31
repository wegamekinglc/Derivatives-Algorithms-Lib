/*
 * Modified by wegamekinglc on 2020/12/13.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dal/platform/consts.hpp>
#include <iostream>
#include <limits>

#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#define DAL_AAD_RESTRICT_ __restrict
#else
#define DAL_AAD_RESTRICT_
#endif
namespace Dal::AAD {
    class TapNode_ {
        const size_t n_;

        double adjoint_ = 0;
        double* pDerivatives_ = nullptr;
        double* pAdjoints_ = nullptr;
        double** pAdjPtrs_ = nullptr;

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        std::uint64_t lifetimeOrdinal_ = 0;
        std::uint64_t lifetimeGeneration_ = 0;
#endif

        friend class Tape_;
        friend class Number_;
        friend struct NativeOperations_;
        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;

        static bool IsNonZero(double value) {
            if constexpr (sizeof(double) == sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559) {
                // Inspect magnitude bits to retain NaN/Inf without an unordered floating-point comparison.
                std::uint64_t bits;
                std::memcpy(&bits, &value, sizeof(value));
                return (bits << 1) != 0;
            } else {
                return value != 0.0;
            }
        }

        // A recorded edge targets an earlier node, whose result slots cannot overlap this node's slots.
        template <bool Z_>
        static void
        PropagateResults(double* DAL_AAD_RESTRICT_ destination, const double* DAL_AAD_RESTRICT_ source, double derivative, size_t numAdj) {
            for (size_t j = 0; j < numAdj; ++j) {
                if constexpr (Z_) {
                    if (!IsNonZero(source[j]))
                        continue;
                }
                destination[j] += derivative * source[j];
            }
        }

#if defined(__GNUC__) || defined(__clang__)
        __attribute__((cold, noinline))
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        static void PropagateNonFiniteResults(double* destination, const double* source, double derivative, size_t numAdj);

    public:
        explicit TapNode_(size_t n = 0) : n_(n) {}

        [[nodiscard]] size_t NumArguments() const { return n_; }

        double& Adjoint() { return adjoint_; }

        double& Adjoint(size_t n) { return pAdjoints_[n]; }

        void PropagateOne() {
            if (!n_)
                return;
            if (IsNonZero(adjoint_)) {
                double** destinations = pAdjPtrs_;
                const double* derivatives = pDerivatives_;
                switch (n_) {
                case 1:
                    *destinations[0] += adjoint_ * derivatives[0];
                    break;
                case 2:
                    *destinations[0] += adjoint_ * derivatives[0];
                    *destinations[1] += adjoint_ * derivatives[1];
                    break;
                case 3:
                    *destinations[0] += adjoint_ * derivatives[0];
                    *destinations[1] += adjoint_ * derivatives[1];
                    *destinations[2] += adjoint_ * derivatives[2];
                    break;
                default:
                    for (size_t i = 0; i < n_; ++i)
                        *destinations[i] += adjoint_ * derivatives[i];
                }
            }
            // Zero the consumed adjoint inline so the next propagation sweep starts
            // clean; this makes a separate ZeroAdjoints pass before each Jacobian
            // row unnecessary. Leaf parameter nodes (n_ == 0) return early above and
            // retain their accumulated adjoint for harvest.
            adjoint_ = 0.0;
        }

        template <size_t R_ = 0> void PropagateAll(size_t numAdj) {
            if constexpr (R_ != 0)
                numAdj = R_;
            if (!n_)
                return;
            bool any = false;
            if constexpr (R_ > 16) {
                size_t active[R_];
                size_t count = 0;
                for (size_t j = 0; j < numAdj; ++j) {
                    if (IsNonZero(pAdjoints_[j]))
                        active[count++] = j;
                }
                any = count != 0;
                if (any && count <= R_ / 4) {
                    double** destinations = pAdjPtrs_;
                    const double* derivatives = pDerivatives_;
                    const double* source = pAdjoints_;
                    for (size_t i = 0; i < n_; ++i) {
                        const double derivative = derivatives[i];
                        // Every selected lane is nonzero, so finite and non-finite derivatives share the same update.
                        for (size_t k = 0; k < count; ++k) {
                            const size_t j = active[k];
                            destinations[i][j] += derivative * source[j];
                        }
                    }
                    std::fill_n(pAdjoints_, numAdj, 0.0);
                    return;
                }
                if constexpr (R_ == 64) {
                    if (count > R_ / 4) {
                        // Avoid fixed-width code bloat on dense 64-lane sweeps; sparse lanes retain the specialized path.
                        PropagateAll<0>(numAdj);
                        return;
                    }
                }
            } else {
                any = std::any_of(pAdjoints_, pAdjoints_ + numAdj, IsNonZero);
            }
            if (any) {
                double** destinations = pAdjPtrs_;
                const double* derivatives = pDerivatives_;
                const double* source = pAdjoints_;
                for (size_t i = 0; i < n_; ++i) {
                    const double derivative = derivatives[i];
                    if (std::isfinite(derivative))
                        PropagateResults<false>(destinations[i], source, derivative, numAdj);
                    else
                        PropagateNonFiniteResults(destinations[i], source, derivative, numAdj);
                }
            }
            // Same consumed-adjoint zeroing as PropagateOne: repeated sweeps must
            // not re-propagate stale slots. Leaves (n_ == 0) retain theirs for harvest.
            std::fill_n(pAdjoints_, numAdj, 0.0);
        }
    };
} // namespace Dal::AAD
#undef DAL_AAD_RESTRICT_
