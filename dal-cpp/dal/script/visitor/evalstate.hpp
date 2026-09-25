//
// Created by wegam on 2026/8/15.
//

#pragma once

#include <algorithm>
#include <type_traits>

#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    template <class T_> struct HistoricalSeedStorage_ {
        Vector_<T_> historicalSeed_;
    };

    // Double state already has a passive initial-value vector.
    template <> struct HistoricalSeedStorage_<double> {};

    //  Variable/stack state shared by the tree-walking evaluators and the compiled-script evaluator.
    template <class T_> struct EvalStateCore_ : HistoricalSeedStorage_<T_> {
        Vector_<T_> variables_;
        Vector_<> variablesInit_;
        Vector_<T_> constVariables_;
        Vector_<Vector_<T_>> vectors_;
        Vector_<Vector_<T_>> vectorSeed_;

        StaticStack_<T_> dStack_;
        StaticStack_<bool> bStack_;

        explicit EvalStateCore_(const Vector_<>& variables,
                                const Vector_<T_>& constVariables = Vector_<T_>(),
                                const Vector_<size_t>& vectorCapacities = {})
            : variablesInit_(variables), constVariables_(constVariables), vectors_(vectorCapacities.size()), vectorSeed_(vectorCapacities.size()) {
            for (size_t i = 0; i < vectorCapacities.size(); ++i)
                vectors_[i].reserve(vectorCapacities[i]);
            InitVariables();
        }

        EvalStateCore_(const EvalStateCore_& rhs)
            : HistoricalSeedStorage_<T_>(rhs), variables_(rhs.variables_), variablesInit_(rhs.variablesInit_), constVariables_(rhs.constVariables_),
              vectors_(rhs.vectors_), vectorSeed_(rhs.vectorSeed_), dStack_(rhs.dStack_), bStack_(rhs.bStack_) {
            for (size_t i = 0; i < vectors_.size(); ++i)
                vectors_[i].reserve(rhs.vectors_[i].capacity());
        }

        EvalStateCore_& operator=(const EvalStateCore_& rhs) {
            if (this == &rhs)
                return *this;
            HistoricalSeedStorage_<T_>::operator=(rhs);
            variables_ = rhs.variables_;
            variablesInit_ = rhs.variablesInit_;
            constVariables_ = rhs.constVariables_;
            vectors_ = rhs.vectors_;
            vectorSeed_ = rhs.vectorSeed_;
            dStack_ = rhs.dStack_;
            bStack_ = rhs.bStack_;
            for (size_t i = 0; i < vectors_.size(); ++i)
                vectors_[i].reserve(rhs.vectors_[i].capacity());
            return *this;
        }

        EvalStateCore_(EvalStateCore_&&) = default;
        EvalStateCore_& operator=(EvalStateCore_&&) = default;

        void Init() {
            InitVariables();
            for (size_t i = 0; i < vectors_.size(); ++i)
                vectors_[i].Assign(vectorSeed_[i].begin(), vectorSeed_[i].end());
            dStack_.Reset();
            bStack_.Reset();
        }

        [[nodiscard]] const Vector_<T_>& VarVals() const { return variables_; }
        [[nodiscard]] const Vector_<Vector_<T_>>& VectorVals() const { return vectors_; }
        void SetHistoricalVectorSeed(Vector_<Vector_<T_>> seed) {
            REQUIRE(seed.size() == vectors_.size(), "historical vector seed size mismatch");
            vectorSeed_ = std::move(seed);
        }
        Vector_<T_>& ConstVarVals() { return constVariables_; }
        const Vector_<T_>& ConstVarVals() const { return constVariables_; }

        void SetHistoricalSeed(Vector_<T_> seed) {
            if constexpr (std::is_same_v<T_, double>)
                variablesInit_ = std::move(seed);
            else
                this->historicalSeed_ = std::move(seed);
        }

    private:
        void InitVariables() {
            if constexpr (!std::is_same_v<T_, double>) {
                if (!this->historicalSeed_.empty()) {
                    variables_ = this->historicalSeed_;
                    return;
                }
            }
            variables_.Resize(variablesInit_.size());
            for (auto i = 0; i < variables_.size(); ++i)
                variables_[i] = T_(variablesInit_[i]);
        }
    };

    //  Preallocate the [nested if level][variable] stores used by fuzzy if blending.
    template <class T_> void ResizeVarStores(Vector_<Vector_<T_>>* varStore0, Vector_<Vector_<T_>>* varStore1, size_t numVars) {
        for (auto& varStore : *varStore0)
            varStore.Resize(numVars);
        for (auto& varStore : *varStore1)
            varStore.Resize(numVars);
    }

    //  Per-path recording sinks of the LSMC driver's fuzzy (AAD) replay: one raw payment
    //  row per event of the current path plus the exercise value and fuzzy condition
    //  degree per exercise date, all live on the worker's tape. The driver resets the
    //  payment rows and advances eventOrdinal_ between paths/events.
    template <class T_> struct LsmcFuzzySinks_ {
        const Vector_<size_t>* eventToExercise_ = nullptr;
        Vector_<T_>* pays_ = nullptr; //  indexed by event ordinal
        Vector_<T_>* h_ = nullptr;
        Vector_<T_>* cond_ = nullptr;
        size_t eventOrdinal_ = 0;
        size_t payoffIdx_ = static_cast<size_t>(-1);

        //  Fuzzy-if payment blend: both branches of an interior degree run, so the row
        //  must end up holding the degree-weighted payment, not the sum of both. The
        //  per-nesting-level snapshots mirror the evaluator's variable stores.
        Vector_<T_> branchPaySnapshot_;
        Vector_<T_> branchPayTrue_;

        void SnapshotBranchPayment(size_t lvl) {
            branchPaySnapshot_.Resize(std::max(branchPaySnapshot_.size(), lvl + 1));
            branchPayTrue_.Resize(std::max(branchPayTrue_.size(), lvl + 1));
            branchPaySnapshot_[lvl] = (*pays_)[eventOrdinal_];
        }

        void CaptureBranchPayment(size_t lvl) {
            branchPayTrue_[lvl] = (*pays_)[eventOrdinal_];
            (*pays_)[eventOrdinal_] = branchPaySnapshot_[lvl];
        }

        void BlendBranchPayment(size_t lvl, const T_& degree) {
            (*pays_)[eventOrdinal_] = degree * branchPayTrue_[lvl] + (1.0 - degree) * (*pays_)[eventOrdinal_];
        }
    };
} // namespace Dal::Script
