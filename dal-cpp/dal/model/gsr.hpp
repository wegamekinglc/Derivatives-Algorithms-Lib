//
// Created by Codex on 2026/9/28.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>
#include <variant>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/logdfinterp.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/model/gsrmultidata.hpp>
#include <dal/platform/consts.hpp>
#include <dal/protocol/conventions.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>

namespace Dal::AAD {
    template <class T_ = double> class GSR_ final : public Model_<T_> {
        struct Step_ {
            Matrix_<T_> lower_;
            Vector_<T_> loading_;
            Vector_<T_> discountNormals_;
            T_ a_ = T_(0.0);
            bool advances_ = false;
            Matrix_<T_> hybridLower_;
            Vector_<T_> hybridNormals_;
        };

        struct Bond_ {
            Vector_<T_> loading_;
            T_ intercept_;
        };
        struct DiscountObservation_ {
            Bond_ start_, end_;
        };
        struct LiborObservation_ {
            Bond_ start_, end_;
            T_ scale_;
            double accrual_;
        };
        struct FixedCoupon_ {
            Bond_ payment_;
            double accrual_;
        };
        struct FloatCoupon_ {
            LiborObservation_ fixing_;
            Bond_ payment_;
            double accrual_;
        };
        struct SwapObservation_ {
            Vector_<FixedCoupon_> fixed_;
            Vector_<FloatCoupon_> floating_;
        };
        using Observation_ = std::variant<DiscountObservation_, LiborObservation_, SwapObservation_>;

        Date_ evaluationDate_;
        String_ currency_;
        Vector_<Date_> nodeDates_;
        Vector_<String_> projectionTenors_;
        std::shared_ptr<const LogDfInterpolation_> interpolation_;
        Vector_<T_> discountLogDF_;
        Vector_<Vector_<T_>> projectionLogDF_;
        Vector_<> gTimes_;
        Vector_<> hTimes_;
        Vector_<String_> factorNames_;
        Matrix_<> correlations_;
        Matrix_<> factorLower_;
        Matrix_<> factorLowerInverse_;
        Vector_<Vector_<T_>> gValues_;
        Vector_<Vector_<T_>> hValues_;
        bool legacy_ = false;
        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;
        Vector_<> productTimeLine_;
        Vector_<Step_> steps_;
        Vector_<Vector_<Observation_>> observations_;
        //  Discount bonds P(t, maturity) prepared per sample from SampleDef_::discountMats_
        //  (delayed PAYS ... ON payments)
        Vector_<Vector_<Bond_>> discountBonds_;

        [[nodiscard]] double Time(const Date_& date) const {
            REQUIRE(date >= evaluationDate_, "InvalidGSRDate: date precedes evaluation date");
            REQUIRE(date <= nodeDates_.back(), "InvalidGSRDate: date exceeds last curve node");
            return (date - evaluationDate_) / DAYS_PER_YEAR;
        }

        [[nodiscard]] Date_ DateAt(double time) const {
            const double days = time * DAYS_PER_YEAR;
            const auto rounded = static_cast<int>(std::llround(days));
            REQUIRE(std::abs(days - rounded) <= 1e-7, "InvalidGSRTimeline: expected whole calendar days on ACT/365 axis");
            return evaluationDate_.AddDays(rounded);
        }

        [[nodiscard]] T_ LogDF(double time, const Vector_<T_>& values) const { return interpolation_->Evaluate(values, time); }
        [[nodiscard]] T_ LogDF(double time) const { return LogDF(time, discountLogDF_); }
        [[nodiscard]] T_ LogDF(const Date_& date) const { return LogDF(Time(date)); }

        [[nodiscard]] const Vector_<T_>& Projection(const String_& tenor) const {
            const int months = PeriodLength_(tenor).Months();
            for (size_t i = 0; i < projectionTenors_.size(); ++i)
                if (PeriodLength_(projectionTenors_[i]).Months() == months)
                    return projectionLogDF_[i];
            return discountLogDF_;
        }

        [[nodiscard]] static size_t PieceAt(const Vector_<>& times, double time) {
            const auto upper = std::upper_bound(times.begin(), times.end(), time);
            return upper == times.begin() ? 0 : static_cast<size_t>(upper - times.begin() - 1);
        }

        [[nodiscard]] T_ G(size_t factor, double time) const { return gValues_[factor][PieceAt(gTimes_, time)]; }
        [[nodiscard]] T_ H(size_t factor, double time) const { return hValues_[factor][PieceAt(hTimes_, time)]; }

        [[nodiscard]] static Matrix_<> SingleRow(const Vector_<>& values) {
            Matrix_<> result(1, static_cast<int>(values.size()));
            for (size_t i = 0; i < values.size(); ++i)
                result(0, static_cast<int>(i)) = values[i];
            return result;
        }

        [[nodiscard]] static T_ Dot(const Vector_<T_>& lhs, const Vector_<T_>& rhs) {
            T_ result(0.0);
            for (size_t i = 0; i < lhs.size(); ++i)
                result += lhs[i] * rhs[i];
            return result;
        }

        [[nodiscard]] static T_ Quadratic(const Vector_<T_>& vector, const Matrix_<T_>& matrix) {
            T_ result(0.0);
            for (int i = 0; i < matrix.Rows(); ++i)
                for (int j = 0; j < matrix.Cols(); ++j)
                    result += vector[i] * matrix(i, j) * vector[j];
            return result;
        }

        // Empty when the factor Cholesky has a zero pivot: singular kernels keep working standalone.
        [[nodiscard]] static Matrix_<> TryLowerInverse(const Matrix_<>& lower) {
            for (int i = 0; i < lower.Rows(); ++i)
                if (!(lower(i, i) > 1e-14))
                    return {};
            return LowerInverse(lower);
        }

        [[nodiscard]] static Matrix_<> LowerInverse(const Matrix_<>& lower) {
            const int n = lower.Rows();
            Matrix_<> inverse(n, n, 0.0);
            for (int col = 0; col < n; ++col) {
                inverse(col, col) = 1.0 / lower(col, col);
                for (int row = col + 1; row < n; ++row) {
                    double value = 0.0;
                    for (int k = col; k < row; ++k)
                        value -= lower(row, k) * inverse(k, col);
                    inverse(row, col) = value / lower(row, row);
                }
            }
            return inverse;
        }

        [[nodiscard]] Vector_<> IntervalKnots(double from, double to) const {
            Vector_<> knots{from, to};
            for (const auto& time : gTimes_)
                if (from < time && time < to)
                    knots.push_back(time);
            for (const auto& time : hTimes_)
                if (from < time && time < to)
                    knots.push_back(time);
            std::sort(knots.begin(), knots.end());
            knots.erase(std::unique(knots.begin(), knots.end()), knots.end());
            return knots;
        }

        [[nodiscard]] Matrix_<T_> StateVariance(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: state variance requires from <= to");
            const int n = static_cast<int>(NumFactors());
            Matrix_<T_> result(n, n, T_(0.0));
            const auto knots = IntervalKnots(from, to);
            for (size_t piece = 1; piece < knots.size(); ++piece)
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        result(i, j) += G(i, knots[piece - 1]) * G(j, knots[piece - 1]) * correlations_(i, j) * (knots[piece] - knots[piece - 1]);
            return result;
        }

        [[nodiscard]] Vector_<T_> BondLoading(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: bond loading requires from <= to");
            Vector_<T_> result(NumFactors(), T_(0.0));
            const auto knots = IntervalKnots(from, to);
            for (size_t i = 1; i < knots.size(); ++i)
                for (size_t factor = 0; factor < NumFactors(); ++factor)
                    result[factor] += H(factor, knots[i - 1]) * (knots[i] - knots[i - 1]);
            return result;
        }

        [[nodiscard]] Vector_<T_> StateDiscountCovariance(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: covariance requires from <= to");
            const size_t n = NumFactors();
            Vector_<T_> result(n, T_(0.0));
            Vector_<T_> loading(n, T_(0.0));
            const auto knots = IntervalKnots(from, to);
            for (size_t piece = knots.size(); piece-- > 1;) {
                const double width = knots[piece] - knots[piece - 1];
                for (size_t i = 0; i < n; ++i)
                    for (size_t j = 0; j < n; ++j)
                        result[i] += G(i, knots[piece - 1]) * G(j, knots[piece - 1]) * correlations_(static_cast<int>(i), static_cast<int>(j)) *
                                     (width * loading[j] + 0.5 * width * width * H(j, knots[piece - 1]));
                for (size_t i = 0; i < n; ++i)
                    loading[i] += width * H(i, knots[piece - 1]);
            }
            return result;
        }

        [[nodiscard]] Vector_<T_> DiscountedStateMean(double time) const {
            auto result = StateDiscountCovariance(0.0, time);
            for (auto& value : result)
                value = -value;
            return result;
        }

        [[nodiscard]] Bond_ PrepareBond(double time, const Date_& maturity, const Matrix_<T_>& variance, const Vector_<T_>& mean) const {
            const double finalTime = Time(maturity);
            REQUIRE(finalTime >= time, "InvalidGSRObservation: bond maturity precedes observation");
            auto loading = BondLoading(time, finalTime);
            const T_ intercept = LogDF(finalTime) - LogDF(time) + Dot(loading, mean) - 0.5 * Quadratic(loading, variance);
            return {std::move(loading), intercept};
        }

        [[nodiscard]] static T_ Bond(const Bond_& bond, const T_* state) {
            T_ exponent = bond.intercept_;
            for (size_t i = 0; i < bond.loading_.size(); ++i)
                exponent -= bond.loading_[i] * state[i];
            return Dal::exp(exponent);
        }

        [[nodiscard]] LiborObservation_ PrepareLibor(double time,
                                                     const Date_& start,
                                                     const Date_& maturity,
                                                     const String_& tenor,
                                                     const DayBasis_& basis,
                                                     const Matrix_<T_>& variance,
                                                     const Vector_<T_>& mean) const {
            const double accrual = basis(start, maturity, nullptr);
            REQUIRE(accrual > 0.0 && start < maturity, "InvalidGSRObservation: invalid Libor accrual");
            const auto& projection = Projection(tenor);
            const T_ scale = Dal::exp(LogDF(Time(start), projection) - LogDF(Time(maturity), projection) - LogDF(start) + LogDF(maturity));
            return {PrepareBond(time, start, variance, mean), PrepareBond(time, maturity, variance, mean), scale, accrual};
        }

        template <class F_> [[nodiscard]] static T_ Libor(const LiborObservation_& request, const F_& price) {
            return (request.scale_ * price(request.start_) / price(request.end_) - 1.0) / request.accrual_;
        }

        [[nodiscard]] Observation_
        PrepareObservation(const String_& name, const Date_& sampleDate, const Matrix_<T_>& variance, const Vector_<T_>& mean) const {
            const Handle_<Index_> index(Index::Parse(name));
            const DateTime_ eventTime(sampleDate, 0.0);
            const double time = Time(sampleDate);
            if (const auto* df = dynamic_cast<const Index::DF_*>(index.get())) {
                const Date_ start = df->StartDate(eventTime), maturity = df->Maturity(eventTime);
                REQUIRE(start >= sampleDate && maturity > start, "InvalidGSRObservation: invalid discount interval");
                return DiscountObservation_{PrepareBond(time, start, variance, mean), PrepareBond(time, maturity, variance, mean)};
            } else if (const auto* libor = dynamic_cast<const Index::Libor_*>(index.get())) {
                const Date_ start = libor->StartDate(eventTime);
                const Date_ maturity = Date::NominalMaturity(start, libor->tenor_.Period(), libor->ccy_);
                return PrepareLibor(time, start, maturity, libor->tenor_.Period().String(), Ccy::Conventions::LiborDayBasis()(libor->ccy_), variance,
                                    mean);
            } else if (const auto* swap = dynamic_cast<const Index::Swap_*>(index.get())) {
                const Date_ start = swap->StartDate(eventTime);
                const Date_ maturity = Date::ParseIncrement(swap->tenor_)->FwdFrom(start);
                const auto& fixedLeg = Ccy::Conventions::SwapFixedLeg()(swap->ccy_);
                const auto& floatLeg = Ccy::Conventions::SwapFloatLeg()(swap->ccy_);
                const auto floatIndex = Ccy::Conventions::SwapFloatIndex()(swap->ccy_);
                const auto fixedPeriods =
                    MakeSchedulePeriods(start, maturity, fixedLeg.paymentFrequency_, fixedLeg.accrualHolidays_, 0, Holidays::None(),
                                        fixedLeg.paymentLag_, fixedLeg.paymentHolidays_, DateGeneration_("Forward"), fixedLeg.businessDayConvention_,
                                        fixedLeg.paymentConvention_, fixedLeg.endOfMonth_);
                const auto floatPeriods =
                    MakeSchedulePeriods(start, maturity, floatLeg.paymentFrequency_, floatLeg.accrualHolidays_, 0, Holidays::None(),
                                        floatLeg.paymentLag_, floatLeg.paymentHolidays_, DateGeneration_("Forward"), floatLeg.businessDayConvention_,
                                        floatLeg.paymentConvention_, floatLeg.endOfMonth_);
                SwapObservation_ result;
                for (const auto& period : fixedPeriods)
                    result.fixed_.push_back({PrepareBond(time, period.paymentDate_, variance, mean),
                                             fixedLeg.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get())});
                for (const auto& period : floatPeriods)
                    result.floating_.push_back({PrepareLibor(time, period.accrualStart_, period.accrualEnd_, floatIndex.Period().String(),
                                                             Ccy::Conventions::LiborDayBasis()(swap->ccy_), variance, mean),
                                                PrepareBond(time, period.paymentDate_, variance, mean),
                                                floatLeg.dayBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get())});
                return result;
            } else
                THROW("UnsupportedGSRObservation: " + name);
        }

        void SetParameterPointers() {
            parameters_.clear();
            parameterLabels_.clear();
            const auto add = [&](T_* value, const String_& label) {
                parameters_.push_back(value);
                parameterLabels_.push_back(label);
            };
            for (size_t i = 1; i < discountLogDF_.size(); ++i)
                add(&discountLogDF_[i], "logdf:OIS:" + Date::ToString(nodeDates_[i]));
            for (size_t row = 0; row < projectionLogDF_.size(); ++row)
                for (size_t i = 1; i < projectionLogDF_[row].size(); ++i)
                    add(&projectionLogDF_[row][i], "logdf:" + projectionTenors_[row] + ":" + Date::ToString(nodeDates_[i]));
            const auto addPieces = [&](Vector_<Vector_<T_>>* values, const Vector_<>& times, const String_& prefix) {
                for (size_t factor = 0; factor < values->size(); ++factor)
                    for (size_t i = 0; i < times.size(); ++i)
                        add(&(*values)[factor][i],
                            prefix + (legacy_ ? String_() : factorNames_[factor] + ":") +
                                Date::ToString(evaluationDate_.AddDays(static_cast<int>(std::llround(times[i] * DAYS_PER_YEAR)))));
            };
            addPieces(&gValues_, gTimes_, "g:");
            addPieces(&hValues_, hTimes_, "H:");
        }

        void ComputeStep(double previous, double current, Step_* step) const {
            const auto variance = StateVariance(previous, current);
            const auto covariance = StateDiscountCovariance(previous, current);
            step->lower_ = CovarianceFactor(variance);
            step->loading_ = BondLoading(previous, current);
            step->discountNormals_ = Vector_<T_>(NumFactors(), T_(0.0));
            for (size_t i = 0; i < NumFactors(); ++i) {
                T_ residual = covariance[i];
                for (size_t j = 0; j < i; ++j)
                    residual -= step->lower_(static_cast<int>(i), static_cast<int>(j)) * step->discountNormals_[j];
                const T_ diagonal = step->lower_(static_cast<int>(i), static_cast<int>(i));
                if (Value(diagonal) > 0.0)
                    step->discountNormals_[i] = residual / diagonal;
            }
            step->a_ = LogDF(current) - LogDF(previous);
            step->a_ += Dot(step->loading_, DiscountedStateMean(previous));
            step->a_ -= 0.5 * Quadratic(step->loading_, StateVariance(0.0, previous));
            step->a_ -= 0.5 * Dot(step->discountNormals_, step->discountNormals_);
            step->advances_ = true;
            FillHybridCoefficients(step);
        }

        // Hybrid stepping receives globally correlated factors, so the kernel applies R = L S
        // (S inverts the factor Cholesky) and q = S^T d instead of its own Cholesky L and d.
        void FillHybridCoefficients(Step_* step) const {
            const int n = static_cast<int>(NumFactors());
            if (n == 1 || factorLowerInverse_.Rows() != n)
                return;
            step->hybridLower_ = Matrix_<T_>(n, n, T_(0.0));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    T_ value(0.0);
                    for (int k = j; k <= i; ++k)
                        value += step->lower_(i, k) * T_(factorLowerInverse_(k, j));
                    step->hybridLower_(i, j) = value;
                }
            step->hybridNormals_.Resize(n);
            for (int j = 0; j < n; ++j) {
                T_ value(0.0);
                for (int i = j; i < n; ++i)
                    value += T_(factorLowerInverse_(i, j)) * step->discountNormals_[i];
                step->hybridNormals_[j] = value;
            }
        }

        void AdvancePath(const Step_& step, const double* gaussian, T_* state, T_* logNumeraire) const {
            T_ logDiscount = step.a_;
            for (size_t i = 0; i < NumFactors(); ++i)
                logDiscount -= step.loading_[i] * state[i] + step.discountNormals_[i] * gaussian[i];
            *logNumeraire -= logDiscount;
            for (size_t i = 0; i < NumFactors(); ++i)
                for (size_t j = 0; j <= i; ++j)
                    state[i] += step.lower_(static_cast<int>(i), static_cast<int>(j)) * gaussian[j];
        }

        template <class F_> [[nodiscard]] static T_ Observe(const Observation_& request, const F_& price) {
            return std::visit(
                [&](const auto& observation) -> T_ {
                    using O_ = std::decay_t<decltype(observation)>;
                    if constexpr (std::is_same_v<O_, DiscountObservation_>)
                        return price(observation.end_) / price(observation.start_);
                    else if constexpr (std::is_same_v<O_, LiborObservation_>)
                        return Libor(observation, price);
                    else {
                        T_ annuity(0.0), floating(0.0);
                        for (const auto& coupon : observation.fixed_)
                            annuity += coupon.accrual_ * price(coupon.payment_);
                        REQUIRE(Value(annuity) > 0.0, "InvalidGSRObservation: non-positive swap annuity");
                        for (const auto& coupon : observation.floating_)
                            floating += coupon.accrual_ * Libor(coupon.fixing_, price) * price(coupon.payment_);
                        return floating / annuity;
                    }
                },
                request);
        }

        void GeneratePathWithState(const Vector_<>& gaussian, Scenario_<T_>* path, T_* state) const {
            T_ logNumeraire(0.0);
            size_t gaussianSlot = 0;
            for (size_t i = 0; i < productTimeLine_.size(); ++i) {
                const auto& step = steps_[i];
                if (step.advances_) {
                    AdvancePath(step, &gaussian[gaussianSlot], state, &logNumeraire);
                    gaussianSlot += NumFactors();
                }
                auto& sample = (*path)[i];
                sample.spot_ = T_(0.0);
                sample.numeraire_ = Dal::exp(logNumeraire);
                for (size_t j = 0; j < observations_[i].size(); ++j)
                    sample.observations_[j] = Observe(observations_[i][j], [&](const Bond_& bond) { return Bond(bond, state); });
                for (size_t k = 0; k < discountBonds_[i].size(); ++k)
                    sample.discounts_[k] = Bond(discountBonds_[i][k], state);
            }
        }

        GSR_(const GSRCurveData_& curve,
             const Vector_<String_>& factorNames,
             const Vector_<Date_>& gDates,
             const Matrix_<>& gValues,
             const Vector_<Date_>& hDates,
             const Matrix_<>& hValues,
             const Matrix_<>& correlations,
             bool legacy)
            : evaluationDate_(curve.evaluationDate_), currency_(curve.currency_), nodeDates_(curve.nodeDates_),
              projectionTenors_(curve.projectionTenors_), factorNames_(factorNames), correlations_(correlations), legacy_(legacy) {
            factorLower_ = CovarianceFactor(correlations_);
            factorLowerInverse_ = TryLowerInverse(factorLower_);
            Vector_<> curveTimes;
            for (const auto& date : nodeDates_)
                curveTimes.push_back((date - evaluationDate_) / DAYS_PER_YEAR);
            interpolation_ = std::make_shared<LogDfInterpolation_>(curveTimes, LogDfScheme_("LOG_LINEAR"));
            for (const double value : curve.discountLogDF_)
                discountLogDF_.push_back(T_(value));
            for (size_t row = 0; row < projectionTenors_.size(); ++row) {
                Vector_<T_> values;
                for (size_t col = 0; col < nodeDates_.size(); ++col)
                    values.push_back(T_(curve.projectionLogDF_(static_cast<int>(row), static_cast<int>(col))));
                projectionLogDF_.push_back(std::move(values));
            }
            const auto loadPieces = [&](const Vector_<Date_>& dates, const Matrix_<>& values, Vector_<>* times, Vector_<Vector_<T_>>* pieces) {
                for (const auto& date : dates)
                    times->push_back((date - evaluationDate_) / DAYS_PER_YEAR);
                for (int row = 0; row < values.Rows(); ++row) {
                    Vector_<T_> factor;
                    for (int col = 0; col < values.Cols(); ++col)
                        factor.push_back(T_(values(row, col)));
                    pieces->push_back(std::move(factor));
                }
            };
            loadPieces(gDates, gValues, &gTimes_, &gValues_);
            loadPieces(hDates, hValues, &hTimes_, &hValues_);
            SetParameterPointers();
        }

    public:
        explicit GSR_(const GSRModelData_& data)
            : GSR_(*data.curve_,
                   {""},
                   data.vol_->gKnotDates_,
                   SingleRow(data.vol_->gValues_),
                   data.vol_->hKnotDates_,
                   SingleRow(data.vol_->hValues_),
                   Matrix_<>(1, 1, 1.0),
                   true) {}
        explicit GSR_(const MultiFactorGSRModelData_& data)
            : GSR_(*data.curve_,
                   data.vol_->factorNames_,
                   data.vol_->gKnotDates_,
                   data.vol_->gValues_,
                   data.vol_->hKnotDates_,
                   data.vol_->hValues_,
                   data.vol_->correlations_,
                   false) {}

        [[nodiscard]] T_ InitialLogDiscount(double time) const { return LogDF(time); }
        [[nodiscard]] T_ InitialLogProjection(double time, const String_& tenor) const { return LogDF(time, Projection(tenor)); }
        [[nodiscard]] Matrix_<T_> GaussianVariance(double time) const { return StateVariance(0.0, time); }
        [[nodiscard]] Vector_<T_> GaussianBondLoading(double expiry, double maturity) const { return BondLoading(expiry, maturity); }
        [[nodiscard]] Vector_<T_> FactorVolatilities(double time) const {
            Vector_<T_> values;
            for (size_t i = 0; i < NumFactors(); ++i)
                values.push_back(G(i, time));
            return values;
        }
        [[nodiscard]] Vector_<T_> ShortRateLoadings(double time) const {
            Vector_<T_> values;
            for (size_t i = 0; i < NumFactors(); ++i)
                values.push_back(H(i, time));
            return values;
        }
        [[nodiscard]] Vector_<> RateKnots(double end) const { return IntervalKnots(0.0, end); }
        [[nodiscard]] T_ ObserveHJM(size_t sample, size_t slot, const Vector_<T_>& state, const Matrix_<T_>& covariance) const {
            REQUIRE(sample < observations_.size() && slot < observations_[sample].size() && state.size() == NumFactors() &&
                        covariance.Rows() == static_cast<int>(NumFactors()) && covariance.Cols() == static_cast<int>(NumFactors()),
                    "InvalidGSRObservation: HJM sample, state or covariance dimensions do not match");
            return ObserveHJMFlat(sample, slot, &state[0], &covariance(0, 0));
        }
        [[nodiscard]] T_ ObserveHJMFlat(size_t sample, size_t slot, const T_* state, const T_* covariance) const {
            REQUIRE(sample < observations_.size() && slot < observations_[sample].size(),
                    "InvalidGSRObservation: HJM sample, state or covariance dimensions do not match");
            const size_t n = NumFactors();
            return Observe(observations_[sample][slot], [&](const Bond_& bond) -> T_ {
                T_ exponent = bond.intercept_;
                for (size_t i = 0; i < bond.loading_.size(); ++i)
                    exponent -= bond.loading_[i] * state[i];
                for (size_t i = 0; i < n; ++i)
                    for (size_t j = 0; j < n; ++j)
                        exponent -= 0.5 * covariance[i * n + j] * bond.loading_[i] * bond.loading_[j];
                return Dal::exp(exponent);
            });
        }
        void ResetAnchorsForRecording() {
            if constexpr (!std::is_same_v<T_, double>) {
                discountLogDF_[0] = T_(0.0);
                for (auto& projection : projectionLogDF_)
                    projection[0] = T_(0.0);
            }
        }
        [[nodiscard]] const Matrix_<>& FactorCorrelations() const { return correlations_; }
        [[nodiscard]] bool HybridFactorsInvertible() const {
            return NumFactors() == 1 || factorLowerInverse_.Rows() == static_cast<int>(NumFactors());
        }
        void AdvanceHybrid(size_t sample, const Vector_<>& factors, const Vector_<size_t>& factorSlots, T_* state, T_* logNumeraire) const {
            REQUIRE(sample < steps_.size() && steps_[sample].advances_, "InvalidGSRPath: hybrid step was not prepared");
            REQUIRE(factorSlots.size() == NumFactors() && factors.size() >= NumFactors(), "InvalidGSRPath: hybrid factor layout mismatch");
            const Step_& step = steps_[sample];
            if (NumFactors() == 1) {
                AdvancePath(step, &factors[factorSlots[0]], state, logNumeraire);
                return;
            }
            REQUIRE(factorLowerInverse_.Rows() == static_cast<int>(NumFactors()),
                    "InvalidGSRFactors: factor correlations must be positive definite for hybrid stepping");
            T_ logDiscount = step.a_;
            for (size_t i = 0; i < NumFactors(); ++i)
                logDiscount -= step.loading_[i] * state[i] + step.hybridNormals_[i] * factors[factorSlots[i]];
            *logNumeraire -= logDiscount;
            for (size_t i = 0; i < NumFactors(); ++i)
                for (size_t j = 0; j <= i; ++j)
                    state[i] += step.hybridLower_(static_cast<int>(i), static_cast<int>(j)) * factors[factorSlots[j]];
        }
        [[nodiscard]] T_ ObserveHybrid(size_t sample, size_t slot, const T_* state) const {
            REQUIRE(sample < observations_.size() && slot < observations_[sample].size(),
                    "InvalidGSRObservation: hybrid observation was not prepared");
            return Observe(observations_[sample][slot], [&](const Bond_& bond) { return Bond(bond, state); });
        }
        //  Fill one hybrid sample's discount factors from its prepared bonds
        void ObserveHybridDiscounts(size_t sample, const T_* state, Vector_<T_>* discounts) const {
            REQUIRE(sample < discountBonds_.size() && discounts->size() == discountBonds_[sample].size(),
                    "InvalidGSRObservation: discount sample or dimensions do not match");
            for (size_t k = 0; k < discounts->size(); ++k)
                (*discounts)[k] = Bond(discountBonds_[sample][k], state);
        }

        [[nodiscard]] size_t NumAssets() const override { return 0; }
        [[nodiscard]] size_t NumFactors() const override { return factorNames_.size(); }
        [[nodiscard]] bool SupportsBrownianBridge() const override { return true; }
        [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return evaluationDate_; }
        [[nodiscard]] bool SupportsDiscountFactors() const override { return true; }
        [[nodiscard]] size_t MaxObservedIndices() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] bool NumeraireIsDeterministic() const override {
            return std::all_of(gValues_.begin(), gValues_.end(), [](const auto& factor) {
                return std::all_of(factor.begin(), factor.end(), [](const T_& value) { return Value(value) == 0.0; });
            });
        }
        [[nodiscard]] bool SupportsIndex(const Index_& index) const override {
            if (const auto* df = dynamic_cast<const Index::DF_*>(&index))
                return df->ccy_.String() == currency_;
            if (const auto* libor = dynamic_cast<const Index::Libor_*>(&index))
                return libor->ccy_.String() == currency_;
            if (const auto* swap = dynamic_cast<const Index::Swap_*>(&index))
                return swap->ccy_.String() == currency_;
            return false;
        }
        [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
            if (!Model_<T_>::ValidParameterValue(parameter, value))
                return false;
            const size_t curveCount = nodeDates_.size() - 1 + projectionTenors_.size() * (nodeDates_.size() - 1);
            if (parameter < curveCount)
                return true;
            if (parameter < curveCount + NumFactors() * gTimes_.size())
                return value >= 0.0;
            return !legacy_ || value > 0.0;
        }
        [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }
        [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override { return CloneRateKernel(); }
        [[nodiscard]] std::unique_ptr<GSR_<T_>> CloneRateKernel() const {
            auto clone = std::make_unique<GSR_<T_>>(*this);
            clone->SetParameterPointers();
            return clone;
        }
        void Allocate(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override {
            this->ValidateTimeline(timeline, definitions);
            productTimeLine_ = timeline;
            steps_.Resize(timeline.size());
            observations_.Resize(timeline.size());
            discountBonds_.Resize(timeline.size());
        }
        void Init(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override { InitObservations(timeline, definitions, true); }
        void InitHJM(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) { InitObservations(timeline, definitions, false); }

    private:
        void InitObservations(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions, bool gaussian) {
            REQUIRE(timeline == productTimeLine_, "InvalidGSRTimeline: Allocate and Init timelines differ");
            this->ValidateTimeline(timeline, definitions);
            // Anchors are not risk parameters. Recreate their constant tape nodes after NewRecording.
            ResetAnchorsForRecording();
            double previous = 0.0;
            for (size_t i = 0; i < timeline.size(); ++i) {
                const double current = timeline[i];
                const Date_ sampleDate = DateAt(current);
                static_cast<void>(Time(sampleDate));
                if (gaussian && current > previous)
                    ComputeStep(previous, current, &steps_[i]);
                else
                    steps_[i] = Step_();
                observations_[i].clear();
                const auto variance = gaussian ? StateVariance(0.0, current) : Matrix_<T_>(NumFactors(), NumFactors(), T_(0.0));
                const auto mean = gaussian ? DiscountedStateMean(current) : Vector_<T_>(NumFactors(), T_(0.0));
                for (const auto& name : definitions[i].indexNames_)
                    observations_[i].push_back(PrepareObservation(name, sampleDate, variance, mean));
                discountBonds_[i].clear();
                for (const double maturity : definitions[i].discountMats_) {
                    REQUIRE(maturity >= current, "InvalidGSRDiscount: discount maturity precedes its sample");
                    discountBonds_[i].push_back(PrepareBond(current, DateAt(maturity), variance, mean));
                }
                previous = current;
            }
        }

    public:
        [[nodiscard]] size_t SimDim() const override {
            return productTimeLine_.empty() ? 0 : NumFactors() * (productTimeLine_.size() - static_cast<size_t>(productTimeLine_.front() == 0.0));
        }
        void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
            REQUIRE(gaussian.size() == SimDim() && path && path->size() == productTimeLine_.size(),
                    "InvalidGSRPath: Gaussian or scenario dimension mismatch");
            if (NumFactors() == 1) {
                T_ state(0.0);
                GeneratePathWithState(gaussian, path, &state);
            } else {
                Vector_<T_> state(NumFactors(), T_(0.0));
                GeneratePathWithState(gaussian, path, &state[0]);
            }
        }
    };
} // namespace Dal::AAD
