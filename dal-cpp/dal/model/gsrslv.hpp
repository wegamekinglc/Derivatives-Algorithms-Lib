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
        Matrix_<> driverLower_, standaloneCorrelations_;
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

        [[nodiscard]] Step_ PrepareStep(double from, double to, const Matrix_<>& correlations) const {
            const int n = static_cast<int>(rates_->NumFactors());
            REQUIRE(correlations.Rows() == n && correlations.Cols() == n, "InvalidGSRSLVHybrid: correlation block must match the rate factors");
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
                    step.covariance_(i, j) = step.g_[i] * step.g_[j] * T_(correlations(i, j));
                    step.covarianceH_[i] += step.covariance_(i, j) * step.h_[j];
                }
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    const T_ exposure = step.h_[i] * step.g_[i] * T_(correlations(i, j));
                    step.bridgeVariance_ += exposure * step.h_[j] * step.g_[j];
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

        [[nodiscard]] T_ Shift(const Step_& step, const T_* x) const {
            T_ shift(0.0);
            for (size_t i = 0; i < rates_->NumFactors(); ++i)
                shift += step.h_[i] * x[i];
            return shift;
        }

        // Shared evolution on a flat state layout: x[n], y[n*n] row-major, variance, latentVariance,
        // logNumeraire. Drivers come either contiguously (standalone correlated vector) or through a
        // hybrid factor-slot indirection; the flag is a compile-time constant in both callers.
        template <bool Indirect>
        void AdvanceCore(const Step_& step,
                         const Vector_<>& drivers,
                         const Vector_<size_t>* driverSlots,
                         double bridge,
                         T_* x,
                         T_* y,
                         T_* variance,
                         T_* latentVariance,
                         T_* logNumeraire) const {
            const size_t n = rates_->NumFactors();
            const auto driver = [&](size_t i) -> double { return Indirect ? drivers[(*driverSlots)[i]] : drivers[i]; };
            const T_ shift = Shift(step, x);
            const T_ leverage = LocalLeverage(step.time_, shift);
            const T_ varianceScale = leverage * leverage * *variance;
            const T_ sqrtVariance = Value(*variance) > 0.0 ? T_(Dal::sqrt(*variance)) : T_(0.0);
            const T_ diffusion = leverage * sqrtVariance;
            const double dt = step.width_;
            T_ yHH(0.0), noiseH(0.0);
            for (size_t i = 0; i < n; ++i) {
                noiseH += step.h_[i] * step.g_[i] * driver(i);
                for (size_t j = 0; j < n; ++j)
                    yHH += step.h_[i] * y[i * n + j] * step.h_[j];
            }
            *logNumeraire += step.bankBase_ + dt * shift + 0.5 * dt * dt * yHH + varianceScale * step.bridgeVariance_ * (dt * dt * dt / 6.0) +
                             diffusion * (0.5 * dt * step.sqrtWidth_ * noiseH + step.bridgeStd_ * bridge);
            for (size_t i = 0; i < n; ++i) {
                T_ drift(0.0);
                for (size_t j = 0; j < n; ++j)
                    drift += y[i * n + j] * step.h_[j];
                x[i] += dt * drift + 0.5 * dt * dt * varianceScale * step.covarianceH_[i] + diffusion * step.sqrtWidth_ * step.g_[i] * driver(i);
            }
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j <= i; ++j) {
                    y[i * n + j] += dt * varianceScale * step.covariance_(i, j);
                    y[j * n + i] = y[i * n + j];
                }
            *latentVariance += dt * kappa_ * (1.0 - *variance) + volOfVol_ * step.sqrtWidth_ * sqrtVariance * driver(n);
            REQUIRE(std::isfinite(Value(*latentVariance)), "InvalidGSRSLVPath: nonfinite latent variance");
            *variance = Value(*latentVariance) > 0.0 ? *latentVariance : T_(0.0);
        }

        void Advance(const Step_& step, const Vector_<>& correlated, double bridge, State_* state) const {
            const size_t n = rates_->NumFactors();
            AdvanceCore<false>(step, correlated, nullptr, bridge, &state->x_[0], &state->y_(0, 0), &state->variance_, &state->latentVariance_,
                               &state->logNumeraire_);
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

        static void ValidateGaussian(const Vector_<>& gaussian, size_t dimension) {
            REQUIRE(gaussian.size() == dimension, "InvalidGSRSLVPath: Gaussian dimension mismatch");
            REQUIRE(std::all_of(gaussian.begin(), gaussian.end(), [](double x) { return std::isfinite(x); }),
                    "InvalidGSRSLVPath: Gaussian values must be finite");
        }

        void ValidateState(const State_& initial) const {
            const size_t n = rates_->NumFactors();
            REQUIRE(initial.x_.size() == n && initial.y_.Rows() == n && initial.y_.Cols() == n,
                    "InvalidGSRSLVPath: continuation state dimensions mismatch");
            REQUIRE(std::isfinite(Value(initial.latentVariance_)) && std::isfinite(Value(initial.logNumeraire_)) &&
                        Value(initial.variance_) == std::max(0.0, Value(initial.latentVariance_)),
                    "InvalidGSRSLVPath: invalid continuation variance or bank account");
            for (const auto& x : initial.x_)
                REQUIRE(std::isfinite(Value(x)), "InvalidGSRSLVPath: nonfinite continuation state");
            for (const auto& y : initial.y_)
                REQUIRE(std::isfinite(Value(y)), "InvalidGSRSLVPath: nonfinite continuation state");
        }

        [[nodiscard]] State_ Generate(const Vector_<>& gaussian, Scenario_<T_>* path, Vector_<State_>* states = nullptr) const {
            REQUIRE(!timeline_.empty() && sampleEnds_.size() == timeline_.size() && steps_.size() + 1 == grid_.size(),
                    "InvalidGSRSLVPath: model not initialized");
            ValidateGaussian(gaussian, SimDim());
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
                if (states)
                    states->push_back(state);
            }
            return state;
        }

    public:
        explicit GSRSLV_(const GSRSLVModelData_& data)
            : rates_(std::make_unique<GSR_<T_>>(*data.gaussian_)), rateShifts_(data.leverage_->rateShifts_), leverageTimes_(data.leverage_->times_),
              leverage_(data.leverage_->values_.Rows(), data.leverage_->values_.Cols()), kappa_(data.kappa_), volOfVol_(data.volOfVol_),
              driverLower_(CovarianceFactor(data.DriverCorrelation())), maxStep_(data.maxStep_) {
            const int n = static_cast<int>(rates_->NumFactors());
            standaloneCorrelations_ = Matrix_<>(n, n, 0.0);
            const auto& drivers = data.DriverCorrelation();
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    standaloneCorrelations_(i, j) = drivers(i, j);
            for (int row = 0; row < leverage_.Rows(); ++row)
                for (int col = 0; col < leverage_.Cols(); ++col)
                    leverage_(row, col) = T_(data.leverage_->values_(row, col));
            SetParameterPointers();
        }
        GSRSLV_(const GSRSLV_& other)
            : rates_(other.rates_->CloneRateKernel()), rateShifts_(other.rateShifts_), leverageTimes_(other.leverageTimes_),
              leverage_(other.leverage_), kappa_(other.kappa_), volOfVol_(other.volOfVol_), driverLower_(other.driverLower_),
              standaloneCorrelations_(other.standaloneCorrelations_), maxStep_(other.maxStep_), timeline_(other.timeline_), grid_(other.grid_),
              sampleEnds_(other.sampleEnds_), observationCounts_(other.observationCounts_), steps_(other.steps_) {
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
        [[nodiscard]] std::unique_ptr<GSRSLV_<T_>> CloneSLVKernel() const { return std::make_unique<GSRSLV_<T_>>(*this); }
        [[nodiscard]] T_ InitialLogDiscount(double time) const { return rates_->InitialLogDiscount(time); }
        [[nodiscard]] double MaxStep() const { return maxStep_; }
        [[nodiscard]] size_t HybridStateDim() const {
            const size_t n = rates_->NumFactors();
            return n + n * n + 3;
        }
        void PrepareHybrid(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions, const Matrix_<>& rateCorrelations) {
            this->ValidateTimeline(timeline, definitions);
            timeline_ = timeline;
            rates_->Allocate(timeline, definitions);
            rates_->InitHJM(timeline, definitions);
            steps_.clear();
            sampleEnds_.clear();
            grid_.clear();
            for (size_t i = 1; i < timeline.size(); ++i)
                steps_.push_back(PrepareStep(timeline[i - 1], timeline[i], rateCorrelations));
        }
        void ResetHybridState(Vector_<T_>* state, size_t offset) const {
            const size_t n = rates_->NumFactors();
            for (size_t i = 0; i < n + n * n + 3; ++i)
                (*state)[offset + i] = i >= n + n * n && i < n + n * n + 2 ? T_(1.0) : T_(0.0);
        }
        void EvolveHybrid(size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, Vector_<T_>* state, size_t offset) const {
            REQUIRE(step < steps_.size(), "InvalidGSRSLVHybrid: step was not prepared");
            REQUIRE(factorSlots.size() == NumFactors() && factors.size() >= NumFactors(),
                    "InvalidGSRSLVHybrid: one named factor per rate, variance and bridge driver is required");
            const size_t n = rates_->NumFactors();
            // The bridge driver reuses no other Gaussian in standalone stepping; as a named hybrid
            // factor it occupies its own independent correlation slot.
            AdvanceCore<true>(steps_[step], factors, &factorSlots, factors[factorSlots[n + 1]], &(*state)[offset], &(*state)[offset + n],
                              &(*state)[offset + n + n * n], &(*state)[offset + n + n * n + 1], &(*state)[offset + n + n * n + 2]);
        }
        [[nodiscard]] T_ ObserveHybrid(size_t sample, size_t slot, const Vector_<T_>& state, size_t offset) const {
            const size_t n = rates_->NumFactors();
            return rates_->ObserveHJMFlat(sample, slot, &state[offset], &state[offset + n]);
        }
        [[nodiscard]] T_ HybridLogNumeraire(const Vector_<T_>& state, size_t offset) const {
            const size_t n = rates_->NumFactors();
            return state[offset + n + n * n + 2];
        }
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
                steps_.push_back(PrepareStep(grid_[i - 1], grid_[i], standaloneCorrelations_));
            for (double time : timeline)
                sampleEnds_.push_back(static_cast<size_t>(std::lower_bound(grid_.begin(), grid_.end(), time) - grid_.begin()));
        }
        [[nodiscard]] size_t SimDim() const override { return grid_.empty() ? 0 : (grid_.size() - 1) * NumFactors(); }
        [[nodiscard]] State_ StateAfter(const Vector_<>& gaussian) const { return Generate(gaussian, nullptr); }
        void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
            REQUIRE(path && path->size() == timeline_.size(), "InvalidGSRSLVPath: scenario dimension mismatch");
            static_cast<void>(Generate(gaussian, path));
        }
        void GeneratePathWithStates(const Vector_<>& gaussian, Scenario_<T_>* path, Vector_<State_>* states) const {
            REQUIRE(path && path->size() == timeline_.size() && states, "InvalidGSRSLVPath: scenario and state output required");
            states->clear();
            static_cast<void>(Generate(gaussian, path, states));
        }
        [[nodiscard]] size_t SimDimFrom(size_t sample) const {
            REQUIRE(sample < sampleEnds_.size() && steps_.size() + 1 == grid_.size(), "InvalidGSRSLVPath: invalid continuation sample");
            return (steps_.size() - sampleEnds_[sample]) * NumFactors();
        }
        void GeneratePathFrom(size_t sample, const State_& initial, const Vector_<>& gaussian, Scenario_<T_>* path) const {
            ValidateGaussian(gaussian, SimDimFrom(sample));
            REQUIRE(path && path->size() == timeline_.size(), "InvalidGSRSLVPath: continuation dimensions mismatch");
            ValidateState(initial);
            const size_t n = rates_->NumFactors();
            auto state = initial;
            Vector_<> correlated(n + 1);
            size_t cursor = sampleEnds_[sample], offset = 0;
            for (; sample < timeline_.size(); ++sample) {
                while (cursor < sampleEnds_[sample]) {
                    const auto* normal = &gaussian[offset];
                    Correlate(normal, &correlated);
                    Advance(steps_[cursor], correlated, normal[NumFactors() - 1], &state);
                    ++cursor;
                    offset += NumFactors();
                }
                WriteSample(sample, state, path);
            }
        }
    };
} // namespace Dal::AAD
