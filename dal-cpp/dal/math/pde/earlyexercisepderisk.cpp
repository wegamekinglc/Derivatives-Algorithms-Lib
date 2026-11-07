#include <dal/platform/platform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <dal/math/pde/earlyexercisepdeinternal.hpp>
#include <dal/math/pde/earlyexercisepderiskinternal.hpp>

namespace Dal {
#include <dal/auto/MG_EarlyExercisePdeRiskAvailability_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeRiskBumpOrigin_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeRiskMembership_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeRiskScenarioStatus_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeRiskStatus_enum.inc>
#include <dal/auto/MG_EarlyExercisePdeRiskWorkLimitSource_enum.inc>
} // namespace Dal

namespace Dal::PDE {
    namespace {
        using Availability_ = EarlyExercisePdeRiskAvailability_::Value_;
        using ScenarioStatus_ = EarlyExercisePdeRiskScenarioStatus_::Value_;
        using Source_ = EarlyExercisePdeRiskWorkLimitSource_::Value_;
        using Membership_ = EarlyExercisePdeRiskMembership_::Value_;
        using Origin_ = EarlyExercisePdeRiskBumpOrigin_::Value_;
        using Status_ = EarlyExercisePdeRiskStatus_::Value_;
        namespace Internal = EarlyExercisePdeInternal;

