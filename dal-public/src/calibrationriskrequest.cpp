//
// Created by Codex on 2026/10/5.
//

#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>

#include <dal/platform/platform.hpp>

#include <dal-public/src/calibrationriskrequest.hpp>

namespace Dal {
    namespace {
        String_ QuoteId(size_t ordinal) { return "quote:" + String_(std::to_string(ordinal)); }

        Vector_<CalibrationQuoteCoordinate_> DupireAxis(const DupireCalibrationSnapshot_& source) {
            const auto& inputs = source.Inputs();
            Vector_<CalibrationQuoteCoordinate_> axis;
            axis.reserve(static_cast<size_t>(inputs.quoteSpreads_.Rows()) * static_cast<size_t>(inputs.quoteSpreads_.Cols()));
            for (int row = 0; row < inputs.quoteSpreads_.Rows(); ++row)
                for (int column = 0; column < inputs.quoteSpreads_.Cols(); ++column) {
                    CalibrationQuoteCoordinate_ coordinate;
                    coordinate.ordinal_ = axis.size();
                    coordinate.id_ = QuoteId(coordinate.ordinal_);
                    coordinate.label_ = "spread:" + String::FromInt(row) + ":" + String::FromInt(column);
                    coordinate.row_ = row;
                    coordinate.column_ = column;
                    coordinate.nativeUnit_ = "decimal-vol";
                    coordinate.value_ = inputs.quoteSpreads_(row, column);
                    coordinate.strike_ = inputs.quoteStrikes_[static_cast<size_t>(row)];
                    coordinate.maturity_ = inputs.quoteMaturities_[static_cast<size_t>(column)];
                    axis.push_back(std::move(coordinate));
                }
            return axis;
        }

        Vector_<CalibrationQuoteCoordinate_> CurveAxis(const RateQuoteRiskProvenance_& source) {
            Vector_<CalibrationQuoteCoordinate_> axis;
            axis.reserve(source.Axis().quotes_.size());
            for (const auto& quote : source.Axis().quotes_) {
                REQUIRE(quote.globalOrdinal_ == static_cast<int>(axis.size()) && quote.blockOrdinal_ >= 0 && !quote.blockKey_.empty() &&
                            !quote.unit_.empty(),
                        "InvalidCalibrationRiskRequest: native curve quote coordinate disagrees with ordinal axis");
                CalibrationQuoteCoordinate_ coordinate;
                coordinate.ordinal_ = axis.size();
                coordinate.id_ = QuoteId(coordinate.ordinal_);
                coordinate.label_ = quote.displayName_;
                coordinate.row_ = quote.globalOrdinal_;
                coordinate.nativeUnit_ = quote.unit_;
                coordinate.blockKey_ = quote.blockKey_;
                coordinate.blockOrdinal_ = quote.blockOrdinal_;
                axis.push_back(std::move(coordinate));
            }
            return axis;
        }

        Vector_<CalibrationQuoteCoordinate_> CompleteAxis(const CalibrationPullback_& source) {
            return std::visit(
                [](const auto& value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>, DupireCalibrationSnapshot_>)
                        return DupireAxis(value);
                    else
                        return CurveAxis(value);
                },
                source.Source());
        }

        Vector_<size_t> SelectedOrdinals(const Vector_<CalibrationQuoteCoordinate_>& axis, const CalibrationRiskRequest_& request) {
            if (!request.inputs_) {
                Vector_<size_t> ordinals(axis.size());
                std::iota(ordinals.begin(), ordinals.end(), size_t{0});
                return ordinals;
            }
            std::map<String_, size_t> available;
            for (const auto& coordinate : axis)
                available.emplace(coordinate.id_, coordinate.ordinal_);
            std::set<String_> seen;
            Vector_<size_t> ordinals;
            ordinals.reserve(request.inputs_->size());
            for (const auto& id : *request.inputs_) {
                REQUIRE(id.find('\0') == String_::npos, "InvalidCalibrationRiskRequest: embedded NUL in input ID");
                REQUIRE(seen.insert(id).second, "InvalidCalibrationRiskRequest: repeated input ID; input=" + id);
                const auto found = available.find(id);
                REQUIRE(found != available.end(), "InvalidCalibrationRiskRequest: unknown input ID; input=" + id);
                ordinals.push_back(found->second);
            }
            return ordinals;
        }

        Vector_<CalibrationQuoteCoordinate_>
        SelectedAxis(const Vector_<CalibrationQuoteCoordinate_>& axis, const Vector_<size_t>& ordinals, const CalibrationRiskRequest_& request) {
            REQUIRE(!request.reportFactors_ || request.reportFactors_->size() == ordinals.size(),
                    "InvalidCalibrationRiskRequest: report factor count must match selected inputs; field=reportFactors");
            Vector_<CalibrationQuoteCoordinate_> selected;
            selected.reserve(ordinals.size());
            for (size_t column = 0; column < ordinals.size(); ++column) {
                auto coordinate = axis[ordinals[column]];
                if (request.reportFactors_)
                    coordinate.reportScale_ = (*request.reportFactors_)[column];
                REQUIRE(std::isfinite(coordinate.reportScale_) && coordinate.reportScale_ > 0.0,
                        "InvalidCalibrationRiskRequest: report factor must be finite and positive; input=" + coordinate.id_);
                selected.push_back(std::move(coordinate));
            }
            return selected;
        }

