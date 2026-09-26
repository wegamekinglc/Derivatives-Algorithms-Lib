//
// Created by Codex on 2026/9/27.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

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

        static Matrix_<> OrderedMatrix(const Vector_<String_>& registry, const Vector_<String_>& inputNames, const Matrix_<>& input) {
            const int n = static_cast<int>(registry.size());
            REQUIRE(n > 0 && inputNames.size() == registry.size() && input.Rows() == n && input.Cols() == n,
                    "InvalidHybridCorrelation: factor names and matrix dimensions must match the registry");
            for (size_t i = 0; i < inputNames.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    REQUIRE(inputNames[i] != inputNames[j], "InvalidHybridCorrelation: duplicate factor " + inputNames[i]);
            Vector_<int> slots;
            for (const auto& name : registry) {
                const auto found = std::find(inputNames.begin(), inputNames.end(), name);
                REQUIRE(found != inputNames.end(), "InvalidHybridCorrelation: missing factor " + name);
                slots.push_back(static_cast<int>(found - inputNames.begin()));
            }
            Matrix_<> ordered(n, n, 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    ordered(i, j) = input(slots[i], slots[j]);
            return ordered;
        }

        static Matrix_<> Factorize(const Matrix_<>& correlations) {
            const int n = correlations.Rows();
            Matrix_<> lower(n, n, 0.0);
            for (int i = 0; i < n; ++i) {
                REQUIRE(std::isfinite(correlations(i, i)) && std::abs(correlations(i, i) - 1.0) <= 1e-12,
                        "InvalidHybridCorrelation: diagonal entries must equal one");
                for (int j = 0; j <= i; ++j) {
                    REQUIRE(std::isfinite(correlations(i, j)) && std::isfinite(correlations(j, i)) &&
                                std::abs(correlations(i, j) - correlations(j, i)) <= 1e-12,
                            "InvalidHybridCorrelation: matrix must be finite and symmetric");
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
            virtual ~HybridComponent_() = default;
            [[nodiscard]] virtual const String_& Name() const = 0;
            [[nodiscard]] virtual const String_& Currency() const = 0;
            [[nodiscard]] virtual size_t StateDim() const = 0;
            [[nodiscard]] virtual size_t FactorDim() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& FactorNames() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& ObservableNames() const = 0;
            [[nodiscard]] virtual const Vector_<T_*>& Parameters() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& ParameterLabels() const = 0;
            [[nodiscard]] virtual bool ProvidesNumeraire() const { return false; }
            [[nodiscard]] virtual bool NumeraireIsDeterministic() const { return false; }
            [[nodiscard]] virtual T_ DomesticRate() const { THROW("InvalidHybridNumeraire: component does not provide a rate"); }
            [[nodiscard]] virtual T_ Numeraire(double) const { THROW("InvalidHybridNumeraire: component does not provide a numeraire"); }
            virtual void Prepare(const Vector_<>& timeline, const T_& domesticRate) = 0;
            virtual void ResetState(Vector_<T_>* state, size_t offset) const = 0;
            virtual void
            Evolve(size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, Vector_<T_>* state, size_t stateOffset) const = 0;
            [[nodiscard]] virtual T_ Observe(size_t slot, const Vector_<T_>& state, size_t stateOffset, bool today) const = 0;
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

        public:
            explicit HybridBSEquity_(const HybridBSEquityData_& data)
                : name_(data.Name()), currency_(data.currency_), factors_({data.factor_}), observables_({data.index_}), spot_(data.spot_),
                  vol_(data.vol_), div_(data.div_), labels_(data.RiskLabels()) {
                SetParameterPointers();
                REQUIRE(std::isfinite(Value(spot_)) && Value(spot_) > 0.0 && std::isfinite(Value(vol_)) && Value(vol_) >= 0.0 &&
                            std::isfinite(Value(div_)),
                        "InvalidHybridComponent: invalid BS equity parameters for " + name_);
            }
            [[nodiscard]] const String_& Name() const override { return name_; }
            [[nodiscard]] const String_& Currency() const override { return currency_; }
            [[nodiscard]] size_t StateDim() const override { return 1; }
            [[nodiscard]] size_t FactorDim() const override { return 1; }
            [[nodiscard]] const Vector_<String_>& FactorNames() const override { return factors_; }
            [[nodiscard]] const Vector_<String_>& ObservableNames() const override { return observables_; }
            [[nodiscard]] const Vector_<T_*>& Parameters() const override { return parameters_; }
            [[nodiscard]] const Vector_<String_>& ParameterLabels() const override { return labels_; }
            void Prepare(const Vector_<>& timeline, const T_& domesticRate) override {
                drifts_.Resize(timeline.size() - 1);
                stds_.Resize(timeline.size() - 1);
                for (size_t step = 0; step + 1 < timeline.size(); ++step) {
                    const double dt = timeline[step + 1] - timeline[step];
                    drifts_[step] = (domesticRate - div_ - 0.5 * vol_ * vol_) * dt;
                    stds_[step] = vol_ * Dal::sqrt(dt);
                    REQUIRE(std::isfinite(Value(drifts_[step])) && std::isfinite(Value(stds_[step])),
                            "InvalidHybridComponent: non-finite BS step for " + name_);
                }
            }
            void ResetState(Vector_<T_>* state, size_t offset) const override { (*state)[offset] = Dal::log(spot_); }
            void
            Evolve(size_t step, const Vector_<>& factors, const Vector_<size_t>& factorSlots, Vector_<T_>* state, size_t stateOffset) const override {
                (*state)[stateOffset] += drifts_[step] + stds_[step] * factors[factorSlots[0]];
            }
            [[nodiscard]] T_ Observe(size_t slot, const Vector_<T_>& state, size_t stateOffset, bool today) const override {
                REQUIRE(slot == 0, "InvalidHybridObservation: BS equity has one output");
                if (today)
                    return spot_;
                return Dal::exp(state[stateOffset]);
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override {
                auto copy = std::make_unique<HybridBSEquity_<T_>>(*this);
                copy->SetParameterPointers();
                return copy;
            }
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

        public:
            explicit HybridDeterministicRate_(const HybridDeterministicRateData_& data)
                : name_(data.Name()), currency_(data.currency_), rate_(data.rate_), labels_(data.RiskLabels()) {
                SetParameterPointers();
                REQUIRE(std::isfinite(Value(rate_)), "InvalidHybridComponent: non-finite domestic rate for " + name_);
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
            [[nodiscard]] T_ DomesticRate() const override { return rate_; }
            [[nodiscard]] T_ Numeraire(double time) const override { return Dal::exp(rate_ * time); }
            void Prepare(const Vector_<>&, const T_&) override {}
            void ResetState(Vector_<T_>*, size_t) const override {}
            void Evolve(size_t, const Vector_<>&, const Vector_<size_t>&, Vector_<T_>*, size_t) const override {}
            [[nodiscard]] T_ Observe(size_t, const Vector_<T_>&, size_t, bool) const override {
                THROW("InvalidHybridObservation: rate component has no spot output");
            }
            [[nodiscard]] std::unique_ptr<HybridComponent_<T_>> Clone() const override {
                auto copy = std::make_unique<HybridDeterministicRate_<T_>>(*this);
                copy->SetParameterPointers();
                return copy;
            }
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
            Vector_<> timeLine_;
            Vector_<> productTimeLine_;
            bool todayOnTimeLine_ = false;
            const Vector_<SampleDef_>* defLine_ = nullptr;
            Vector_<Vector_<OutputSlot_>> outputSlots_;
            Vector_<T_> numeraires_;

            void SetCorrelation(std::shared_ptr<const FactorCorrelation_> correlation) {
                REQUIRE(correlation && correlation->FactorNames() == factorNames_,
                        "InvalidHybridCorrelation: provider factors must match the hybrid factor registry");
                correlation_ = std::move(correlation);
                BuildSlotsAndParameters();
            }

            void ValidateAndOrderComponents() {
                REQUIRE(!components_.empty() && !domesticCurrency_.empty(), "InvalidHybridModel: components and domestic currency are required");
                for (const auto& component : components_)
                    REQUIRE(component, "InvalidHybridComponent: null component");
                std::sort(components_.begin(), components_.end(), [](const auto& lhs, const auto& rhs) { return lhs->Name() < rhs->Name(); });
                size_t numNumeraires = 0;
                for (size_t i = 0; i < components_.size(); ++i) {
                    const auto& component = *components_[i];
                    REQUIRE(!component.Name().empty() && component.Currency() == domesticCurrency_,
                            "InvalidHybridCurrency: component " + component.Name() + " must use " + domesticCurrency_);
                    REQUIRE(i == 0 || component.Name() != components_[i - 1]->Name(), "DuplicateHybridComponent: " + component.Name());
                    REQUIRE(component.FactorNames().size() == component.FactorDim(),
                            "InvalidHybridFactor: factor labels must match the component factor dimension for " + component.Name());
                    if (component.ProvidesNumeraire()) {
                        rateSlot_ = i;
                        ++numNumeraires;
                    }
                    if (!component.ObservableNames().empty() && assetNames_.empty())
                        spotSlot_ = i;
                    for (const auto& name : component.ObservableNames()) {
                        REQUIRE(std::find(assetNames_.begin(), assetNames_.end(), name) == assetNames_.end(), "DuplicateHybridObservable: " + name);
                        assetNames_.push_back(name);
                    }
                    for (const auto& name : component.FactorNames()) {
                        REQUIRE(!name.empty() && std::find(factorNames_.begin(), factorNames_.end(), name) == factorNames_.end(),
                                "DuplicateHybridFactor: " + name);
                        factorNames_.push_back(name);
                    }
                }
                REQUIRE(numNumeraires == 1, "InvalidHybridNumeraire: exactly one domestic numeraire provider is required");
                REQUIRE(!assetNames_.empty(), "InvalidHybridModel: at least one observable is required");
                std::sort(factorNames_.begin(), factorNames_.end());
            }

            void BuildSlotsAndParameters() {
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
                    parameters_.Append(component.Parameters());
                    parameterLabels_.Append(component.ParameterLabels());
                }
            }

            [[nodiscard]] OutputSlot_ FindOutput(const String_& name) const {
                const Handle_<Index_> index(Index::Parse(name));
                REQUIRE(index, "UnsupportedModelObservation: " + name);
                for (size_t i = 0; i < components_.size(); ++i) {
                    const auto& observables = components_[i]->ObservableNames();
                    for (size_t j = 0; j < observables.size(); ++j)
                        if (observables[j] == index->Name())
                            return {i, j};
                }
                THROW("UnsupportedModelObservation: " + name);
            }

            void FillSample(size_t sample, const Vector_<T_>& state, bool today, Sample_<T_>* output) const {
                if ((*defLine_)[sample].numeraire_)
                    output->numeraire_ = numeraires_[sample];
                output->spot_ = components_[spotSlot_]->Observe(0, state, stateOffsets_[spotSlot_], today);
                for (size_t i = 0; i < outputSlots_[sample].size(); ++i) {
                    const auto slot = outputSlots_[sample][i];
                    if (slot.component_ == spotSlot_ && slot.observable_ == 0)
                        output->observations_[i] = output->spot_;
                    else
                        output->observations_[i] =
                            components_[slot.component_]->Observe(slot.observable_, state, stateOffsets_[slot.component_], today);
                }
            }

            void Advance(size_t step, const Vector_<>& gaussian, Vector_<>* factors, Vector_<T_>* state) const {
                const auto& lower = correlation_->LowerAt(step);
                const size_t n = factorNames_.size();
                for (size_t i = 0; i < n; ++i) {
                    double correlated = 0.0;
                    for (size_t j = 0; j <= i; ++j)
                        correlated += lower(static_cast<int>(i), static_cast<int>(j)) * gaussian[step * n + j];
                    (*factors)[i] = correlated;
                }
                for (const size_t i : evolvingComponents_)
                    components_[i]->Evolve(step, *factors, factorSlots_[i], state, stateOffsets_[i]);
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
                return std::find(assetNames_.begin(), assetNames_.end(), index.Name()) != assetNames_.end();
            }
            [[nodiscard]] size_t MaxObservedIndices() const override { return assetNames_.size(); }
            [[nodiscard]] size_t MaxOutputSlotsPerSample() const override { return std::numeric_limits<size_t>::max(); }
            [[nodiscard]] size_t NumFactors() const override { return factorNames_.size(); }
            [[nodiscard]] bool SupportsBrownianBridge() const override { return true; }
            [[nodiscard]] bool NumeraireIsDeterministic() const override { return components_[rateSlot_]->NumeraireIsDeterministic(); }
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
                copy->todayOnTimeLine_ = todayOnTimeLine_;
                copy->defLine_ = defLine_;
                copy->outputSlots_ = outputSlots_;
                copy->numeraires_ = numeraires_;
                return copy;
            }
            void Allocate(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                this->ValidateTimeline(productTimeLine, defLine);
                defLine_ = &defLine;
                productTimeLine_ = productTimeLine;
                timeLine_ = {0.0};
                for (const double time : productTimeLine)
                    if (time > 0.0)
                        timeLine_.push_back(time);
                todayOnTimeLine_ = productTimeLine[0] == 0.0;
                outputSlots_.Resize(defLine.size());
                for (size_t sample = 0; sample < defLine.size(); ++sample) {
                    outputSlots_[sample].clear();
                    for (const auto& name : defLine[sample].indexNames_)
                        outputSlots_[sample].push_back(FindOutput(name));
                }
                numeraires_.Resize(defLine.size());
            }
            void Init(const Vector_<>& productTimeLine, const Vector_<SampleDef_>& defLine) override {
                REQUIRE(defLine_ == &defLine && productTimeLine == productTimeLine_, "InvalidHybridTimeline: call Allocate before Init");
                const T_ domesticRate = components_[rateSlot_]->DomesticRate();
                for (auto& component : components_)
                    component->Prepare(timeLine_, domesticRate);
                for (size_t sample = 0; sample < defLine.size(); ++sample)
                    if (defLine[sample].numeraire_) {
                        numeraires_[sample] = components_[rateSlot_]->Numeraire(productTimeLine[sample]);
                        REQUIRE(std::isfinite(Value(numeraires_[sample])) && Value(numeraires_[sample]) > 0.0,
                                "InvalidHybridNumeraire: non-finite or zero domestic numeraire");
                    }
            }
            [[nodiscard]] size_t SimDim() const override { return (timeLine_.size() - 1) * factorNames_.size(); }
            void GeneratePath(const Vector_<>& gaussian, Scenario_<T_>* path) const override {
                REQUIRE(defLine_ && path && path->size() == defLine_->size() && gaussian.size() == SimDim(),
                        "InvalidHybridPath: path or Gaussian dimension mismatch");
                auto* state = &(*path)[0].modelScratch_;
                auto* factors = &(*path)[0].modelFactorScratch_;
                state->Resize(totalState_);
                factors->Resize(factorNames_.size());
                for (size_t i = 0; i < components_.size(); ++i)
                    components_[i]->ResetState(state, stateOffsets_[i]);
                size_t sample = 0;
                if (todayOnTimeLine_)
                    FillSample(sample++, *state, true, &(*path)[0]);
                for (size_t step = 0; step + 1 < timeLine_.size(); ++step, ++sample) {
                    Advance(step, gaussian, factors, state);
                    FillSample(sample, *state, false, &(*path)[sample]);
                }
            }
        };

        template <class T_> std::unique_ptr<HybridComponent_<T_>> CreateHybridComponent(const HybridComponentData_& data) {
            if (const auto* equity = dynamic_cast<const HybridBSEquityData_*>(&data))
                return std::make_unique<HybridBSEquity_<T_>>(*equity);
            if (const auto* rate = dynamic_cast<const HybridDeterministicRateData_*>(&data))
                return std::make_unique<HybridDeterministicRate_<T_>>(*rate);
            THROW("UnsupportedHybridComponent: " + data.Type());
        }
    } // namespace AAD

} // namespace Dal
