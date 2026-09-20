//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <optional>

#include <dal/indice/fixingsnapshot.hpp>
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

    class Index_;

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
            // Polynomial degree of the LSMC regression basis for EXERCISE valuation
            int lsmcBasisDegree_ = 3;
        };

        struct ScriptValuationSettings_ {
            TodayFixingPolicy_ todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
            std::optional<Date_> evaluationDate_;
            Handle_<MarketFixingSnapshot_> fixings_;
        };

        Date_ CaptureScriptEvaluationDate();
        inline const char* FixingSourceKind(const ScriptValuationSettings_& settings) {
            return settings.fixings_ ? "ExplicitSnapshot" : "GlobalSnapshot";
        }
        Handle_<Index_> ParseSettingIndex(const String_& name, const String_& field);
        ScriptValuationSettings_ ResolveValuationSettings(const ScriptValuationSettings_& settings,
                                                          const Handle_<MarketFixingSnapshot_>& snapshot = {});
        void ValidateRNG(const String_& method);
        void ValidateSmoothing(double smooth);
        void ValidateLsmcBasisDegree(int degree);
        void ValidateSimulationSettings(const MonteCarloSettings_& settings);
    } // namespace Script
} // namespace Dal
