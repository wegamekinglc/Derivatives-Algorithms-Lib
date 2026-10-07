//
// Created on 2026/10/04.
//

#pragma once

#include <dal/math/aad/tape.hpp>

namespace Dal::AAD {
    struct TapeStatistics_ {
        size_t nodes_ = 0;
        size_t edges_ = 0;
        size_t blocks_ = 0;
        size_t liveBytes_ = 0;
        size_t occupiedBytes_ = 0;
        size_t capacityBytes_ = 0;
        size_t reverseEvents_ = 0;
        size_t reverseEventCapacityBytes_ = 0;
        size_t reverseScratchPeakBytes_ = 0;
    };

    // Explicit scalar scan; owned event storage is measured at allocation boundaries.
    inline TapeStatistics_ MeasureTape(const Tape_& tape) {
        TapeStatistics_ result;
        result.nodes_ = tape.nodes_.OccupiedSlots();
        auto node = tape.nodes_.Begin();
        for (size_t remaining = result.nodes_; remaining > 0; --remaining) {
            result.edges_ += node->NumArguments();
            // A full final block has no successor block; do not advance beyond its final node.
            if (remaining > 1)
                ++node;
        }
        const size_t width = tape.multi_ ? tape.numAdj_ : 0;
        result.liveBytes_ =
            result.nodes_ * sizeof(TapNode_) + result.edges_ * (sizeof(double) + sizeof(double*)) + result.nodes_ * width * sizeof(double);
        result.occupiedBytes_ = tape.nodes_.OccupiedSlots() * sizeof(TapNode_) + tape.ders_.OccupiedSlots() * sizeof(double) +
                                tape.argPtrs_.OccupiedSlots() * sizeof(double*) + tape.adjointsMulti_.OccupiedSlots() * sizeof(double);
        result.blocks_ =
            tape.nodes_.AllocatedBlocks() + tape.ders_.AllocatedBlocks() + tape.argPtrs_.AllocatedBlocks() + tape.adjointsMulti_.AllocatedBlocks();
        result.capacityBytes_ =
            tape.nodes_.AllocatedBlocks() * BLOCK_SIZE * sizeof(TapNode_) + tape.ders_.AllocatedBlocks() * DATA_SIZE * sizeof(double) +
            tape.argPtrs_.AllocatedBlocks() * DATA_SIZE * sizeof(double*) + tape.adjointsMulti_.AllocatedBlocks() * ADJ_SIZE * sizeof(double);
        result.reverseEvents_ = tape.ReverseEventCount();
        result.reverseEventCapacityBytes_ = tape.ReverseEventCapacityBytes();
        result.reverseScratchPeakBytes_ = tape.ReverseScratchPeakBytes();
        result.capacityBytes_ += result.reverseEventCapacityBytes_;
        return result;
    }
} // namespace Dal::AAD
