//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <optional>

#include <dal/model/base.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration TodayFixingPolicy
    Source for a fixing on the evaluation date
switchable
alternative MODEL
alternative REQUIREHISTORICAL
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_TodayFixingPolicy_enum.hpp>

    namespace Script {
        struct ScriptProductSettings_ {
            String_ defaultIndex_;
        };

        struct MonteCarloSettings_ {
            String_ rsg_ = "sobol";
            bool useBb_ = false;
            bool enableAad_ = false;
            double smooth_ = 0.01;
            std::optional<bool> compiled_;
        };

        struct ScriptValuationSettings_ {
            TodayFixingPolicy_ todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
            Vector_<ModelIndexBinding_> modelBindings_;
        };

        Date_ CaptureScriptEvaluationDate();
        void ValidateRNG(const String_& method);
        void ValidateSimulationSettings(const MonteCarloSettings_& settings);
    } // namespace Script
} // namespace Dal
