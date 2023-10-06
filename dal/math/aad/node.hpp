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
#include <iostream>

namespace Dal::AAD {
    class TapNode_ {
        const size_t n_;
        static size_t numAdj_;

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
            if (!n_ || fabs(adjoint_) <= Dal::EPSILON)
                return;

            for (size_t i = 0; i < n_; ++i)
                *(pAdjPtrs_[i]) += adjoint_ * pDerivatives_[i];
        }

        void PropagateAll() {
            if (!n_ || std::all_of(pAdjoints_, pAdjoints_ + numAdj_, [](double x) { return fabs(x) <= Dal::EPSILON; }))
                return;

            for (size_t i = 0; i < n_; ++i) {
                double* adjPtr = pAdjPtrs_[i];
                double ders = pDerivatives_[i];
                for (size_t j = 0; j < numAdj_; ++j)
                    adjPtr[j] += ders * pAdjoints_[j];
            }
        }

    };
} // namespace Dal