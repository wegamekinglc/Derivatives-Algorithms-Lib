//
// Created by Codex on 2026/10/5.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <dal/curve/quoteriskmapping.hpp>

#include <dal-public/src/calibrationrisk.hpp>

namespace Dal {
    namespace {
        struct MethodInfo_ {
            String_ domain_;
            String_ method_;
            String_ unit_;
            String_ boundary_;
        };

        const MethodInfo_& MethodInfo(const CalibrationPullback_::Source_& source) {
            static const MethodInfo_ dupire{"DUPIRE", "NativeAADCalibrationVJP", "decimal-vol", "FixedBaseIVSDeterministicCarryFixedGrids"};
            static const MethodInfo_ curve{"RATE_CURVE", "RetainedCurveEffectiveInverse", "DECIMAL_QUOTE", "FrozenCalibrationEffectiveInverse"};
            return std::holds_alternative<DupireCalibrationSnapshot_>(source) ? dupire : curve;
        }

        std::pair<int, int> Shape(const CalibrationPullback_::Source_& source, bool parameters) {
            return std::visit(
                [parameters](const auto& value) -> std::pair<int, int> {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, DupireCalibrationSnapshot_>) {
                        const auto& matrix = parameters ? value.Surface()->vols_ : value.Inputs().quoteSpreads_;
                        return {matrix.Rows(), matrix.Cols()};
                    } else {
                        const size_t rows = parameters ? value.Axis().parameters_.size() : value.Axis().quotes_.size();
                        REQUIRE(rows <= static_cast<size_t>(std::numeric_limits<int>::max()),
                                "InvalidCalibrationPullback: axis exceeds matrix limits");
                        return {static_cast<int>(rows), 1};
                    }
                },
                source);
        }

        void CheckShape(int rows, int columns) {
            REQUIRE(rows > 0 && columns > 0, "InvalidCalibrationPullback: matrix dimensions must be positive");
            const auto width = static_cast<size_t>(columns);
            const auto height = static_cast<size_t>(rows);
            REQUIRE(height <= std::numeric_limits<size_t>::max() / width / sizeof(double),
                    "InvalidCalibrationPullback: matrix storage exceeds size limits");
            REQUIRE(height <= static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / width,
                    "InvalidCalibrationPullback: matrix storage exceeds iterator limits");
        }

        void ValidateCurve(const RateQuoteRiskProvenance_& calibration) {
            REQUIRE(calibration.Available(), calibration.Reason());
            REQUIRE(!calibration.CalibrationRecord().empty(), "QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED");
            static const std::array<const char*, 4> kinds{"SINGLE_CURVE", "JOINT_XCCY", "STAGED_XCCY_BASIS", "JOINT_MULTI_CURVE"};
            REQUIRE(std::any_of(kinds.begin(), kinds.end(), [&](const char* kind) { return calibration.Kind() == kind; }),
                    "InvalidCalibrationPullback: unsupported curve provenance kind");
            const auto& inverse = calibration.EffectiveInverse();
            CheckShape(inverse.Rows(), inverse.Cols());
            REQUIRE(static_cast<size_t>(inverse.Rows()) == calibration.Axis().parameters_.size() &&
                        static_cast<size_t>(inverse.Cols()) == calibration.Axis().quotes_.size(),
                    "InvalidCalibrationPullback: inverse dimensions disagree with curve axes");
            REQUIRE(std::isfinite(calibration.Tolerance()) && calibration.Tolerance() > 0.0,
                    "InvalidCalibrationPullback: curve tolerance must be finite and positive");
        }

