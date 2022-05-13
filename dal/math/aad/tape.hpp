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

#include <dal/math/aad/blocklist.hpp>
#include <dal/math/aad/node.hpp>

namespace Dal {
    constexpr size_t BLOCK_SIZE = 16384;
    constexpr size_t ADJ_SIZE = 32768;
    constexpr size_t DATA_SIZE = 65536;

    class Tape_ {
        static bool multi_;
        BlockList_<double, ADJ_SIZE> adjoints_multi_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> arg_ptrs_;
        BlockList_<Node_, BLOCK_SIZE> nodes_;

        char pad_[64];
        friend auto SetNumResultsForAAD(bool, const size_t&);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;

    public:
        template <size_t N_> Node_* RecordNode() {
            Node_* node = nodes_.EmplaceBack(N_);
            if (multi_) {
                node->p_adjoints_ = adjoints_multi_.EmplaceBackMulti(Node_::num_adj_);
                std::fill(node->p_adjoints_, node->p_adjoints_ + Node_::num_adj_, 0.0);
            }

            if constexpr (static_cast<bool>(N_)) {
                node->p_derivatives_ = ders_.EmplaceBackMulti<N_>();
                node->p_adj_ptrs_ = arg_ptrs_.EmplaceBackMulti<N_>();
            }
            return node;
        }

        void ResetAdjoints() {
            if (multi_)
                adjoints_multi_.Memset(0);
            else {
                for (auto it = nodes_.Begin(); it != nodes_.End(); ++it)
                    it->adjoint_ = 0.;
            }
        }

        void Clear() {
            adjoints_multi_.Clear();
            ders_.Clear();
            arg_ptrs_.Clear();
            nodes_.Clear();
        }

        void Rewind() {
            if (multi_)
                adjoints_multi_.Rewind();
            ders_.Rewind();
            arg_ptrs_.Rewind();
            nodes_.Rewind();
        }

        void Mark() {
            if (multi_)
                adjoints_multi_.SetMark();
            ders_.SetMark();
            arg_ptrs_.SetMark();
            nodes_.SetMark();
        }

        void RewindToMark() {
            if (multi_)
                adjoints_multi_.RewindToMark();
            ders_.RewindToMark();
            arg_ptrs_.RewindToMark();
            nodes_.RewindToMark();
        }

        using Iterator_ = typename BlockList_<Node_, BLOCK_SIZE>::Iterator_;

        auto Begin() { return nodes_.Begin(); }

        auto End() { return nodes_.End(); }

        auto MarkIt() { return nodes_.Mark(); }

        auto Find(Node_* node) { return nodes_.Find(node); }
    };
} // namespace Dal
