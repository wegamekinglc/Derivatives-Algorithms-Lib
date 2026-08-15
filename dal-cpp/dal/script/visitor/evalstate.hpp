//
// Created by wegam on 2026/8/15.
//

#pragma once

#include <dal/math/stacks.hpp>
#include <dal/math/vectors.hpp>

namespace Dal::Script {
    //  Variable/stack state shared by the tree-walking evaluators and the compiled-script evaluator.
    template <class T_> struct EvalStateCore_ {
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

    private:
        void InitVariables() {
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
} // namespace Dal::Script