        bool SameText(const String_& lhs, const String_& rhs) { return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin()); }

        template <class V_, class C_> bool SameValues(const V_& lhs, const V_& rhs, C_ compare) {
            return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(), compare);
        }

        bool SameRanges(const Vector_<RateQuoteRiskRange_>& lhs, const Vector_<RateQuoteRiskRange_>& rhs) {
            return SameValues(lhs, rhs, [](const auto& left, const auto& right) {
                return SameText(left.blockKey_, right.blockKey_) && left.offset_ == right.offset_ && left.size_ == right.size_;
            });
        }

        bool SameParameters(const RateQuoteRiskAxis_& lhs, const RateQuoteRiskAxis_& rhs) {
            return SameValues(lhs.parameters_, rhs.parameters_, [](const auto& left, const auto& right) {
                return SameText(left.blockKey_, right.blockKey_) && left.blockOrdinal_ == right.blockOrdinal_ &&
                       left.globalOrdinal_ == right.globalOrdinal_ && left.date_ == right.date_ && left.component_ == right.component_;
            });
        }

        bool SameQuotes(const RateQuoteRiskAxis_& lhs, const RateQuoteRiskAxis_& rhs) {
            return SameValues(lhs.quotes_, rhs.quotes_, [](const auto& left, const auto& right) {
                return SameText(left.blockKey_, right.blockKey_) && left.blockOrdinal_ == right.blockOrdinal_ &&
                       left.globalOrdinal_ == right.globalOrdinal_ && SameText(left.displayName_, right.displayName_) &&
                       SameText(left.unit_, right.unit_);
            });
        }

        bool SameAxis(const RateQuoteRiskAxis_& lhs, const RateQuoteRiskAxis_& rhs) {
            return SameText(lhs.scheme_, rhs.scheme_) && SameText(lhs.fingerprint_, rhs.fingerprint_) &&
                   SameRanges(lhs.parameterRanges_, rhs.parameterRanges_) && SameRanges(lhs.residualRanges_, rhs.residualRanges_) &&
                   SameParameters(lhs, rhs) && SameQuotes(lhs, rhs);
        }

        bool SameState(const RateQuoteRiskState_& lhs, const RateQuoteRiskState_& rhs) {
            return SameText(lhs.scheme_, rhs.scheme_) && SameText(lhs.fingerprint_, rhs.fingerprint_) &&
                   SameValues(lhs.components_, rhs.components_, [](const auto& left, const auto& right) {
                       return SameText(left.componentKey_, right.componentKey_) && SameText(left.fingerprint_, right.fingerprint_);
                   });
        }

        bool SameBindings(const RateQuoteRiskProvenance_& lhs, const RateQuoteRiskProvenance_& rhs) {
            return SameValues(lhs.ComponentKeyByParameterBlock(), rhs.ComponentKeyByParameterBlock(), [](const auto& left, const auto& right) {
                return SameText(left.first, right.first) && SameText(left.second, right.second);
            });
        }

        bool SameCurve(const RateQuoteRiskProvenance_& lhs, const RateQuoteRiskProvenance_& rhs) {
            if (!SameText(lhs.State().fingerprint_, rhs.State().fingerprint_) || !SameText(lhs.Axis().fingerprint_, rhs.Axis().fingerprint_))
                return false;
            return SameText(lhs.Kind(), rhs.Kind()) && SameText(lhs.CalibrationId(), rhs.CalibrationId()) && SameAxis(lhs.Axis(), rhs.Axis()) &&
                   SameState(lhs.State(), rhs.State()) && SameBindings(lhs, rhs) && lhs.CalibrationRecord() == rhs.CalibrationRecord();
        }

        Matrix_<> CheckedSeeds(const CalibrationPullback_& calibration, const Matrix_<>& adjoints, bool parameters) {
            const auto [rows, columns] = Shape(calibration.Source(), parameters);
            CheckShape(rows, columns);
            const String_ role = parameters ? "parameters" : "directQuotes";
            REQUIRE(adjoints.Rows() == rows && adjoints.Cols() == columns, "InvalidCalibrationPullback: seed dimensions disagree; field=" + role);
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column)
                    REQUIRE(std::isfinite(adjoints(row, column)), "InvalidCalibrationPullback: non-finite seed; field=" + role +
                                                                      "; row=" + String::FromInt(row) + "; column=" + String::FromInt(column));
            return adjoints;
        }

        DupireQuoteRisk_ DupirePullback(const DupireCalibrationSnapshot_& calibration,
                                        const CalibrationParameterAdjoints_& parameters,
                                        const std::optional<CalibrationDirectQuoteAdjoints_>& direct) {
            std::optional<DupireDirectQuoteAdjoints_> typedDirect;
            if (direct)
                typedDirect = DupireDirectQuoteAdjoints_{std::get<DupireCalibrationSnapshot_>(direct->Calibration().Source()), direct->Adjoints()};
            try {
                return PullbackDupireCalibration(calibration, DupireParameterAdjoints_{calibration, parameters.Adjoints()}, typedDirect);
            } catch (const Exception_& error) {
                THROW("InvalidCalibrationPullback: " + String_(error.what()));
            }
        }

        std::array<Matrix_<>, 3> CurvePullback(const CalibrationPullback_& calibration,
                                               const CalibrationParameterAdjoints_& parameters,
                                               const std::optional<CalibrationDirectQuoteAdjoints_>& direct) {
            if (direct)
                REQUIRE(calibration.Matches(direct->Calibration()), "CalibrationSnapshotMismatch: direct curve provenance does not match");
            const int rows = calibration.QuoteRows();
            std::array<Matrix_<>, 3> result{Matrix_<>(rows, 1), direct ? direct->Adjoints() : Matrix_<>(rows, 1, 0.0), Matrix_<>(rows, 1)};
            const auto& provenance = std::get<RateQuoteRiskProvenance_>(calibration.Source());
            const Vector_<> gradient(parameters.Adjoints().begin(), parameters.Adjoints().end());
            RateQuoteRiskInternal::ForEachQuoteAdjoint(
                provenance, gradient,
                [&](int row, double sensitivity, double) {
                    result[0](row, 0) = sensitivity;
                    const double total = sensitivity + result[1](row, 0);
                    REQUIRE(std::isfinite(total), "InvalidCalibrationPullback: non-finite total quote adjoint; row=" + String::FromInt(row));
                    result[2](row, 0) = total;
                },
                "InvalidCalibrationPullback: non-finite curve quote adjoint");
            return result;
        }
    } // namespace

    CalibrationPullback_::CalibrationPullback_(Source_ source) : source_(std::make_shared<const Source_>(std::move(source))) {}
    const String_& CalibrationPullback_::Domain() const { return MethodInfo(Source()).domain_; }
    int CalibrationPullback_::ParameterRows() const { return Shape(Source(), true).first; }
    int CalibrationPullback_::ParameterCols() const { return Shape(Source(), true).second; }
    int CalibrationPullback_::QuoteRows() const { return Shape(Source(), false).first; }
    int CalibrationPullback_::QuoteCols() const { return Shape(Source(), false).second; }
    const String_& CalibrationPullback_::Method() const { return MethodInfo(Source()).method_; }
    const String_& CalibrationPullback_::Unit() const { return MethodInfo(Source()).unit_; }
    const String_& CalibrationPullback_::Boundary() const { return MethodInfo(Source()).boundary_; }

    bool CalibrationPullback_::Matches(const CalibrationPullback_& other) const {
        if (source_ == other.source_)
            return true;
        if (Source().index() != other.Source().index())
            return false;
        if (const auto* dupire = std::get_if<DupireCalibrationSnapshot_>(&Source()))
            return dupire->Matches(std::get<DupireCalibrationSnapshot_>(other.Source()));
        return SameCurve(std::get<RateQuoteRiskProvenance_>(Source()), std::get<RateQuoteRiskProvenance_>(other.Source()));
    }

    CalibrationPullback_ NewCalibrationPullback(const DupireCalibrationSnapshot_& calibration) {
        CheckShape(calibration.Surface()->vols_.Rows(), calibration.Surface()->vols_.Cols());
        CheckShape(calibration.Inputs().quoteSpreads_.Rows(), calibration.Inputs().quoteSpreads_.Cols());
        return CalibrationPullback_(calibration);
    }

    CalibrationPullback_ NewCalibrationPullback(const RateQuoteRiskProvenance_& calibration) {
        ValidateCurve(calibration);
        return CalibrationPullback_(calibration);
    }

    template <class T_>
    CalibrationAdjoints_<T_>::CalibrationAdjoints_(const CalibrationPullback_& calibration, const Matrix_<>& adjoints)
        : calibration_(calibration), adjoints_(CheckedSeeds(calibration, adjoints, std::is_same_v<T_, CalibrationParameterSeedTag_>)) {}

    template class CalibrationAdjoints_<CalibrationParameterSeedTag_>;
    template class CalibrationAdjoints_<CalibrationDirectQuoteSeedTag_>;

    CalibrationParameterAdjoints_ NewCalibrationParameterAdjoints(const CalibrationPullback_& calibration, const Matrix_<>& adjoints) {
        return CalibrationParameterAdjoints_(calibration, adjoints);
    }

    CalibrationDirectQuoteAdjoints_ NewCalibrationDirectQuoteAdjoints(const CalibrationPullback_& calibration, const Matrix_<>& adjoints) {
        return CalibrationDirectQuoteAdjoints_(calibration, adjoints);
    }

    struct CalibrationQuoteRisk_::Data_ {
        CalibrationPullback_ calibration_;
        Matrix_<> calibrationAdjoints_;
        Matrix_<> directAdjoints_;
        Matrix_<> totalAdjoints_;
    };

    CalibrationQuoteRisk_::CalibrationQuoteRisk_(const CalibrationPullback_& calibration,
                                                 Matrix_<>&& calibrated,
                                                 Matrix_<>&& direct,
                                                 Matrix_<>&& total)
        : data_(std::make_shared<const Data_>(Data_{calibration, std::move(calibrated), std::move(direct), std::move(total)})) {}

    const CalibrationPullback_& CalibrationQuoteRisk_::Calibration() const { return data_->calibration_; }
    const Matrix_<>& CalibrationQuoteRisk_::CalibrationAdjoints() const { return data_->calibrationAdjoints_; }
    const Matrix_<>& CalibrationQuoteRisk_::DirectAdjoints() const { return data_->directAdjoints_; }
    const Matrix_<>& CalibrationQuoteRisk_::TotalAdjoints() const { return data_->totalAdjoints_; }
    const String_& CalibrationQuoteRisk_::Method() const { return Calibration().Method(); }
    const String_& CalibrationQuoteRisk_::Unit() const { return Calibration().Unit(); }
    const String_& CalibrationQuoteRisk_::Boundary() const { return Calibration().Boundary(); }

    CalibrationQuoteRisk_ PullbackCalibration(const CalibrationPullback_& calibration,
                                              const CalibrationParameterAdjoints_& parameters,
                                              const std::optional<CalibrationDirectQuoteAdjoints_>& direct) {
        REQUIRE(calibration.Matches(parameters.Calibration()), "CalibrationSnapshotMismatch: parameter calibration does not match");
        if (direct)
            REQUIRE(calibration.Source().index() == direct->Calibration().Source().index(),
                    "CalibrationSnapshotMismatch: direct domain does not match");
        if (const auto* dupire = std::get_if<DupireCalibrationSnapshot_>(&calibration.Source())) {
            const auto result = DupirePullback(*dupire, parameters, direct);
            return CalibrationQuoteRisk_(calibration, Matrix_<>(result.CalibrationAdjoints()), Matrix_<>(result.DirectAdjoints()),
                                         Matrix_<>(result.TotalAdjoints()));
        }
        auto result = CurvePullback(calibration, parameters, direct);
        return CalibrationQuoteRisk_(calibration, std::move(result[0]), std::move(result[1]), std::move(result[2]));
    }
} // namespace Dal
