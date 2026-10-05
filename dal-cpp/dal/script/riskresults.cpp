//
// Created by Codex on 2026/10/4.
//

#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/riskresults.hpp>
#include <dal/script/simulation.hpp>

namespace Dal::Script {
    namespace {
        void ValidateCoordinate(const RiskCoordinate_& coordinate) {
            REQUIRE2(coordinate.family_ == "model" || coordinate.family_ == "constant",
                     "InvalidRiskResult: unknown coordinate family; input=" + coordinate.id_, ScriptError_);
            const String_ expected = coordinate.family_ + ":" + String_(std::to_string(coordinate.ordinal_));
            REQUIRE2(coordinate.id_ == expected, "InvalidRiskResult: inconsistent ordinal ID; input=" + coordinate.id_, ScriptError_);
            REQUIRE2(!coordinate.label_.empty() && !coordinate.nativeUnit_.empty(),
                     "InvalidRiskResult: coordinate label and native unit must be nonempty; input=" + coordinate.id_, ScriptError_);
            REQUIRE2(!coordinate.physicalUnit_ || !coordinate.physicalUnit_->empty(),
                     "InvalidRiskResult: physical unit must be nonempty or absent; input=" + coordinate.id_, ScriptError_);
            REQUIRE2(std::isfinite(coordinate.value_), "InvalidRiskResult: non-finite coordinate value; input=" + coordinate.id_, ScriptError_);
            REQUIRE2(std::isfinite(coordinate.reportScale_) && coordinate.reportScale_ > 0.0,
                     "InvalidRiskResult: report scale must be finite and positive; input=" + coordinate.id_, ScriptError_);
        }

        void ValidateCompleteAxis(const Vector_<RiskCoordinate_>& axis) {
            size_t modelOrdinal = 0, constantOrdinal = 0;
            bool constantsStarted = false;
            for (size_t position = 0; position < axis.size(); ++position) {
                const auto& coordinate = axis[position];
                ValidateCoordinate(coordinate);
                if (coordinate.family_ == "model") {
                    REQUIRE2(!constantsStarted && coordinate.ordinal_ == modelOrdinal,
                             "InvalidRiskResult: model coordinates must be contiguous and precede constants; input=" + coordinate.id_, ScriptError_);
                    ++modelOrdinal;
                } else {
                    constantsStarted = true;
                    REQUIRE2(coordinate.ordinal_ == constantOrdinal,
                             "InvalidRiskResult: constant coordinates must be contiguous; input=" + coordinate.id_, ScriptError_);
                    ++constantOrdinal;
                }
            }
        }

        void ValidateSourceAxis(const SimResults_& source, const Vector_<RiskCoordinate_>& axis) {
            REQUIRE2(source.names_.size() == axis.size() && source.risks_.size() == axis.size(),
                     "InvalidRiskResult: complete input axis, names and risk dimensions must agree", ScriptError_);
            ValidateCompleteAxis(axis);
            for (size_t position = 0; position < axis.size(); ++position)
                REQUIRE2(axis[position].label_ == source.names_[position],
                         "InvalidRiskResult: source name and coordinate position disagree; input=" + axis[position].id_, ScriptError_);
        }

        void ValidateProvenance(const RiskResultProvenance_& provenance) {
            REQUIRE2(!provenance.method_.empty() && !provenance.modelType_.empty(), "InvalidRiskResult: method and model type must be nonempty",
                     ScriptError_);
            REQUIRE2(provenance.engine_ == "native" && provenance.normalization_ == "mean" && provenance.calibration_ == "fixed",
                     "InvalidRiskResult: expected native engine, mean normalization and fixed calibration", ScriptError_);
            REQUIRE2(!provenance.evaluationDate_ || provenance.evaluationDate_->IsValid(),
                     "InvalidRiskResult: evaluation date must be valid or absent", ScriptError_);
        }

