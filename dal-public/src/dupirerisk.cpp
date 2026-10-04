//
// Created by Codex on 2026/10/5.
//

#include <limits>

#include <dal/model/factory.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>

#include <dal-public/src/dupirerisk.hpp>

namespace Dal {
    namespace {
        class ConstantVolIVS_ final : public AAD::IVS_ {
            double volatility_;

        public:
            explicit ConstantVolIVS_(const BSModelData_& model) : IVS_(model.spot_, model.rate_, model.div_), volatility_(model.vol_) {}
            [[nodiscard]] double ImpliedVol(double, double) const override { return volatility_; }
        };

        const Script::RiskExecutionSnapshot_& CheckedExecution(const Script::RiskResult_& source) {
            const auto& provenance = source.Provenance();
            REQUIRE(provenance.engine_ == "native" && provenance.normalization_ == "mean" && provenance.calibration_ == "fixed",
                    "InvalidDupirePullback: expected native mean model risk with fixed calibration");
            REQUIRE(source.OutputIds() == Vector_<String_>{"payoff"} && source.Values().size() == 1 && source.Jacobian().Rows() == 1 &&
                        source.Jacobian().Cols() == static_cast<int>(source.InputAxis().size()),
                    "InvalidDupirePullback: expected one scalar payoff with a matching selected input axis");
            REQUIRE(provenance.execution_ && !provenance.execution_->modelSnapshotJson_.empty(),
                    "InvalidDupirePullback: modelSnapshotJson is required");
            REQUIRE(provenance.execution_->simulation_.enableAad_, "InvalidDupirePullback: price-only execution cannot provide surface risk");
            return *provenance.execution_;
        }

        void ValidateMethod(const String_& method, bool expired) {
            REQUIRE(method == "NativeAAD" || method == "NativeAADWithRetrainedPolicySecant" || (method == "Expired" && expired),
                    "InvalidDupirePullback: unsupported valuation method; method=" + method);
        }

        Handle_<HybridModelData_> RestoreModel(const String_& snapshot, const String_& modelType) {
            NOTE("InvalidDupirePullback: modelSnapshotJson");
            const auto stored = JSON::ReadString(snapshot.data(), snapshot.size(), {});
            const auto data = handle_cast<HybridModelData_>(stored);
            REQUIRE(data && modelType == data->Type(), "InvalidDupirePullback: retained model must be HybridModelData");
            return data;
        }

        void ValidateModelCoordinate(const Script::RiskCoordinate_& coordinate, size_t ordinal, const String_& label, double value) {
            REQUIRE(coordinate.family_ == "model" && coordinate.ordinal_ == ordinal && coordinate.id_ == "model:" + String_(std::to_string(ordinal)),
                    "DupireSnapshotMismatch: model ordinal identity changed; input=" + coordinate.id_);
            REQUIRE(coordinate.label_ == label && coordinate.value_ == value,
                    "DupireSnapshotMismatch: model coordinate disagrees with retained model; input=" + coordinate.id_);
            REQUIRE(coordinate.nativeUnit_ == "model-coordinate" && !coordinate.physicalUnit_,
                    "DupireSnapshotMismatch: Hybrid model coordinate unit changed; input=" + coordinate.id_);
        }

        void ValidateModelAxis(const AAD::Model_<double>& model, const Vector_<Script::RiskCoordinate_>& axis) {
            const auto& parameters = model.Parameters();
            const auto& labels = model.ParameterLabels();
            REQUIRE(parameters.size() == labels.size() && parameters.size() <= axis.size(),
                    "DupireSnapshotMismatch: complete model parameter extent changed");
            for (size_t ordinal = 0; ordinal < parameters.size(); ++ordinal)
                ValidateModelCoordinate(axis[ordinal], ordinal, labels[ordinal], *parameters[ordinal]);
            for (size_t position = parameters.size(); position < axis.size(); ++position)
                REQUIRE(axis[position].family_ == "constant", "DupireSnapshotMismatch: unexpected extra model coordinate");
        }

        void ValidateSurface(const HybridLocalVolEquityData_& equity, const DupireCalibrationSnapshot_& calibration) {
            const auto& actual = *equity.surface_;
            const auto& expected = *calibration.Surface();
            REQUIRE(actual.spots_ == expected.spots_ && actual.times_ == expected.times_,
                    "DupireSnapshotMismatch: component surface grids changed; component=" + equity.Name());
            REQUIRE(actual.vols_.Rows() == expected.vols_.Rows() && actual.vols_.Cols() == expected.vols_.Cols() &&
                        std::equal(actual.vols_.begin(), actual.vols_.end(), expected.vols_.begin()),
                    "DupireSnapshotMismatch: component surface values changed; component=" + equity.Name());
        }

