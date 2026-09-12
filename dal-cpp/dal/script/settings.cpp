//
// Created by Codex on 2026/9/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/script/settings.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
#include <dal/auto/MG_TodayFixingPolicy_enum.inc>

    namespace Script {
        Date_ CaptureScriptEvaluationDate() { return Global::Dates_::EvaluationDate(); }

        void ValidateRNG(const String_& method) {
            REQUIRE2(method == "sobol" || method == "mrg32" || method == "irn", "rng method is not known", ScriptError_);
        }

        void ValidateSimulationSettings(const MonteCarloSettings_& settings) {
            ValidateRNG(settings.rsg_);
            REQUIRE2(std::isfinite(settings.smooth_) && settings.smooth_ > 0.0, "InvalidSmoothing: expected a finite positive width", ScriptError_);
        }
    } // namespace Script
} // namespace Dal