        void Check(bool condition, const String_& field, double value, const char* constraint) {
            if (!condition) {
                std::ostringstream message;
                message.precision(17);
                message << "InvalidEarlyExercisePdeRiskRequest; " << field << "; value=" << value << "; " << constraint;
                THROW(message.str());
            }
        }
        void Budget(size_t required, size_t limit, const char* field) {
            REQUIRE(required <= limit, String_("EarlyExercisePdeRiskBudgetExceeded; ") + field + "; required=" + String_(std::to_string(required)) +
                                           "; limit=" + String_(std::to_string(limit)));
        }
        size_t Add(size_t a, size_t b) {
            REQUIRE(b <= std::numeric_limits<size_t>::max() - a,
                    "EarlyExercisePdeRiskBudgetExceeded; extent; required=overflow; limit=SIZE_MAX; addition overflow");
            return a + b;
        }
        size_t Mul(size_t a, size_t b) {
            REQUIRE(a == 0 || b <= std::numeric_limits<size_t>::max() / a,
                    "EarlyExercisePdeRiskBudgetExceeded; extent; required=overflow; limit=SIZE_MAX; multiplication overflow");
            return a * b;
        }
        template <class F_> auto PriceCall(F_ call) {
            try {
                return call();
            } catch (const Exception_& e) {
                const std::string message(e.what());
                const bool budget =
                    message.find("EarlyExercisePdeBudgetExceeded") != std::string::npos || message.find("overflow") != std::string::npos;
                THROW(String_(budget ? "EarlyExercisePdeRiskBudgetExceeded; valuation; " : "InvalidEarlyExercisePdeRiskRequest; valuation; ") +
                      String_(message));
            }
        }
        String_ Indexed(const char* name, size_t i) { return String_(name) + "[" + String_(std::to_string(i)) + "]"; }
        double Upper(const EarlyExercisePdeRiskRequest_& r) {
            return r.valuation_.settings_.upper_.value_or(4.0 * std::max(r.valuation_.market_.spot_, r.valuation_.contract_.strike_));
        }
        double SpotBump(const EarlyExercisePdeRiskRequest_& r) {
            return r.risk_.spotBump_.value_or(2.0 * (Upper(r) / (r.valuation_.settings_.gridPoints_ - 1)));
        }
        double CarryBump(const Vector_<double>& bumps, size_t i) { return bumps.empty() ? 1e-4 : bumps[i]; }
        void Positive(double value, const String_& field) { Check(std::isfinite(value) && value > 0.0, field, value, "must be finite and positive"); }
        void Bump(double value, const String_& field) {
            Positive(value, field);
            Positive(value / 2.0, field + ".half");
        }
        void Offset(double value, double bump, const String_& field, double low, double high) {
            double previous = -std::numeric_limits<double>::infinity();
            for (double offset : {-bump, -bump / 2.0, 0.0, bump / 2.0, bump}) {
                const double point = value + offset;
                Check(std::isfinite(point), field, point, "offset arithmetic must be finite");
                if (point < low || point > high)
                    continue;
                Check(std::isfinite(point - value), field, point, "parameter differences must be finite");
                Check(point > previous, field, point, "both scales must form distinct binary64 parameter values");
                previous = point;
            }
        }
        void ValidateBumps(const EarlyExercisePdeRiskRequest_& r) {
            const auto& v = r.valuation_;
            const size_t m = v.market_.carry_.size();
            const auto& s = r.risk_;
            Check(s.rateBumps_.empty() || s.rateBumps_.size() == m, "risk.rate_bumps.length", s.rateBumps_.size(), "must be empty or carry length");
            Check(s.dividendYieldBumps_.empty() || s.dividendYieldBumps_.size() == m, "risk.dividend_yield_bumps.length",
                  s.dividendYieldBumps_.size(), "must be empty or carry length");
            const double h = SpotBump(r), dx = Upper(r) / (v.settings_.gridPoints_ - 1), ratio = h / dx;
            Positive(h, "risk.spot_bump");
            Positive(2.0 * h, "risk.spot_bump.coarse");
            Check(std::isfinite(ratio) && std::round(ratio) >= 2.0 && std::round(ratio) <= 9007199254740991.0 &&
                      std::abs(ratio - std::round(ratio)) <= 8.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(ratio)),
                  "risk.spot_bump/dx", ratio, "must be an integer number of cells >=2, <=2^53-1 within 8 epsilon64");
            Offset(v.market_.spot_, 2.0 * h, "risk.spot_bump", 0.0, Upper(r));
            Offset(v.market_.volatility_, s.volatilityBump_, "risk.volatility_bump", 0.0, std::numeric_limits<double>::infinity());
            for (size_t i = 0; i < m; ++i) {
                Offset(v.market_.carry_[i].rate_, CarryBump(s.rateBumps_, i), Indexed("risk.rate_bumps", i), -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::infinity());
                Offset(v.market_.carry_[i].dividendYield_, CarryBump(s.dividendYieldBumps_, i), Indexed("risk.dividend_yield_bumps", i),
                       -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
            }
        }
        struct Axis_ {
            String_ id_;
            std::optional<size_t> segment_;
            int kind_ = 0; // Spot, volatility, rate and yield are private dispatch indices.
            double value_ = 0.0, coarse_ = 0.0;
            EarlyExercisePdeRiskBumpOrigin_ origin_;
            Availability_ admission_ = Availability_::AVAILABLE;
            size_t firstScenario_ = 0;
        };
        double AxisOffset(const Axis_& axis, size_t index) { return axis.coarse_ * (index < 2 ? 1.0 : 0.5) * (index % 2 == 0 ? -1.0 : 1.0); }
        bool SpotLegal(const EarlyExercisePdeRiskRequest_& r) {
            const double coarse = 2.0 * SpotBump(r);
            return r.valuation_.market_.spot_ - coarse > 0.0 && r.valuation_.market_.spot_ + coarse < Upper(r);
        }
        size_t ScenarioCount(const EarlyExercisePdeRiskRequest_& r) {
            if (r.valuation_.market_.volatility_ == 0.0)
                return 1;
            size_t axes = (r.risk_.delta_ || r.risk_.gamma_) && SpotLegal(r) ? 1 : 0;
            axes = Add(axes, r.risk_.vega_ && r.valuation_.market_.volatility_ >= r.risk_.volatilityBump_ ? 1 : 0);
            if (r.risk_.carryRisk_)
                axes = Add(axes, Mul(2, r.valuation_.market_.carry_.size()));
            return Add(1, Mul(4, axes));
        }
        Axis_ SpotAxis(const EarlyExercisePdeRiskRequest_& r) {
            return {"Spot",
                    {},
                    0,
                    r.valuation_.market_.spot_,
                    2.0 * SpotBump(r),
                    Origin_(r.risk_.spotBump_ ? Origin_::EXPLICIT : Origin_::DEFAULT),
                    SpotLegal(r) ? Availability_::AVAILABLE : Availability_::UNAVAILABLE_DOMAIN_BOUNDARY};
        }
        Axis_ VolatilityAxis(const EarlyExercisePdeRiskRequest_& r) {
            return {"Volatility",
                    {},
                    1,
                    r.valuation_.market_.volatility_,
                    r.risk_.volatilityBump_,
                    Origin_(r.risk_.volatilityBump_ == 1e-3 ? Origin_::DEFAULT : Origin_::EXPLICIT),
                    r.valuation_.market_.volatility_ >= r.risk_.volatilityBump_ ? Availability_::AVAILABLE
                                                                                : Availability_::UNAVAILABLE_DOMAIN_BOUNDARY};
        }
        Axis_ CarryAxis(const EarlyExercisePdeRiskRequest_& r, size_t i, int kind) {
            const bool rate = kind == 2;
            const auto& bumps = rate ? r.risk_.rateBumps_ : r.risk_.dividendYieldBumps_;
            return {Indexed(rate ? "Rate" : "DividendYield", i),
                    i,
                    kind,
                    rate ? r.valuation_.market_.carry_[i].rate_ : r.valuation_.market_.carry_[i].dividendYield_,
                    CarryBump(bumps, i),
                    Origin_(bumps.empty() ? Origin_::DEFAULT : Origin_::EXPLICIT)};
        }
        void AppendCarryAxes(const EarlyExercisePdeRiskRequest_& r, Vector_<Axis_>* axes) {
            if (!r.risk_.carryRisk_)
                return;
            for (size_t i = 0; i < r.valuation_.market_.carry_.size(); ++i)
                for (int kind : {2, 3})
                    axes->push_back(CarryAxis(r, i, kind));
        }
        Vector_<Axis_> Axes(const EarlyExercisePdeRiskRequest_& r) {
            Vector_<Axis_> result;
            const auto& v = r.valuation_;
            const auto& s = r.risk_;
            result.reserve(Add(2, Mul(2, v.market_.carry_.size())));
            if (s.delta_ || s.gamma_)
                result.push_back(SpotAxis(r));
            if (s.vega_)
                result.push_back(VolatilityAxis(r));
            AppendCarryAxes(r, &result);
            if (v.market_.volatility_ == 0.0)
                for (auto& axis : result)
                    axis.admission_ = Availability_::UNAVAILABLE_DEGENERATE_DIFFUSION;
            return result;
        }
        void SetParameter(EarlyExercisePdeMarket_* market, const Axis_& axis, double value) {
            switch (axis.kind_) {
            case 0:
                market->spot_ = value;
                break;
            case 1:
                market->volatility_ = value;
                break;
            case 2:
                market->carry_[*axis.segment_].rate_ = value;
                break;
            default:
                market->carry_[*axis.segment_].dividendYield_ = value;
                break;
            }
        }
        void Stencil(EarlyExercisePdeRiskRecord_* record, size_t first, bool fine) {
            const double a = fine ? record->fineBump_ : record->coarseBump_;
            const size_t minus = first + (fine ? 2 : 0);
            auto& entries = fine ? record->fineStencil_ : record->coarseStencil_;
            entries.reserve(record->derivativeOrder_ == 2 ? 3 : 2);
            const double weight = record->derivativeOrder_ == 2 ? (1.0 / a) / a : 0.5 / a;
            entries.push_back({minus, record->derivativeOrder_ == 2 ? weight : -weight});
            if (record->derivativeOrder_ == 2)
                entries.push_back({0, -2.0 * weight});
            entries.push_back({minus + 1, weight});
        }
        String_ RiskId(const Axis_& axis, int order) {
            if (axis.kind_ == 0)
                return order == 2 ? "Gamma" : "Delta";
            return axis.kind_ == 1 ? "Vega" : axis.id_;
        }
        void RecordSegment(const EarlyExercisePdeRiskRequest_& r, const Axis_& axis, EarlyExercisePdeRiskRecord_* record) {
            record->segmentIndex_ = axis.segment_;
            if (axis.segment_) {
                record->startDate_ =
                    *axis.segment_ == 0 ? r.valuation_.contract_.evaluationDate_ : r.valuation_.market_.carry_[*axis.segment_ - 1].endDate_;
                record->endDate_ = r.valuation_.market_.carry_[*axis.segment_].endDate_;
            }
        }
        EarlyExercisePdeRiskRecord_ Record(const EarlyExercisePdeRiskRequest_& r, const Axis_& axis, int order, size_t index) {
            EarlyExercisePdeRiskRecord_ record;
            record.riskIndex_ = index;
            record.riskId_ = RiskId(axis, order);
            record.parameterId_ = axis.id_;
            record.derivativeOrder_ = order;
            RecordSegment(r, axis, &record);
            record.parameterValue_ = axis.value_;
            static const char* units[] = {"price per spot price unit", "price per decimal volatility", "price per decimal rate",
                                          "price per decimal dividend yield"};
            record.unit_ = order == 2 ? "price per squared spot price unit" : units[axis.kind_];
            record.method_ = "FiniteScaleCentralDifference";
            record.interpretation_ = order == 2 ? "FINITE_SCALE_MODEL_APPROXIMATION" : "FINITE_SCALE_FIRST_DERIVATIVE";
            record.coarseBump_ = axis.coarse_;
            record.fineBump_ = axis.coarse_ / 2.0;
            record.bumpOrigin_ = axis.origin_;
            const auto& s = r.risk_;
            const double tolerances[] = {order == 2 ? s.gammaStabilityTolerance_ : s.deltaStabilityTolerance_, s.vegaStabilityTolerance_,
                                         s.rateStabilityTolerance_, s.dividendYieldStabilityTolerance_};
            record.stabilityTolerance_ = tolerances[axis.kind_];
            record.status_ = EarlyExercisePdeRiskAvailability_(axis.admission_);
            if (axis.admission_ == Availability_::AVAILABLE) {
                Stencil(&record, axis.firstScenario_, false);
                Stencil(&record, axis.firstScenario_, true);
            }
            return record;
        }
        struct Envelope_ {
            size_t payload_, workspace_, peak_;
        };
        size_t RequestDynamic(const EarlyExercisePdeRiskRequest_& r) {
            return Add(Add(Mul(r.valuation_.market_.carry_.size(), sizeof(EarlyExercisePdeCarrySegment_)),
                           Mul(r.valuation_.contract_.exerciseDates_.size(), sizeof(Date_))),
                       Mul(Add(r.risk_.rateBumps_.size(), r.risk_.dividendYieldBumps_.size()), sizeof(double)));
        }
        size_t SetupEnvelope(const EarlyExercisePdeRiskRequest_& r, size_t scenarios) {
            return Add(Add(16384, Mul(Add(2, Mul(2, r.valuation_.market_.carry_.size())), 512)),
                       Add(Mul(scenarios, Add(512, Mul(r.valuation_.market_.carry_.size(), sizeof(EarlyExercisePdeCarrySegment_)))),
                           Mul(4, RequestDynamic(r))));
        }
        Envelope_ Admit(const EarlyExercisePdeRiskRequest_& r, size_t scenarios, const Internal::PlanLayout_& layout) {
            const size_t records = Add(3, Mul(2, r.valuation_.market_.carry_.size()));
            const size_t payload = Add(Add(Add(Add(32768, Mul(2, RequestDynamic(r))), layout.payloadBytes_),
                                           Add(Mul(r.valuation_.settings_.gridPoints_, 8), Mul(Add(layout.counts_.steps_, 1), 128))),
                                       Add(Mul(records, 4096), Add(Mul(scenarios, 4096), Mul(r.valuation_.market_.carry_.size(), 64))));
            const size_t setup = SetupEnvelope(r, scenarios);
            const size_t temporary =
                Add(Add(setup, layout.planBytes_), std::max(layout.canonicalWorkspaceBytes_, Add(layout.payloadBytes_, layout.workspaceBytes_)));
            Budget(payload, r.risk_.numericPayloadBudgetBytes_, "risk.numeric_payload_budget_bytes");
            Budget(temporary, r.risk_.workspaceBudgetBytes_, "risk.workspace_budget_bytes");
            return {payload, temporary, Add(payload, temporary)};
        }
        void ResolveRiskConfiguration(const EarlyExercisePdeRiskRequest_& r, EarlyExercisePdeRiskPlan_* p) {
            p->resolvedRisk_ = r.risk_;
            p->resolvedRisk_.spotBump_ = SpotBump(r);
            p->spotBumpOrigin_ = Origin_(r.risk_.spotBump_ ? Origin_::EXPLICIT : Origin_::DEFAULT);
            p->volatilityBumpOrigin_ = Origin_(r.risk_.volatilityBump_ == 1e-3 ? Origin_::DEFAULT : Origin_::EXPLICIT);
            const size_t m = r.valuation_.market_.carry_.size();
            p->rateBumpOrigins_ =
                Vector_<EarlyExercisePdeRiskBumpOrigin_>(m, Origin_(r.risk_.rateBumps_.empty() ? Origin_::DEFAULT : Origin_::EXPLICIT));
            p->dividendYieldBumpOrigins_ =
                Vector_<EarlyExercisePdeRiskBumpOrigin_>(m, Origin_(r.risk_.dividendYieldBumps_.empty() ? Origin_::DEFAULT : Origin_::EXPLICIT));
            if (p->resolvedRisk_.rateBumps_.empty())
                p->resolvedRisk_.rateBumps_ = Vector_<double>(m, 1e-4);
            if (p->resolvedRisk_.dividendYieldBumps_.empty())
                p->resolvedRisk_.dividendYieldBumps_ = Vector_<double>(m, 1e-4);
        }
        EarlyExercisePdeRiskPlan_
        ExportPlan(const EarlyExercisePdeRiskRequest_& r, const Internal::Plan_& plan, size_t scenarios, const Envelope_& envelope) {
            EarlyExercisePdeRiskPlan_ p;
            const auto& v = plan.Request();
            p.planId_ = plan.Id();
            p.evaluationDate_ = v.contract_.evaluationDate_;
            p.maturityDate_ = v.contract_.maturityDate_;
            p.upper_ = plan.Upper();
            p.gridPoints_ = v.settings_.gridPoints_;
            p.gridSpacing_ = p.upper_ / (p.gridPoints_ - 1);
            p.grid_ = plan.Grid();
            p.timeGrid_ = plan.TimeGrid();
            p.steps_.reserve(plan.Steps().size());
            for (size_t i = 0; i < plan.Steps().size(); ++i) {
                const auto& step = plan.Steps()[plan.Steps().size() - 1 - i];
                p.steps_.push_back({static_cast<int>(i + 1), static_cast<int>(i), step.source_ - step.target_, step.theta_, step.carry_, step.damped_,
                                    v.contract_.exerciseStyle_ == EarlyExerciseStyle_::Value_::BERMUDAN && p.timeGrid_[i].isExerciseDate_});
            }
            p.epsilon_ = plan.Epsilon();
            p.theta_ = v.settings_.theta_;
            p.effectiveRannacherIntervals_ = p.theta_ == 0.5 ? v.settings_.rannacherIntervals_ : 0;
            p.penaltyFactor_ = v.settings_.penaltyFactor_;
            p.maxIterationsPerStep_ = v.settings_.maxIterationsPerStep_;
            p.linearBackwardErrorLimit_ = v.settings_.linearBackwardErrorLimit_;
            ResolveRiskConfiguration(r, &p);
            p.plannedScenarios_ = scenarios;
            p.basePlannedSteps_ = plan.BasePlannedSteps();
            p.plannedSteps_ = static_cast<int>(plan.Steps().size());
            p.plannedNumericPayloadBytes_ = envelope.payload_;
            p.plannedWorkspaceBytes_ = envelope.workspace_;
            p.plannedPeakNativeBytes_ = envelope.peak_;
            return p;
        }
        bool HasEvaluationExercise(const EarlyExercisePdeResult_& result) {
            return !result.ExerciseLevels().empty() && result.ExerciseLevels().front().timeIndex_ == 0;
        }
        bool Contains(const EarlyExercisePdeExerciseRegion_& region, int low, int high) {
            return low >= region.firstNode_ && high <= region.lastNode_;
        }
        Membership_ ContactMembership(const EarlyExercisePdeResult_& result, int low, int high) {
            const auto& level = result.ExerciseLevels().front();
            bool contact = false;
            for (size_t j = 0; j < level.regionCount_; ++j) {
                const auto& region = result.ExerciseRegions()[level.regionOffset_ + j];
                if (Contains(region, low, high))
                    return Membership_::EXERCISE;
                contact = contact || Contains(region, low, low) || Contains(region, high, high);
            }
            return contact ? Membership_::BOUNDARY : Membership_::CONTINUATION;
        }
        Membership_ Membership(const EarlyExercisePdeResult_& result) {
            if (!HasEvaluationExercise(result))
                return Membership_::NOT_EXERCISABLE;
            const auto& grid = result.Grid();
            const double spot = result.Request().market_.spot_;
            const auto it = std::lower_bound(grid.begin(), grid.end(), spot);
            const int high = static_cast<int>(it - grid.begin()), low = *it == spot ? high : high - 1;
            return ContactMembership(result, low, high);
        }
        bool ClosedIntersection(double a, double b, double low, double high) { return b >= low && a <= high; }
        bool RegionIntersects(const EarlyExercisePdeExerciseRegion_& region, const Vector_<double>& grid, double low, double high) {
            return (!region.leftClipped_ && ClosedIntersection(grid[region.firstNode_ - 1], grid[region.firstNode_], low, high)) ||
                   (!region.rightClipped_ && ClosedIntersection(grid[region.lastNode_], grid[region.lastNode_ + 1], low, high));
        }
        bool BoundaryIntersects(const EarlyExercisePdeResult_& result, double low, double high) {
            if (!HasEvaluationExercise(result))
                return false;
            const auto& level = result.ExerciseLevels().front();
            const auto& grid = result.Grid();
            for (size_t i = 0; i < level.regionCount_; ++i) {
                const auto& region = result.ExerciseRegions()[level.regionOffset_ + i];
                if (RegionIntersects(region, grid, low, high))
                    return true;
            }
            return false;
        }
        bool LevelChanged(const EarlyExercisePdeResult_& a, const EarlyExercisePdeResult_& b, size_t index) {
            const auto& x = a.ExerciseLevels()[index];
            const auto& y = b.ExerciseLevels()[index];
            if (x.regionCount_ != y.regionCount_)
                return true;
            for (size_t k = 0; k < x.regionCount_; ++k) {
                const auto& p = a.ExerciseRegions()[x.regionOffset_ + k];
                const auto& q = b.ExerciseRegions()[y.regionOffset_ + k];
                if (p.firstNode_ != q.firstNode_ || p.lastNode_ != q.lastNode_)
                    return true;
            }
            return false;
        }
        bool FutureChanged(const EarlyExercisePdeResult_& a, const EarlyExercisePdeResult_& b) {
            const auto& x = a.ExerciseLevels();
            const auto& y = b.ExerciseLevels();
            if (x.size() != y.size())
                return true;
            for (size_t j = 0; j < x.size(); ++j) {
                if (x[j].timeIndex_ == 0 || x[j].kind_ == EarlyExercisePdeLevelKind_::Value_::TERMINAL)
                    continue;
                if (LevelChanged(a, b, j))
                    return true;
            }
            return false;
        }
        bool UpwindChanged(const EarlyExercisePdeMarket_& a, const EarlyExercisePdeMarket_& b, int nodes) {
            for (size_t j = 0; j < a.carry_.size(); ++j)
                for (int i = 1; i + 1 < nodes; ++i) {
                    const auto upwind = [i, j](const EarlyExercisePdeMarket_& m) {
                        return 0.5 * (m.volatility_ * i) * (m.volatility_ * i) < 0.5 * std::abs((m.carry_[j].rate_ - m.carry_[j].dividendYield_) * i);
                    };
                    if (upwind(a) != upwind(b))
                        return true;
                }
            return false;
        }
        std::optional<double> Finite(std::optional<double> value) { return value && std::isfinite(*value) ? value : std::nullopt; }
        String_ BoundedReason(const String_& reason) {
            size_t bytes = std::min<size_t>(reason.size(), 1024);
            while (bytes < reason.size() && bytes > 0 && (static_cast<unsigned char>(reason[bytes]) & 0xc0) == 0x80)
                --bytes;
            return String_(reason.begin(), reason.begin() + bytes);
        }
        void SummarizeFailure(std::optional<EarlyExercisePdeFailure_>* failure) {
            if (!*failure)
                return;
            (*failure)->reason_ = BoundedReason((*failure)->reason_);
            (*failure)->residual_ = Finite((*failure)->residual_);
            (*failure)->threshold_ = Finite((*failure)->threshold_);
        }
        std::optional<double> MaxIterationChange(const EarlyExercisePdeResult_& result) {
            std::optional<double> change;
            for (const auto& step : result.StepDiagnostics())
                if (std::isfinite(step.iterationChange_))
                    change = std::max(change.value_or(0.0), step.iterationChange_);
            return change;
        }
        bool PublishPrice(EarlyExercisePdeRiskScenario_* row, const EarlyExercisePdeResult_& result, const EarlyExercisePdeRiskRequest_& r) {
            row->status_ = EarlyExercisePdeRiskScenarioStatus_(ScenarioStatus_::NUMERICAL_FAILURE);
            if (!result.IsSuccess())
                return false;
            const double price = result.Price();
            if (!std::isfinite(price)) {
                row->solverStatus_ = EarlyExercisePdeStatus_(EarlyExercisePdeStatus_::Value_::NONFINITE);
                row->failure_ = EarlyExercisePdeFailure_{
                    *row->solverStatus_,           0, 0, r.valuation_.contract_.evaluationDate_, {}, {}, {}, result.Execution().workUnits_,
                    "nonfinite interpolated price"};
                return false;
            }
            row->status_ = EarlyExercisePdeRiskScenarioStatus_(ScenarioStatus_::SUCCESS);
            row->priceAvailable_ = true;
            row->price_ = price;
            return true;
        }
        void SummarizeContact(EarlyExercisePdeRiskScenario_* row,
                              const EarlyExercisePdeResult_& result,
                              const EarlyExercisePdeRiskRequest_& r,
                              const EarlyExercisePdeResult_* base) {
            row->evaluationMembership_ = EarlyExercisePdeRiskMembership_(Membership(result));
            if (r.risk_.delta_ || r.risk_.gamma_)
                row->evaluationBoundaryIntersectsSpotStencil_ =
                    BoundaryIntersects(result, r.valuation_.market_.spot_ - 2.0 * SpotBump(r), r.valuation_.market_.spot_ + 2.0 * SpotBump(r));
            row->futureContactChanged_ = base ? FutureChanged(*base, result) : false;
            row->upwindChanged_ = base ? UpwindChanged(base->Request().market_, result.Request().market_, r.valuation_.settings_.gridPoints_) : false;
        }
        void Summarize(EarlyExercisePdeRiskScenario_* row,
                       const EarlyExercisePdeResult_& result,
                       const EarlyExercisePdeRiskRequest_& r,
                       const EarlyExercisePdeResult_* base) {
            const auto& e = result.Execution();
            row->solverStatus_ = result.Status();
            row->actualSteps_ = e.actualSteps_;
            row->completedSteps_ = e.completedSteps_;
            row->totalSolves_ = e.totalSolves_;
            row->workUnits_ = e.workUnits_;
            row->numericPayloadBytes_ = e.numericPayloadBytes_;
            row->peakWorkspaceBytes_ = e.peakWorkspaceBytes_;
            row->maxLinearBackwardError_ = Finite(e.maxLinearBackwardError_);
            row->maxObstacleViolation_ = Finite(e.maxObstacleViolation_);
            row->maxDualViolation_ = Finite(e.maxDualViolation_);
            row->maxComplementarityResidual_ = Finite(e.maxComplementarityResidual_);
            row->maxIterationChange_ = MaxIterationChange(result);
            row->failure_ = result.Failure();
            SummarizeFailure(&row->failure_);
            if (!PublishPrice(row, result, r))
                return;
            SummarizeContact(row, result, r, base);
        }
        bool Available(const EarlyExercisePdeRiskRecord_& r) {
            return r.status_ == Availability_::AVAILABLE || r.status_ == Availability_::AVAILABLE_EXERCISE_REGION;
        }
        std::optional<double> Estimate(const Vector_<EarlyExercisePdeRiskStencilEntry_>& stencil,
                                       const Vector_<EarlyExercisePdeRiskScenario_>& scenarios) {
            if (stencil.empty())
                return {};
            double sum = 0.0;
            for (const auto& entry : stencil) {
                if (!scenarios[entry.scenarioIndex_].price_)
                    return {};
                sum += entry.weight_ * *scenarios[entry.scenarioIndex_].price_;
            }
            return Finite(sum);
        }
        void EstimatePair(EarlyExercisePdeRiskRecord_* record, const Vector_<EarlyExercisePdeRiskScenario_>& scenarios, double strike) {
            record->fineEstimate_ = Estimate(record->fineStencil_, scenarios);
            record->coarseEstimate_ = Estimate(record->coarseStencil_, scenarios);
            if (record->fineEstimate_ && record->coarseEstimate_) {
                record->stepDifference_ = Finite(*record->fineEstimate_ - *record->coarseEstimate_);
                if (record->stepDifference_) {
                    const double scale = record->derivativeOrder_ == 2 ? strike : (record->parameterId_ == "Spot" ? 1.0 : 1.0 / strike);
                    record->normalizedStepDifference_ = Finite(std::abs(*record->stepDifference_) * scale);
                }
            }
        }
        struct Qualification_ {
            bool blocked_ = false, failed_ = false, boundary_ = false, exerciseSwitch_ = false, exercise_ = false;
            void Observe(const EarlyExercisePdeRiskScenario_& row, const EarlyExercisePdeRiskScenario_& base) {
                blocked_ = blocked_ || row.status_ == ScenarioStatus_::NOT_RUN_WORK_LIMIT || row.workLimitSource_ == Source_::GLOBAL;
                failed_ = failed_ || row.status_ == ScenarioStatus_::NUMERICAL_FAILURE;
                boundary_ = boundary_ || row.evaluationBoundaryIntersectsSpotStencil_.value_or(false);
                exerciseSwitch_ = exerciseSwitch_ || row.evaluationMembership_ != base.evaluationMembership_ ||
                                  row.evaluationMembership_ == EarlyExercisePdeRiskMembership_(Membership_::BOUNDARY);
                exercise_ = exercise_ && row.evaluationMembership_ == EarlyExercisePdeRiskMembership_(Membership_::EXERCISE);
            }
        };
        Qualification_ CollectQualification(const EarlyExercisePdeRiskRecord_& record, const Vector_<EarlyExercisePdeRiskScenario_>& scenarios) {
            const auto& base = scenarios[0];
            Qualification_ q{false, false, base.evaluationBoundaryIntersectsSpotStencil_.value_or(false),
                             base.evaluationMembership_ == EarlyExercisePdeRiskMembership_(Membership_::BOUNDARY),
                             base.evaluationMembership_ == EarlyExercisePdeRiskMembership_(Membership_::EXERCISE)};
            for (const auto* stencil : {&record.coarseStencil_, &record.fineStencil_})
                for (const auto& entry : *stencil) {
                    const auto& row = scenarios[entry.scenarioIndex_];
                    q.Observe(row, base);
                }
            return q;
        }
        Availability_
        FailureAdmission(const EarlyExercisePdeRiskRecord_& record, const EarlyExercisePdeRiskScenario_& base, const Qualification_& q) {
            if (!base.priceAvailable_)
                return Availability_::UNAVAILABLE_BASE_FAILED;
            if (q.blocked_)
                return Availability_::UNAVAILABLE_WORK_LIMIT;
            if (q.failed_)
                return Availability_::UNAVAILABLE_SCENARIO_FAILED;
            return record.status_.Switch();
        }
        bool FiniteEstimates(const EarlyExercisePdeRiskRecord_& record) {
            return record.fineEstimate_ && record.coarseEstimate_ && record.normalizedStepDifference_;
        }
        Availability_ ScreenAdmission(const EarlyExercisePdeRiskRecord_& record, const Qualification_& q) {
            if (record.parameterId_ == "Spot" && q.boundary_)
                return Availability_::UNAVAILABLE_NEAR_EXERCISE_BOUNDARY;
            if (q.exerciseSwitch_)
                return Availability_::UNAVAILABLE_EXERCISE_SWITCH;
            if (!FiniteEstimates(record))
                return Availability_::UNAVAILABLE_NONFINITE;
            if (*record.normalizedStepDifference_ > record.stabilityTolerance_)
                return Availability_::UNAVAILABLE_STEP_UNSTABLE;
            return q.exercise_ ? Availability_::AVAILABLE_EXERCISE_REGION : Availability_::AVAILABLE;
        }
        void Qualify(EarlyExercisePdeRiskRecord_* record, const Vector_<EarlyExercisePdeRiskScenario_>& scenarios, double strike) {
            EstimatePair(record, scenarios, strike);
            const auto q = CollectQualification(*record, scenarios);
            auto status = FailureAdmission(*record, scenarios[0], q);
            if (status == Availability_::AVAILABLE)
                status = ScreenAdmission(*record, q);
            record->status_ = EarlyExercisePdeRiskAvailability_(status);
            if (Available(*record))
                record->value_ = record->fineEstimate_;
            else
                record->reason_ = String_(record->status_.String());
        }
        template <class T_> size_t Bytes(const Vector_<T_>& v) { return Mul(v.capacity(), sizeof(T_)); }
        size_t TextBytes(const String_& s) {
            const auto ptr = reinterpret_cast<uintptr_t>(s.data()), start = reinterpret_cast<uintptr_t>(&s);
            return ptr >= start && ptr < start + sizeof(s) ? 0 : Add(s.capacity(), 1);
        }
        size_t FailureBytes(const std::optional<EarlyExercisePdeFailure_>& f) { return f ? TextBytes(f->reason_) : 0; }
        size_t SetupBytes(const Vector_<Axis_>& axes, const Vector_<EarlyExercisePdeMarket_>& markets) {
            size_t bytes = Add(Add(sizeof(axes), sizeof(markets)), Add(Bytes(axes), Bytes(markets)));
            for (const auto& axis : axes)
                bytes = Add(bytes, TextBytes(axis.id_));
            for (const auto& market : markets)
                bytes = Add(bytes, Bytes(market.carry_));
            return bytes;
        }
        size_t Payload(const EarlyExercisePdeRiskResult_& result) {
            const auto& p = result.Plan();
            size_t bytes = Add(sizeof(result), RequestDynamic(result.Request()));
            const auto& request = result.Request();
            bytes = Add(bytes, Mul(request.valuation_.market_.carry_.capacity() - request.valuation_.market_.carry_.size(),
                                   sizeof(EarlyExercisePdeCarrySegment_)));
            bytes = Add(bytes, Mul(request.valuation_.contract_.exerciseDates_.capacity() - request.valuation_.contract_.exerciseDates_.size(),
                                   sizeof(Date_)));
            bytes = Add(bytes, Mul(request.risk_.rateBumps_.capacity() - request.risk_.rateBumps_.size() +
                                       request.risk_.dividendYieldBumps_.capacity() - request.risk_.dividendYieldBumps_.size(),
                                   sizeof(double)));
            bytes = Add(bytes, result.BaseResult().Execution().numericPayloadBytes_ - sizeof(EarlyExercisePdeResult_));
            bytes = Add(bytes, FailureBytes(result.BaseResult().Failure()));
            bytes = Add(bytes, Add(Bytes(p.grid_), Add(Bytes(p.timeGrid_), Bytes(p.steps_))));
            bytes = Add(bytes, Add(TextBytes(p.planId_), TextBytes(p.interpolationPolicy_)));
            bytes = Add(bytes, Add(Bytes(p.resolvedRisk_.rateBumps_), Bytes(p.resolvedRisk_.dividendYieldBumps_)));
            bytes = Add(bytes, Add(Bytes(p.rateBumpOrigins_), Bytes(p.dividendYieldBumpOrigins_)));
            bytes = Add(bytes, Add(Bytes(result.Risks()), Bytes(result.Scenarios())));
            for (const auto& record : result.Risks()) {
                for (const auto* text :
                     {&record.riskId_, &record.parameterId_, &record.unit_, &record.reason_, &record.method_, &record.interpretation_})
                    bytes = Add(bytes, TextBytes(*text));
                bytes = Add(bytes, Add(Bytes(record.fineStencil_), Bytes(record.coarseStencil_)));
            }
            for (const auto& row : result.Scenarios()) {
                bytes = Add(bytes, Add(TextBytes(row.scenarioId_), TextBytes(row.planId_)));
                if (row.parameterId_)
                    bytes = Add(bytes, TextBytes(*row.parameterId_));
                bytes = Add(bytes, FailureBytes(row.failure_));
            }
            return bytes;
        }