        size_t SurfaceOffset(const HybridModelData_& data, const DupireCalibrationSnapshot_& calibration, const String_& component) {
            auto ordered = data.components_;
            std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) { return lhs->Name() < rhs->Name(); });
            size_t offset = 0;
            for (const auto& item : ordered) {
                const auto typed = AAD::CreateHybridComponent<double>(*item);
                if (item->Name() == component) {
                    const auto* equity = dynamic_cast<const HybridLocalVolEquityData_*>(item.get());
                    REQUIRE(equity, "InvalidDupirePullback: component must be local-vol equity; component=" + component);
                    ValidateSurface(*equity, calibration);
                    const auto& vols = equity->surface_->vols_;
                    const size_t nodes = static_cast<size_t>(vols.Rows()) * static_cast<size_t>(vols.Cols());
                    REQUIRE(typed->Parameters().size() == nodes + 2, "DupireSnapshotMismatch: local-vol parameter layout changed");
                    return offset + 2;
                }
                offset += typed->Parameters().size();
            }
            THROW("InvalidDupirePullback: unknown component; component=" + component);
        }

        Vector_<size_t> SelectedModelColumns(const Script::RiskResult_& source, const AAD::Model_<double>& model) {
            const auto& parameters = model.Parameters();
            Vector_<size_t> columns(parameters.size(), std::numeric_limits<size_t>::max());
            for (size_t column = 0; column < source.InputAxis().size(); ++column) {
                const auto& coordinate = source.InputAxis()[column];
                if (coordinate.family_ != "model")
                    continue;
                REQUIRE(coordinate.ordinal_ < parameters.size(), "DupireSnapshotMismatch: selected model ordinal is out of bounds");
                ValidateModelCoordinate(coordinate, coordinate.ordinal_, model.ParameterLabels()[coordinate.ordinal_],
                                        *parameters[coordinate.ordinal_]);
                REQUIRE(columns[coordinate.ordinal_] == std::numeric_limits<size_t>::max(), "InvalidDupirePullback: repeated selected model ordinal");
                columns[coordinate.ordinal_] = column;
            }
            return columns;
        }

        Matrix_<> SurfaceSeeds(const Script::RiskResult_& source,
                               const DupireCalibrationSnapshot_& calibration,
                               size_t offset,
                               const Vector_<size_t>& columns) {
            const auto& surface = calibration.Surface()->vols_;
            Matrix_<> seeds(surface.Rows(), surface.Cols());
            for (int row = 0; row < seeds.Rows(); ++row)
                for (int column = 0; column < seeds.Cols(); ++column) {
                    const size_t ordinal = offset + static_cast<size_t>(row) * static_cast<size_t>(seeds.Cols()) + static_cast<size_t>(column);
                    REQUIRE(ordinal < columns.size() && columns[ordinal] != std::numeric_limits<size_t>::max(),
                            "InvalidDupirePullback: missing selected surface risk; input=model:" + String_(std::to_string(ordinal)));
                    const double adjoint = source.Jacobian()(0, static_cast<int>(columns[ordinal]));
                    REQUIRE(std::isfinite(adjoint), "InvalidDupirePullback: non-finite selected surface risk");
                    seeds(row, column) = adjoint;
                }
            return seeds;
        }
    } // namespace

    DupireCalibrationSnapshot_ CalibrateDupireWithRisk(const BSModelData_& baseModel, const DupireRiskInputs_& inputs, const String_& name) {
        return CalibrateDupireWithRisk(ConstantVolIVS_(baseModel), inputs, name);
    }

    DupireParameterAdjoints_
    ExtractDupireParameterAdjoints(const Script::RiskResult_& valuation, const DupireCalibrationSnapshot_& calibration, const String_& component) {
        const auto& execution = CheckedExecution(valuation);
        ValidateMethod(valuation.Provenance().method_, execution.allExpired_);
        const auto data = RestoreModel(execution.modelSnapshotJson_, valuation.Provenance().modelType_);
        const auto model = CreateModel<double>(handle_cast<ModelData_>(data));
        ValidateModelAxis(*model, valuation.CompleteInputAxis());
        const auto offset = SurfaceOffset(*data, calibration, component);
        const auto columns = SelectedModelColumns(valuation, *model);
        return {calibration, SurfaceSeeds(valuation, calibration, offset, columns)};
    }

    DupireScriptQuoteRisk_::DupireScriptQuoteRisk_(const Script::RiskResult_& valuation, DupireQuoteRisk_&& quoteRisk, const String_& component)
        : valuation_(valuation), quoteRisk_(std::move(quoteRisk)), component_(component),
          method_(valuation.Provenance().method_ + "Then" + quoteRisk_.Method()) {}

    DupireScriptQuoteRisk_ PullbackDupireScriptRisk(const Script::RiskResult_& valuation,
                                                    const DupireCalibrationSnapshot_& calibration,
                                                    const String_& component,
                                                    const std::optional<DupireDirectQuoteAdjoints_>& direct) {
        const auto parameters = ExtractDupireParameterAdjoints(valuation, calibration, component);
        return DupireScriptQuoteRisk_(valuation, PullbackDupireCalibration(calibration, parameters, direct), component);
    }
} // namespace Dal
