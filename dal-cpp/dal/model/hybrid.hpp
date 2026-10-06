//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include <dal/model/gsr.hpp>
#include <dal/model/gsrslv.hpp>
#include <dal/model/hybriddata.hpp>

namespace Dal {
    // Immutable after construction. A time-bucketed provider can later select a precomputed lower factor by step.
    class FactorCorrelation_ {
    public:
        virtual ~FactorCorrelation_() = default;
        [[nodiscard]] virtual const Vector_<String_>& FactorNames() const = 0;
        [[nodiscard]] virtual const Matrix_<>& LowerAt(size_t step) const = 0;
    };

    class ConstantFactorCorrelation_ final : public FactorCorrelation_ {
        Vector_<String_> factorNames_;
        Matrix_<> lower_;

        static void ValidateDimensions(const Vector_<String_>& registry, const Vector_<String_>& inputNames, const Matrix_<>& input) {
            const int n = static_cast<int>(registry.size());
            REQUIRE(n > 0 && inputNames.size() == registry.size() && input.Rows() == n && input.Cols() == n,
                    "InvalidHybridCorrelation: factor names and matrix dimensions must match the registry");
        }

        static void ValidateUniqueNames(const Vector_<String_>& inputNames) {
            for (size_t i = 0; i < inputNames.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    REQUIRE(inputNames[i] != inputNames[j], "InvalidHybridCorrelation: duplicate factor " + inputNames[i]);
        }

        static Vector_<int> InputSlots(const Vector_<String_>& registry, const Vector_<String_>& inputNames) {
            Vector_<int> slots;
            for (const auto& name : registry) {
                const auto found = std::find(inputNames.begin(), inputNames.end(), name);
                REQUIRE(found != inputNames.end(), "InvalidHybridCorrelation: missing factor " + name);
                slots.push_back(static_cast<int>(found - inputNames.begin()));
            }
            return slots;
        }

        static Matrix_<> OrderedMatrix(const Vector_<String_>& registry, const Vector_<String_>& inputNames, const Matrix_<>& input) {
            ValidateDimensions(registry, inputNames, input);
            ValidateUniqueNames(inputNames);
            const auto slots = InputSlots(registry, inputNames);
            const int n = static_cast<int>(registry.size());
            Matrix_<> ordered(n, n, 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    ordered(i, j) = input(slots[i], slots[j]);
            return ordered;
        }

        static void ValidateDiagonal(const Matrix_<>& correlations, int i) {
            REQUIRE(std::isfinite(correlations(i, i)) && std::abs(correlations(i, i) - 1.0) <= 1e-12,
                    "InvalidHybridCorrelation: diagonal entries must equal one");
        }

        static void ValidateSymmetricEntry(const Matrix_<>& correlations, int i, int j) {
            REQUIRE(std::isfinite(correlations(i, j)) && std::isfinite(correlations(j, i)) &&
                        std::abs(correlations(i, j) - correlations(j, i)) <= 1e-12,
                    "InvalidHybridCorrelation: matrix must be finite and symmetric");
        }

        static Matrix_<> Factorize(const Matrix_<>& correlations) {
            const int n = correlations.Rows();
            Matrix_<> lower(n, n, 0.0);
            for (int i = 0; i < n; ++i) {
                ValidateDiagonal(correlations, i);
                for (int j = 0; j <= i; ++j) {
                    ValidateSymmetricEntry(correlations, i, j);
                    double sum = correlations(i, j);
                    for (int k = 0; k < j; ++k)
                        sum -= lower(i, k) * lower(j, k);
                    if (i == j) {
                        REQUIRE(std::isfinite(sum) && sum > 1e-14, "InvalidHybridCorrelation: matrix must be positive definite");
                        lower(i, j) = std::sqrt(sum);
                    } else
                        lower(i, j) = sum / lower(j, j);
                }
            }
            return lower;
        }

    public:
        ConstantFactorCorrelation_(Vector_<String_> registry, const Vector_<String_>& inputNames, const Matrix_<>& input)
            : factorNames_(std::move(registry)), lower_(Factorize(OrderedMatrix(factorNames_, inputNames, input))) {}
        [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factorNames_; }
        [[nodiscard]] const Matrix_<>& LowerAt(size_t) const override { return lower_; }
    };

    inline std::shared_ptr<const FactorCorrelation_> CreateHybridCorrelation(const HybridCorrelationData_& data, const Vector_<String_>& registry) {
        if (const auto* constant = dynamic_cast<const HybridConstantCorrelationData_*>(&data))
            return std::make_shared<ConstantFactorCorrelation_>(registry, constant->factorNames_, constant->correlations_);
        THROW("UnsupportedHybridCorrelation: " + data.Type());
    }

    namespace AAD {
        template <class T_> class HybridComponent_ {
        public:
            static void* operator new(size_t bytes) { return Dal::Detail::AllocateBufferObject(bytes); }
            static void* operator new(size_t bytes, std::align_val_t alignment) { return Dal::Detail::AllocateBufferObject(bytes, alignment); }
            static void operator delete(void* storage) noexcept { Dal::Detail::DeallocateBufferObject(storage); }
            static void operator delete(void* storage, std::align_val_t alignment) noexcept { Dal::Detail::DeallocateBufferObject(storage, alignment); }
            static void* operator new(size_t bytes, const std::nothrow_t&) noexcept { return Dal::Detail::AllocateBufferObjectNothrow(bytes); }
            static void* operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept {
                return Dal::Detail::AllocateBufferObjectNothrow(bytes, alignment);
            }
            static void operator delete(void* storage, const std::nothrow_t&) noexcept { Dal::Detail::DeallocateBufferObject(storage); }
            static void operator delete(void* storage, std::align_val_t alignment, const std::nothrow_t&) noexcept {
                Dal::Detail::DeallocateBufferObject(storage, alignment);
            }
            static void* operator new(size_t, void* storage) noexcept { return storage; }
            static void* operator new(size_t, std::align_val_t, void* storage) noexcept { return storage; }
            static void operator delete(void*, void*) noexcept {}
            static void operator delete(void*, std::align_val_t, void*) noexcept {}

            virtual ~HybridComponent_() = default;
            [[nodiscard]] virtual const String_& Name() const = 0;
            [[nodiscard]] virtual const String_& Currency() const = 0;
            [[nodiscard]] virtual size_t StateDim() const = 0;
            [[nodiscard]] virtual size_t FactorDim() const = 0;
            [[nodiscard]] virtual size_t AuxiliaryFactorDim() const { return 0; }
            [[nodiscard]] virtual const Vector_<String_>& FactorNames() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& ObservableNames() const = 0;
            [[nodiscard]] virtual const Vector_<T_*>& Parameters() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& ParameterLabels() const = 0;
            [[nodiscard]] virtual bool ValidParameterValue(size_t parameter, double value) const {
                return parameter < Parameters().size() && std::isfinite(value);
            }
            [[nodiscard]] virtual bool ProvidesNumeraire() const { return false; }
            [[nodiscard]] virtual bool NumeraireIsDeterministic() const { return false; }
            //  Discount factors for delayed PAYS ... ON payments: the domestic rate component
            //  declares support, optionally registers requested maturities during Allocate
            //  (state-dependent kernels need preparation) and fills them per sample
            [[nodiscard]] virtual bool ProvidesDiscountFactors() const { return false; }
            virtual void RegisterDiscounts(size_t, const Vector_<>&) {}
            virtual void FillDiscounts(size_t sample,
                                       double sampleTime,
                                       const Vector_<>& maturities,
                                       const Vector_<T_>& state,
                                       size_t stateOffset,
                                       Vector_<T_>* discounts) const {
                static_cast<void>(sample);
                static_cast<void>(sampleTime);
                static_cast<void>(maturities);
                static_cast<void>(state);
                static_cast<void>(stateOffset);
                static_cast<void>(discounts);
                THROW("InvalidHybridDiscounts: component does not provide discount factors");
            }
            [[nodiscard]] virtual std::optional<Date_> EvaluationDate() const { return std::nullopt; }
            [[nodiscard]] virtual bool SupportsIndex(const Index_&) const { return false; }
            [[nodiscard]] virtual double MaxStep() const { return std::numeric_limits<double>::infinity(); }
            // Euler-type kernels need their piecewise breakpoints on the shared timeline; exact
            // integration kernels (plain GSR) integrate interior breakpoints inside each step.
            [[nodiscard]] virtual Vector_<> TimelineKnots() const { return {}; }
            virtual void BeginAllocate(const Vector_<>&) {}
            virtual size_t RegisterObservation(size_t, const String_&) { THROW("InvalidHybridObservation: component cannot register indices"); }
            [[nodiscard]] virtual T_ DomesticRate() const { THROW("InvalidHybridNumeraire: component does not provide a rate"); }
            [[nodiscard]] virtual T_ Numeraire(double) const { THROW("InvalidHybridNumeraire: component does not provide a numeraire"); }
            [[nodiscard]] virtual T_ LogDiscount(double time) const { return -DomesticRate() * time; }
            [[nodiscard]] virtual T_ PathLogNumeraire(double time, const Vector_<T_>&, size_t) const { return -LogDiscount(time); }
            virtual void BeginInit() {}
            virtual void PrepareCorrelation(const Matrix_<>& factorCorrelations) { static_cast<void>(factorCorrelations); }
            virtual void Prepare(const Vector_<>& timeline, const Vector_<T_>& integratedCarry) = 0;
            virtual void ResetState(Vector_<T_>* state, size_t offset) const = 0;
            virtual void Evolve(size_t step,
                                const Vector_<>& factors,
                                const Vector_<size_t>& factorSlots,
                                const T_& carryAdjustment,
                                Vector_<T_>* state,
                                size_t stateOffset) const = 0;
            [[nodiscard]] virtual T_ Observe(size_t sample, size_t slot, const Vector_<T_>& state, size_t stateOffset, bool today) const = 0;
            [[nodiscard]] virtual std::unique_ptr<HybridComponent_<T_>> Clone() const = 0;
        };

        template <class T_> class HybridBSEquity_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            T_ spot_;
            T_ vol_;
            T_ div_;
            Vector_<T_> drifts_;
            Vector_<T_> stds_;
            Vector_<T_*> parameters_;
            Vector_<String_> labels_;

            void SetParameterPointers() { parameters_ = {&spot_, &vol_, &div_}; }
            void ValidateParameters() const {
                REQUIRE(std::isfinite(Value(spot_)) && Value(spot_) > 0.0 && std::isfinite(Value(vol_)) && Value(vol_) >= 0.0 &&
                            std::isfinite(Value(div_)),
                        "InvalidHybridComponent: invalid BS equity parameters for " + name_);
            }

        public:
            HybridBSEquity_(const HybridBSEquity_& other) { *this = other; }
            HybridBSEquity_& operator=(const HybridBSEquity_& other) {
                if (this != &other) {
                    name_ = other.name_;
                    currency_ = other.currency_;
                    factors_ = other.factors_;
                    observables_ = other.observables_;
                    spot_ = other.spot_;
                    vol_ = other.vol_;
                    div_ = other.div_;
                    drifts_ = other.drifts_;
                    stds_ = other.stds_;
                    labels_ = other.labels_;
                    SetParameterPointers();
                }
                return *this;
            }
            explicit HybridBSEquity_(const HybridBSEquityData_& data)
                : name_(data.Name()), currency_(data.currency_), factors_({data.factor_}), observables_({data.index_}), spot_(data.spot_),
                  vol_(data.vol_), div_(data.div_), labels_(data.RiskLabels()) {
                SetParameterPointers();
                ValidateParameters();
            }
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return 1; }
            [[nodiscard]] size_t FactorDim() const override { return 1; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
            [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
                return HybridComponent_<T_>::ValidParameterValue(parameter, value) && (parameter != 0 || value > 0.0) &&
                       (parameter != 1 || value >= 0.0);
            }
            void Prepare(const Vector_<>& timeline, const Vector_<T_>& integratedCarry) override {
                ValidateParameters();
                REQUIRE(integratedCarry.size() + 1 == timeline.size(), "InvalidHybridCurve: carry must match the model timeline");
                drifts_.Resize(timeline.size() - 1);
                stds_.Resize(timeline.size() - 1);
                for (size_t step = 0; step + 1 < timeline.size(); ++step) {
                    const double dt = timeline[step + 1] - timeline[step];
                    drifts_[step] = integratedCarry[step] - (div_ + 0.5 * vol_ * vol_) * dt;
                    stds_[step] = vol_ * Dal::sqrt(dt);
                    REQUIRE(std::isfinite(Value(drifts_[step])) && std::isfinite(Value(stds_[step])),
                            "InvalidHybridComponent: non-finite BS step for " + name_);
                }
            }
            void ResetState(Vector_<T_>* state, size_t offset) const override { (*state)[offset] = Dal::log(spot_); }
            void Evolve(size_t step,
                        const Vector_<>& factors,
                        const Vector_<size_t>& factorSlots,
                        const T_& carryAdjustment,
                        Vector_<T_>* state,
                        size_t stateOffset) const override {
                (*state)[stateOffset] += drifts_[step] + carryAdjustment + stds_[step] * factors[factorSlots[0]];
            }
            [[nodiscard]] T_ Observe(size_t, size_t slot, const Vector_<T_>& state, size_t stateOffset, bool today) const override {
                REQUIRE(slot == 0, "InvalidHybridObservation: BS equity has one output");
                if (today)
                    return spot_;
                return Dal::exp(state[stateOffset]);
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override { return std::make_unique<HybridBSEquity_<T_>>(*this); }
        };

        template <class T_> class HybridLocalVolEquity_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            T_ spot_;
            T_ div_;
            LocalVolSurface_<T_> surface_;
            double maxStep_;
            Vector_<> timeline_;
            Vector_<T_> integratedCarry_;
            Vector_<T_*> parameters_;
            Vector_<String_> labels_;

            void CopyState(const HybridLocalVolEquity_& other) {
                name_ = other.name_;
                currency_ = other.currency_;
                factors_ = other.factors_;
                observables_ = other.observables_;
                spot_ = other.spot_;
                div_ = other.div_;
                maxStep_ = other.maxStep_;
                timeline_ = other.timeline_;
                integratedCarry_ = other.integratedCarry_;
                labels_ = other.labels_;
                SetParameterPointers();
            }

            void SetParameterPointers() {
                parameters_ = {&spot_, &div_};
                parameters_.Append(surface_.Parameters());
            }
            void ValidateParameters() const {
                REQUIRE(std::isfinite(Value(spot_)) && Value(spot_) > 0.0 && std::isfinite(Value(div_)),
                        "InvalidHybridComponent: invalid local-vol equity spot or dividend for " + name_);
                for (const auto* vol : surface_.Parameters())
                    REQUIRE(std::isfinite(Value(*vol)) && Value(*vol) >= 0.0,
                            "InvalidHybridComponent: local volatility must be finite and nonnegative for " + name_);
            }

        public:
            HybridLocalVolEquity_(const HybridLocalVolEquity_& other) : surface_(other.surface_) { CopyState(other); }
            HybridLocalVolEquity_& operator=(const HybridLocalVolEquity_& other) {
                if (this != &other) {
                    surface_ = other.surface_;
                    CopyState(other);
                }
                return *this;
            }
            explicit HybridLocalVolEquity_(const HybridLocalVolEquityData_& data)
                : name_(data.Name()), currency_(data.currency_), factors_({data.factor_}), observables_({data.index_}), spot_(data.spot_),
                  div_(data.div_), surface_(*data.surface_), maxStep_(data.maxStep_), labels_(data.RiskLabels()) {
                SetParameterPointers();
                ValidateParameters();
            }
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return 1; }
            [[nodiscard]] size_t FactorDim() const override { return 1; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
            [[nodiscard]] double MaxStep() const override { return maxStep_; }
            [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
                return HybridComponent_<T_>::ValidParameterValue(parameter, value) && (parameter != 0 || value > 0.0) &&
                       (parameter < 2 || value >= 0.0);
            }
            void Prepare(const Vector_<>& timeline, const Vector_<T_>& integratedCarry) override {
                ValidateParameters();
                REQUIRE(integratedCarry.size() + 1 == timeline.size(), "InvalidHybridCurve: carry must match the model timeline");
                timeline_ = timeline;
                integratedCarry_ = integratedCarry;
            }
            void ResetState(Vector_<T_>* state, size_t offset) const override { (*state)[offset] = Dal::log(spot_); }
            void Evolve(size_t step,
                        const Vector_<>& factors,
                        const Vector_<size_t>& factorSlots,
                        const T_& carryAdjustment,
                        Vector_<T_>* state,
                        size_t stateOffset) const override {
                const double dt = timeline_[step + 1] - timeline_[step];
                const T_ vol = surface_.Vol(timeline_[step], Dal::exp((*state)[stateOffset]));
                const T_ standardDeviation = vol * Dal::sqrt(dt);
                (*state)[stateOffset] += integratedCarry_[step] + carryAdjustment - div_ * dt - 0.5 * standardDeviation * standardDeviation +
                                         standardDeviation * factors[factorSlots[0]];
            }
            [[nodiscard]] T_ Observe(size_t, size_t slot, const Vector_<T_>& state, size_t stateOffset, bool today) const override {
                REQUIRE(slot == 0, "InvalidHybridObservation: local-vol equity has one output");
                if (today)
                    return spot_;
                return Dal::exp(state[stateOffset]);
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override { return std::make_unique<HybridLocalVolEquity_<T_>>(*this); }
        };

        template <class T_> class HybridGSRRate_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            Vector_<SampleDef_> definitions_;
            Vector_<> timelineKnots_;
            std::unique_ptr<GSR_<T_>> model_;

        public:
            explicit HybridGSRRate_(const HybridGSRRateData_& data)
                : name_(data.Name()), currency_(data.currency_), factors_(data.FactorNames()),
                  model_(data.multiVol_ ? std::make_unique<GSR_<T_>>(MultiFactorGSRModelData_(data.Name(), data.curve_, data.multiVol_))
                                        : std::make_unique<GSR_<T_>>(GSRModelData_(data.Name(), data.curve_, data.vol_))) {
                REQUIRE(model_->NumFactors() == factors_.size(), "InvalidHybridFactor: factor labels must match the Gaussian kernel");
                REQUIRE(model_->HybridFactorsInvertible(), "InvalidGSRFactors: factor correlations must be positive definite");
                timelineKnots_ = model_->RateKnots((data.curve_->nodeDates_.back() - data.curve_->evaluationDate_) / DAYS_PER_YEAR);
            }
            HybridGSRRate_(const HybridGSRRate_& other)
                : name_(other.name_), currency_(other.currency_), factors_(other.factors_), observables_(other.observables_),
                  definitions_(other.definitions_), timelineKnots_(other.timelineKnots_), model_(other.model_->CloneRateKernel()) {}
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return model_->NumFactors() + 1; }
            [[nodiscard]] size_t FactorDim() const override { return model_->NumFactors(); }
            [[nodiscard]] size_t AuxiliaryFactorDim() const override { return 1; }
            [[nodiscard]] Vector_<> TimelineKnots() const override { return timelineKnots_; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return model_->Parameters(); }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return model_->ParameterLabels(); }
            [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
                return model_->ValidParameterValue(parameter, value);
            }
            [[nodiscard]] bool ProvidesNumeraire() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return model_->NumeraireIsDeterministic(); }
            [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return model_->EvaluationDate(); }
            [[nodiscard]] bool SupportsIndex(const Index_& index) const override { return model_->SupportsIndex(index); }
            [[nodiscard]] bool ProvidesDiscountFactors() const override { return true; }
            [[nodiscard]] T_ LogDiscount(double time) const override { return model_->InitialLogDiscount(time); }
            [[nodiscard]] T_ PathLogNumeraire(double, const Vector_<T_>& state, size_t offset) const override {
                return state[offset + model_->NumFactors()];
            }
            void BeginInit() override { model_->ResetAnchorsForRecording(); }
            void BeginAllocate(const Vector_<>& timeline) override {
                definitions_.Resize(timeline.size());
                for (auto& definition : definitions_) {
                    definition.indexNames_.clear();
                    definition.discountMats_.clear();
                    definition.numeraire_ = true;
                }
            }
            size_t RegisterObservation(size_t sample, const String_& name) override {
                REQUIRE(sample < definitions_.size(), "InvalidHybridObservation: rate sample index is outside the timeline");
                auto& names = definitions_[sample].indexNames_;
                names.push_back(name);
                return names.size() - 1;
            }
            void RegisterDiscounts(size_t sample, const Vector_<>& maturities) override {
                REQUIRE(sample < definitions_.size(), "InvalidHybridDiscounts: rate sample index is outside the timeline");
                definitions_[sample].discountMats_.Append(maturities);
            }
            void PrepareCorrelation(const Matrix_<>& factorCorrelations) override {
                REQUIRE(factorCorrelations.Rows() == static_cast<int>(FactorDim()) && factorCorrelations.Cols() == static_cast<int>(FactorDim()),
                        "InvalidHybridCorrelation: correlation block must match the Gaussian factors");
                const auto& kernel = model_->FactorCorrelations();
                for (int i = 0; i < factorCorrelations.Rows(); ++i)
                    for (int j = 0; j < factorCorrelations.Cols(); ++j)
                        REQUIRE(std::abs(factorCorrelations(i, j) - kernel(i, j)) <= 1e-10,
                                "InvalidHybridCorrelation: hybrid factor correlations must match the Gaussian kernel");
            }
            void Prepare(const Vector_<>& timeline, const Vector_<T_>&) override {
                model_->Allocate(timeline, definitions_);
                model_->InitHybrid(timeline, definitions_);
            }
            void ResetState(Vector_<T_>* state, size_t offset) const override {
                for (size_t i = 0; i <= model_->NumFactors(); ++i)
                    (*state)[offset + i] = T_(0.0);
            }
            void Evolve(size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, const T_&, Vector_<T_>* state, size_t stateOffset)
                const override {
                model_->AdvanceHybrid(step + 1, factors, factorSlots, &(*state)[stateOffset], &(*state)[stateOffset + model_->NumFactors()]);
            }
            [[nodiscard]] T_ Observe(size_t sample, size_t slot, const Vector_<T_>& state, size_t stateOffset, bool) const override {
                return model_->ObserveHybrid(sample, slot, &state[stateOffset]);
            }
            void FillDiscounts(size_t sample,
                               double,
                               const Vector_<>&,
                               const Vector_<T_>& state,
                               size_t stateOffset,
                               Vector_<T_>* discounts) const override {
                model_->ObserveHybridDiscounts(sample, &state[stateOffset], discounts);
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override { return std::make_unique<HybridGSRRate_<T_>>(*this); }
        };

