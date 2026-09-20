//
// Created by Codex on 2026/9/13.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/indice/index.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/script/settings.hpp>
#include <dal/storage/globals.hpp>

namespace Dal {
#include <dal/auto/MG_TodayFixingPolicy_enum.inc>

    namespace Script {
        Date_ CaptureScriptEvaluationDate() { return Global::Dates_::EvaluationDate(); }

        Handle_<Index_> ParseSettingIndex(const String_& name, const String_& field) {
            const String_ context = "; " + field + "=" + name + "; expected a non-empty, fully parsed index name";
            try {
                const Handle_<Index_> result(Index::Parse(name));
                REQUIRE2(result, "InvalidIndex: null parsed index", ScriptError_);
                return result;
            } catch (const Exception_& error) {
                THROW2("InvalidIndex: " + String_(error.what()) + context, ScriptError_);
            }
        }

        bool TryParseTodayFixingPolicy(const String_& name, TodayFixingPolicy_* policy) {
            //  String_ comparison is case-insensitive (ci_traits); compare through std::string
            const std::string exact(name.data(), name.size());
            if (exact == "Model") {
                *policy = TodayFixingPolicy_::Value_::MODEL;
                return true;
            }
            if (exact == "RequireHistorical") {
                *policy = TodayFixingPolicy_::Value_::REQUIREHISTORICAL;
                return true;
            }
            return false;
        }

        ScriptValuationSettings_ ResolveValuationSettings(const ScriptValuationSettings_& settings, const Handle_<MarketFixingSnapshot_>& snapshot) {
            auto result = settings;
            REQUIRE2(!snapshot || !result.fixings_ || snapshot == result.fixings_,
                     "InvalidSetting: valuation.fixings_ and snapshot; expected one explicit snapshot", ScriptError_);
            if (snapshot)
                result.fixings_ = snapshot;
            if (!result.evaluationDate_)
                result.evaluationDate_ = CaptureScriptEvaluationDate();
            REQUIRE2(result.evaluationDate_->IsValid(), "InvalidSetting: valuation.evaluationDate_=invalid Date; expected a valid date",
                     ScriptError_);
            REQUIRE2(result.todayFixingPolicy_ == TodayFixingPolicy_::Value_::MODEL ||
                         result.todayFixingPolicy_ == TodayFixingPolicy_::Value_::REQUIREHISTORICAL,
                     "InvalidSetting: InvalidTodayFixingPolicy; valuation.todayFixingPolicy_=" +
                         String_(std::to_string(static_cast<int>(result.todayFixingPolicy_.val_))) + "; expected Model or RequireHistorical",
                     ScriptError_);
            return result;
        }

        void ValidateRNG(const String_& method) {
            REQUIRE2(method == "sobol" || method == "mrg32" || method == "irn",
                     "InvalidSetting: simulation.rsg_=" + method + "; expected sobol, mrg32 or irn; rng method is not known", ScriptError_);
        }

        void ValidateSmoothing(double smooth) {
            REQUIRE2(std::isfinite(smooth) && smooth > 0.0,
                     "InvalidSetting: InvalidSmoothing; simulation.smooth_=" + String_(std::to_string(smooth)) +
                         "; expected a finite positive width",
                     ScriptError_);
        }

        void ValidateLsmcBasisDegree(int degree) {
            REQUIRE2(degree >= 1 && degree <= 8,
                     "InvalidSetting: InvalidLsmcBasisDegree; simulation.lsmcBasisDegree_=" + String_(std::to_string(degree)) +
                         "; expected an integer between 1 and 8",
                     ScriptError_);
        }

        void ValidateSimulationSettings(const MonteCarloSettings_& settings) {
            ValidateRNG(settings.rsg_);
            ValidateSmoothing(settings.smooth_);
            ValidateLsmcBasisDegree(settings.lsmcBasisDegree_);
        }
    } // namespace Script
} // namespace Dal