        void ValidateExecutionSnapshot(const std::optional<RiskExecutionSnapshot_>& execution, int paths) {
            if (!execution)
                return;
            REQUIRE2(execution->pathsPerReplicate_ == paths && execution->pricingReplicates_ > 0,
                     "InvalidRiskResult: execution path count must match normalization and pricing replicates must be positive", ScriptError_);
            REQUIRE2(execution->productDates_.size() == execution->productEvents_.size(),
                     "InvalidRiskResult: execution product date/event dimensions disagree", ScriptError_);
            for (const auto& observation : execution->observations_)
                REQUIRE2(!observation.value_ || std::isfinite(*observation.value_),
                         "InvalidRiskResult: non-finite frozen history value; index=" + observation.index_, ScriptError_);
        }

        Vector_<size_t> SelectedInputs(const Vector_<RiskCoordinate_>& axis, const RiskRequest_& request) {
            if (!request.inputs_) {
                Vector_<size_t> indices(axis.size());
                std::iota(indices.begin(), indices.end(), size_t{0});
                return indices;
            }
            std::map<String_, size_t> available;
            for (size_t position = 0; position < axis.size(); ++position)
                available.emplace(axis[position].id_, position);
            std::set<String_> selected;
            Vector_<size_t> indices;
            indices.reserve(request.inputs_->size());
            for (const auto& id : *request.inputs_) {
                REQUIRE2(selected.insert(id).second, "InvalidRiskRequest: repeated input ID; input=" + id, ScriptError_);
                const auto found = available.find(id);
                REQUIRE2(found != available.end(), "InvalidRiskRequest: unknown input ID; input=" + id, ScriptError_);
                indices.push_back(found->second);
            }
            return indices;
        }

        void ValidateRequest(const RiskRequest_& request, size_t columns) {
            REQUIRE2(!request.outputs_ || (request.outputs_->size() == 1 && request.outputs_->front() == "payoff"),
                     "InvalidRiskRequest: outputs must select the single supported payoff", ScriptError_);
            REQUIRE2(columns <= static_cast<size_t>(std::numeric_limits<int>::max()),
                     "InvalidRiskRequest: input count exceeds the matrix dimension limit", ScriptError_);
            REQUIRE2(!request.reportFactors_ || request.reportFactors_->size() == columns,
                     "InvalidRiskRequest: report factor count must match selected inputs", ScriptError_);
            if (request.reportFactors_)
                for (const double factor : *request.reportFactors_)
                    REQUIRE2(std::isfinite(factor) && factor > 0.0,
                             "InvalidRiskRequest: report factor must be finite and positive; field=reportFactors", ScriptError_);
            const size_t payload = RiskResultPayloadBytes(1, columns);
            REQUIRE2(!request.numericPayloadBudgetBytes_ || payload <= *request.numericPayloadBudgetBytes_,
                     "RiskResultBudgetExceeded: numeric values/Jacobian payload exceeds numericPayloadBudgetBytes", ScriptError_);
        }

        Vector_<RiskCoordinate_>
        SelectedAxis(const SimResults_& source, const Vector_<RiskCoordinate_>& axis, const Vector_<size_t>& indices, const RiskRequest_& request) {
            Vector_<RiskCoordinate_> selected;
            selected.reserve(indices.size());
            for (size_t column = 0; column < indices.size(); ++column) {
                auto coordinate = axis[indices[column]];
                if (request.reportFactors_)
                    coordinate.reportScale_ = (*request.reportFactors_)[column];
                const double risk = source.risks_[indices[column]];
                REQUIRE2(std::isfinite(coordinate.reportScale_) && coordinate.reportScale_ > 0.0,
                         "InvalidRiskRequest: report factor must be finite and positive; input=" + coordinate.id_, ScriptError_);
                REQUIRE2(std::isfinite(risk), "InvalidRiskResult: non-finite requested mean risk; output=payoff; input=" + coordinate.id_,
                         ScriptError_);
                REQUIRE2(std::isfinite(risk * coordinate.reportScale_),
                         "InvalidRiskResult: non-finite reported risk; output=payoff; input=" + coordinate.id_, ScriptError_);
                selected.push_back(std::move(coordinate));
            }
            return selected;
        }
    } // namespace

