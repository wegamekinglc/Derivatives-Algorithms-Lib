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

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD)

#include <dal/math/aad/blocklist.hpp>
#include <dal/math/aad/node.hpp>


namespace Dal::AAD {
    class Number_;
    constexpr size_t BLOCK_SIZE = 16384;
    constexpr size_t ADJ_SIZE = 32768;
    constexpr size_t DATA_SIZE = 65536;

    class Tape_ {
    public:
        explicit Tape_(bool = true) : pad_{} {}

        using Iterator_ = BlockList_<TapNode_, BLOCK_SIZE>::Iterator_;

        static bool multi_;
        BlockList_<double, ADJ_SIZE> adjointsMulti_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> argPtrs_;
        BlockList_<TapNode_, BLOCK_SIZE> nodes_;
        char pad_[64];

        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;
        friend void Clear(Tape_& tape);
        friend void Mark(Tape_& tape);
        friend void RewindToMark(Tape_& tape);
        friend void Rewind(Tape_& tape);
        friend void PropagateMarkToStart(Tape_& tape);
        friend void PropagateToStart(Tape_& tape);
        friend void PropagateToMark(Tape_& tape);

        template <size_t N_> TapNode_* RecordNode() {
            TapNode_* node = nodes_.EmplaceBack(N_);
            if (multi_) {
                node->pAdjoints_ = adjointsMulti_.EmplaceBackMulti(TapNode_::numAdj_);
                std::fill_n(node->pAdjoints_, TapNode_::numAdj_, 0.0);
            }

            if constexpr (static_cast<bool>(N_)) {
                node->pDerivatives_ = ders_.EmplaceBackMulti<N_>();
                node->pAdjPtrs_ = argPtrs_.EmplaceBackMulti<N_>();
            }
            return node;
        }

    };

    void Clear(Tape_& tape);
    void Mark(Tape_& tape);
    void RewindToMark(Tape_& tape);
    void Rewind(Tape_& tape);
    void PropagateMarkToStart(Tape_& tape);
    void PropagateToStart(Tape_& tape);
    void PropagateToMark(Tape_& tape);
    void NewRecording(Tape_& tape);
    void Activate(Tape_& tape);
    void Deactivate(Tape_& tape);
} // namespace Dal::AAD
#elif defined(DAL_USE_XAD_AAD)
#include <XAD/XAD.hpp>

namespace Dal::AAD {

    class Tape_ {
    public:
        using tape_type = xad::adj<double>::tape_type;
        tape_type tape_;
        tape_type::position_type start_;
        tape_type::position_type mark_;

        explicit Tape_(bool activate = true) : tape_(activate), start_(tape_.getPosition()), mark_(start_) { }
    };

    void Clear(Tape_& tape);
    void Mark(Tape_& tape);
    void RewindToMark(Tape_& tape);
    void Rewind(Tape_& tape);
    void PropagateMarkToStart(Tape_& tape);
    void PropagateToStart(Tape_& tape);
    void PropagateToMark(Tape_& tape);
    void NewRecording(Tape_& tape);
    void Activate(Tape_& tape);
    void Deactivate(Tape_& tape);
} // namespace Dal::AAD
#elif defined(DAL_USE_CODIPACK_AAD)
#include <codi.hpp>

namespace Dal::AAD {

    class Tape_ {
    public:
        using active_type = codi::RealReverse;
        using tape_type = typename active_type::Tape;
        using position_type = typename tape_type::Position;

        tape_type& tape_;
        position_type start_;
        position_type mark_;

        explicit Tape_(bool activate = true) : tape_(active_type::getTape()), start_(tape_.getPosition()), mark_(start_) {
            if (activate)
                tape_.setActive();
        }
    };

    void Clear(Tape_& tape);
    void Mark(Tape_& tape);
    void RewindToMark(Tape_& tape);
    void Rewind(Tape_& tape);
    void PropagateMarkToStart(Tape_& tape);
    void PropagateToStart(Tape_& tape);
    void PropagateToMark(Tape_& tape);
    void NewRecording(Tape_& tape);
    void Activate(Tape_& tape);
    void Deactivate(Tape_& tape);
} // namespace Dal::AAD

#endif
