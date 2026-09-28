//
// Created by Codex on 2026/9/28.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

#include <dal/platform/platform.hpp>

#include <dal/currency/currencydata.hpp>
#include <dal/curve/logdfinterp.hpp>
#include <dal/indice/index/ir.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/model/gsrdata.hpp>
#include <dal/platform/consts.hpp>
#include <dal/protocol/conventions.hpp>
#include <dal/time/dateincrement.hpp>
#include <dal/time/schedules.hpp>

namespace Dal::AAD {
    template <class T_ = double> class GSR_ final : public Model_<T_> {
        struct Step_ {
            T_ sigma_ = T_(0.0);
            T_ a_ = T_(0.0);
            T_ bMinus_ = T_(0.0);
            T_ bPlus_ = T_(0.0);
            bool advances_ = false;
        };

        struct Observation_ {
            enum class Kind_ { DF, LIBOR, SWAP };
            Kind_ kind_ = Kind_::DF;
            Date_ start_;
            Date_ maturity_;
            String_ projectionTenor_;
            Vector_<SchedulePeriod_> fixedPeriods_;
            Vector_<SchedulePeriod_> floatPeriods_;
            DayBasis_ indexBasis_ = DayBasis::Act365F();
            DayBasis_ fixedBasis_ = DayBasis::Act365F();
            DayBasis_ floatBasis_ = DayBasis::Act365F();
        };

        Date_ evaluationDate_;
        String_ currency_;
        Vector_<Date_> nodeDates_;
        Vector_<String_> projectionTenors_;
        std::shared_ptr<const LogDfInterpolation_> interpolation_;
        Vector_<T_> discountLogDF_;
        Vector_<Vector_<T_>> projectionLogDF_;
        Vector_<> gTimes_;
        Vector_<> hTimes_;
        Vector_<T_> gValues_;
        Vector_<T_> hValues_;
        Vector_<T_*> parameters_;
        Vector_<String_> parameterLabels_;
        Vector_<> productTimeLine_;
        Vector_<Step_> steps_;
        Vector_<Vector_<Observation_>> observations_;

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

        [[nodiscard]] T_ G(double time) const { return gValues_[PieceAt(gTimes_, time)]; }
        [[nodiscard]] T_ H(double time) const { return hValues_[PieceAt(hTimes_, time)]; }

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

        [[nodiscard]] T_ StateVariance(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: state variance requires from <= to");
            T_ result(0.0);
            const auto knots = IntervalKnots(from, to);
            for (size_t i = 1; i < knots.size(); ++i) {
                const T_ g = G(knots[i - 1]);
                result += g * g * (knots[i] - knots[i - 1]);
            }
            return result;
        }

        [[nodiscard]] T_ BondLoading(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: bond loading requires from <= to");
            T_ result(0.0);
            const auto knots = IntervalKnots(from, to);
            for (size_t i = 1; i < knots.size(); ++i)
                result += H(knots[i - 1]) * (knots[i] - knots[i - 1]);
            return result;
        }

        // Integral of g(u)^2 B(u,to), accumulated backward over the merged knot grid.
        [[nodiscard]] T_ StateDiscountCovariance(double from, double to) const {
            REQUIRE(from <= to, "InvalidGSRInterval: covariance requires from <= to");
            T_ result(0.0);
            T_ loading(0.0);
            const auto knots = IntervalKnots(from, to);
            for (size_t i = knots.size(); i-- > 1;) {
                const double width = knots[i] - knots[i - 1];
                const T_ g = G(knots[i - 1]);
                const T_ h = H(knots[i - 1]);
                result += g * g * (width * loading + 0.5 * width * width * h);
                loading += width * h;
            }
            return result;
        }

        [[nodiscard]] T_ DiscountedStateMean(double time) const { return -StateDiscountCovariance(0.0, time); }

        [[nodiscard]] T_ Bond(double time, const Date_& maturity, const T_& state) const {
            const double finalTime = Time(maturity);
            REQUIRE(finalTime >= time, "InvalidGSRObservation: bond maturity precedes observation");
            const T_ loading = BondLoading(time, finalTime);
            return Dal::exp(LogDF(finalTime) - LogDF(time) - loading * (state - DiscountedStateMean(time)) -
                            0.5 * loading * loading * StateVariance(0.0, time));
        }

        [[nodiscard]] T_
        Libor(double time, const Date_& start, const Date_& maturity, const String_& tenor, const DayBasis_& basis, const T_& state) const {
            const double accrual = basis(start, maturity, nullptr);
            REQUIRE(accrual > 0.0 && start < maturity, "InvalidGSRObservation: invalid Libor accrual");
            const auto& projection = Projection(tenor);
            const T_ initialProjectionRatio = Dal::exp(LogDF(Time(start), projection) - LogDF(Time(maturity), projection));
            const T_ initialDiscountRatio = Dal::exp(LogDF(start) - LogDF(maturity));
            const T_ stochasticRatio = Bond(time, start, state) / Bond(time, maturity, state);
            return (initialProjectionRatio * stochasticRatio / initialDiscountRatio - 1.0) / accrual;
        }

        [[nodiscard]] T_ SwapRate(double time, const Observation_& request, const T_& state) const {
            T_ annuity(0.0);
            T_ floatPv(0.0);
            for (const auto& period : request.fixedPeriods_)
                annuity += request.fixedBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get()) *
                           Bond(time, period.paymentDate_, state);
            REQUIRE(Value(annuity) > 0.0, "InvalidGSRObservation: non-positive swap annuity");
            for (const auto& period : request.floatPeriods_) {
                const T_ fixing = Libor(time, period.accrualStart_, period.accrualEnd_, request.projectionTenor_, request.indexBasis_, state);
                floatPv += fixing * request.floatBasis_(period.accrualStart_, period.accrualEnd_, period.dayCountContext_.get()) *
                           Bond(time, period.paymentDate_, state);
            }
            return floatPv / annuity;
        }

