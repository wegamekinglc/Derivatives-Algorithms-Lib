//
// Created by wegam on 2026/8/15.
//

#pragma once

#include <type_traits>

#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>

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

        StaticStack_<T_> dStack_;
        StaticStack_<bool> bStack_;

        explicit EvalStateCore_(const Vector_<>& variables, const Vector_<T_>& constVariables = Vector_<T_>())
            : variablesInit_(variables), constVariables_(constVariables) {
            InitVariables();
        }

        void Init() {
            InitVariables();
            dStack_.Reset();
            bStack_.Reset();
        }

        [[nodiscard]] const Vector_<T_>& VarVals() const { return variables_; }
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

    //  Per-path recording sinks of the LSMC driver's fuzzy (AAD) replay: raw payments per
    //  PAYS event of the current path plus the exercise value and fuzzy condition degree
    //  per exercise date, all live on the worker's tape. The driver resets the payment
    //  row and advances eventOrdinal_ between paths/events.
    template <class T_> struct LsmcFuzzySinks_ {
        const Vector_<size_t>* eventToPays_ = nullptr;
        const Vector_<size_t>* eventToExercise_ = nullptr;
        Vector_<T_>* pays_ = nullptr;
        Vector_<T_>* h_ = nullptr;
        Vector_<T_>* cond_ = nullptr;
        size_t eventOrdinal_ = 0;
    };
} // namespace Dal::Script
