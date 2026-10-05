//
// Created by Codex on 2026/10/5.
//

#include <algorithm>
#include <limits>
#include <set>
#include <typeinfo>
#include <utility>

#include <dal/model/factory.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/globals.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/dupireriskinternal.hpp>
#include <dal-public/src/dupireriskrequest.hpp>
#include <dal-public/src/riskvalueinternal.hpp>
#include <dal-public/src/valuevalidation.hpp>

namespace Dal {
    namespace {
        size_t SurfaceNodes(const DupireCalibrationSnapshot_& calibration) {
            const auto rows = static_cast<size_t>(calibration.Surface()->vols_.Rows());
            const auto columns = static_cast<size_t>(calibration.Surface()->vols_.Cols());
            REQUIRE(rows > 0 && columns > 0 && rows <= (std::numeric_limits<size_t>::max)() / columns,
                    "InvalidDupireRiskRequest: invalid or overflowing surface extent");
            const size_t nodes = rows * columns;
            REQUIRE(nodes <= static_cast<size_t>((std::numeric_limits<int>::max)()),
                    "InvalidDupireRiskRequest: surface extent exceeds the matrix dimension limit");
            return nodes;
        }

        size_t CombinedPayload(const DupireCalibrationSnapshot_& calibration, const DupireScriptRiskRequest_& request) {
            const auto nodes = SurfaceNodes(calibration);
            const size_t bytes = Detail::DupireRiskPayloadBytes(nodes, request.directBindings_.size(), calibration.Inputs().quoteSpreads_.Rows(),
                                                                calibration.Inputs().quoteSpreads_.Cols());
            REQUIRE(!request.quotes_.numericPayloadBudgetBytes_ || bytes <= *request.quotes_.numericPayloadBudgetBytes_,
                    "CalibrationRiskBudgetExceeded: combined valuation/quote payload exceeds numericPayloadBudgetBytes; requiredBytes=" +
                        String_(std::to_string(bytes)));
            return bytes;
        }

        void ValidateExternalDirect(const DupireCalibrationSnapshot_& calibration, const DupireScriptRiskRequest_& request) {
            REQUIRE(request.directBindings_.empty() || !request.direct_,
                    "InvalidDupireRiskRequest: direct bindings and external direct adjoints are mutually exclusive");
            if (!request.direct_)
                return;
            const auto* source = std::get_if<DupireCalibrationSnapshot_>(&request.direct_->Calibration().Source());
            REQUIRE(source && calibration.MatchesQuotes(*source), "DupireSnapshotMismatch: direct quote definition does not match");
        }

        void ValidateSealTypes(const HybridModelData_& model) {
            REQUIRE(typeid(model) == typeid(HybridModelData_) && model.correlation_ &&
                        typeid(*model.correlation_) == typeid(HybridConstantCorrelationData_),
                    "InvalidDupireRiskRequest: exact native Hybrid model and constant correlation are required");
            for (const auto& item : model.components_) {
                REQUIRE(item, "InvalidDupireRiskRequest: null Hybrid component");
                const auto& type = typeid(*item);
                REQUIRE(type == typeid(HybridBSEquityData_) || type == typeid(HybridLocalVolEquityData_) ||
                            type == typeid(HybridDeterministicRateData_),
                        "InvalidDupireRiskRequest: unsupported Hybrid component type; component=" + item->Name());
                if (const auto* local = dynamic_cast<const HybridLocalVolEquityData_*>(item.get()))
                    REQUIRE(local->surface_ && typeid(*local->surface_) == typeid(LocalVolSurfaceData_),
                            "InvalidDupireRiskRequest: unsupported Hybrid surface type; component=" + item->Name());
            }
        }

        Handle_<ModelData_> SealedModel(const Handle_<ModelData_>& source) {
            const auto hybrid = handle_cast<HybridModelData_>(source);
            REQUIRE(hybrid, "InvalidDupireRiskRequest: Hybrid model is required");
            ValidateSealTypes(*hybrid);
            const auto snapshot = JSON::WriteString(*source);
            const auto copy = handle_cast<ModelData_>(JSON::ReadString(snapshot.data(), snapshot.size(), {}));
            REQUIRE(copy && typeid(*copy) == typeid(HybridModelData_), "InvalidDupireRiskRequest: invalid sealed Hybrid model");
            return copy;
        }