        [[nodiscard]] Observation_ PrepareObservation(const String_& name, const Date_& sampleDate) const {
            const Handle_<Index_> index(Index::Parse(name));
            const DateTime_ eventTime(sampleDate, 0.0);
            Observation_ result;
            if (const auto* df = dynamic_cast<const Index::DF_*>(index.get())) {
                result.kind_ = Observation_::Kind_::DF;
                result.start_ = df->StartDate(eventTime);
                result.maturity_ = df->Maturity(eventTime);
            } else if (const auto* libor = dynamic_cast<const Index::Libor_*>(index.get())) {
                result.kind_ = Observation_::Kind_::LIBOR;
                result.start_ = libor->StartDate(eventTime);
                result.maturity_ = Date::NominalMaturity(result.start_, libor->tenor_.Period(), libor->ccy_);
                result.projectionTenor_ = libor->tenor_.Period().String();
                result.indexBasis_ = Ccy::Conventions::LiborDayBasis()(libor->ccy_);
            } else if (const auto* swap = dynamic_cast<const Index::Swap_*>(index.get())) {
                result.kind_ = Observation_::Kind_::SWAP;
                result.start_ = swap->StartDate(eventTime);
                result.maturity_ = Date::ParseIncrement(swap->tenor_)->FwdFrom(result.start_);
                const auto& fixedLeg = Ccy::Conventions::SwapFixedLeg()(swap->ccy_);
                const auto& floatLeg = Ccy::Conventions::SwapFloatLeg()(swap->ccy_);
                const auto floatIndex = Ccy::Conventions::SwapFloatIndex()(swap->ccy_);
                result.projectionTenor_ = floatIndex.Period().String();
                result.indexBasis_ = Ccy::Conventions::LiborDayBasis()(swap->ccy_);
                result.fixedBasis_ = fixedLeg.dayBasis_;
                result.floatBasis_ = floatLeg.dayBasis_;
                result.fixedPeriods_ =
                    MakeSchedulePeriods(result.start_, result.maturity_, fixedLeg.paymentFrequency_, fixedLeg.accrualHolidays_, 0, Holidays::None(),
                                        fixedLeg.paymentLag_, fixedLeg.paymentHolidays_, DateGeneration_("Forward"), fixedLeg.businessDayConvention_,
                                        fixedLeg.paymentConvention_, fixedLeg.endOfMonth_);
                result.floatPeriods_ =
                    MakeSchedulePeriods(result.start_, result.maturity_, floatLeg.paymentFrequency_, floatLeg.accrualHolidays_, 0, Holidays::None(),
                                        floatLeg.paymentLag_, floatLeg.paymentHolidays_, DateGeneration_("Forward"), floatLeg.businessDayConvention_,
                                        floatLeg.paymentConvention_, floatLeg.endOfMonth_);
            } else
                THROW("UnsupportedGSRObservation: " + name);
            REQUIRE(result.start_ >= sampleDate && result.maturity_ > result.start_,
                    "InvalidGSRObservation: start/maturity must follow the observation date");
            static_cast<void>(Time(result.maturity_));
            if (result.kind_ == Observation_::Kind_::SWAP) {
                for (const auto& period : result.fixedPeriods_)
                    static_cast<void>(Time(period.paymentDate_));
                for (const auto& period : result.floatPeriods_)
                    static_cast<void>(Time(period.paymentDate_));
            }
            return result;
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
            for (size_t i = 0; i < gValues_.size(); ++i)
                add(&gValues_[i], "g:" + Date::ToString(evaluationDate_.AddDays(static_cast<int>(std::llround(gTimes_[i] * DAYS_PER_YEAR)))));
            for (size_t i = 0; i < hValues_.size(); ++i)
                add(&hValues_[i], "H:" + Date::ToString(evaluationDate_.AddDays(static_cast<int>(std::llround(hTimes_[i] * DAYS_PER_YEAR)))));
        }

