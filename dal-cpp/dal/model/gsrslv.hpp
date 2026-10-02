//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <dal/model/gsr.hpp>
#include <dal/model/gsrslvdata.hpp>

namespace Dal::AAD {
    template <class T_ = double> class GSRSLV_ final : public Model_<T_> {
        struct Step_ {
            double time_, width_, sqrtWidth_;
            Vector_<T_> g_, h_, covarianceH_;
            Matrix_<T_> covariance_;
            T_ bankBase_, bridgeVariance_, bridgeStd_;
        };

        std::unique_ptr<GSR_<T_>> rates_;
        Vector_<> rateShifts_, leverageTimes_;
        Matrix_<T_> leverage_;
        T_ kappa_, volOfVol_;
        Matrix_<> driverLower_;
        double maxStep_;
        Vector_<> timeline_, grid_;
        Vector_<size_t> sampleEnds_, observationCounts_;
        Vector_<Step_> steps_;
        Vector_<T_*> parameters_;
        Vector_<String_> labels_;

        [[nodiscard]] T_ LeverageAtTime(int row, double time) const {
            if (time <= leverageTimes_.front())
                return leverage_(row, 0);
            if (time >= leverageTimes_.back())
                return leverage_(row, leverage_.Cols() - 1);
            const int hi = static_cast<int>(std::upper_bound(leverageTimes_.begin(), leverageTimes_.end(), time) - leverageTimes_.begin());
            const double weight = (time - leverageTimes_[hi - 1]) / (leverageTimes_[hi] - leverageTimes_[hi - 1]);
            return (1.0 - weight) * leverage_(row, hi - 1) + weight * leverage_(row, hi);
        }

        void SetParameterPointers() {
            parameters_ = rates_->Parameters();
            labels_ = rates_->ParameterLabels();
            parameters_.push_back(&kappa_);
            labels_.push_back("kappa");
            parameters_.push_back(&volOfVol_);
            labels_.push_back("volOfVol");
            for (int row = 0; row < leverage_.Rows(); ++row)
                for (int col = 0; col < leverage_.Cols(); ++col) {
                    parameters_.push_back(&leverage_(row, col));
                    labels_.push_back("leverage:" + String_(std::to_string(row)) + ":" + String_(std::to_string(col)));
                }
        }

        [[nodiscard]] Step_ PrepareStep(double from, double to) const {
            const int n = static_cast<int>(rates_->NumFactors());
            Step_ step;
            step.time_ = from;
            step.width_ = to - from;
            step.sqrtWidth_ = std::sqrt(step.width_);
            step.g_ = rates_->FactorVolatilities(from);
            step.h_ = rates_->ShortRateLoadings(from);
            step.covariance_ = Matrix_<T_>(n, n, T_(0.0));
            step.covarianceH_ = Vector_<T_>(n, T_(0.0));
            step.bridgeVariance_ = T_(0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    double correlation = 0.0;
                    for (int k = 0; k <= std::min(i, j); ++k)
                        correlation += driverLower_(i, k) * driverLower_(j, k);
                    step.covariance_(i, j) = step.g_[i] * step.g_[j] * correlation;
                    step.covarianceH_[i] += step.covariance_(i, j) * step.h_[j];
                }
            for (int k = 0; k < n; ++k) {
                T_ exposure(0.0);
                for (int i = k; i < n; ++i)
                    exposure += step.h_[i] * step.g_[i] * driverLower_(i, k);
                step.bridgeVariance_ += exposure * exposure;
            }
            step.bankBase_ = rates_->InitialLogDiscount(from) - rates_->InitialLogDiscount(to);
            step.bridgeStd_ =
                Value(step.bridgeVariance_) > 0.0 ? T_(Dal::sqrt(step.bridgeVariance_ * (step.width_ * step.width_ * step.width_ / 12.0))) : T_(0.0);
            return step;
        }

        [[nodiscard]] Vector_<> IntegrationKnots() const {
            auto knots = rates_->RateKnots(timeline_.back());
            for (double time : timeline_)
                knots.push_back(time);
            for (double time : leverageTimes_)
                if (time > 0.0 && time < timeline_.back())
                    knots.push_back(time);
            std::sort(knots.begin(), knots.end());
            knots.erase(std::unique(knots.begin(), knots.end()), knots.end());
            return knots;
        }

        [[nodiscard]] Vector_<> Grid() const {
            const auto knots = IntegrationKnots();
            Vector_<> grid{0.0};
            for (size_t i = 1; i < knots.size(); ++i) {
                const double count = std::max(1.0, std::ceil((knots[i] - knots[i - 1]) / maxStep_));
                REQUIRE(std::isfinite(count) && count <= 1000000.0 && grid.size() + count <= 1000001.0,
                        "InvalidGSRSLVStep: integration grid exceeds one million steps");
                const auto divisions = static_cast<size_t>(count);
                for (size_t j = 1; j <= divisions; ++j)
                    grid.push_back(j == divisions ? knots[i] : knots[i - 1] + (knots[i] - knots[i - 1]) * j / divisions);
            }
            return grid;
        }

    public:
        struct State_ {
            Vector_<T_> x_;
            Matrix_<T_> y_;
            T_ variance_ = T_(1.0), latentVariance_ = T_(1.0), logNumeraire_ = T_(0.0);

            explicit State_(size_t factors) : x_(factors, T_(0.0)), y_(factors, factors, T_(0.0)) {}
        };

    private:
        void Correlate(const double* gaussian, Vector_<>* correlated) const {
            std::fill(correlated->begin(), correlated->end(), 0.0);
            for (int i = 0; i < driverLower_.Rows(); ++i)
                for (int j = 0; j <= i; ++j)
                    (*correlated)[i] += driverLower_(i, j) * gaussian[j];
        }

        [[nodiscard]] T_ Shift(const Step_& step, const State_& state) const {
            T_ shift(0.0);
            for (size_t i = 0; i < state.x_.size(); ++i)
                shift += step.h_[i] * state.x_[i];
            return shift;
        }

        void AdvanceBank(const Step_& step,
                         const Vector_<>& correlated,
                         double bridge,
                         const T_& shift,
                         const T_& varianceScale,
                         const T_& diffusion,
                         State_* state) const {
            const size_t n = state->x_.size();
            T_ yHH(0.0), noiseH(0.0);
            for (size_t i = 0; i < n; ++i) {
                noiseH += step.h_[i] * step.g_[i] * correlated[i];
                for (size_t j = 0; j < n; ++j)
                    yHH += step.h_[i] * state->y_(i, j) * step.h_[j];
            }
            const double dt = step.width_;
            state->logNumeraire_ += step.bankBase_ + dt * shift + 0.5 * dt * dt * yHH + varianceScale * step.bridgeVariance_ * (dt * dt * dt / 6.0) +
                                    diffusion * (0.5 * dt * step.sqrtWidth_ * noiseH + step.bridgeStd_ * bridge);
        }

        void AdvanceRates(const Step_& step, const Vector_<>& correlated, const T_& varianceScale, const T_& diffusion, State_* state) const {
            const size_t n = state->x_.size();
            const double dt = step.width_;
            for (size_t i = 0; i < n; ++i) {
                T_ drift(0.0);
                for (size_t j = 0; j < n; ++j)
                    drift += state->y_(i, j) * step.h_[j];
                state->x_[i] +=
                    dt * drift + 0.5 * dt * dt * varianceScale * step.covarianceH_[i] + diffusion * step.sqrtWidth_ * step.g_[i] * correlated[i];
            }
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j <= i; ++j) {
                    state->y_(i, j) += dt * varianceScale * step.covariance_(i, j);
                    state->y_(j, i) = state->y_(i, j);
                }
        }