        void ValidateCarry(const HybridModelData_& model, const DupireCalibrationSnapshot_& calibration, const String_& component) {
            const HybridLocalVolEquityData_* target = nullptr;
            const HybridDeterministicRateData_* rate = nullptr;
            for (const auto& item : model.components_) {
                if (item->Name() == component)
                    target = dynamic_cast<const HybridLocalVolEquityData_*>(item.get());
                if (const auto* flat = dynamic_cast<const HybridDeterministicRateData_*>(item.get()))
                    rate = flat;
            }
            REQUIRE(target, "InvalidDupireRiskRequest: named local-vol equity is required; component=" + component);
            REQUIRE(rate && rate->rate_ == calibration.Rate() && target->spot_ == calibration.Spot() && target->div_ == calibration.DividendYield(),
                    "DupireSnapshotMismatch: automatic request requires matching flat deterministic carry; component=" + component);
        }

        Vector_<Script::RiskCoordinate_> RequiredAxis(const Vector_<Script::RiskCoordinate_>& complete, const Detail::DupireSurfaceLayout_& layout) {
            const size_t count = static_cast<size_t>(layout.rows_) * layout.columns_;
            REQUIRE(layout.offset_ <= complete.size() && count <= complete.size() - layout.offset_,
                    "InvalidDupireRiskRequest: required surface ordinals exceed the complete axis");
            Vector_<Script::RiskCoordinate_> required;
            required.reserve(count);
            for (size_t ordinal = 0; ordinal < count; ++ordinal)
                required.push_back(complete[layout.offset_ + ordinal]);
            return required;
        }

        const CalibrationQuoteCoordinate_& DirectQuote(const CalibrationRiskPlan_& quotes, const String_& id) {
            const auto& axis = quotes.CompleteInputAxis();
            const auto quote = std::find_if(axis.begin(), axis.end(), [&](const auto& coordinate) { return coordinate.id_ == id; });
            REQUIRE(quote != axis.end(), "InvalidDupireRiskRequest: unknown direct quote ID=" + id);
            return *quote;
        }

        Vector_<Script::RiskCoordinate_> WithDirectBindings(const Vector_<Script::RiskCoordinate_>& complete,
                                                            Vector_<Script::RiskCoordinate_> required,
                                                            const CalibrationRiskPlan_& quotes,
                                                            const Vector_<DupireQuoteBinding_>& bindings) {
            std::set<size_t> constants;
            std::set<String_> quoteIds;
            for (const auto& binding : bindings) {
                const auto constant = std::find_if(complete.begin(), complete.end(), [&](const auto& coordinate) {
                    return coordinate.family_ == "constant" && coordinate.ordinal_ == binding.constantOrdinal_;
                });
                REQUIRE(constant != complete.end(),
                        "InvalidDupireRiskRequest: unknown direct constant ordinal=" + String_(std::to_string(binding.constantOrdinal_)));
                const auto& quote = DirectQuote(quotes, binding.quoteId_);
                REQUIRE(constants.insert(binding.constantOrdinal_).second,
                        "InvalidDupireRiskRequest: repeated direct constant; input=" + constant->id_);
                REQUIRE(quoteIds.insert(quote.id_).second, "InvalidDupireRiskRequest: repeated direct quote; input=" + quote.id_);
                REQUIRE(quote.value_ && std::isfinite(constant->value_) && constant->value_ == *quote.value_,
                        "InvalidDupireRiskRequest: direct constant/quote values disagree; input=" + constant->id_ + "; quote=" + quote.id_);
                required.push_back(*constant);
            }
            return required;
        }

        void ValidateRequiredAxis(const Vector_<Script::RiskCoordinate_>& complete, const Vector_<Script::RiskCoordinate_>& required) {
            Script::RiskRequest_ request;
            request.inputs_ = Vector_<String_>();
            for (const auto& coordinate : required)
                request.inputs_->push_back(coordinate.id_);
            static_cast<void>(Script::PlanScalarRiskRequest(complete, request, true));
        }

        bool SameCoordinateIdentity(const Script::RiskCoordinate_& lhs, const Script::RiskCoordinate_& rhs) {
            return lhs.id_ == rhs.id_ && lhs.label_ == rhs.label_ && lhs.family_ == rhs.family_ && lhs.ordinal_ == rhs.ordinal_;
        }

        bool SameCoordinateValue(const Script::RiskCoordinate_& lhs, const Script::RiskCoordinate_& rhs) {
            return lhs.value_ == rhs.value_ && lhs.nativeUnit_ == rhs.nativeUnit_ && lhs.physicalUnit_ == rhs.physicalUnit_ &&
                   lhs.reportScale_ == rhs.reportScale_;
        }