        void ComputeStep(double previous, double current, Step_* step) const {
            const T_ variance = StateVariance(previous, current);
            const T_ loading = BondLoading(previous, current);
            const T_ covariance = StateDiscountCovariance(previous, current);
            if (Value(variance) > 0.0) {
                step->sigma_ = Dal::sqrt(variance);
                step->bPlus_ = covariance / variance;
            } else {
                step->sigma_ = T_(0.0);
                step->bPlus_ = T_(0.0);
            }
            step->bMinus_ = loading - step->bPlus_;
            step->a_ = LogDF(current) - LogDF(previous);
            step->a_ += loading * DiscountedStateMean(previous);
            step->a_ -= 0.5 * loading * loading * StateVariance(0.0, previous);
            step->a_ -= 0.5 * step->bPlus_ * step->bPlus_ * variance;
            step->advances_ = true;
        }

        void AdvancePath(const Step_& step, double gaussian, T_* state, T_* logNumeraire) const {
            const T_ nextState = *state + step.sigma_ * gaussian;
            if (NumeraireIsDeterministic())
                *logNumeraire -= step.a_;
            else
                *logNumeraire -= step.a_ - step.bMinus_ * *state - step.bPlus_ * nextState;
            *state = nextState;
        }

        [[nodiscard]] T_ Observe(double time, const Observation_& request, const T_& state) const {
            switch (request.kind_) {
            case Observation_::Kind_::DF:
                return Bond(time, request.maturity_, state) / Bond(time, request.start_, state);
            case Observation_::Kind_::LIBOR:
                return Libor(time, request.start_, request.maturity_, request.projectionTenor_, request.indexBasis_, state);
            case Observation_::Kind_::SWAP:
                return SwapRate(time, request, state);
            }
            THROW("UnsupportedGSRObservation: unknown observation kind");
        }