        void Advance(const Step_& step, const Vector_<>& correlated, double bridge, State_* state) const {
            const T_ shift = Shift(step, *state);
            const T_ leverage = LocalLeverage(step.time_, shift);
            const T_ varianceScale = leverage * leverage * state->variance_;
            const T_ sqrtVariance = Value(state->variance_) > 0.0 ? T_(Dal::sqrt(state->variance_)) : T_(0.0);
            const T_ diffusion = leverage * sqrtVariance;
            AdvanceBank(step, correlated, bridge, shift, varianceScale, diffusion, state);
            AdvanceRates(step, correlated, varianceScale, diffusion, state);
            state->latentVariance_ +=
                step.width_ * kappa_ * (1.0 - state->variance_) + volOfVol_ * step.sqrtWidth_ * sqrtVariance * correlated[state->x_.size()];
            REQUIRE(std::isfinite(Value(state->latentVariance_)), "InvalidGSRSLVPath: nonfinite latent variance");
            state->variance_ = Value(state->latentVariance_) > 0.0 ? state->latentVariance_ : T_(0.0);
        }

        void WriteSample(size_t sample, const State_& state, Scenario_<T_>* path) const {
            auto& output = (*path)[sample];
            REQUIRE(output.observations_.size() == observationCounts_[sample], "InvalidGSRSLVPath: observation dimension mismatch");
            output.spot_ = T_(0.0);
            output.numeraire_ = Dal::exp(state.logNumeraire_);
            REQUIRE(std::isfinite(Value(output.numeraire_)) && Value(output.numeraire_) > 0.0,
                    "InvalidGSRSLVPath: bank account overflow or underflow");
            for (size_t slot = 0; slot < output.observations_.size(); ++slot) {
                output.observations_[slot] = rates_->ObserveHJM(sample, slot, state.x_, state.y_);
                REQUIRE(std::isfinite(Value(output.observations_[slot])), "InvalidGSRSLVPath: nonfinite rate observation");
            }
        }