    size_t RiskResultPayloadBytes(size_t outputs, size_t inputs) {
        const size_t maximum = std::numeric_limits<size_t>::max();
        REQUIRE2(outputs > 0 && inputs < maximum, "InvalidRiskRequest: invalid or overflowing numeric result extent", ScriptError_);
        const size_t valuesPerOutput = inputs + 1;
        REQUIRE2(outputs <= maximum / sizeof(double) / valuesPerOutput, "InvalidRiskRequest: numeric result payload byte count overflows",
                 ScriptError_);
        return outputs * valuesPerOutput * sizeof(double);
    }

    RiskRequest_ PlanScalarRiskRequest(const Vector_<RiskCoordinate_>& completeAxis, const RiskRequest_& request, bool enableAad) {
        ValidateCompleteAxis(completeAxis);
        REQUIRE2(enableAad || !request.inputs_ || request.inputs_->empty(),
                 "InvalidRiskRequest: price-only execution cannot select risk inputs; field=simulation.enableAad", ScriptError_);
        auto planned = request;
        if (!enableAad)
            planned.inputs_ = Vector_<String_>();
        const auto indices = SelectedInputs(completeAxis, planned);
        ValidateRequest(planned, indices.size());
        return planned;
    }

    RiskResult_::RiskResult_(double value,
                             Matrix_<>&& jacobian,
                             Vector_<RiskCoordinate_>&& inputAxis,
                             const Vector_<RiskCoordinate_>& completeAxis,
                             const RiskResultProvenance_& provenance)
        : values_({value}), jacobian_(std::move(jacobian)), inputAxis_(std::move(inputAxis)), completeInputAxis_(completeAxis),
          provenance_(provenance) {}

    Matrix_<> RiskResult_::ReportedJacobian() const {
        Matrix_<> reported(jacobian_);
        for (size_t column = 0; column < inputAxis_.size(); ++column)
            reported(0, static_cast<int>(column)) *= inputAxis_[column].reportScale_;
        return reported;
    }

    std::map<String_, double> RiskResult_::LegacyValues() const {
        std::map<String_, double> values{{"PV", values_[0]}};
        for (size_t column = 0; column < inputAxis_.size(); ++column) {
            const String_ key = "d_" + inputAxis_[column].label_;
            REQUIRE2(values.emplace(key, jacobian_(0, static_cast<int>(column))).second,
                     "LegacyRiskCollision: duplicate display key; key=" + key + "; input=" + inputAxis_[column].id_, ScriptError_);
        }
        return values;
    }

    RiskResult_ ProjectMonteCarloRiskResult(const SimResults_& source,
                                            int paths,
                                            const Vector_<RiskCoordinate_>& completeAxis,
                                            const RiskRequest_& request,
                                            const RiskResultProvenance_& provenance) {
        REQUIRE2(paths > 0, "InvalidRiskResult: paths must be a positive integer", ScriptError_);
        ValidateSourceAxis(source, completeAxis);
        ValidateProvenance(provenance);
        ValidateExecutionSnapshot(provenance.execution_, paths);
        const auto indices = SelectedInputs(completeAxis, request);
        ValidateRequest(request, indices.size());
        auto selected = SelectedAxis(source, completeAxis, indices, request);
        const double value = source.aggregated_ / static_cast<double>(paths);
        REQUIRE2(std::isfinite(source.aggregated_) && std::isfinite(value), "InvalidRiskResult: non-finite payoff sum or mean; output=payoff",
                 ScriptError_);
        Matrix_<> jacobian(1, static_cast<int>(indices.size()));
        for (size_t column = 0; column < indices.size(); ++column)
            jacobian(0, static_cast<int>(column)) = source.risks_[indices[column]];
        return RiskResult_(value, std::move(jacobian), std::move(selected), completeAxis, provenance);
    }
} // namespace Dal::Script
