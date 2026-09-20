//
// Created by Codex on 2026/9/21.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal::Script {
    //  Recording sinks installed by the LSMC driver (dal/script/lsmc.cpp) before
    //  evaluating an EXERCISE stream: raw payments per PAYS event and the
    //  (x, h, condition) triple per exercise event land in the driver's storage
    //  rows; the driver advances eventOrdinal_ and pathSlot_ between events/paths.
    struct LsmcSinks_ {
        const Vector_<size_t>* eventToPays_ = nullptr;
        const Vector_<size_t>* eventToExercise_ = nullptr;
        Vector_<Vector_<>>* pays_ = nullptr;
        Vector_<Vector_<>>* x_ = nullptr;
        Vector_<Vector_<>>* h_ = nullptr;
        Vector_<Vector_<char>>* cond_ = nullptr; //  empty row = unconditional day
        size_t eventOrdinal_ = 0;
        size_t pathSlot_ = 0;
    };

    //  Shared recording kernels of the double hard-decision mode: the tree-walk
    //  LsmcEvaluator_ and the compiled recording opcodes (Detail::RecordLsmcPayment/
    //  RecordLsmcExercise) write the same driver rows through these
    FORCE_INLINE void RecordLsmcPaymentRow(const LsmcSinks_& sinks, double payment) {
        (*sinks.pays_)[(*sinks.eventToPays_)[sinks.eventOrdinal_]][sinks.pathSlot_] += payment;
    }

    //  Exercise leaves the script state untouched; only the driver's rows move
    FORCE_INLINE void RecordLsmcExerciseRow(const LsmcSinks_& sinks, double value, double cond, double spot) {
        const size_t slot = (*sinks.eventToExercise_)[sinks.eventOrdinal_];
        (*sinks.x_)[slot][sinks.pathSlot_] = spot;
        (*sinks.h_)[slot][sinks.pathSlot_] = value;
        if (sinks.cond_) {
            auto& row = (*sinks.cond_)[slot];
            if (!row.empty())
                row[sinks.pathSlot_] = static_cast<char>(cond);
        }
    }

    //  Fuzzy (AAD) replay tier of the same seam: the recorded rows stay live on
    //  the worker's tape, and the tree-walk FuzzyEvaluator_ shares these with the
    //  compiled fuzzy opcodes (Detail::RecordLsmcFuzzyPayment/RecordLsmcFuzzyExercise)
    template <class T_> FORCE_INLINE void RecordLsmcFuzzyPaymentRow(const LsmcFuzzySinks_<T_>& sinks, const T_& payment) {
        (*sinks.pays_)[sinks.eventOrdinal_] += payment;
    }

    template <class T_>
    FORCE_INLINE void RecordLsmcFuzzyExerciseRow(const LsmcFuzzySinks_<T_>& sinks, const T_& value, const T_& cond) {
        const size_t slot = (*sinks.eventToExercise_)[sinks.eventOrdinal_];
        (*sinks.h_)[slot] = value;
        (*sinks.cond_)[slot] = cond;
    }
} // namespace Dal::Script
