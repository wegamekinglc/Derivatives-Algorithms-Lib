#include <dal/platform/platform.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <dal/math/pde/earlyexercisepderisk.hpp>
#include <dal/math/pde/earlyexercisepdeinternal.hpp>

#include "reference.hpp"

namespace {
    using Writer_ = rapidjson::Writer<rapidjson::OStreamWrapper>;
    void Write(Writer_& w, double v) {
        if (std::isfinite(v))
            w.Double(v);
        else
            w.Null();
    }
    void Write(Writer_& w, bool v) { w.Bool(v); }
    void Write(Writer_& w, int v) { w.Int(v); }
    void Write(Writer_& w, size_t v) { w.Uint64(v); }
    void Write(Writer_& w, const char* v) { w.String(v); }
    void Write(Writer_& w, const Dal::String_& v) { w.String(v.c_str(), v.size()); }
    void Write(Writer_& w, const Dal::Date_& v) { Write(w, Dal::Date::ToString(v)); }
    template <class T_> auto Write(Writer_& w, const T_& v) -> decltype(v.String(), void()) { Write(w, v.String()); }
    template <class T_> void Write(Writer_& w, const std::optional<T_>& v);
    template <class T_> void Write(Writer_& w, const Dal::Vector_<T_>& values);
#define FIELD(member)                                                                                                                                \
    w.Key(#member);                                                                                                                                  \
    Write(w, x.member)
#define OBJECT(type, ...)                                                                                                                            \
    void Write(Writer_& w, const Dal::PDE::type& x) {                                                                                                \
        w.StartObject();                                                                                                                             \
        __VA_ARGS__;                                                                                                                                 \
        w.EndObject();                                                                                                                               \
    }
    OBJECT(EarlyExercisePdeCarrySegment_, FIELD(endDate_); FIELD(rate_); FIELD(dividendYield_))
    OBJECT(EarlyExercisePdeMarket_, FIELD(spot_); FIELD(volatility_); FIELD(carry_))
    OBJECT(EarlyExercisePdeContract_, FIELD(evaluationDate_); FIELD(maturityDate_); FIELD(strike_); FIELD(optionType_); FIELD(exerciseStyle_);
           FIELD(exerciseDates_))
    OBJECT(EarlyExercisePdeSettings_, FIELD(gridPoints_); FIELD(upper_); FIELD(ordinarySteps_); FIELD(theta_); FIELD(rannacherIntervals_);
           FIELD(penaltyFactor_);
           FIELD(absoluteTolerance_);
           FIELD(relativeTolerance_);
           FIELD(linearBackwardErrorLimit_);
           FIELD(maxIterationsPerStep_);
           FIELD(maxActualSteps_);
           FIELD(maxWorkUnits_);
           FIELD(numericPayloadBudgetBytes_);
           FIELD(workspaceBudgetBytes_))
    OBJECT(EarlyExercisePdeRequest_, FIELD(contract_); FIELD(market_); FIELD(settings_))
    OBJECT(EarlyExercisePdeRiskSettings_, FIELD(delta_); FIELD(gamma_); FIELD(vega_); FIELD(carryRisk_); FIELD(spotBump_); FIELD(volatilityBump_);
           FIELD(rateBumps_);
           FIELD(dividendYieldBumps_);
           FIELD(deltaStabilityTolerance_);
           FIELD(gammaStabilityTolerance_);
           FIELD(vegaStabilityTolerance_);
           FIELD(rateStabilityTolerance_);
           FIELD(dividendYieldStabilityTolerance_);
           FIELD(maxScenarios_);
           FIELD(maxWorkUnits_);
           FIELD(numericPayloadBudgetBytes_);
           FIELD(workspaceBudgetBytes_))
    OBJECT(EarlyExercisePdeRiskRequest_, FIELD(valuation_); FIELD(risk_))
    OBJECT(EarlyExercisePdeRiskStencilEntry_, FIELD(scenarioIndex_); FIELD(weight_))
    OBJECT(EarlyExercisePdeRiskRecord_, FIELD(riskIndex_); FIELD(riskId_); FIELD(parameterId_); FIELD(derivativeOrder_); FIELD(segmentIndex_);
           FIELD(startDate_);
           FIELD(endDate_);
           FIELD(parameterValue_);
           FIELD(unit_);
           FIELD(value_);
           FIELD(status_);
           FIELD(reason_);
           FIELD(method_);
           FIELD(interpretation_);
           FIELD(fineBump_);
           FIELD(coarseBump_);
           FIELD(bumpOrigin_);
           FIELD(fineEstimate_);
           FIELD(coarseEstimate_);
           FIELD(stepDifference_);
           FIELD(normalizedStepDifference_);
           FIELD(stabilityTolerance_);
           FIELD(fineStencil_);
           FIELD(coarseStencil_))
    OBJECT(EarlyExercisePdeTimeNode_, FIELD(timeYears_); FIELD(calendarDate_); FIELD(isExerciseDate_); FIELD(isCarryKnot_); FIELD(isMaturity_))
    OBJECT(EarlyExercisePdeRiskPlanStep_, FIELD(sourceTimeIndex_); FIELD(timeIndex_); FIELD(dt_); FIELD(theta_); FIELD(carryIndex_); FIELD(damped_);
           FIELD(projection_))
    OBJECT(EarlyExercisePdeRiskPlan_, FIELD(schemaVersion_); FIELD(planId_); FIELD(evaluationDate_); FIELD(maturityDate_); FIELD(upper_);
           FIELD(gridPoints_);
           FIELD(gridSpacing_);
           FIELD(grid_);
           FIELD(timeGrid_);
           FIELD(steps_);
           FIELD(epsilon_);
           FIELD(theta_);
           FIELD(effectiveRannacherIntervals_);
           FIELD(penaltyFactor_);
           FIELD(maxIterationsPerStep_);
           FIELD(linearBackwardErrorLimit_);
           FIELD(interpolationPolicy_);
           FIELD(resolvedRisk_);
           FIELD(spotBumpOrigin_);
           FIELD(volatilityBumpOrigin_);
           FIELD(rateBumpOrigins_);
           FIELD(dividendYieldBumpOrigins_);
           FIELD(plannedScenarios_);
           FIELD(basePlannedSteps_);
           FIELD(plannedSteps_);
           FIELD(plannedNumericPayloadBytes_);
           FIELD(plannedWorkspaceBytes_);
           FIELD(plannedPeakNativeBytes_))
    OBJECT(EarlyExercisePdeFailure_, FIELD(status_); FIELD(timeIndex_); FIELD(iteration_); FIELD(calendarDate_); FIELD(node_); FIELD(residual_);
           FIELD(threshold_);
           FIELD(workUnits_);
           FIELD(reason_))
    OBJECT(EarlyExercisePdeRiskScenario_, FIELD(scenarioIndex_); FIELD(scenarioId_); FIELD(planId_); FIELD(parameterId_); FIELD(segmentIndex_);
           FIELD(parameterValue_);
           FIELD(offset_);
           FIELD(status_);
           FIELD(solverStatus_);
           FIELD(workLimitSource_);
           FIELD(priceAvailable_);
           FIELD(price_);
           FIELD(epsilon_);
           FIELD(plannedSteps_);
           FIELD(actualSteps_);
           FIELD(completedSteps_);
           FIELD(totalSolves_);
           FIELD(workUnits_);
           FIELD(numericPayloadBytes_);
           FIELD(peakWorkspaceBytes_);
           FIELD(maxIterationChange_);
           FIELD(maxLinearBackwardError_);
           FIELD(maxObstacleViolation_);
           FIELD(maxDualViolation_);
           FIELD(maxComplementarityResidual_);
           FIELD(failure_);
           FIELD(evaluationMembership_);
           FIELD(evaluationBoundaryIntersectsSpotStencil_);
           FIELD(futureContactChanged_);
           FIELD(upwindChanged_))
    OBJECT(EarlyExercisePdeRiskExecution_, FIELD(plannedScenarios_); FIELD(attemptedScenarios_); FIELD(successfulScenarios_); FIELD(failedScenarios_);
           FIELD(notRunScenarios_);
           FIELD(totalSolves_);
           FIELD(workUnits_);
           FIELD(plannedNumericPayloadBytes_);
           FIELD(plannedWorkspaceBytes_);
           FIELD(plannedPeakNativeBytes_);
           FIELD(numericPayloadBytes_);
           FIELD(peakWorkspaceBytes_);
           FIELD(peakNativeBytes_);
           FIELD(workLimitScenarioIndex_);
           FIELD(workLimitSource_))
    OBJECT(EarlyExercisePdeStepDiagnostic_, FIELD(rollbackStep_); FIELD(sourceTimeIndex_); FIELD(timeIndex_); FIELD(solveCount_); FIELD(dt_);
           FIELD(theta_);
           FIELD(iterationChange_);
           FIELD(damped_);
           FIELD(projection_);
           FIELD(completed_);
           FIELD(linearBackwardError_);
           FIELD(projectionChange_);
           FIELD(projectionResidual_);
           FIELD(obstacleViolation_);
           FIELD(dualViolation_);
           FIELD(complementarityResidual_);
           FIELD(normalizedObstacleViolation_);
           FIELD(normalizedDualViolation_);
           FIELD(normalizedComplementarityResidual_))
    OBJECT(EarlyExercisePdeExerciseLevel_, FIELD(timeIndex_); FIELD(calendarDate_); FIELD(kind_); FIELD(regionOffset_); FIELD(regionCount_);
           FIELD(epsilon_))
    OBJECT(EarlyExercisePdeExerciseRegion_, FIELD(timeIndex_); FIELD(regionIndex_); FIELD(firstNode_); FIELD(lastNode_); FIELD(leftClipped_);
           FIELD(rightClipped_))
    template <class T_> void Write(Writer_& w, const std::optional<T_>& v) {
        if (v)
            Write(w, *v);
        else
            w.Null();
    }
    template <class T_> void Write(Writer_& w, const Dal::Vector_<T_>& values) {
        w.StartArray();
        for (const auto& v : values)
            Write(w, v);
        w.EndArray();
    }
#undef FIELD
#undef OBJECT
    double Number(const rapidjson::Value& j, const char* key, double fallback) { return j.HasMember(key) ? j[key].GetDouble() : fallback; }
    bool Flag(const rapidjson::Value& j, const char* key, bool fallback) { return j.HasMember(key) ? j[key].GetBool() : fallback; }
    PdeRiskReference::Input_ ReferenceInput(const rapidjson::Value& j) {
        PdeRiskReference::Input_ r;
        r.spot_ = Number(j, "spot", 100);
        r.strike_ = Number(j, "strike", 100);
        r.volatility_ = Number(j, "vol", .2);
        r.days_ = Number(j, "days", 365);
        r.call_ = Flag(j, "call", false);
        r.american_ = Flag(j, "american", false);
        r.carry_.clear();
        for (const auto& s : j["carry"].GetArray())
            r.carry_.push_back({s[0].GetDouble(), s[1].GetDouble(), s[2].GetDouble()});
        r.exercise_.clear();
        for (const auto& d : j["exercise"].GetArray())
            r.exercise_.push_back(d.GetDouble());
        return r;
    }
    Dal::PDE::EarlyExercisePdeRiskRequest_ Request(const rapidjson::Value& j) {
        using namespace Dal;
        using namespace Dal::PDE;
        const auto input = ReferenceInput(j);
        const auto& e = j["evaluation"];
        const Date_ evaluation(e[0].GetInt(), e[1].GetInt(), e[2].GetInt());
        const auto date = [&](int day) { return Date::FromExcel(Date::ToExcel(evaluation) + day); };
        const auto maturity = date(static_cast<int>(input.days_));
        EarlyExercisePdeRiskRequest_ r;
        r.valuation_.contract_ = {evaluation,
                                  maturity,
                                  input.strike_,
                                  OptionType_(input.call_ ? "CALL" : "PUT"),
                                  EarlyExerciseStyle_(input.american_ ? "AMERICAN" : "BERMUDAN"),
                                  {}};
        for (double day : input.exercise_)
            r.valuation_.contract_.exerciseDates_.push_back(date(static_cast<int>(day)));
        r.valuation_.market_ = {input.spot_, input.volatility_, {}};
        for (const auto& s : input.carry_)
            r.valuation_.market_.carry_.push_back({date(static_cast<int>(s.end_)), s.rate_, s.yield_});
        auto& s = r.valuation_.settings_;
        s.gridPoints_ = static_cast<int>(Number(j, "nodes", 1601));
        s.ordinarySteps_ = static_cast<int>(Number(j, "steps", 8000));
        if (!Flag(j, "default_upper", false))
            s.upper_ = Number(j, "upper", 400);
        s.theta_ = Number(j, "theta", 1);
        s.rannacherIntervals_ = static_cast<int>(Number(j, "rannacher", 2));
        s.penaltyFactor_ = Number(j, "penalty", 1e8);
        s.linearBackwardErrorLimit_ = Number(j, "linear", 1e-12);
        s.maxIterationsPerStep_ = static_cast<int>(Number(j, "iterations", 100));
        if (!Flag(j, "default_caps", false)) {
            s.maxActualSteps_ = 200000;
            s.maxWorkUnits_ = 1000000000;
            s.numericPayloadBudgetBytes_ = s.workspaceBudgetBytes_ = 1073741824;
            r.risk_.maxWorkUnits_ = 10000000000ULL;
            r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = 2147483648ULL;
        }
        r.risk_.delta_ = Flag(j, "delta", true);
        r.risk_.gamma_ = Flag(j, "gamma", true);
        r.risk_.vega_ = Flag(j, "vega", true);
        r.risk_.carryRisk_ = Flag(j, "carry_risk", true);
        if (j.HasMember("h"))
            r.risk_.spotBump_ = j["h"].GetDouble();
        r.risk_.volatilityBump_ = Number(j, "vol_bump", .001);
        if (j.HasMember("carry_bump")) {
            r.risk_.rateBumps_ = Vector_<double>(input.carry_.size(), j["carry_bump"].GetDouble());
            r.risk_.dividendYieldBumps_ = r.risk_.rateBumps_;
        }
        if (j.HasMember("work"))
            r.risk_.maxWorkUnits_ = j["work"].GetUint64();
        return r;
    }
    Dal::PDE::EarlyExercisePdeMarket_ ScenarioMarket(const Dal::PDE::EarlyExercisePdeMarket_& base,
                                                     const Dal::PDE::EarlyExercisePdeRiskScenario_& scenario) {
        auto market = base;
        if (!scenario.parameterId_)
            return market;
        if (*scenario.parameterId_ == "Spot")
            market.spot_ = *scenario.parameterValue_;
        else if (*scenario.parameterId_ == "Volatility")
            market.volatility_ = *scenario.parameterValue_;
        else if (scenario.parameterId_->find("Rate[") == 0)
            market.carry_[*scenario.segmentIndex_].rate_ = *scenario.parameterValue_;
        else
            market.carry_[*scenario.segmentIndex_].dividendYield_ = *scenario.parameterValue_;
        return market;
    }
    void Audit(Writer_& w,
               const rapidjson::Value& j,
               const Dal::PDE::EarlyExercisePdeRiskRequest_& request,
               const Dal::PDE::EarlyExercisePdeRiskResult_& result) {
        using namespace Dal::PDE;
        Dal::Vector_<EarlyExercisePdeMarket_> markets;
        for (const auto& row : result.Scenarios())
            markets.push_back(ScenarioMarket(request.valuation_.market_, row));
        const auto plan = EarlyExercisePdeInternal::BuildPlan(request.valuation_, markets);
        w.Key("audit_plan_id");
        Write(w, plan.Id());
        if (Flag(j, "audit_contacts", false)) {
            w.Key("contact_audit");
            w.StartArray();
            for (size_t i = 0; i < markets.size(); ++i) {
                auto r = request.valuation_;
                r.market_ = markets[i];
                const auto observed = EarlyExercisePdeInternal::Rollback(r, plan);
                size_t nonterminal = 0;
                Dal::Vector_<EarlyExercisePdeExerciseRegion_> evaluation;
                for (const auto& level : observed.ExerciseLevels()) {
                    if (level.kind_ != Dal::EarlyExercisePdeLevelKind_("TERMINAL"))
                        nonterminal += level.regionCount_;
                    if (level.timeIndex_ == 0)
                        for (size_t k = 0; k < level.regionCount_; ++k)
                            evaluation.push_back(observed.ExerciseRegions()[level.regionOffset_ + k]);
                }
                w.StartObject();
                w.Key("scenario");
                Write(w, result.Scenarios()[i].scenarioId_);
                w.Key("same_price");
                Write(w, observed.IsSuccess() && result.Scenarios()[i].price_ == observed.Price());
                w.Key("market");
                Write(w, markets[i]);
                w.Key("nonterminal_regions");
                Write(w, nonterminal);
                w.Key("evaluation_regions");
                Write(w, evaluation);
                w.EndObject();
            }
            w.EndArray();
        }
        if (Flag(j, "audit_parallel", false)) {
            w.Key("parallel_audit");
            w.StartArray();
            for (bool rate : {true, false}) {
                const double bump = Number(j, "carry_bump", .0001) / 2;
                double values[2]{};
                for (int side = 0; side < 2; ++side) {
                    auto r = request.valuation_;
                    for (auto& segment : r.market_.carry_)
                        (rate ? segment.rate_ : segment.dividendYield_) += side == 0 ? -bump : bump;
                    const auto observed = EarlyExercisePdeInternal::Rollback(r, plan);
                    if (!observed.IsSuccess())
                        throw std::runtime_error("parallel audit price failed");
                    values[side] = observed.Price();
                }
                w.StartObject();
                w.Key("rate");
                Write(w, rate);
                w.Key("bump");
                Write(w, bump);
                w.Key("minus");
                Write(w, values[0]);
                w.Key("plus");
                Write(w, values[1]);
                w.Key("partial");
                Write(w, (values[1] - values[0]) / (2 * bump));
                w.EndObject();
            }
            w.EndArray();
        }
    }
    void Product(Writer_& w, const rapidjson::Value& j, bool audit) {
        const auto request = Request(j);
        w.Key("original");
        Write(w, request);
        const auto result = Dal::PDE::EvaluateEarlyExercisePdeRisk(request);
        w.Key("status");
        Write(w, result.Status());
        w.Key("price");
        if (result.PriceAvailable())
            Write(w, result.Price());
        else
            w.Null();
        w.Key("plan");
        Write(w, result.Plan());
        w.Key("risks");
        Write(w, result.Risks());
        w.Key("scenarios");
        Write(w, result.Scenarios());
        w.Key("execution");
        Write(w, result.Execution());
        w.Key("resolved_valuation");
        Write(w, result.BaseResult().Request());
        w.Key("base_steps");
        Write(w, result.BaseResult().StepDiagnostics());
        w.Key("base_levels");
        Write(w, result.BaseResult().ExerciseLevels());
        w.Key("base_regions");
        Write(w, result.BaseResult().ExerciseRegions());
        w.Key("base_values");
        if (result.PriceAvailable())
            Write(w, result.BaseResult().Values());
        else
            w.Null();
        if (audit)
            Audit(w, j, request, result);
    }
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: driver product|analytic|tree|log request.json\n";
        return 2;
    }
    std::ifstream input(argv[2]);
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    rapidjson::Document j;
    j.Parse(text.c_str());
    if (j.HasParseError())
        return 2;
    rapidjson::OStreamWrapper stream(std::cout);
    Writer_ w(stream);
    w.StartObject();
    try {
        const std::string mode(argv[1]);
        if (mode == "product" || mode == "audit")
            Product(w, j, mode == "audit");
        else {
            const auto r = ReferenceInput(j);
            if (mode == "analytic") {
                w.Key("values");
                w.StartArray();
                for (double v : PdeRiskReference::Analytic(r))
                    Write(w, v);
                w.EndArray();
            } else if (mode == "tree") {
                w.Key("price");
                Write(w, PdeRiskReference::Tree(r, j["tree_steps"].GetInt()));
            } else if (mode == "log") {
                const auto result = PdeRiskReference::LogLcp(r, j["level"].GetInt());
                w.Key("price");
                Write(w, result.price_);
                w.Key("residual");
                Write(w, result.maxResidual_);
                w.Key("iterations");
                Write(w, result.maxIterations_);
                w.Key("steps");
                Write(w, result.steps_);
            } else
                throw std::runtime_error("unknown mode");
        }
    } catch (const std::exception& e) {
        w.Key("error");
        w.String(e.what());
    }
    w.EndObject();
    std::cout << '\n';
}
