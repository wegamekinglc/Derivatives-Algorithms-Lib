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
        constexpr double DEFAULT_SMOOTH = 0.01;
        constexpr int DEFAULT_LSMC_BASIS_DEGREE = 3;
        constexpr double DEFAULT_LSMC_POLICY_BUMP_RELATIVE = 1e-3;

        struct ScriptProductSettings_ {
            String_ defaultIndex_;
        };

        struct MonteCarloSettings_ {
            String_ rsg_ = "sobol";
            bool useBb_ = false;
            bool enableAad_ = false;
            double smooth_ = DEFAULT_SMOOTH;
            std::optional<bool> compiled_;
            // Polynomial degree of the LSMC regression basis for EXERCISE valuation
            int lsmcBasisDegree_ = DEFAULT_LSMC_BASIS_DEGREE;
            // Positive training count for EXERCISE valuation; unset uses the pricing count.
            std::optional<int> lsmcTrainingPaths_ = std::nullopt;
            // Positive held-out count enables degree selection on a disjoint Sobol block.
            // Unset retains the fixed-degree fast path.
            std::optional<int> lsmcValidationPaths_ = std::nullopt;
            // Optional randomized-QMC pricing replicates for one frozen LSMC policy.
            std::optional<int> lsmcRqmcReplicates_ = std::nullopt;
            // User-visible, exactly representable seeds; unset uses zero in RQMC mode.
            std::optional<int> lsmcTrainingSeed_ = std::nullopt;
            std::optional<int> lsmcPricingSeed_ = std::nullopt;
            // Frozen keeps the pathwise partial derivative. RetrainedBump adds the
            // common-path policy secant for model parameters and script constants.
            String_ lsmcPolicyRiskMode_ = "Frozen";
            double lsmcPolicyBumpRelative_ = DEFAULT_LSMC_POLICY_BUMP_RELATIVE;
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
        //  Binding-facing parser: deliberately case-sensitive exact Model / RequireHistorical
        //  (unlike the Machinist String_ constructor, which is case-insensitive). Returns
        //  false on any other spelling so each layer keeps its own error context.
        bool TryParseTodayFixingPolicy(const String_& name, TodayFixingPolicy_* policy);
        ScriptValuationSettings_ ResolveValuationSettings(const ScriptValuationSettings_& settings,
                                                          const Handle_<MarketFixingSnapshot_>& snapshot = {});
        void ValidateRNG(const String_& method);
        void ValidateSmoothing(double smooth);
        void ValidateLsmcBasisDegree(int degree);
        void ValidateLsmcTrainingPaths(int count);
        void ValidateLsmcValidationPaths(int count);
        void ValidateLsmcRqmcReplicates(int count);
        void ValidateLsmcSeed(int seed, const String_& field);
        void ValidateLsmcPolicyRiskMode(const String_& mode);
        void ValidateLsmcPolicyBumpRelative(double bump);
        void ValidateSimulationSettings(const MonteCarloSettings_& settings);
    } // namespace Script
} // namespace Dal
