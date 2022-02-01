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

namespace Dal {
    class Node_ {
        const size_t n_;
        static size_t num_adj_;

        double adjoint_ = 0;
        double* p_derivatives_ = nullptr;
        double* p_adjoints_ = nullptr;
        double** p_adj_ptrs_ = nullptr;

        friend class Tape_;
        friend class Number_;
        friend auto SetNumResultsForAAD(bool, const size_t&);
        friend struct NumResultsResetterForAAD_;

    public:
        Node_(const size_t& n = 0) : n_(n) {}

        double& Adjoint() { return adjoint_; }

        double& Adjoint(const size_t& n) { return p_adjoints_[n]; }

        void PropagateOne() {
            if (!n_ || !adjoint_)
                return;

            for (size_t i = 0; i < n_; ++i)
                *(p_adj_ptrs_[i]) += adjoint_ * p_derivatives_[i];
        }

        void PropagateAll() {
            if (!n_ || std::all_of(p_adjoints_, p_adjoints_ + num_adj_, [](const double& x) { return !x; }))
                return;

            for (size_t i = 0; i < n_; ++i) {
                double* adj_ptr = p_adj_ptrs_[i];
                double ders = p_derivatives_[i];
                for (size_t j = 0; j < num_adj_; ++j)
                    adj_ptr[j] += ders * p_adjoints_[j];
            }
        }
    };
} // namespace Dal