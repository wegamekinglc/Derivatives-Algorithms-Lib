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
    constexpr size_t BLOCK_SIZE = 16384;
    constexpr size_t ADJ_SIZE = 32768;
    constexpr size_t DATA_SIZE = 65536;

    class Tape_ {
        static bool multi_;
        BlockList_<double, ADJ_SIZE> adjointsMulti_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> argPtrs_;
        BlockList_<TapNode_, BLOCK_SIZE> nodes_;
        char pad_[64];

        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;

    public:
        template <size_t N_> TapNode_* RecordNode() {
            TapNode_* node = nodes_.EmplaceBack(N_);
            if (multi_) {
                node->pAdjoints_ = adjointsMulti_.EmplaceBackMulti(TapNode_::numAdj_);
                std::fill(node->pAdjoints_, node->pAdjoints_ + TapNode_::numAdj_, 0.0);
            }

            if constexpr (static_cast<bool>(N_)) {
                node->pDerivatives_ = ders_.EmplaceBackMulti<N_>();
                node->pAdjPtrs_ = argPtrs_.EmplaceBackMulti<N_>();
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
            BlockList_<double, ADJ_SIZE>::Iterator_ adjointsMultiPos_;
            BlockList_<double, DATA_SIZE>::Iterator_ dersPos_;
            BlockList_<double*, DATA_SIZE>::Iterator_ argPtrsPos_;
            BlockList_<TapNode_, BLOCK_SIZE>::Iterator_ nodesPos_;
        };

        Iterator_ Begin() { return nodes_.Begin(); }
        Iterator_ End() { return nodes_.End(); }
        Iterator_ Find(TapNode_* node) { return nodes_.Find(node); }

        // api just for compatible with Codi
        Position_ getPosition();
        Position_ getZeroPosition();
        void resetTo(const Position_&, bool reset_adjoints = true);
        static void evaluate(const Position_&, const Position_&);
        void evaluate();
        void Mark();
        void RewindToMark();
        void Rewind();
        Iterator_ MarkIt();
    };
} // namespace Dal::AAD
