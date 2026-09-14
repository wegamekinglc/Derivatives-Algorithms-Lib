//
// Created by wegamekinglc on 2026/9/15.
//

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>

namespace Dal::Script {
    template <>
    void ScriptCompiled_::Evaluate<AAD::Number_>(const Scenario_<AAD::Number_>& scenario, EvalState_<AAD::Number_>& state) const {
        EvaluateImpl(scenario, state);
    }

    template <>
    void ScriptProduct_::Evaluate<AAD::Number_, FuzzyEvaluator_<AAD::Number_>>(const Scenario_<AAD::Number_>& scenario,
                                                                           FuzzyEvaluator_<AAD::Number_>& eval) const {
        EvaluateImpl(scenario, eval);
    }
} // namespace Dal::Script
