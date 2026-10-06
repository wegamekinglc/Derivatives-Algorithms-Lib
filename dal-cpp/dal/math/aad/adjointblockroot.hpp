//
// Created by Codex on 2026/10/06.
//

#pragma once

#include <cmath>
#include <string>

#include <dal/math/aad/adjointblocks.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::AAD {
    namespace Detail {
        inline void ValidateAdjointBlockSeeds(const Vector_<Number_>& outputs,
                                              const AdjointBlock_& block,
                                              const Number_& activeZero,
                                              const Vector_<Number_>* roots) {
            REQUIRE(roots != nullptr && roots != &outputs, "SeedAdjointBlock: distinct root storage must not be null or alias outputs");
            REQUIRE(block.width_ > 0 && block.width_ <= ADJ_SIZE && block.outputs_ > 0 && block.outputs_ <= block.width_,
                    "SeedAdjointBlock: positive live rows must fit the bounded width");
            REQUIRE(block.firstOutput_ <= outputs.size() && block.outputs_ <= outputs.size() - block.firstOutput_,
                    "SeedAdjointBlock: output block is out of range");
            REQUIRE(Tape()->multi_ && Tape()->numAdj_ == block.width_, "SeedAdjointBlock: requires matching vector recording width");
            REQUIRE(roots->capacity() >= block.width_, "SeedAdjointBlock: root capacity must be reserved before recording");
            REQUIRE(Value(activeZero) == 0.0, "SeedAdjointBlock: active zero must have value zero");
            static_cast<void>(NativeOperations_::ReadAdjoint(activeZero));
            for (size_t lane = 0; lane < block.outputs_; ++lane)
                REQUIRE(std::isfinite(Value(outputs[block.firstOutput_ + lane])),
                        "SeedAdjointBlock: output must be finite; output=" + String_(std::to_string(block.firstOutput_ + lane)));
        }
    } // namespace Detail

    inline void SeedAdjointBlock(const Vector_<Number_>& outputs, const AdjointBlock_& block, const Number_& activeZero, Vector_<Number_>* roots) {
        Detail::ValidateAdjointBlockSeeds(outputs, block, activeZero, roots);
        roots->Resize(block.width_);
        for (size_t lane = 0; lane < block.outputs_; ++lane) {
            (*roots)[lane] = NativeOperations_::ActiveRoot(outputs[block.firstOutput_ + lane], activeZero);
            for (size_t padding = block.outputs_; padding < block.width_; ++padding)
                NativeOperations_::SetSeed((*roots)[lane], 0.0, padding);
            NativeOperations_::SetSeed((*roots)[lane], 1.0, lane);
        }
        for (size_t lane = block.outputs_; lane < block.width_; ++lane)
            (*roots)[lane] = Number_();
    }
} // namespace Dal::AAD
