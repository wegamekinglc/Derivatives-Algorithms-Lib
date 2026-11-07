#pragma once

#include <dal/math/pde/earlyexercisepderisk.hpp>

namespace Dal::PDE::EarlyExercisePdeRiskInternal {
    struct TestControl_ {
        size_t scenarioIndex_ = 0;
        bool failFirstSolve_ = false, extraFirstSolve_ = false, nonfinitePrice_ = false;
        size_t* rollbacksStarted_ = nullptr;
    };
    struct Evaluator_ {
        struct State_;
        static EarlyExercisePdeRiskResult_ Run(const EarlyExercisePdeRiskRequest_&, const TestControl_*);
        static void Initialize(EarlyExercisePdeRiskResult_*, const State_&);
        static void RunScenario(EarlyExercisePdeRiskResult_*, State_*, size_t);
        static void RunAll(EarlyExercisePdeRiskResult_*, State_*);
        static void Finish(EarlyExercisePdeRiskResult_*, const State_&);
    };
    [[nodiscard]] EarlyExercisePdeRiskResult_ Evaluate(const EarlyExercisePdeRiskRequest_&, const TestControl_&);
    [[nodiscard]] size_t CheckedAdd(size_t, size_t);
    [[nodiscard]] size_t CheckedMultiply(size_t, size_t);
} // namespace Dal::PDE::EarlyExercisePdeRiskInternal