        void CheckExecutedAxis(const Vector_<Script::RiskCoordinate_>& planned, const Vector_<Script::RiskCoordinate_>& executed) {
            REQUIRE(planned.size() == executed.size(), "DupireSnapshotMismatch: executed input extent changed");
            for (size_t ordinal = 0; ordinal < planned.size(); ++ordinal) {
                const auto& before = planned[ordinal];
                const auto& after = executed[ordinal];
                REQUIRE(SameCoordinateIdentity(before, after) && SameCoordinateValue(before, after),
                        "DupireSnapshotMismatch: executed input changed; input=" + before.id_);
            }
        }

        std::optional<CalibrationDirectQuoteAdjoints_>
        BoundDirect(const Script::RiskResult_& valuation, const CalibrationRiskPlan_& quotes, const Vector_<DupireQuoteBinding_>& bindings) {
            if (bindings.empty())
                return {};
            const auto& calibration = quotes.Calibration();
            Matrix_<> seeds(calibration.QuoteRows(), calibration.QuoteCols(), 0.0);
            const size_t start = valuation.InputAxis().size() - bindings.size();
            for (size_t ordinal = 0; ordinal < bindings.size(); ++ordinal) {
                const auto& quote = DirectQuote(quotes, bindings[ordinal].quoteId_);
                seeds(quote.row_, quote.column_) = valuation.Jacobian()(0, static_cast<int>(start + ordinal));
            }
            return NewCalibrationDirectQuoteAdjoints(calibration, seeds);
        }
    } // namespace

    namespace Detail {
        size_t DupireRiskPayloadBytes(size_t surfaceNodes, size_t directBindings, size_t quoteRows, size_t quoteColumns) {
            const size_t matrixLimit = static_cast<size_t>((std::numeric_limits<int>::max)());
            REQUIRE(surfaceNodes > 0 && surfaceNodes <= matrixLimit && directBindings <= matrixLimit - surfaceNodes,
                    "InvalidDupireRiskRequest: invalid or overflowing required surface/direct extent");
            const size_t valuation = Script::RiskResultPayloadBytes(1, surfaceNodes + directBindings);
            const size_t quotes = CalibrationRiskPayloadBytes(quoteRows, quoteColumns);
            REQUIRE(valuation <= (std::numeric_limits<size_t>::max)() - quotes,
                    "InvalidDupireRiskRequest: combined numeric result byte count overflows");
            return valuation + quotes;
        }
    } // namespace Detail

    struct DupireScriptRiskPlan_::Data_ {
        Handle_<ScriptProductData_> product_;
        Handle_<ModelData_> model_;
        CalibrationRiskPlan_ quotePlan_;
        String_ component_;
        Vector_<Script::RiskCoordinate_> completeAxis_;
        Vector_<Script::RiskCoordinate_> requiredAxis_;
        Vector_<DupireQuoteBinding_> directBindings_;
        std::optional<CalibrationDirectQuoteAdjoints_> direct_;
        int numPaths_;
        ScriptValuationSettings_ valuation_;
        MonteCarloSettings_ simulation_;
        size_t numericPayloadBytes_;

        Data_(const Handle_<ScriptProductData_>& product,
              const Handle_<ModelData_>& model,
              CalibrationRiskPlan_&& quotePlan,
              const String_& component,
              Vector_<Script::RiskCoordinate_>&& complete,
              Vector_<Script::RiskCoordinate_>&& required,
              DupireScriptRiskRequest_ request,
              size_t bytes)
            : product_(product), model_(model), quotePlan_(std::move(quotePlan)), component_(component), completeAxis_(std::move(complete)),
              requiredAxis_(std::move(required)), directBindings_(std::move(request.directBindings_)), direct_(std::move(request.direct_)),
              numPaths_(request.numPaths_), valuation_(std::move(request.valuation_)), simulation_(std::move(request.simulation_)),
              numericPayloadBytes_(bytes) {}
    };

    DupireScriptRiskPlan_::DupireScriptRiskPlan_(std::shared_ptr<const Data_> data) : data_(std::move(data)) {}
    const String_& DupireScriptRiskPlan_::Component() const { return data_->component_; }
    const CalibrationRiskPlan_& DupireScriptRiskPlan_::QuotePlan() const { return data_->quotePlan_; }
    const Vector_<Script::RiskCoordinate_>& DupireScriptRiskPlan_::CompleteInputAxis() const { return data_->completeAxis_; }
    const Vector_<Script::RiskCoordinate_>& DupireScriptRiskPlan_::RequiredInputAxis() const { return data_->requiredAxis_; }
    const Vector_<DupireQuoteBinding_>& DupireScriptRiskPlan_::DirectBindings() const { return data_->directBindings_; }
    int DupireScriptRiskPlan_::NumPaths() const { return data_->numPaths_; }
    const ScriptValuationSettings_& DupireScriptRiskPlan_::ValuationSettings() const { return data_->valuation_; }
    const MonteCarloSettings_& DupireScriptRiskPlan_::SimulationSettings() const { return data_->simulation_; }
    size_t DupireScriptRiskPlan_::NumericPayloadBytes() const { return data_->numericPayloadBytes_; }

