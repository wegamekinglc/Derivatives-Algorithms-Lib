#pragma once

#include <algorithm>
#include <array>
#include <type_traits>

#include <dal/model/xccygaussiandata.hpp>
#include <dal/model/xccygaussianmath.hpp>

namespace Dal::AAD {
    template <class T_> struct XccyGaussianState_ {
        Date_ date_;
        std::array<T_, 5> values_ = XccyGaussianDetail::ZeroState<T_>();
    };

    template <class T_ = double> class XccyGaussian_ final : public Model_<T_> {
        struct Curve_ {
            Handle_<XccyGaussianCurveData_> data_;
            Vector_<T_> logDF_;
        };
        struct Deterministics_ {
            T_ phiDomestic_, phiForeign_, fxVariance_;
        };
        struct Step_ {
            XccyGaussianDetail::Transition_<T_> transition_;
            Matrix_<T_> lower_;
        };

        Handle_<XccyGaussianMarket_> market_;
        Handle_<XccyGaussianDynamics_> dynamics_;
        T_ spot_;
        Vector_<Curve_> curves_;
        std::array<Vector_<T_>, 3> sigmas_;
        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;
        Vector_<String_> assetNames_;
        Vector_<Date_> grid_;
        Vector_<Step_> steps_;
        Vector_<Deterministics_> deterministics_;
        Vector_<> cachedParameterValues_;

        static void PlanCheck(bool valid, const String_& field, const char* expected) {
            if (!valid)
                XccyGaussianDetail::Check(false, "InvalidCalibrationPlan", "/model/" + field, expected);
        }
        static void ContextCheck(bool valid, const String_& field, const char* expected) {
            if (!valid)
                XccyGaussianDetail::Check(false, "CalibrationContextMismatch", "/model/" + field, expected);
        }
        [[nodiscard]] static T_ Positive(const T_& value, const Date_& date) {
            XccyGaussianDetail::CheckAtDate(std::isfinite(Value(value)) && Value(value) > 0., "positiveValue", date, Value(value));
            return value;
        }
        void RequireFreshNumeric() const {
            ContextCheck(cachedParameterValues_.size() == parameters_.size(), "refresh", "prepared numeric content");
            for (size_t i = 0; i < parameters_.size(); ++i)
                ContextCheck(cachedParameterValues_[i] == Value(*parameters_[i]), "refresh",
                             "RefreshNumeric after changing parameters or starting a new AAD recording");
        }
        [[noreturn]] static void GenericUnsupported() {
            XccyGaussianDetail::Check(false, "MCCrossCurrencyUnsupported", "/model/observations", "dedicated XCCY observation and MC adapters");
            THROW("unreachable XCCY admission");
        }

        [[nodiscard]] double Time(const Date_& date) const {
            PlanCheck(date.IsValid() && date >= market_->valuationTime_.Date() && date <= dynamics_->bucketEndDates_.back(), "dynamics/coverage",
                      "date within explicit volatility buckets starting at V");
            return (date - market_->valuationTime_.Date()) / 365.;
        }

        [[nodiscard]] T_ LogDF(size_t index, const Date_& date) const {
            const auto& curve = curves_[index];
            const auto& dates = curve.data_->nodeDates_;
            ContextCheck(date >= dates.front() && date <= dates.back(), "market/" + curve.data_->route_ + "/coverage",
                         "date within snapshot coverage");
            const auto found = std::lower_bound(dates.begin(), dates.end(), date);
            const size_t upper = static_cast<size_t>(found - dates.begin());
            if (*found == date)
                return upper == 0 ? T_(0.) : curve.logDF_[upper];
            const double weight = static_cast<double>(date - dates[upper - 1]) / (dates[upper] - dates[upper - 1]);
            const T_ lowerValue = upper == 1 ? T_(0.) : curve.logDF_[upper - 1];
            return lowerValue * (1. - weight) + curve.logDF_[upper] * weight;
        }

        [[nodiscard]] bool IsForeign(const String_& currency) const {
            ContextCheck(currency == market_->pair_.domestic_.String() || currency == market_->pair_.foreign_.String(), "market/currency",
                         "configured domestic or foreign currency");
            return currency == market_->pair_.foreign_.String();
        }
        [[nodiscard]] T_ EffectiveLogDF(bool foreign, const Date_& date) const { return foreign ? LogDF(1, date) - LogDF(2, date) : LogDF(0, date); }

        [[nodiscard]] Deterministics_ CalculateDeterministics(const Date_& date) const {
            const auto moments = Transition(market_->valuationTime_.Date(), date);
            return {-EffectiveLogDF(false, date) + .5 * moments.covariance_(2, 2), -EffectiveLogDF(true, date) + .5 * moments.covariance_(3, 3),
                    moments.covariance_(4, 4)};
        }
        [[nodiscard]] Deterministics_ Deterministics(const Date_& date) const {
            const auto found = std::lower_bound(grid_.begin(), grid_.end(), date);
            if (found != grid_.end() && *found == date && deterministics_.size() == grid_.size()) {
                RequireFreshNumeric();
                return deterministics_[static_cast<size_t>(found - grid_.begin())];
            }
            return CalculateDeterministics(date);
        }

        [[nodiscard]] T_ Phi(bool foreign, const Date_& date) const {
            const auto value = Deterministics(date);
            return foreign ? value.phiForeign_ : value.phiDomestic_;
        }

        void AddCurve(const Handle_<XccyGaussianCurveData_>& data) {
            Curve_ curve{data, {}};
            for (const double value : data->logDF_)
                curve.logDF_.push_back(T_(value));
            curves_.push_back(std::move(curve));
        }

        void RegisterParameters() {
            parameters_.clear();
            parameterLabels_.clear();
            parameters_.push_back(&spot_);
            parameterLabels_.push_back("fxspot:" + assetNames_.front());
            for (auto& curve : curves_)
                for (size_t i = 1; i < curve.logDF_.size(); ++i) {
                    parameters_.push_back(&curve.logDF_[i]);
                    parameterLabels_.push_back("logdf:" + curve.data_->currency_ + ":" + curve.data_->route_ + ":" +
                                               Date::ToString(curve.data_->nodeDates_[i]));
                }
            const Vector_<String_> names{market_->pair_.domestic_.String(), market_->pair_.foreign_.String(), assetNames_.front()};
            for (size_t driver = 0; driver < 3; ++driver)
                for (size_t i = 0; i < sigmas_[driver].size(); ++i) {
                    parameters_.push_back(&sigmas_[driver][i]);
                    parameterLabels_.push_back("sigma:" + names[driver] + ":" + Date::ToString(dynamics_->bucketEndDates_[i]));
                }
        }

        void ValidateNumericParameters() const {
            PlanCheck(std::isfinite(Value(spot_)) && Value(spot_) > 0., "market/fxSpot", "positive finite spot");
            for (const auto& curve : curves_)
                for (const auto& value : curve.logDF_) {
                    const double discount = std::exp(Value(value));
                    PlanCheck(std::isfinite(discount) && discount > 0., "market/logDF", "positive finite discounts");
                }
            for (const auto& driver : sigmas_)
                for (const auto& value : driver)
                    ValidateSigma(Value(value));
        }

        static void ValidateSigma(double value) {
            PlanCheck(std::isfinite(value) && value >= 0., "dynamics/sigma", "finite nonnegative sigma");
            if constexpr (!std::is_same_v<T_, double>)
                XccyGaussianDetail::Check(value > 0., "DerivativeUnavailable", "/model/dynamics/sigma",
                                          "interior positive sigma for AAD; zero volatility remains value supported");
        }

        XccyGaussian_(const Handle_<XccyGaussianMarket_>& market, const Handle_<XccyGaussianDynamics_>& dynamics)
            : market_(market), dynamics_(dynamics), spot_(market->fxSpot_),
              assetNames_{String_(market->pair_.foreign_.String()) + "/" + market->pair_.domestic_.String()} {
            AddCurve(market_->domestic_);
            AddCurve(market_->foreign_);
            AddCurve(market_->basis_);
            for (const auto& projection : market_->projections_)
                AddCurve(projection);
            const std::array<Vector_<>, 3> values{dynamics_->domesticSigma_, dynamics_->foreignSigma_, dynamics_->fxSigma_};
            for (size_t driver = 0; driver < 3; ++driver)
                for (const double value : values[driver])
                    sigmas_[driver].push_back(T_(value));
            RegisterParameters();
        }

        void ValidateState(const XccyGaussianState_<T_>& state) const {
            (void)Time(state.date_);
            for (const auto& value : state.values_)
                XccyGaussianDetail::CheckAtDate(std::isfinite(Value(value)), "state", state.date_, Value(value));
        }

        [[nodiscard]] static Matrix_<T_> FactorAtDate(const Matrix_<T_>& covariance, const Date_& from, const Date_& to) {
            try {
                return XccyGaussianDetail::ScaledFactor(covariance);
            } catch (const CalibrationError_& error) {
                XccyGaussianDetail::RethrowAtDate(error, from, to);
            }
        }

        static void ValidateMoments(const XccyGaussianDetail::Transition_<T_>& transition, const Date_& date) {
            for (int i = 0; i < 5; ++i) {
                XccyGaussianDetail::CheckAtDate(std::isfinite(Value(transition.shift_[i])), "mean", date, Value(transition.shift_[i]));
                for (int j = 0; j < 5; ++j)
                    XccyGaussianDetail::CheckAtDate(std::isfinite(Value(transition.covariance_(i, j))), "covariance", date,
                                                    Value(transition.covariance_(i, j)));
            }
        }

    public:
        explicit XccyGaussian_(const XccyGaussianModelData_& data) : XccyGaussian_(data.market_, data.dynamics_) {}
        XccyGaussian_(const XccyGaussian_& other) : XccyGaussian_(other.market_, other.dynamics_) {
            for (size_t i = 0; i < parameters_.size(); ++i)
                *parameters_[i] = *other.parameters_[i];
            if (!other.grid_.empty())
                Prepare(other.grid_);
        }

        [[nodiscard]] static String_ LayoutVersion() { return "dal.xccy-gaussian-exact/1"; }
        [[nodiscard]] static Vector_<String_> InnovationSlots() { return {"xd", "xf", "Id", "If", "Z"}; }
        [[nodiscard]] Vector_<String_> FactorNames() const { return market_->FactorNames(); }
        [[nodiscard]] size_t NumFactors() const override { return 3; }
        [[nodiscard]] bool SupportsBrownianBridge() const override { return false; }
        [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return market_->valuationTime_.Date(); }
        [[nodiscard]] const Vector_<String_>& AssetNames() const override { return assetNames_; }
        [[nodiscard]] size_t SimDim() const override { return steps_.size() * 5; }
        [[nodiscard]] const Vector_<Date_>& Grid() const { return grid_; }
        [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }
        [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
            if (parameter >= parameters_.size() || !std::isfinite(value))
                return false;
            if (parameter == 0)
                return value > 0.;
            if (parameterLabels_[parameter].find("sigma:") == 0)
                return value >= 0.;
            return std::isfinite(std::exp(value)) && std::exp(value) > 0.;
        }
        [[nodiscard]] XccyGaussianState_<T_> InitialState() const { return {market_->valuationTime_.Date()}; }

        [[nodiscard]] XccyGaussianDetail::Transition_<T_> Transition(const Date_& from, const Date_& to) const {
            ValidateNumericParameters();
            (void)Time(from);
            (void)Time(to);
            PlanCheck(from <= to, "transition/interval", "from <= to");
            XccyGaussianDetail::Transition_<T_> result;
            for (int i = 0; i < 5; ++i)
                result.flow_(i, i) = 1.;
            try {
                Date_ current = from;
                while (current < to) {
                    const auto bucket = std::upper_bound(dynamics_->bucketEndDates_.begin(), dynamics_->bucketEndDates_.end(), current);
                    const size_t index = static_cast<size_t>(bucket - dynamics_->bucketEndDates_.begin());
                    const Date_ next = std::min(to, *bucket);
                    const std::array<T_, 3> sigma{sigmas_[0][index], sigmas_[1][index], sigmas_[2][index]};
                    const auto piece = XccyGaussianDetail::ConstantTransition(dynamics_->domesticA_, dynamics_->foreignA_, sigma,
                                                                              dynamics_->correlation_, (next - current) / 365.);
                    result = XccyGaussianDetail::Compose(result, piece);
                    current = next;
                }
                ValidateMoments(result, to);
            } catch (const CalibrationError_& error) {
                XccyGaussianDetail::RethrowAtDate(error, from, to);
            }
            return result;
        }

        void Prepare(const Vector_<Date_>& sampleDates) {
            ValidateNumericParameters();
            Vector_<Date_> dates = sampleDates;
            dates.push_back(market_->valuationTime_.Date());
            std::sort(dates.begin(), dates.end());
            dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
            for (const auto& date : dates)
                (void)Time(date);
            const Date_ horizon = dates.back();
            for (const auto& date : dynamics_->bucketEndDates_)
                if (date < horizon)
                    dates.push_back(date);
            std::sort(dates.begin(), dates.end());
            dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
            Vector_<Step_> steps;
            Vector_<Deterministics_> deterministics;
            for (size_t i = 0; i < dates.size(); ++i) {
                deterministics.push_back(CalculateDeterministics(dates[i]));
                if (i > 0) {
                    auto transition = Transition(dates[i - 1], dates[i]);
                    auto lower = FactorAtDate(transition.covariance_, dates[i - 1], dates[i]);
                    steps.push_back({std::move(transition), std::move(lower)});
                }
            }
            Vector_<> parameterValues;
            for (const auto* parameter : parameters_)
                parameterValues.push_back(Value(*parameter));
            grid_ = std::move(dates);
            steps_ = std::move(steps);
            deterministics_ = std::move(deterministics);
            cachedParameterValues_ = std::move(parameterValues);
        }

        void RefreshNumeric() { Prepare(grid_); }

        [[nodiscard]] Vector_<XccyGaussianState_<T_>> GenerateStates(const Vector_<>& normals) const {
            PlanCheck(!grid_.empty() && normals.size() == SimDim(), "innovations", "prepared grid and exactly five normals per advancing interval");
            RequireFreshNumeric();
            for (const double value : normals)
                XccyGaussianDetail::NumericalCheck(std::isfinite(value), "innovation");
            Vector_<XccyGaussianState_<T_>> result{InitialState()};
            for (size_t step = 0; step < steps_.size(); ++step) {
                XccyGaussianState_<T_> next{grid_[step + 1], steps_[step].transition_.shift_};
                for (int i = 0; i < 5; ++i)
                    for (int j = 0; j < 5; ++j)
                        next.values_[i] +=
                            steps_[step].transition_.flow_(i, j) * result.back().values_[j] + steps_[step].lower_(i, j) * normals[5 * step + j];
                ValidateState(next);
                result.push_back(std::move(next));
            }
            return result;
        }

        [[nodiscard]] T_ DomesticNumeraire(const XccyGaussianState_<T_>& state) const {
            ValidateState(state);
            return Positive(Dal::exp(Phi(false, state.date_) + state.values_[2]), state.date_);
        }
        [[nodiscard]] T_ ForeignNumeraire(const XccyGaussianState_<T_>& state) const {
            ValidateState(state);
            return Positive(Dal::exp(Phi(true, state.date_) + state.values_[3]), state.date_);
        }
        [[nodiscard]] T_ Spot(const XccyGaussianState_<T_>& state) const {
            ValidateState(state);
            const auto deterministic = Deterministics(state.date_);
            return Positive(spot_ * Dal::exp(deterministic.phiDomestic_ - deterministic.phiForeign_ + state.values_[2] - state.values_[3] -
                                             .5 * deterministic.fxVariance_ + state.values_[4]),
                            state.date_);
        }
        [[nodiscard]] T_ Bond(const String_& currency, const XccyGaussianState_<T_>& state, const Date_& maturity) const {
            ValidateState(state);
            const bool foreign = IsForeign(currency);
            const auto moments = Transition(state.date_, maturity);
            const int integralSlot = foreign ? 3 : 2;
            const int stateSlot = foreign ? 1 : 0;
            const double a = foreign ? dynamics_->foreignA_ : dynamics_->domesticA_;
            return Positive(Dal::exp(-(Phi(foreign, maturity) - Phi(foreign, state.date_)) -
                                     static_cast<double>(XccyGaussianDetail::H(a, (maturity - state.date_) / 365.)) * state.values_[stateSlot] +
                                     .5 * moments.covariance_(integralSlot, integralSlot)),
                            maturity);
        }
        [[nodiscard]] T_ ForeignOwnBond(const XccyGaussianState_<T_>& state, const Date_& maturity) const {
            return Positive(Bond(market_->pair_.foreign_.String(), state, maturity) * Dal::exp(LogDF(2, maturity) - LogDF(2, state.date_)), maturity);
        }
        [[nodiscard]] T_
        ProjectionBond(const String_& currency, const String_& tenor, const XccyGaussianState_<T_>& state, const Date_& maturity) const {
            const bool foreign = IsForeign(currency);
            for (size_t i = 3; i < curves_.size(); ++i)
                if (curves_[i].data_->currency_ == currency && curves_[i].data_->route_ == tenor)
                    return Positive(Bond(currency, state, maturity) * Dal::exp(LogDF(i, maturity) - EffectiveLogDF(foreign, maturity) -
                                                                               LogDF(i, state.date_) + EffectiveLogDF(foreign, state.date_)),
                                    maturity);
            ContextCheck(false, "market/projection/" + currency + "/" + tenor, "explicit projection route");
            return T_(0.);
        }
        [[nodiscard]] T_ ForwardRate(const String_& currency,
                                     const String_& tenor,
                                     const XccyGaussianState_<T_>& state,
                                     const Date_& start,
                                     const Date_& end,
                                     double accrual) const {
            PlanCheck(state.date_ <= start && start < end && std::isfinite(accrual) && accrual > 0., "projection/geometry",
                      "fixing <= start < end and positive source accrual");
            const T_ rate = (ProjectionBond(currency, tenor, state, start) / ProjectionBond(currency, tenor, state, end) - 1.) / accrual;
            XccyGaussianDetail::NumericalCheck(std::isfinite(Value(rate)), "forwardRate");
            return rate;
        }
        [[nodiscard]] T_ FxForward(const XccyGaussianState_<T_>& state, const Date_& maturity) const {
            return Positive(Spot(state) * Bond(market_->pair_.foreign_.String(), state, maturity) /
                                Bond(market_->pair_.domestic_.String(), state, maturity),
                            maturity);
        }

        void Allocate(const Vector_<>&, const Vector_<SampleDef_>&) override { GenericUnsupported(); }
        void Init(const Vector_<>&, const Vector_<SampleDef_>&) override { GenericUnsupported(); }
        void GeneratePath(const Vector_<>&, Scenario_<T_>*) const override { GenericUnsupported(); }
        [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override { return std::make_unique<XccyGaussian_>(*this); }
    };
} // namespace Dal::AAD

namespace Dal {
    template <class T_ = double> [[nodiscard]] std::unique_ptr<AAD::XccyGaussian_<T_>> NewXccyGaussianModel(const XccyGaussianModelData_& data) {
        return std::make_unique<AAD::XccyGaussian_<T_>>(data);
    }
} // namespace Dal