        Vector_<EarlyExercisePdeMarket_> ScenarioMarkets(const EarlyExercisePdeRiskRequest_& input, Vector_<Axis_>* axes, size_t count) {
            Vector_<EarlyExercisePdeMarket_> markets;
            markets.reserve(count - 1);
            for (auto& axis : *axes) {
                axis.firstScenario_ = markets.size() + 1;
                if (axis.admission_ != Availability_::AVAILABLE)
                    continue;
                for (size_t k = 0; k < 4; ++k) {
                    auto market = input.valuation_.market_;
                    SetParameter(&market, axis, axis.value_ + AxisOffset(axis, k));
                    markets.push_back(std::move(market));
                }
            }
            return markets;
        }
        EarlyExercisePdeRiskScenario_ ScenarioRow(const Axis_& axis, size_t offsetIndex, size_t index) {
            static const char* suffix[] = {":C-", ":C+", ":F-", ":F+"};
            EarlyExercisePdeRiskScenario_ row;
            const double value = axis.value_ + AxisOffset(axis, offsetIndex);
            row.scenarioIndex_ = index;
            row.scenarioId_ = axis.id_ + suffix[offsetIndex];
            row.parameterId_ = axis.id_;
            row.segmentIndex_ = axis.segment_;
            row.parameterValue_ = value;
            row.offset_ = value - axis.value_;
            return row;
        }
        Vector_<EarlyExercisePdeRiskScenario_> ScenarioRows(const Vector_<Axis_>& axes, size_t count, const Internal::Plan_& plan) {
            Vector_<EarlyExercisePdeRiskScenario_> rows;
            rows.reserve(count);
            rows.push_back({});
            rows.back().scenarioId_ = "Base";
            for (const auto& axis : axes)
                if (axis.admission_ == Availability_::AVAILABLE)
                    for (size_t k = 0; k < 4; ++k)
                        rows.push_back(ScenarioRow(axis, k, rows.size()));
            for (auto& row : rows) {
                row.planId_ = plan.Id();
                row.epsilon_ = plan.Epsilon();
                row.plannedSteps_ = static_cast<int>(plan.Steps().size());
            }
            return rows;
        }
        Vector_<EarlyExercisePdeRiskRecord_> RiskRecords(const EarlyExercisePdeRiskRequest_& input, const Vector_<Axis_>& axes) {
            Vector_<EarlyExercisePdeRiskRecord_> records;
            records.reserve(Add(3, Mul(2, input.valuation_.market_.carry_.size())));
            for (const auto& axis : axes) {
                if (axis.kind_ == 0) {
                    if (input.risk_.delta_)
                        records.push_back(Record(input, axis, 1, records.size()));
                    if (input.risk_.gamma_)
                        records.push_back(Record(input, axis, 2, records.size()));
                } else
                    records.push_back(Record(input, axis, 1, records.size()));
            }
            return records;
        }
        void MarkNotRun(EarlyExercisePdeRiskScenario_* row, bool baseFailed) {
            row->status_ =
                EarlyExercisePdeRiskScenarioStatus_(baseFailed ? ScenarioStatus_::NOT_RUN_BASE_FAILED : ScenarioStatus_::NOT_RUN_WORK_LIMIT);
            row->workLimitSource_ = EarlyExercisePdeRiskWorkLimitSource_(baseFailed ? Source_::NONE : Source_::GLOBAL);
        }
        void ClearUnattemptedObservations(EarlyExercisePdeRiskScenario_* row) {
            row->status_ = EarlyExercisePdeRiskScenarioStatus_(ScenarioStatus_::NOT_RUN_WORK_LIMIT);
            row->solverStatus_.reset();
            row->failure_.reset();
            row->actualSteps_ = row->completedSteps_ = 0;
            row->numericPayloadBytes_ = row->peakWorkspaceBytes_ = 0;
            row->maxIterationChange_.reset();
            row->maxLinearBackwardError_.reset();
            row->maxObstacleViolation_.reset();
            row->maxDualViolation_.reset();
            row->maxComplementarityResidual_.reset();
        }
        bool UnattemptedBump(const EarlyExercisePdeRiskScenario_& row, size_t index, bool global) {
            return index != 0 && global && row.totalSolves_ == 0 && row.completedSteps_ == 0;
        }
        bool
        AccountWorkLimit(EarlyExercisePdeRiskScenario_* row, size_t remaining, size_t index, int nodes, EarlyExercisePdeRiskExecution_* execution) {
            if (row->solverStatus_ != EarlyExercisePdeStatus_(EarlyExercisePdeStatus_::Value_::WORK_LIMIT))
                return false;
            const bool global = static_cast<size_t>(nodes - 2) > remaining - row->workUnits_;
            row->workLimitSource_ = EarlyExercisePdeRiskWorkLimitSource_(global ? Source_::GLOBAL : Source_::SCENARIO);
            if (!execution->workLimitScenarioIndex_) {
                execution->workLimitScenarioIndex_ = index;
                execution->workLimitSource_ = row->workLimitSource_;
            }
            if (UnattemptedBump(*row, index, global))
                ClearUnattemptedObservations(row);
            return global;
        }
        void AccountScenario(const EarlyExercisePdeRiskScenario_& row, EarlyExercisePdeRiskExecution_* execution) {
            execution->totalSolves_ = Add(execution->totalSolves_, row.totalSolves_);
            execution->workUnits_ = Add(execution->workUnits_, row.workUnits_);
            if (row.status_ == ScenarioStatus_::NOT_RUN_WORK_LIMIT) {
                ++execution->notRunScenarios_;
                return;
            }
            ++execution->attemptedScenarios_;
            if (row.priceAvailable_)
                ++execution->successfulScenarios_;
            else
                ++execution->failedScenarios_;
        }
        EarlyExercisePdeRequest_ ScenarioRequest(const EarlyExercisePdeRiskRequest_& input, const EarlyExercisePdeMarket_* market, size_t remaining) {
            auto scenario = input.valuation_;
            if (market)
                scenario.market_ = *market;
            scenario.settings_.maxWorkUnits_ = std::min(remaining, input.valuation_.settings_.maxWorkUnits_);
            return scenario;
        }
        EarlyExercisePdeResult_ ExecuteScenario(const EarlyExercisePdeRequest_& scenario,
                                                const Internal::Plan_& plan,
                                                size_t index,
                                                const EarlyExercisePdeRiskInternal::TestControl_* control) {
            Internal::RollbackControl_ solverControl;
            const bool controlled = control && control->scenarioIndex_ == index;
            if (controlled)
                solverControl = {control->failFirstSolve_, control->extraFirstSolve_, control->nonfinitePrice_};
            if (control && control->rollbacksStarted_)
                ++*control->rollbacksStarted_;
            return PriceCall([&]() { return Internal::RollbackForRisk(scenario, plan, controlled ? &solverControl : nullptr); });
        }
        void ObserveMemory(const EarlyExercisePdeRiskResult_& result,
                           EarlyExercisePdeRiskExecution_* execution,
                           const EarlyExercisePdeRequest_& scenario,
                           const EarlyExercisePdeResult_& observed,
                           bool base,
                           const Internal::Plan_& plan,
                           size_t setupBytes) {
            size_t temporary = Add(Add(setupBytes, plan.StorageBytes()),
                                   Add(sizeof(scenario), Add(Bytes(scenario.market_.carry_), Bytes(scenario.contract_.exerciseDates_))));
            temporary = Add(temporary, observed.Execution().peakWorkspaceBytes_);
            temporary = Add(temporary, base ? sizeof(observed) : Add(observed.Execution().numericPayloadBytes_, FailureBytes(observed.Failure())));
            execution->peakWorkspaceBytes_ = std::max(execution->peakWorkspaceBytes_, temporary);
            execution->peakNativeBytes_ = std::max(execution->peakNativeBytes_, Add(Payload(result), temporary));
        }
        Status_ AggregateStatus(bool baseFailed, bool globalStop, bool complete) {
            if (baseFailed)
                return Status_::BASE_FAILED;
            if (globalStop)
                return Status_::WORK_LIMIT;
            return complete ? Status_::COMPLETE : Status_::PARTIAL;
        }
    } // namespace

