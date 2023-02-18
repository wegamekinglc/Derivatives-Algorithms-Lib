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


namespace Dal::AAD {
    class Number_;
    constexpr size_t BLOCK_SIZE = 16384 * 8;
    constexpr size_t ADJ_SIZE = 32768 * 8;
    constexpr size_t DATA_SIZE = 65536 * 8;

    class Tape_ {
        static bool multi_;
        BlockList_<double, ADJ_SIZE> adjoints_multi_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> arg_ptrs_;
        BlockList_<TapNode_, BLOCK_SIZE> nodes_;

        char pad_[64];
        friend auto SetNumResultsForAAD(bool, const size_t&);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;

    public:
        template <size_t N_> TapNode_* RecordNode() {
            TapNode_* node = nodes_.EmplaceBack(N_);
            if (multi_) {
                node->p_adjoints_ = adjoints_multi_.EmplaceBackMulti(TapNode_::num_adj_);
                std::fill(node->p_adjoints_, node->p_adjoints_ + TapNode_::num_adj_, 0.0);
            }

            if constexpr (static_cast<bool>(N_)) {
                node->p_derivatives_ = ders_.EmplaceBackMulti<N_>();
                node->p_adj_ptrs_ = arg_ptrs_.EmplaceBackMulti<N_>();
            }
            return node;
        }

        void ResetAdjoints();
        void Clear();

        void setActive();
        void registerInput(Number_& input);

        void reset();

        using Iterator_ = typename BlockList_<TapNode_, BLOCK_SIZE>::Iterator_;

        struct Position_ {
            BlockList_<double, ADJ_SIZE>::Iterator_ adjoints_multi_pos_;
            BlockList_<double, DATA_SIZE>::Iterator_ ders_pos_;
            BlockList_<double*, DATA_SIZE>::Iterator_ arg_ptrs_pos_;
            BlockList_<TapNode_, BLOCK_SIZE>::Iterator_ nodes_pos_;
        };

        Iterator_ Begin() { return nodes_.Begin(); }
        Iterator_ End() { return nodes_.End(); }
        Iterator_ Find(TapNode_* node) { return nodes_.Find(node); }

        Position_ getPosition();
        Position_ getZeroPosition();
        void resetTo(const Position_&);
        void evaluate(const Position_&, const Position_&);
        void evaluate();
    };
} // namespace Dal::AAD