    public:
        explicit GSR_(const GSRModelData_& data)
            : evaluationDate_(data.curve_->evaluationDate_), currency_(data.curve_->currency_), nodeDates_(data.curve_->nodeDates_),
              projectionTenors_(data.curve_->projectionTenors_) {
            Vector_<> curveTimes;
            for (const auto& date : nodeDates_)
                curveTimes.push_back((date - evaluationDate_) / DAYS_PER_YEAR);
            interpolation_ = std::make_shared<LogDfInterpolation_>(curveTimes, LogDfScheme_("LOG_LINEAR"));
            for (const double value : data.curve_->discountLogDF_)
                discountLogDF_.push_back(T_(value));
            for (size_t row = 0; row < projectionTenors_.size(); ++row) {
                Vector_<T_> values;
                for (size_t col = 0; col < nodeDates_.size(); ++col)
                    values.push_back(T_(data.curve_->projectionLogDF_(static_cast<int>(row), static_cast<int>(col))));
                projectionLogDF_.push_back(std::move(values));
            }
            for (size_t i = 0; i < data.vol_->gKnotDates_.size(); ++i) {
                gTimes_.push_back((data.vol_->gKnotDates_[i] - evaluationDate_) / DAYS_PER_YEAR);
                gValues_.push_back(T_(data.vol_->gValues_[i]));
            }
            for (size_t i = 0; i < data.vol_->hKnotDates_.size(); ++i) {
                hTimes_.push_back((data.vol_->hKnotDates_[i] - evaluationDate_) / DAYS_PER_YEAR);
                hValues_.push_back(T_(data.vol_->hValues_[i]));
            }
            SetParameterPointers();
        }

        [[nodiscard]] size_t NumAssets() const override { return 0; }
        [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return evaluationDate_; }
        [[nodiscard]] size_t MaxObservedIndices() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
        [[nodiscard]] bool NumeraireIsDeterministic() const override {
            return std::all_of(gValues_.begin(), gValues_.end(), [](const T_& value) { return Value(value) == 0.0; });
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
            if (parameter < curveCount + gValues_.size())
                return value >= 0.0;
            return value > 0.0;
        }
        [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }
        [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override {
            auto clone = std::make_unique<GSR_<T_>>(*this);
            clone->SetParameterPointers();
            return clone;
        }
        void Allocate(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override {
            this->ValidateTimeline(timeline, definitions);
            productTimeLine_ = timeline;
            steps_.Resize(timeline.size());
            observations_.Resize(timeline.size());
        }
        void Init(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override {
            REQUIRE(timeline == productTimeLine_, "InvalidGSRTimeline: Allocate and Init timelines differ");
            if constexpr (!std::is_same_v<T_, double>) {
                // Anchors are not risk parameters. Recreate their constant tape nodes after NewRecording.
                discountLogDF_[0] = T_(0.0);
                for (auto& projection : projectionLogDF_)
                    projection[0] = T_(0.0);
            }
            double previous = 0.0;
            for (size_t i = 0; i < timeline.size(); ++i) {
                const double current = timeline[i];
                const Date_ sampleDate = DateAt(current);
                static_cast<void>(Time(sampleDate));
                if (current > previous)
                    ComputeStep(previous, current, &steps_[i]);
                else
                    steps_[i] = Step_();
                observations_[i].clear();
                for (const auto& name : definitions[i].indexNames_)
                    observations_[i].push_back(PrepareObservation(name, sampleDate));
                previous = current;
            }
        }
        [[nodiscard]] size_t SimDim() const override { return productTimeLine_.size() - static_cast<size_t>(productTimeLine_.front() == 0.0); }
        void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
            REQUIRE(gaussian.size() == SimDim() && path && path->size() == productTimeLine_.size(),
                    "InvalidGSRPath: Gaussian or scenario dimension mismatch");
            T_ state(0.0);
            T_ logNumeraire(0.0);
            size_t gaussianSlot = 0;
            for (size_t i = 0; i < productTimeLine_.size(); ++i) {
                const auto& step = steps_[i];
                if (step.advances_)
                    AdvancePath(step, gaussian[gaussianSlot++], &state, &logNumeraire);
                auto& sample = (*path)[i];
                sample.spot_ = T_(0.0);
                sample.numeraire_ = Dal::exp(logNumeraire);
                for (size_t j = 0; j < observations_[i].size(); ++j)
                    sample.observations_[j] = Observe(productTimeLine_[i], observations_[i][j], state);
            }
        }
    };
} // namespace Dal::AAD
