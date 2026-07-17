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

#include <cmath>
#include <algorithm>
#include <iostream>
#include <dal/platform/consts.hpp>

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)
namespace Dal::AAD {
    class TapNode_ {
        const size_t n_;

        double adjoint_ = 0;
        double* pDerivatives_ = nullptr;
        double* pAdjoints_ = nullptr;
        double** pAdjPtrs_ = nullptr;

        friend class Tape_;
        friend class Number_;
        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;

    public:
        explicit TapNode_(size_t n = 0) : n_(n) {}

        double& Adjoint() { return adjoint_; }

        double& Adjoint(size_t n) { return pAdjoints_[n]; }

        void PropagateOne() {
            if (!n_)
                return;
            if (std::abs(adjoint_) > Dal::EPSILON) {
                for (size_t i = 0; i < n_; ++i)
                    *(pAdjPtrs_[i]) += adjoint_ * pDerivatives_[i];
            }
            // Zero the consumed adjoint inline so the next propagation sweep starts
            // clean; this makes a separate ZeroAdjoints pass before each Jacobian
            // row unnecessary. Leaf parameter nodes (n_ == 0) return early above and
            // retain their accumulated adjoint for harvest.
            adjoint_ = 0.0;
        }

        void PropagateAll(size_t numAdj) {
            if (!n_ || std::all_of(pAdjoints_, pAdjoints_ + numAdj, [](double x) { return std::abs(x) <= Dal::EPSILON; }))
                return;

            for (size_t i = 0; i < n_; ++i) {
                double* adjPtr = pAdjPtrs_[i];
                double ders = pDerivatives_[i];
                for (size_t j = 0; j < numAdj; ++j)
                    adjPtr[j] += ders * pAdjoints_[j];
            }
        }

    };
} // namespace Dal::AAD
#else
#endif