    void ValidateEarlyExercisePdeRiskSettings(const EarlyExercisePdeRiskSettings_& s) {
        Check(s.delta_ || s.gamma_ || s.vega_ || s.carryRisk_, "risk.switches", 0, "at least one risk must be requested");
        if (s.spotBump_) {
            Positive(*s.spotBump_, "risk.spot_bump");
            Positive(2.0 * *s.spotBump_, "risk.spot_bump.coarse");
        }
        Bump(s.volatilityBump_, "risk.volatility_bump");
        for (size_t i = 0; i < s.rateBumps_.size(); ++i)
            Bump(s.rateBumps_[i], Indexed("risk.rate_bumps", i));
        for (size_t i = 0; i < s.dividendYieldBumps_.size(); ++i)
            Bump(s.dividendYieldBumps_[i], Indexed("risk.dividend_yield_bumps", i));
        Positive(s.deltaStabilityTolerance_, "risk.delta_stability_tolerance");
        Positive(s.gammaStabilityTolerance_, "risk.gamma_stability_tolerance");
        Positive(s.vegaStabilityTolerance_, "risk.vega_stability_tolerance");
        Positive(s.rateStabilityTolerance_, "risk.rate_stability_tolerance");
        Positive(s.dividendYieldStabilityTolerance_, "risk.dividend_yield_stability_tolerance");
    }
    void ValidateEarlyExercisePdeRiskRequest(const EarlyExercisePdeRiskRequest_& r) {
        ValidateEarlyExercisePdeRiskSettings(r.risk_);
        PriceCall([&]() { ValidateEarlyExercisePdeRequest(r.valuation_); });
        ValidateBumps(r);
    }
    struct EarlyExercisePdeRiskInternal::Evaluator_::State_ {
        const EarlyExercisePdeRiskRequest_& input_;
        const Internal::Plan_& plan_;
        const Vector_<Axis_>& axes_;
        const Vector_<EarlyExercisePdeMarket_>& markets_;
        const Envelope_& envelope_;
        const TestControl_* control_;
        bool globalStop_ = false, baseFailed_ = false;
    };
    void EarlyExercisePdeRiskInternal::Evaluator_::Initialize(EarlyExercisePdeRiskResult_* result, const State_& state) {
        const size_t count = state.markets_.size() + 1;
        result->request_ = state.input_;
        result->scenarios_ = ScenarioRows(state.axes_, count, state.plan_);
        result->risks_ = RiskRecords(state.input_, state.axes_);
        result->plan_ = ExportPlan(state.input_, state.plan_, count, state.envelope_);
        auto& e = result->execution_;
        e.plannedScenarios_ = count;
        e.plannedNumericPayloadBytes_ = state.envelope_.payload_;
        e.plannedWorkspaceBytes_ = state.envelope_.workspace_;
        e.plannedPeakNativeBytes_ = state.envelope_.peak_;
        e.peakWorkspaceBytes_ = Add(SetupBytes(state.axes_, state.markets_), state.plan_.PeakTemporaryBytes());
        e.peakNativeBytes_ = e.peakWorkspaceBytes_;
    }
    void EarlyExercisePdeRiskInternal::Evaluator_::RunScenario(EarlyExercisePdeRiskResult_* result, State_* state, size_t index) {
        auto& row = result->scenarios_[index];
        auto& e = result->execution_;
        const size_t remaining = state->input_.risk_.maxWorkUnits_ - e.workUnits_;
        const auto scenario = ScenarioRequest(state->input_, index == 0 ? nullptr : &state->markets_[index - 1], remaining);
        auto evaluated = ExecuteScenario(scenario, state->plan_, index, state->control_);
        Summarize(&row, evaluated, state->input_, index == 0 ? nullptr : &*result->base_);
        state->globalStop_ = AccountWorkLimit(&row, remaining, index, scenario.settings_.gridPoints_, &e);
        AccountScenario(row, &e);
        if (index == 0) {
            state->baseFailed_ = !row.priceAvailable_;
            result->base_ = std::move(evaluated);
        }
        const auto& observed = index == 0 ? result->BaseResult() : evaluated;
        ObserveMemory(*result, &e, scenario, observed, index == 0, state->plan_, SetupBytes(state->axes_, state->markets_));
    }
    void EarlyExercisePdeRiskInternal::Evaluator_::RunAll(EarlyExercisePdeRiskResult_* result, State_* state) {
        for (size_t i = 0; i < result->scenarios_.size(); ++i) {
            if (state->baseFailed_ || state->globalStop_) {
                MarkNotRun(&result->scenarios_[i], state->baseFailed_);
                ++result->execution_.notRunScenarios_;
            } else
                RunScenario(result, state, i);
        }
    }
    void EarlyExercisePdeRiskInternal::Evaluator_::Finish(EarlyExercisePdeRiskResult_* result, const State_& state) {
        for (auto& record : result->risks_)
            Qualify(&record, result->scenarios_, state.input_.valuation_.contract_.strike_);
        const bool complete = std::all_of(result->risks_.begin(), result->risks_.end(), Available);
        result->status_ = EarlyExercisePdeRiskStatus_(AggregateStatus(state.baseFailed_, state.globalStop_, complete));
        auto& e = result->execution_;
        e.numericPayloadBytes_ = Payload(*result);
        Budget(e.numericPayloadBytes_, state.envelope_.payload_, "retained_storage_layout");
        e.peakNativeBytes_ =
            std::max(e.peakNativeBytes_, Add(e.numericPayloadBytes_, Add(SetupBytes(state.axes_, state.markets_), state.plan_.StorageBytes())));
        Budget(e.peakWorkspaceBytes_, state.envelope_.workspace_, "temporary_storage_layout");
        Budget(e.peakNativeBytes_, state.envelope_.peak_, "peak_storage_layout");
    }
    EarlyExercisePdeRiskResult_ EarlyExercisePdeRiskInternal::Evaluator_::Run(const EarlyExercisePdeRiskRequest_& input,
                                                                              const TestControl_* control) {
        ValidateEarlyExercisePdeRiskRequest(input);
        const size_t count = ScenarioCount(input);
        Budget(count, input.risk_.maxScenarios_, "risk.max_scenarios");
        Budget(SetupEnvelope(input, count), input.risk_.workspaceBudgetBytes_, "risk.workspace_budget_bytes.setup");
        auto axes = Axes(input);
        const auto markets = ScenarioMarkets(input, &axes, count);
        const auto layout = PriceCall([&]() { return Internal::PreviewPlan(input.valuation_, markets); });
        const auto envelope = Admit(input, count, layout);
        const auto plan = PriceCall([&]() { return Internal::BuildPlan(input.valuation_, markets); });
        State_ state{input, plan, axes, markets, envelope, control};
        EarlyExercisePdeRiskResult_ result;
        Initialize(&result, state);
        RunAll(&result, &state);
        Finish(&result, state);
        return result;
    }
    EarlyExercisePdeRiskResult_ EvaluateEarlyExercisePdeRisk(const EarlyExercisePdeRiskRequest_& input) {
        return EarlyExercisePdeRiskInternal::Evaluator_::Run(input, nullptr);
    }
    EarlyExercisePdeRiskResult_ EarlyExercisePdeRiskInternal::Evaluate(const EarlyExercisePdeRiskRequest_& input, const TestControl_& control) {
        return Evaluator_::Run(input, &control);
    }
    size_t EarlyExercisePdeRiskInternal::CheckedAdd(size_t a, size_t b) { return Add(a, b); }
    size_t EarlyExercisePdeRiskInternal::CheckedMultiply(size_t a, size_t b) { return Mul(a, b); }
    bool EarlyExercisePdeRiskResult_::PriceAvailable() const { return !scenarios_.empty() && scenarios_[0].priceAvailable_; }
    double EarlyExercisePdeRiskResult_::Price() const {
        REQUIRE(PriceAvailable(),
                String_("EarlyExercisePdeRiskPriceUnavailable; aggregate=") + status_.String() + "; base=" + base_->Status().String());
        return *scenarios_[0].price_;
    }
    double EarlyExercisePdeRiskResult_::RiskValue(const String_& riskId) const {
        for (const auto& record : risks_)
            if (riskId.size() == record.riskId_.size() && std::memcmp(riskId.data(), record.riskId_.data(), riskId.size()) == 0) {
                REQUIRE(record.value_,
                        String_("EarlyExercisePdeRiskValueUnavailable; ") + riskId + "; " + record.status_.String() + "; " + record.reason_);
                return *record.value_;
            }
        String_ requested;
        for (const auto& record : risks_)
            requested += (requested.empty() ? "" : ",") + record.riskId_;
        THROW("EarlyExercisePdeRiskUnknownRiskId; " + riskId + "; requested=" + requested);
    }
} // namespace Dal::PDE