        void ValidateReports(const CalibrationRiskPlan_& plan, const CalibrationQuoteRisk_& risk) {
            for (const auto& coordinate : plan.InputAxis())
                for (const auto* contribution : {&risk.CalibrationAdjoints(), &risk.DirectAdjoints(), &risk.TotalAdjoints()})
                    REQUIRE(std::isfinite((*contribution)(coordinate.row_, coordinate.column_) * coordinate.reportScale_),
                            "InvalidCalibrationRiskRequest: non-finite reported contribution; input=" + coordinate.id_);
        }

        Matrix_<> Projection(const CalibrationRiskPlan_& plan, const Matrix_<>& matrix, bool reported) {
            Matrix_<> result(1, static_cast<int>(plan.InputAxis().size()));
            for (size_t column = 0; column < plan.InputAxis().size(); ++column) {
                const auto& coordinate = plan.InputAxis()[column];
                result(0, static_cast<int>(column)) = matrix(coordinate.row_, coordinate.column_) * (reported ? coordinate.reportScale_ : 1.0);
            }
            return result;
        }
    } // namespace

    size_t CalibrationRiskPayloadBytes(size_t quoteRows, size_t quoteCols) {
        const size_t maximum = (std::numeric_limits<size_t>::max)();
        constexpr size_t BYTES_PER_QUOTE = 3 * sizeof(double);
        REQUIRE(quoteRows > 0 && quoteCols > 0 && quoteCols <= maximum / BYTES_PER_QUOTE && quoteRows <= maximum / BYTES_PER_QUOTE / quoteCols,
                "InvalidCalibrationRiskRequest: invalid or overflowing numeric result extent");
        return quoteRows * quoteCols * BYTES_PER_QUOTE;
    }

    CalibrationRiskPlan_::CalibrationRiskPlan_(const CalibrationPullback_& calibration,
                                               Vector_<CalibrationQuoteCoordinate_>&& completeAxis,
                                               Vector_<CalibrationQuoteCoordinate_>&& selectedAxis,
                                               Vector_<size_t>&& selectedOrdinals,
                                               size_t numericPayloadBytes)
        : calibration_(calibration), completeAxis_(std::move(completeAxis)), selectedAxis_(std::move(selectedAxis)),
          selectedOrdinals_(std::move(selectedOrdinals)), numericPayloadBytes_(numericPayloadBytes) {}

    CalibrationRiskPlan_ PlanCalibrationRiskRequest(const CalibrationPullback_& calibration, const CalibrationRiskRequest_& request) {
        const auto rows = static_cast<size_t>(calibration.QuoteRows());
        const auto columns = static_cast<size_t>(calibration.QuoteCols());
        const size_t bytes = CalibrationRiskPayloadBytes(rows, columns);
        REQUIRE(rows * columns <= static_cast<size_t>((std::numeric_limits<int>::max)()),
                "InvalidCalibrationRiskRequest: quote count exceeds the matrix column limit");
        REQUIRE(!request.numericPayloadBudgetBytes_ || bytes <= *request.numericPayloadBudgetBytes_,
                "CalibrationRiskBudgetExceeded: full numeric contribution payload exceeds numericPayloadBudgetBytes");
        auto axis = CompleteAxis(calibration);
        auto ordinals = SelectedOrdinals(axis, request);
        auto selected = SelectedAxis(axis, ordinals, request);
        return CalibrationRiskPlan_(calibration, std::move(axis), std::move(selected), std::move(ordinals), bytes);
    }

    CalibrationRiskResult_::CalibrationRiskResult_(const CalibrationRiskPlan_& plan, CalibrationQuoteRisk_&& quoteRisk)
        : plan_(plan), quoteRisk_(std::move(quoteRisk)) {}

    Matrix_<> CalibrationRiskResult_::Jacobian() const { return Projection(plan_, quoteRisk_.TotalAdjoints(), false); }
    Matrix_<> CalibrationRiskResult_::CalibrationJacobian() const { return Projection(plan_, quoteRisk_.CalibrationAdjoints(), false); }
    Matrix_<> CalibrationRiskResult_::DirectJacobian() const { return Projection(plan_, quoteRisk_.DirectAdjoints(), false); }
    Matrix_<> CalibrationRiskResult_::ReportedJacobian() const { return Projection(plan_, quoteRisk_.TotalAdjoints(), true); }

    CalibrationRiskResult_ PullbackCalibrationWithRisk(const CalibrationRiskPlan_& plan,
                                                       const CalibrationParameterAdjoints_& parameters,
                                                       const std::optional<CalibrationDirectQuoteAdjoints_>& direct) {
        auto risk = PullbackCalibration(plan.Calibration(), parameters, direct);
        ValidateReports(plan, risk);
        return CalibrationRiskResult_(plan, std::move(risk));
    }
} // namespace Dal