        template <class T_> class HybridGSRSLVRate_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            Vector_<SampleDef_> definitions_;
            Vector_<> timelineKnots_;
            Matrix_<> simulationCorrelations_;
            Matrix_<> rateCorrelations_;
            std::unique_ptr<GSRSLV_<T_>> model_;

            static Vector_<> Knots(const GSRSLVModelData_& data) {
                Vector_<> knots;
                // Hybrid timelines subdivide in whole calendar days, so a breakpoint off the day
                // grid would either shift the kernel's piecewise boundary or break path parity
                // with the standalone model; reject it instead of snapping silently.
                for (const double time : data.BreakpointTimes()) {
                    const double days = time * DAYS_PER_YEAR;
                    const double rounded = static_cast<double>(std::llround(days));
                    REQUIRE(std::abs(days - rounded) <= 1e-9, "InvalidGSRSLVHybrid: breakpoints must fall on whole calendar days");
                    knots.push_back(rounded / DAYS_PER_YEAR);
                }
                return knots;
            }

        public:
            explicit HybridGSRSLVRate_(const HybridGSRSLVRateData_& data)
                : name_(data.Name()), currency_(data.currency_), factors_(data.FactorNames()), timelineKnots_(Knots(*data.model_)),
                  simulationCorrelations_(data.model_->FactorCorrelations()), model_(std::make_unique<GSRSLV_<T_>>(*data.model_)) {
                REQUIRE(model_->NumFactors() == factors_.size(), "InvalidHybridFactor: factor labels must match the SLV kernel drivers");
                rateCorrelations_ = Matrix_<>(model_->NumFactors() - 2, model_->NumFactors() - 2, 0.0);
                for (int i = 0; i < rateCorrelations_.Rows(); ++i)
                    for (int j = 0; j < rateCorrelations_.Cols(); ++j)
                        rateCorrelations_(i, j) = simulationCorrelations_(i, j);
            }
            HybridGSRSLVRate_(const HybridGSRSLVRate_& other)
                : name_(other.name_), currency_(other.currency_), factors_(other.factors_), observables_(other.observables_),
                  definitions_(other.definitions_), timelineKnots_(other.timelineKnots_), simulationCorrelations_(other.simulationCorrelations_),
                  rateCorrelations_(other.rateCorrelations_), model_(other.model_->CloneSLVKernel()) {}
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return model_->HybridStateDim(); }
            [[nodiscard]] size_t FactorDim() const override { return factors_.size(); }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return model_->Parameters(); }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return model_->ParameterLabels(); }
            [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
                return model_->ValidParameterValue(parameter, value);
            }
            [[nodiscard]] bool ProvidesNumeraire() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return model_->NumeraireIsDeterministic(); }
            [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return model_->EvaluationDate(); }
            [[nodiscard]] bool SupportsIndex(const Index_& index) const override { return model_->SupportsIndex(index); }
            [[nodiscard]] double MaxStep() const override { return model_->MaxStep(); }
            [[nodiscard]] Vector_<> TimelineKnots() const override { return timelineKnots_; }
            [[nodiscard]] T_ LogDiscount(double time) const override { return model_->InitialLogDiscount(time); }
            [[nodiscard]] T_ PathLogNumeraire(double, const Vector_<T_>& state, size_t offset) const override {
                return model_->HybridLogNumeraire(state, offset);
            }
            void BeginAllocate(const Vector_<>& timeline) override {
                definitions_.Resize(timeline.size());
                for (auto& definition : definitions_) {
                    definition.indexNames_.clear();
                    definition.discountMats_.clear();
                    definition.numeraire_ = true;
                }
            }
            size_t RegisterObservation(size_t sample, const String_& name) override {
                REQUIRE(sample < definitions_.size(), "InvalidHybridObservation: rate sample index is outside the timeline");
                auto& names = definitions_[sample].indexNames_;
                names.push_back(name);
                return names.size() - 1;
            }
            void PrepareCorrelation(const Matrix_<>& factorCorrelations) override {
                const int total = static_cast<int>(FactorDim());
                REQUIRE(factorCorrelations.Rows() == total && factorCorrelations.Cols() == total,
                        "InvalidHybridCorrelation: correlation block must match the SLV drivers");
                for (int i = 0; i < total; ++i)
                    for (int j = 0; j < total; ++j)
                        REQUIRE(std::abs(factorCorrelations(i, j) - simulationCorrelations_(i, j)) <= 1e-10,
                                "InvalidHybridCorrelation: hybrid factor correlations must match the SLV kernel");
            }
            void Prepare(const Vector_<>& timeline, const Vector_<T_>&) override { model_->PrepareHybrid(timeline, definitions_, rateCorrelations_); }
            void ResetState(Vector_<T_>* state, size_t offset) const override { model_->ResetHybridState(state, offset); }
            void Evolve(size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, const T_&, Vector_<T_>* state, size_t stateOffset)
                const override {
                model_->EvolveHybrid(step, factors, factorSlots, state, stateOffset);
            }
            [[nodiscard]] T_ Observe(size_t sample, size_t slot, const Vector_<T_>& state, size_t stateOffset, bool) const override {
                return model_->ObserveHybrid(sample, slot, state, stateOffset);
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override { return std::make_unique<HybridGSRSLVRate_<T_>>(*this); }
        };

        template <class T_> class HybridDeterministicRate_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            T_ rate_;
            Vector_<T_*> parameters_;
            Vector_<String_> labels_;

            void SetParameterPointers() { parameters_ = {&rate_}; }
            void ValidateRate() const { REQUIRE(std::isfinite(Value(rate_)), "InvalidHybridComponent: non-finite domestic rate for " + name_); }

        public:
            HybridDeterministicRate_(const HybridDeterministicRate_& other) { *this = other; }
            HybridDeterministicRate_& operator=(const HybridDeterministicRate_& other) {
                if (this != &other) {
                    name_ = other.name_;
                    currency_ = other.currency_;
                    factors_ = other.factors_;
                    observables_ = other.observables_;
                    rate_ = other.rate_;
                    labels_ = other.labels_;
                    SetParameterPointers();
                }
                return *this;
            }
            explicit HybridDeterministicRate_(const HybridDeterministicRateData_& data)
                : name_(data.Name()), currency_(data.currency_), rate_(data.rate_), labels_(data.RiskLabels()) {
                SetParameterPointers();
                ValidateRate();
            }
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return 0; }
            [[nodiscard]] size_t FactorDim() const override { return 0; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
            [[nodiscard]] bool ProvidesNumeraire() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return true; }
            [[nodiscard]] bool ProvidesDiscountFactors() const override { return true; }
            [[nodiscard]] T_ DomesticRate() const override {
                ValidateRate();
                return rate_;
            }
            [[nodiscard]] T_ Numeraire(double time) const override { return Dal::exp(rate_ * time); }
            void FillDiscounts(size_t,
                               double sampleTime,
                               const Vector_<>& maturities,
                               const Vector_<T_>&,
                               size_t,
                               Vector_<T_>* discounts) const override {
                for (size_t k = 0; k < maturities.size(); ++k)
                    (*discounts)[k] = Dal::exp(this->LogDiscount(maturities[k]) - this->LogDiscount(sampleTime));
            }
            void Prepare(const Vector_<>&, const Vector_<T_>&) override {}
            void ResetState(Vector_<T_>*, size_t) const override {}
            void Evolve(size_t, const Vector_<>&, const Vector_<size_t>&, const T_&, Vector_<T_>*, size_t) const override {}
            [[nodiscard]] T_ Observe(size_t, size_t, const Vector_<T_>&, size_t, bool) const override {
                THROW("InvalidHybridObservation: rate component has no spot output");
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override {
                return std::make_unique<HybridDeterministicRate_<T_>>(*this);
            }
        };

        template <class T_> class HybridLogDfRate_ final : public HybridComponent_<T_> {
            String_ name_;
            String_ currency_;
            Vector_<String_> factors_;
            Vector_<String_> observables_;
            Vector_<T_> logDF_;
            std::shared_ptr<const LogDfInterpolation_> interpolation_;
            Vector_<T_*> parameters_;
            Vector_<String_> labels_;

            void SetParameterPointers() {
                parameters_.clear();
                for (size_t i = 1; i < logDF_.size(); ++i)
                    parameters_.push_back(&logDF_[i]);
            }

        public:
            HybridLogDfRate_(const HybridLogDfRate_& other) { *this = other; }
            HybridLogDfRate_& operator=(const HybridLogDfRate_& other) {
                if (this != &other) {
                    name_ = other.name_;
                    currency_ = other.currency_;
                    factors_ = other.factors_;
                    observables_ = other.observables_;
                    interpolation_ = other.interpolation_;
                    logDF_ = other.logDF_;
                    labels_ = other.labels_;
                    SetParameterPointers();
                }
                return *this;
            }
            explicit HybridLogDfRate_(const HybridLogDfRateData_& data)
                : name_(data.Name()), currency_(data.currency_),
                  interpolation_(std::make_shared<LogDfInterpolation_>(data.times_, LogDfScheme_(data.scheme_))), labels_(data.RiskLabels()) {
                logDF_.reserve(data.logDF_.size());
                for (const double value : data.logDF_)
                    logDF_.push_back(T_(value));
                SetParameterPointers();
            }
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return 0; }
            [[nodiscard]] size_t FactorDim() const override { return 0; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
            [[nodiscard]] bool ProvidesNumeraire() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return true; }
            [[nodiscard]] bool ProvidesDiscountFactors() const override { return true; }
            [[nodiscard]] T_ LogDiscount(double time) const override {
                // The t=0 anchor is fixed, so it must not retain a tape node from model construction.
                T_ result(0.0);
                for (const auto& [index, weight] : interpolation_->WeightsAt(time))
                    if (index > 0)
                        result += weight * logDF_[index];
                return result;
            }
            [[nodiscard]] T_ Numeraire(double time) const override { return Dal::exp(-LogDiscount(time)); }
            void FillDiscounts(size_t,
                               double sampleTime,
                               const Vector_<>& maturities,
                               const Vector_<T_>&,
                               size_t,
                               Vector_<T_>* discounts) const override {
                for (size_t k = 0; k < maturities.size(); ++k)
                    (*discounts)[k] = Dal::exp(this->LogDiscount(maturities[k]) - this->LogDiscount(sampleTime));
            }
            void Prepare(const Vector_<>&, const Vector_<T_>&) override {}
            void ResetState(Vector_<T_>*, size_t) const override {}
            void Evolve(size_t, const Vector_<>&, const Vector_<size_t>&, const T_&, Vector_<T_>*, size_t) const override {}
            [[nodiscard]] T_ Observe(size_t, size_t, const Vector_<T_>&, size_t, bool) const override {
                THROW("InvalidHybridObservation: rate component has no spot output");
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override { return std::make_unique<HybridLogDfRate_<T_>>(*this); }
        };

        template <class T_ = double> class HybridModel_ final : public Model_<T_> {
            struct OutputSlot_ {
                size_t component_;
                size_t observable_;
            };

            String_ domesticCurrency_;
            Vector_<std::unique_ptr<HybridComponent_<T_>>> components_;
            std::shared_ptr<const FactorCorrelation_> correlation_;
            Vector_<String_> factorNames_;
            Vector_<String_> assetNames_;
            Vector_<Vector_<size_t>> factorSlots_;
            Vector_<size_t> stateOffsets_;
            Vector_<size_t> evolvingComponents_;
            Vector_<T_*> parameters_;
            Vector_<String_> parameterLabels_;
            size_t rateSlot_ = 0;
            size_t spotSlot_ = 0;
            size_t totalState_ = 0;
            size_t totalFactors_ = 0;
            Vector_<> timeLine_;
            Vector_<> productTimeLine_;
            Vector_<size_t> productGridIndices_;
            bool todayOnTimeLine_ = false;
            const Vector_<SampleDef_>* defLine_ = nullptr;
            Vector_<Vector_<OutputSlot_>> outputSlots_;
            Vector_<T_> numeraires_;
            Vector_<T_> initialCarry_;

            void SetCorrelation(std::shared_ptr<const FactorCorrelation_> correlation) {
                REQUIRE(correlation && correlation->FactorNames() == factorNames_,
                        "InvalidHybridCorrelation: provider factors must match the hybrid factor registry");
                correlation_ = std::move(correlation);
                BuildSlotsAndParameters();
            }

            void ValidateComponentIdentity(size_t i) const {
                const auto& component = *components_[i];
                REQUIRE(!component.Name().empty() && component.Currency() == domesticCurrency_,
                        "InvalidHybridCurrency: component " + component.Name() + " must use " + domesticCurrency_);
                REQUIRE(i == 0 || component.Name() != components_[i - 1]->Name(), "DuplicateHybridComponent: " + component.Name());
                REQUIRE(component.FactorNames().size() == component.FactorDim(),
                        "InvalidHybridFactor: factor labels must match the component factor dimension for " + component.Name());
                REQUIRE(component.Parameters().size() == component.ParameterLabels().size(),
                        "InvalidHybridComponent: parameter labels must match parameters for " + component.Name());
            }

            void AddObservableNames(const HybridComponent_<T_>& component) {
                for (const auto& name : component.ObservableNames()) {
                    REQUIRE(std::find(assetNames_.begin(), assetNames_.end(), name) == assetNames_.end(), "DuplicateHybridObservable: " + name);
                    assetNames_.push_back(name);
                }
            }

            void AddFactorNames(const HybridComponent_<T_>& component) {
                for (const auto& name : component.FactorNames()) {
                    REQUIRE(!name.empty() && std::find(factorNames_.begin(), factorNames_.end(), name) == factorNames_.end(),
                            "DuplicateHybridFactor: " + name);
                    factorNames_.push_back(name);
                }
            }

            void RegisterComponent(size_t i, size_t* numNumeraires) {
                const auto& component = *components_[i];
                ValidateComponentIdentity(i);
                if (component.ProvidesNumeraire()) {
                    rateSlot_ = i;
                    ++*numNumeraires;
                }
                if (!component.ObservableNames().empty() && assetNames_.empty())
                    spotSlot_ = i;
                AddObservableNames(component);
                AddFactorNames(component);
            }

            void ValidateAndOrderComponents() {
                REQUIRE(!components_.empty() && !domesticCurrency_.empty(), "InvalidHybridModel: components and domestic currency are required");
                for (const auto& component : components_)
                    REQUIRE(component, "InvalidHybridComponent: null component");
                std::sort(components_.begin(), components_.end(), [](const auto& lhs, const auto& rhs) { return lhs->Name() < rhs->Name(); });
                size_t numNumeraires = 0;
                for (size_t i = 0; i < components_.size(); ++i)
                    RegisterComponent(i, &numNumeraires);
                REQUIRE(numNumeraires == 1, "InvalidHybridNumeraire: exactly one domestic numeraire provider is required");
                REQUIRE(!assetNames_.empty(), "InvalidHybridModel: at least one observable is required");
                std::sort(factorNames_.begin(), factorNames_.end());
            }

            void BuildSlotsAndParameters() {
                totalFactors_ = factorNames_.size();
                stateOffsets_.Resize(components_.size());
                factorSlots_.Resize(components_.size());
                for (size_t i = 0; i < components_.size(); ++i) {
                    const auto& component = *components_[i];
                    stateOffsets_[i] = totalState_;
                    totalState_ += component.StateDim();
                    if (component.StateDim() > 0 || component.FactorDim() > 0)
                        evolvingComponents_.push_back(i);
                    for (const auto& name : component.FactorNames()) {
                        const auto found = std::find(factorNames_.begin(), factorNames_.end(), name);
                        factorSlots_[i].push_back(static_cast<size_t>(found - factorNames_.begin()));
                    }
                    for (size_t auxiliary = 0; auxiliary < component.AuxiliaryFactorDim(); ++auxiliary)
                        factorSlots_[i].push_back(totalFactors_++);
                    parameters_.Append(component.Parameters());
                    parameterLabels_.Append(component.ParameterLabels());
                }
            }

            // Reconstruct each component's correlation block from the global Cholesky factor so
            // multi-factor kernels can validate and consume their intra-factor correlations.
            void DeliverFactorCorrelations() {
                const auto& lower = correlation_->LowerAt(0);
                const int total = static_cast<int>(factorNames_.size());
                Matrix_<> full(total, total, 0.0);
                for (int i = 0; i < total; ++i)
                    for (int j = 0; j < total; ++j) {
                        double value = 0.0;
                        for (int k = 0; k <= std::min(i, j); ++k)
                            value += lower(i, k) * lower(j, k);
                        full(i, j) = value;
                    }
                for (size_t i = 0; i < components_.size(); ++i) {
                    const auto& slots = factorSlots_[i];
                    if (slots.empty())
                        continue;
                    const size_t named = components_[i]->FactorDim();
                    Matrix_<> block(static_cast<int>(named), static_cast<int>(named), 0.0);
                    for (size_t a = 0; a < named; ++a)
                        for (size_t b = 0; b < named; ++b)
                            block(static_cast<int>(a), static_cast<int>(b)) = full(static_cast<int>(slots[a]), static_cast<int>(slots[b]));
                    components_[i]->PrepareCorrelation(block);
                }
            }

            [[nodiscard]] OutputSlot_ FindOutput(size_t gridIndex, const String_& name) {
                const Handle_<Index_> index(Index::Parse(name));
                REQUIRE(index, "UnsupportedModelObservation: " + name);
                for (size_t i = 0; i < components_.size(); ++i) {
                    const auto& observables = components_[i]->ObservableNames();
                    for (size_t j = 0; j < observables.size(); ++j)
                        if (observables[j] == index->Name())
                            return {i, j};
                    if (components_[i]->SupportsIndex(*index))
                        return {i, components_[i]->RegisterObservation(gridIndex, name)};
                }
                THROW("UnsupportedModelObservation: " + name);
            }

            void FillSample(size_t sample, const Vector_<T_>& state, bool today, Sample_<T_>* output) const {
                const size_t gridIndex = productGridIndices_[sample];
                if ((*defLine_)[sample].numeraire_) {
                    if (NumeraireIsDeterministic())
                        output->numeraire_ = numeraires_[sample];
                    else
                        output->numeraire_ =
                            Dal::exp(components_[rateSlot_]->PathLogNumeraire(timeLine_[gridIndex], state, stateOffsets_[rateSlot_]));
                }
                if (!output->discounts_.empty())
                    components_[rateSlot_]->FillDiscounts(gridIndex, timeLine_[gridIndex], (*defLine_)[sample].discountMats_, state,
                                                          stateOffsets_[rateSlot_], &output->discounts_);
                output->spot_ = components_[spotSlot_]->Observe(gridIndex, 0, state, stateOffsets_[spotSlot_], today);
                for (size_t i = 0; i < outputSlots_[sample].size(); ++i) {
                    const auto slot = outputSlots_[sample][i];
                    if (slot.component_ == spotSlot_ && slot.observable_ == 0)
                        output->observations_[i] = output->spot_;
                    else
                        output->observations_[i] =
                            components_[slot.component_]->Observe(gridIndex, slot.observable_, state, stateOffsets_[slot.component_], today);
                }
            }

            void Advance(size_t step, const Vector_<>& gaussian, Vector_<>* factors, Vector_<T_>* state) const {
                const auto& lower = correlation_->LowerAt(step);
                const size_t n = factorNames_.size();
                for (size_t i = 0; i < n; ++i) {
                    double correlated = 0.0;
                    for (size_t j = 0; j <= i; ++j)
                        correlated += lower(static_cast<int>(i), static_cast<int>(j)) * gaussian[step * totalFactors_ + j];
                    (*factors)[i] = correlated;
                }
                for (size_t i = n; i < totalFactors_; ++i)
                    (*factors)[i] = gaussian[step * totalFactors_ + i];
                T_ previousLogNumeraire(0.0);
                if (!NumeraireIsDeterministic())
                    previousLogNumeraire = components_[rateSlot_]->PathLogNumeraire(timeLine_[step], *state, stateOffsets_[rateSlot_]);
                components_[rateSlot_]->Evolve(step, *factors, factorSlots_[rateSlot_], T_(0.0), state, stateOffsets_[rateSlot_]);
                T_ carryAdjustment(0.0);
                if (!NumeraireIsDeterministic())
                    carryAdjustment = components_[rateSlot_]->PathLogNumeraire(timeLine_[step + 1], *state, stateOffsets_[rateSlot_]) -
                                      previousLogNumeraire - initialCarry_[step];
                for (const size_t i : evolvingComponents_)
                    if (i != rateSlot_)
                        components_[i]->Evolve(step, *factors, factorSlots_[i], carryAdjustment, state, stateOffsets_[i]);
            }

        public:
            HybridModel_(String_ domesticCurrency,
                         Vector_<std::unique_ptr<HybridComponent_<T_>>> components,
                         std::shared_ptr<const FactorCorrelation_> correlation)
                : domesticCurrency_(std::move(domesticCurrency)), components_(std::move(components)) {
                ValidateAndOrderComponents();
                SetCorrelation(std::move(correlation));
            }
            HybridModel_(String_ domesticCurrency,
                         Vector_<std::unique_ptr<HybridComponent_<T_>>> components,
                         const HybridCorrelationData_& correlationData)
                : domesticCurrency_(std::move(domesticCurrency)), components_(std::move(components)) {
                ValidateAndOrderComponents();
                SetCorrelation(CreateHybridCorrelation(correlationData, factorNames_));
            }
            [[nodiscard]] bool SupportsIndex(const Index_& index) const override {
                if (std::find(assetNames_.begin(), assetNames_.end(), index.Name()) != assetNames_.end())
                    return true;
                for (const auto& component : components_)
                    if (component->SupportsIndex(index))
                        return true;
                return false;
            }
            [[nodiscard]] size_t MaxObservedIndices() const override { return std::numeric_limits<size_t>::max(); }
            [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
            [[nodiscard]] size_t NumFactors() const override { return totalFactors_; }
            [[nodiscard]] size_t StateDim() const { return totalState_; }
            [[nodiscard]] bool SupportsBrownianBridge() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return components_[rateSlot_]->NumeraireIsDeterministic(); }
            [[nodiscard]] bool SupportsDiscountFactors() const override { return components_[rateSlot_]->ProvidesDiscountFactors(); }
            [[nodiscard]] std::optional<Date_> EvaluationDate() const override { return components_[rateSlot_]->EvaluationDate(); }
            [[nodiscard]] bool ValidParameterValue(size_t parameter, double value) const override {
                if (!Model_<T_>::ValidParameterValue(parameter, value))
                    return false;
                for (const auto& component : components_) {
                    if (parameter < component->Parameters().size())
                        return component->ValidParameterValue(parameter, value);
                    parameter -= component->Parameters().size();
                }
                return false;
            }
            [[nodiscard]] size_t NumAssets() const override { return assetNames_.size(); }
            [[nodiscard]] const Vector_<String_>& AssetNames() const override { return assetNames_; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const { return factorNames_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return parameterLabels_; }
            [[nodiscard]] std::unique_ptr<Model_<T_>> Clone() const override {
                Vector_<std::unique_ptr<HybridComponent_<T_>>> copies;
                for (const auto& component : components_)
                    copies.push_back(component->Clone());
                auto copy = std::make_unique<HybridModel_<T_>>(domesticCurrency_, std::move(copies), correlation_);
                copy->timeLine_ = timeLine_;
                copy->productTimeLine_ = productTimeLine_;
                copy->productGridIndices_ = productGridIndices_;
                copy->todayOnTimeLine_ = todayOnTimeLine_;
                copy->defLine_ = defLine_;
                copy->outputSlots_ = outputSlots_;
                copy->numeraires_ = numeraires_;
                copy->initialCarry_ = initialCarry_;
                return copy;
            }
            [[nodiscard]] Vector_<> GridAnchors(const Vector_<>& productTimeLine) const {
                Vector_<> anchors(productTimeLine);
                for (const auto& component : components_)
                    for (const double knot : component->TimelineKnots())
                        if (std::isfinite(knot) && knot > 0.0 && knot < productTimeLine.back())
                            anchors.push_back(knot);
                std::sort(anchors.begin(), anchors.end());
                anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
                return anchors;
            }

            void AppendSubSteps(double from, double to, double maxStep) {
                if (!std::isfinite(maxStep))
                    return;
                if (!EvaluationDate()) {
                    const double steps = std::ceil((to - from) / maxStep);
                    REQUIRE(steps <= 1000000.0, "InvalidHybridTimeline: local-vol step limit exceeded");
                    const size_t count = static_cast<size_t>(steps);
                    for (size_t i = 1; i < count; ++i)
                        timeLine_.push_back(from + (to - from) * i / count);
                    return;
                }
                const double startDays = from * 365.0;
                const double endDays = to * 365.0;
                REQUIRE(std::abs(startDays - std::round(startDays)) <= 1e-7 && std::abs(endDays - std::round(endDays)) <= 1e-7,
                        "InvalidHybridTimeline: GSR samples require whole calendar days on ACT/365 axis");
                const int stepDays = std::max(1, static_cast<int>(std::floor(std::min(maxStep, to - from) * 365.0)));
                for (int day = static_cast<int>(std::round(startDays)) + stepDays; day < static_cast<int>(std::round(endDays)); day += stepDays)
                    timeLine_.push_back(day / 365.0);
            }

            void Allocate(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                this->ValidateTimeline(productTimeLine, defLine);
                defLine_ = &defLine;
                productTimeLine_ = productTimeLine;
                timeLine_ = {0.0};
                productGridIndices_.Resize(productTimeLine.size());
                double maxStep = std::numeric_limits<double>::infinity();
                for (const auto& component : components_)
                    maxStep = std::min(maxStep, component->MaxStep());
                REQUIRE(!EvaluationDate() || maxStep >= 1.0 / DAYS_PER_YEAR,
                        "InvalidHybridTimeline: maximum step must be at least one calendar day for dated rate models");
                size_t productSample = 0;
                for (const double time : GridAnchors(productTimeLine)) {
                    if (time > timeLine_.back()) {
                        AppendSubSteps(timeLine_.back(), time, maxStep);
                        timeLine_.push_back(time);
                    }
                    while (productSample < productTimeLine.size() && productTimeLine[productSample] == time) {
                        productGridIndices_[productSample] = timeLine_.size() - 1;
                        ++productSample;
                    }
                }
                REQUIRE(productSample == productTimeLine.size(), "InvalidHybridTimeline: not every product sample reached the grid");
                todayOnTimeLine_ = productTimeLine[0] == 0.0;
                for (auto& component : components_)
                    component->BeginAllocate(timeLine_);
                outputSlots_.Resize(defLine.size());
                for (size_t sample = 0; sample < defLine.size(); ++sample) {
                    outputSlots_[sample].clear();
                    for (const auto& name : defLine[sample].indexNames_)
                        outputSlots_[sample].push_back(FindOutput(productGridIndices_[sample], name));
                }
                for (size_t sample = 0; sample < defLine.size(); ++sample) {
                    if (defLine[sample].discountMats_.empty())
                        continue;
                    REQUIRE(components_[rateSlot_]->ProvidesDiscountFactors(),
                            "InvalidHybridDiscounts: the domestic rate component does not provide discount factors");
                    components_[rateSlot_]->RegisterDiscounts(productGridIndices_[sample], defLine[sample].discountMats_);
                }
                numeraires_.Resize(defLine.size());
            }
            void Init(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                REQUIRE(defLine_ == &defLine && productTimeLine == productTimeLine_, "InvalidHybridTimeline: call Allocate before Init");
                for (auto& component : components_)
                    component->BeginInit();
                DeliverFactorCorrelations();
                Vector_<T_> logDiscounts(timeLine_.size());
                for (size_t i = 0; i < timeLine_.size(); ++i) {
                    logDiscounts[i] = components_[rateSlot_]->LogDiscount(timeLine_[i]);
                    REQUIRE(std::isfinite(Value(logDiscounts[i])), "InvalidHybridCurve: non-finite logDF on model timeline");
                }
                Vector_<T_> integratedCarry(timeLine_.size() - 1);
                for (size_t step = 0; step < integratedCarry.size(); ++step)
                    integratedCarry[step] = logDiscounts[step] - logDiscounts[step + 1];
                initialCarry_ = integratedCarry;
                for (auto& component : components_)
                    component->Prepare(timeLine_, integratedCarry);
                for (size_t sample = 0; sample < defLine.size(); ++sample)
                    if (defLine[sample].numeraire_) {
                        numeraires_[sample] = Dal::exp(-logDiscounts[productGridIndices_[sample]]);
                        REQUIRE(std::isfinite(Value(numeraires_[sample])) && Value(numeraires_[sample]) > 0.0,
                                "InvalidHybridNumeraire: non-finite or zero domestic numeraire");
                    }
            }
            [[nodiscard]] size_t SimDim() const override { return (timeLine_.size() - 1) * totalFactors_; }
            void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
                REQUIRE(defLine_ && path && path->size() == defLine_->size() && gaussian.size() == SimDim(),
                        "InvalidHybridPath: path or Gaussian dimension mismatch");
                auto* state = &(*path)[0].modelScratch_;
                auto* factors = &(*path)[0].modelFactorScratch_;
                state->Resize(totalState_);
                factors->Resize(totalFactors_);
                for (size_t i = 0; i < components_.size(); ++i)
                    components_[i]->ResetState(state, stateOffsets_[i]);
                size_t sample = 0;
                if (todayOnTimeLine_)
                    FillSample(sample++, *state, true, &(*path)[0]);
                for (size_t step = 0; step + 1 < timeLine_.size(); ++step) {
                    Advance(step, gaussian, factors, state);
                    if (sample < productGridIndices_.size() && productGridIndices_[sample] == step + 1) {
                        FillSample(sample, *state, false, &(*path)[sample]);
                        ++sample;
                    }
                }
                REQUIRE(sample == productGridIndices_.size(), "InvalidHybridPath: not all product samples were generated");
            }
        };

        template <class T_> std::unique_ptr<HybridComponent_<T_>> CreateHybridComponent(const HybridComponentData_& data) {
            if (const auto* equity = dynamic_cast<const HybridBSEquityData_*>(&data))
                return std::make_unique<HybridBSEquity_<T_>>(*equity);
            if (const auto* equity = dynamic_cast<const HybridLocalVolEquityData_*>(&data))
                return std::make_unique<HybridLocalVolEquity_<T_>>(*equity);
            if (const auto* rate = dynamic_cast<const HybridGSRRateData_*>(&data))
                return std::make_unique<HybridGSRRate_<T_>>(*rate);
            if (const auto* slvRate = dynamic_cast<const HybridGSRSLVRateData_*>(&data))
                return std::make_unique<HybridGSRSLVRate_<T_>>(*slvRate);
            if (const auto* rate = dynamic_cast<const HybridDeterministicRateData_*>(&data))
                return std::make_unique<HybridDeterministicRate_<T_>>(*rate);
            if (const auto* curve = dynamic_cast<const HybridLogDfRateData_*>(&data))
                return std::make_unique<HybridLogDfRate_<T_>>(*curve);
            THROW("UnsupportedHybridComponent: " + data.Type());
        }
    } // namespace AAD

} // namespace Dal