    DupireScriptRiskPlan_ PlanDupireScriptRisk(const Handle_<ScriptProductData_>& product,
                                               const Handle_<ModelData_>& model,
                                               const DupireCalibrationSnapshot_& calibration,
                                               const String_& component,
                                               const DupireScriptRiskRequest_& request) {
        XGLOBAL::ValuationMutationGuard_ guard;
        auto copied = request;
        Detail::CheckScriptValuationInputs(product, model);
        REQUIRE(copied.numPaths_ > 0, "InvalidDupireRiskRequest: numPaths must be positive");
        Script::ValidateSimulationSettings(copied.simulation_);
        REQUIRE(copied.simulation_.enableAad_, "InvalidDupireRiskRequest: native AAD execution is required");
        const size_t bytes = CombinedPayload(calibration, copied);
        ValidateExternalDirect(calibration, copied);
        const auto frozenModel = SealedModel(model);
        const auto hybrid = handle_cast<HybridModelData_>(frozenModel);
        const auto passive = CreateModel<double>(frozenModel);
        ValidateCarry(*hybrid, calibration, component);
        const auto layout = Detail::DupireSurfaceLayout(*hybrid, calibration, component);
        const Handle_<ScriptProductData_> frozenProduct(
            new ScriptProductData_(product->Name(), product->Dates(), product->EventTexts(), product->Settings()));
        auto parsed = frozenProduct->Product();
        parsed.IndexVariables();
        auto complete = Detail::ScriptRiskInputAxis(*passive, parsed);
        auto required = RequiredAxis(complete, layout);
        auto quoteRequest = copied.quotes_;
        quoteRequest.numericPayloadBudgetBytes_.reset();
        auto quotePlan = PlanCalibrationRiskRequest(NewCalibrationPullback(calibration), quoteRequest);
        required = WithDirectBindings(complete, std::move(required), quotePlan, copied.directBindings_);
        ValidateRequiredAxis(complete, required);
        if (!copied.valuation_.evaluationDate_)
            copied.valuation_.evaluationDate_ = Script::CaptureScriptEvaluationDate();
        return DupireScriptRiskPlan_(std::make_shared<const DupireScriptRiskPlan_::Data_>(
            frozenProduct, frozenModel, std::move(quotePlan), component, std::move(complete), std::move(required), std::move(copied), bytes));
    }

    DupireScriptRiskResult_::DupireScriptRiskResult_(Script::RiskResult_&& valuation,
                                                     CalibrationRiskResult_&& risk,
                                                     const String_& component,
                                                     size_t bytes)
        : valuation_(std::move(valuation)), quoteRisk_(std::move(risk)), component_(component),
          method_(valuation_.Provenance().method_ + "Then" + quoteRisk_.QuoteRisk().Method()), numericPayloadBytes_(bytes) {}

    DupireScriptRiskResult_ ValueByMonteCarloWithDupireRisk(const DupireScriptRiskPlan_& plan) {
        XGLOBAL::ValuationMutationGuard_ guard;
        const auto& data = *plan.data_;
        Script::RiskRequest_ request;
        request.inputs_ = Vector_<String_>();
        for (const auto& coordinate : data.requiredAxis_)
            request.inputs_->push_back(coordinate.id_);
        request.numericPayloadBudgetBytes_ = data.numericPayloadBytes_ - data.quotePlan_.NumericPayloadBytes();
        auto valuation = ValueByMonteCarloWithRisk(data.product_, data.model_, data.numPaths_, request, data.valuation_, data.simulation_);
        CheckExecutedAxis(data.completeAxis_, valuation.CompleteInputAxis());
        CheckExecutedAxis(data.requiredAxis_, valuation.InputAxis());
        const auto& calibration = std::get<DupireCalibrationSnapshot_>(data.quotePlan_.Calibration().Source());
        const auto parameters = ExtractDupireParameterAdjoints(valuation, calibration, data.component_);
        const auto direct = data.direct_ ? data.direct_ : BoundDirect(valuation, data.quotePlan_, data.directBindings_);
        auto risk = PullbackCalibrationWithRisk(data.quotePlan_, NewCalibrationParameterAdjoints(data.quotePlan_.Calibration(), parameters.adjoints_),
                                                direct);
        return DupireScriptRiskResult_(std::move(valuation), std::move(risk), data.component_, data.numericPayloadBytes_);
    }
} // namespace Dal