        [[nodiscard]] State_ Generate(const Vector_<>& gaussian, Scenario_<T_>* path) const {
            REQUIRE(gaussian.size() == SimDim() && !timeline_.empty() && sampleEnds_.size() == timeline_.size() && steps_.size() + 1 == grid_.size(),
                    "InvalidGSRSLVPath: Gaussian dimension mismatch or model not initialized");
            REQUIRE(std::all_of(gaussian.begin(), gaussian.end(), [](double x) { return std::isfinite(x); }),
                    "InvalidGSRSLVPath: Gaussian values must be finite");
            State_ state(rates_->NumFactors());
            Vector_<> correlated(rates_->NumFactors() + 1);
            size_t cursor = 0;
            for (size_t sample = 0; sample < timeline_.size(); ++sample) {
                while (cursor < sampleEnds_[sample]) {
                    const auto* normal = &gaussian[cursor * NumFactors()];
                    Correlate(normal, &correlated);
                    Advance(steps_[cursor], correlated, normal[NumFactors() - 1], &state);
                    ++cursor;
                }
                REQUIRE(std::isfinite(Value(state.variance_)) && std::isfinite(Value(state.logNumeraire_)),
                        "InvalidGSRSLVPath: nonfinite variance or bank account; refine the integration step");
                if (path)
                    WriteSample(sample, state, path);
            }
            return state;
        }

    public:
        explicit GSRSLV_(const GSRSLVModelData_& data)
            : rates_(std::make_unique<GSR_<T_>>(*data.gaussian_)), rateShifts_(data.leverage_->rateShifts_), leverageTimes_(data.leverage_->times_),
              leverage_(data.leverage_->values_.Rows(), data.leverage_->values_.Cols()), kappa_(data.kappa_), volOfVol_(data.volOfVol_),
              driverLower_(CovarianceFactor(data.DriverCorrelation())), maxStep_(data.maxStep_) {
            for (int row = 0; row < leverage_.Rows(); ++row)
                for (int col = 0; col < leverage_.Cols(); ++col)
                    leverage_(row, col) = T_(data.leverage_->values_(row, col));
            SetParameterPointers();
        }
        GSRSLV_(const GSRSLV_& other)
            : rates_(other.rates_->CloneRateKernel()), rateShifts_(other.rateShifts_), leverageTimes_(other.leverageTimes_),
              leverage_(other.leverage_), kappa_(other.kappa_), volOfVol_(other.volOfVol_), driverLower_(other.driverLower_),
              maxStep_(other.maxStep_), timeline_(other.timeline_), grid_(other.grid_), sampleEnds_(other.sampleEnds_),
              observationCounts_(other.observationCounts_), steps_(other.steps_) {
            SetParameterPointers();
        }

