//
// Created by Codex on 2026/10/16.
//

#pragma once

#include <dal/model/blackscholes.hpp>
#include <dal/script/detail/soaevaluator.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
    namespace Detail::CompiledSoa {
        template <class P_> const ScriptProduct_& Product(const P_& product) {
            if constexpr (std::is_base_of_v<PreparedScript_, P_>)
                return product.Product();
            else
                return product;
        }

        template <class P_> const ObservationPlan_* Observations(const P_& product) {
            if constexpr (std::is_base_of_v<PreparedScript_, P_>)
                return &product.Plan();
            return nullptr;
        }

        template <class P_> void RequirePreparedAdmission(const P_& product, const AAD::Model_<double>* model, const String_& rsg, bool useBb) {
            if constexpr (std::is_base_of_v<PreparedScript_, P_>) {
                const auto& settings = product.Simulation();
                REQUIRE2(!settings.enableAad_ && settings.compiled_.value_or(false) && settings.rsg_ == rsg && settings.useBb_ == useBb &&
                             product.PreparedModelType() == typeid(*model),
                         "UnsupportedExecutionMode: execution differs from preparation", ScriptError_);
                ValidateSimulationSettings(settings);
            }
        }

        template <class P_> void RequireSampleAdmission(const P_& product) {
            REQUIRE2(product.TimeLine().size() == product.DefLine().size(), "InvalidModelTimeline: sample definitions must match dates",
                     ScriptError_);
            for (size_t sample = 0; sample < product.DefLine().size(); ++sample) {
                const auto& definition = product.DefLine()[sample];
                // Legacy preprocessing emits one unused forward placeholder at the sample time.
                const bool legacyPlaceholder = !Observations(product) && definition.forwardMats_.size() == 1 &&
                                               definition.forwardMats_[0].size() == 1 && definition.forwardMats_[0][0] == product.TimeLine()[sample];
                REQUIRE2(definition.discountMats_.empty() && definition.liborDefs_.empty() && (definition.forwardMats_.empty() || legacyPlaceholder),
                         "UnsupportedSoaSamples: delayed payments, rates and forwards are unsupported", ScriptError_);
            }
        }

        template <class P_> void RequireObservationAdmission(const P_& product, const AAD::Model_<double>* model) {
            if (const auto* observations = Observations(product)) {
                for (const auto& request : observations->Requests()) {
                    if (request.historyValueId_) {
                        REQUIRE2(std::isfinite(observations->KnownValue(*request.historyValueId_)), "InvalidSoaObservation: non-finite history",
                                 ScriptError_);
                    } else {
                        REQUIRE2(request.modelSlot_ && request.index_ && model->SupportsIndex(*request.index_),
                                 "UnsupportedSoaObservation: unresolved or unsupported index", ScriptError_);
                        const auto& slot = *request.modelSlot_;
                        REQUIRE2(slot.sampleId_ < product.DefLine().size() && slot.outputId_ < product.DefLine()[slot.sampleId_].indexNames_.size(),
                                 "InvalidSoaObservation: sample or output out of range", ScriptError_);
                    }
                }
            }
        }

        template <class P_> void RequireAdmission(const P_& product, const AAD::Model_<double>* model, const String_& rsg, bool useBb) {
            product.RequireExecutable();
            ValidateRNG(rsg);
            REQUIRE2(model, "InvalidSoaModel: null model", ScriptError_);
            REQUIRE2(typeid(*model) == typeid(AAD::BlackScholes_<double>), "UnsupportedSoaModel: exact BlackScholes required", ScriptError_);
            REQUIRE2(ThreadPool_::GetInstance()->NumThreads() == 1, "UnsupportedSoaExecution: single worker required", ScriptError_);
            REQUIRE2(!Product(product).ContainsExercise() && Product(product).VectorNames().empty(),
                     "UnsupportedSoaExecution: ordinary scalar-variable MC required", ScriptError_);
            RequirePreparedAdmission(product, model, rsg, useBb);
            RequireSampleAdmission(product);
            RequireObservationAdmission(product, model);
        }

        class BatchModel_ {
            Vector_<> drifts_, stds_, numeraires_;
            bool today_;
            double spot_, logSpot_;

        public:
            BatchModel_(const AAD::BlackScholes_<double>& model, const Vector_<>& timeline, const Vector_<AAD::SampleDef_>& definitions)
                : today_(!timeline.empty() && timeline[0] == 0.0), spot_(model.Spot()), logSpot_(std::log(spot_)) {
                AAD::Detail::ValidateBlackScholesParameters(model.Spot(), model.Vol(), model.Rate(), model.Div());
                for (size_t e = 0; e < timeline.size(); ++e) {
                    numeraires_.push_back(definitions[e].numeraire_ ? AAD::Detail::BlackScholesNumeraire(model.Rate(), timeline[e]) : 1.0);
                    if (today_ && e == 0)
                        continue;
                    const double previous = e == 0 ? 0.0 : timeline[e - 1];
                    const auto step = AAD::Detail::BlackScholesCoefficients(model.Vol(), model.Rate() - model.Div(), timeline[e] - previous);
                    drifts_.push_back(step.drift_);
                    stds_.push_back(step.std_);
                }
            }

            [[nodiscard]] const Vector_<>& Numeraires() const { return numeraires_; }

            template <size_t L_> void Generate(const Vector_<Vector_<>>& shocks, size_t count, std::vector<Row_<L_>>* spots) const {
                Row_<L_> logSpots;
                std::fill_n(logSpots.begin(), count, logSpot_);
                if (today_)
                    std::fill_n((*spots)[0].begin(), count, spot_);
                bool valid = true;
                for (size_t step = 0; step < drifts_.size(); ++step)
                    for (size_t lane = 0; lane < count; ++lane) {
                        logSpots[lane] += drifts_[step] + stds_[step] * shocks[lane][step];
                        const double value = std::exp(logSpots[lane]);
                        (*spots)[step + static_cast<size_t>(today_)][lane] = value;
                        valid &= std::isfinite(value);
                    }
                REQUIRE2(valid, "InvalidModelPath: non-finite spot", ScriptError_);
            }
        };

        template <class P_> Plan_ BuildPlan(const P_& product) {
            const auto* observations = Observations(product);
            REQUIRE2(!observations || observations->EventToSample().size() == product.EventDates().size(),
                     "InvalidSoaObservation: event layout mismatch", ScriptError_);
            Vector_<size_t> eventSamples(product.EventDates().size());
            for (size_t event = 0; event < eventSamples.size(); ++event)
                eventSamples[event] = observations ? observations->EventToSample()[event] : event;
            const auto state = product.template BuildEvalState<double>();
            return {product.Compile(), state.variablesInit_, state.constVariables_, product.DefLine().size(), std::move(eventSamples), observations};
        }

        template <size_t L_, class P_>
        SimResults_ Run(const P_& product, const AAD::Model_<double>& model, const Plan_& plan, size_t nPaths, const String_& rsg, bool useBb) {
            auto random = CreateRNG(rsg, model, useBb, std::nullopt, NormalPrecision(product));
            Vector_<Vector_<>> shocks(L_, Vector_<>(model.SimDim()));
            std::vector<Row_<L_>> spots(product.DefLine().size());
            typename Plan_::State_<L_> state(plan);
            const BatchModel_ batchModel(static_cast<const AAD::BlackScholes_<double>&>(model), product.TimeLine(), product.DefLine());
            SimResults_ result(Vector::Join(model.ParameterLabels(), product.ConstVarNames()));
            const ChunkPlan_ chunks(nPaths);
            // Match the scalar driver's path order, RNG positioning and chunk reduction.
            for (size_t chunk = 0; chunk < chunks.BatchCount(); ++chunk) {
                const auto batch = chunks.BatchAt(chunk);
                if (random)
                    random->SkipNormalTo(batch.firstPath_);
                double sum = 0.0;
                for (size_t first = 0; first < batch.pathCount_; first += L_) {
                    const size_t count = std::min(L_, batch.pathCount_ - first);
                    if (random)
                        for (size_t lane = 0; lane < count; ++lane)
                            random->FillNormal(&shocks[lane]);
                    batchModel.Generate(shocks, count, &spots);
                    plan.Evaluate(spots, batchModel.Numeraires(), count, &state);
                    for (size_t lane = 0; lane < count; ++lane) {
                        const double value = state.variables_[product.PayOffIdx()][lane];
                        REQUIRE2(std::isfinite(value), "InvalidPayoff: non-finite path value", ScriptError_);
                        sum += value;
                    }
                }
                result.aggregated_ += sum;
            }
            return result;
        }
    } // namespace Detail::CompiledSoa

    template <size_t L_ = 32, class P_>
    SimResults_
    MCDoubleSimulationSoa(const P_& product, AAD::Model_<double>* model, size_t nPaths, const String_& rsg = "sobol", bool useBb = false) {
        static_assert(L_ == 8 || L_ == 16 || L_ == 32 || L_ == 64, "SoA supports 8, 16, 32 or 64 lanes");
        static_assert(std::is_same_v<P_, ScriptProduct_> || std::is_same_v<P_, PreparedScript_>, "SoA requires a core script product");
        product.RequireExecutable();
        REQUIRE2(nPaths > 0, "InvalidPathCount: number of paths must be positive", ScriptError_);
        ValidateRNG(rsg);
        REQUIRE2(model, "InvalidSoaModel: null model", ScriptError_);
        if (product.EventDates().empty())
            return SimResults_(Vector::Join(model->ParameterLabels(), product.ConstVarNames()));
        Detail::CompiledSoa::RequireAdmission(product, model, rsg, useBb);
        if (nPaths < 16)
            return MCDoubleSimulation(product, model, nPaths, rsg, useBb, true);
        Detail::InitializeDoubleSimulation(product, model, true, false);
        const auto plan = Detail::CompiledSoa::BuildPlan(product);
        return Detail::CompiledSoa::Run<L_>(product, *model, plan, nPaths, rsg, useBb);
    }
} // namespace Dal::Script