        [[nodiscard]] T_ LocalLeverage(double time, const T_& shift) const {
            REQUIRE(std::isfinite(time) && std::isfinite(Value(shift)), "InvalidGSRLeverage: query time and shift must be finite");
            if (Value(shift) <= rateShifts_.front())
                return LeverageAtTime(0, time);
            if (Value(shift) >= rateShifts_.back())
                return LeverageAtTime(leverage_.Rows() - 1, time);
            const int hi = static_cast<int>(std::upper_bound(rateShifts_.begin(), rateShifts_.end(), Value(shift)) - rateShifts_.begin());
            const T_ weight = (shift - rateShifts_[hi - 1]) / (rateShifts_[hi] - rateShifts_[hi - 1]);
            return (1.0 - weight) * LeverageAtTime(hi - 1, time) + weight * LeverageAtTime(hi, time);
        }
        [[nodiscard]] size_t NumAssets() const override { return 0; }
        [[nodiscard]] size_t NumFactors() const override { return rates_->NumFactors() + 2; }
        [[nodiscard]] bool SupportsBrownianBridge() const override { return true; }
        [[nodiscard]] bool NumeraireIsDeterministic() const override { return rates_->NumeraireIsDeterministic(); }
        [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return rates_->EvaluationDate(); }
        [[nodiscard]] bool SupportsIndex(const Index_& index) const override { return rates_->SupportsIndex(index); }
        [[nodiscard]] size_t MaxObservedIndices() const override { return rates_->MaxObservedIndices(); }
        [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return rates_->MaxOutputSlotsPerSample(); }
        [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
        [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
        [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
            if (parameter < rates_->NumParams())
                return rates_->ValidParameterValue(parameter, value);
            return Model_<T_>::ValidParameterValue(parameter, value) && (parameter < rates_->NumParams() + 2 ? value >= 0.0 : value > 0.0);
        }
        [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override { return std::make_unique<GSRSLV_<T_>>(*this); }
        void Allocate(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override {
            this->ValidateTimeline(timeline, definitions);
            timeline_ = timeline;
            rates_->Allocate(timeline, definitions);
            grid_ = Grid();
            steps_.clear();
            sampleEnds_.clear();
            observationCounts_.clear();
            for (const auto& definition : definitions)
                observationCounts_.push_back(definition.indexNames_.size());
        }
        void Init(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) override {
            REQUIRE(timeline == timeline_, "InvalidGSRSLVTimeline: Allocate and Init timelines differ");
            rates_->InitHJM(timeline, definitions);
            steps_.clear();
            sampleEnds_.clear();
            for (size_t i = 1; i < grid_.size(); ++i)
                steps_.push_back(PrepareStep(grid_[i - 1], grid_[i]));
            for (double time : timeline)
                sampleEnds_.push_back(static_cast<size_t>(std::lower_bound(grid_.begin(), grid_.end(), time) - grid_.begin()));
        }
        [[nodiscard]] size_t SimDim() const override { return grid_.empty() ? 0 : (grid_.size() - 1) * NumFactors(); }
        [[nodiscard]] State_ StateAfter(const Vector_<>& gaussian) const { return Generate(gaussian, nullptr); }
        void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
            REQUIRE(path && path->size() == timeline_.size(), "InvalidGSRSLVPath: scenario dimension mismatch");
            static_cast<void>(Generate(gaussian, path));
        }
    };
} // namespace Dal::AAD
