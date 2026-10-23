#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <numeric>

#include <dal/calibration/validation_internal.hpp>
#include <dal/curve/ycconst.hpp>
#include <dal/curve/yclogdf.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/model/xccygaussiandata.hpp>

namespace Dal {
    namespace {
        void Check(bool valid, const String_& field, const String_& expected, const String_& actual = "") {
            CalibrationInternal::Check(valid, "MCCrossCurrencyUnsupported", "/model/market/" + field, expected, actual);
        }

        struct NativePiece_ {
            Vector_<Date_> dates_;
            Vector_<> values_;
            Vector_<String_> labels_;
            bool logDF_ = false;
            bool paired_ = false;
        };

        Vector_<String_> Labels(const Vector_<Date_>& dates, const String_& prefix, bool excludeAnchor) {
            Vector_<String_> result;
            for (size_t i = excludeAnchor ? 1 : 0; i < dates.size(); ++i)
                result.push_back(prefix + ":" + Date::ToString(dates[i]));
            return result;
        }

        NativePiece_ LogPiece(const DiscountLogDF_& curve, const String_& prefix) {
            Check(curve.Scheme() == LogDfScheme_::Value_::LOG_LINEAR, prefix, "lossless log-linear logDF curve");
            Check(curve.DayCount() == DayBasis_("ACT365F") || curve.DayCount() == DayBasis_("ACT360"), prefix, "an actual-days linear time axis");
            auto values = curve.NodeLogDF();
            Check(values.front() == 0., prefix, "exactly zero pinned native anchor logDF");
            values.erase(values.begin());
            return {curve.NodeDates(), values, Labels(curve.NodeDates(), "logdf:" + prefix, true), true, false};
        }

        NativePiece_ ForwardPiece(const Tape::DiscountPWC_<double>& curve, const String_& prefix) {
            return {curve.KnotDates(), curve.FRight(), Labels(curve.KnotDates(), "forward:" + prefix, false), false, false};
        }

        NativePiece_ ForwardPiece(const Tape::DiscountPWLF_<double>& curve, const String_& prefix) {
            const auto left = curve.FLeft(), right = curve.FRight();
            for (size_t i = 1; i < left.size(); ++i)
                Check(left[i] == right[i - 1], prefix, "piecewise-constant forwards on every interval");
            auto values = left;
            auto labels = Labels(curve.KnotDates(), "forward-left:" + prefix, false);
            values.Append(right);
            const auto rightLabels = Labels(curve.KnotDates(), "forward-right:" + prefix, false);
            labels.Append(rightLabels);
            return {curve.KnotDates(), values, labels, false, true};
        }

        void CapturePieces(const DiscountCurve_& curve, const String_& currency, const String_& route, Vector_<NativePiece_>* pieces) {
            CalibrationInternal::Check(curve.ccy_.String() == currency, "CalibrationContextMismatch", "/model/market/" + route + "/currency",
                                       currency, curve.ccy_.String());
            const String_ prefix = currency + ":" + route + (pieces->empty() ? String_("") : ":base" + String::FromInt(pieces->size()));
            Handle_<DiscountCurve_> base;
            if (const auto* log = dynamic_cast<const DiscountLogDF_*>(&curve)) {
                pieces->push_back(LogPiece(*log, prefix));
                base = log->Base();
            } else if (const auto* constant = dynamic_cast<const Tape::DiscountPWC_<double>*>(&curve)) {
                pieces->push_back(ForwardPiece(*constant, prefix));
                base = constant->Base();
            } else if (const auto* forward = dynamic_cast<const Tape::DiscountPWLF_<double>*>(&curve)) {
                pieces->push_back(ForwardPiece(*forward, prefix));
                base = forward->Base();
            } else
                Check(false, route, "log-linear logDF or explicit piecewise-constant forwards", curve.Type());
            if (base)
                CapturePieces(*base, currency, route, pieces);
        }

        Vector_<> LogWeights(const NativePiece_& piece, const Date_& date) {
            Vector_<> result(piece.values_.size(), 0.);
            const auto next = std::upper_bound(piece.dates_.begin(), piece.dates_.end(), date);
            const size_t upper = std::min(static_cast<size_t>(next - piece.dates_.begin()), piece.dates_.size() - 1);
            const size_t lower = upper - 1;
            const double weight = static_cast<double>(date - piece.dates_[lower]) / (piece.dates_[upper] - piece.dates_[lower]);
            if (lower > 0)
                result[lower - 1] = 1. - weight;
            result[upper - 1] = weight;
            return result;
        }

        Vector_<> ForwardWeights(const NativePiece_& piece, const Date_& date) {
            Vector_<> result(piece.values_.size(), 0.);
            for (size_t i = 1; i < piece.dates_.size() && date > piece.dates_[i - 1]; ++i) {
                const double width = piece.dates_[i] - piece.dates_[i - 1];
                const double days = std::min(date, piece.dates_[i]) - piece.dates_[i - 1];
                if (piece.paired_) {
                    const double stopWeight = days * days / (2. * width);
                    result[i] -= stopWeight / 365.;
                    result[piece.dates_.size() + i - 1] -= (days - stopWeight) / 365.;
                } else
                    result[i - 1] -= days / 365.;
            }
            return result;
        }

        Vector_<> Weights(const NativePiece_& piece, const Date_& date) {
            return piece.logDF_ ? LogWeights(piece, date) : ForwardWeights(piece, date);
        }

        Vector_<Date_> SnapshotDates(const Vector_<NativePiece_>& pieces, const Date_& valuationDate, const String_& route) {
            Date_ finalDate = pieces.front().dates_.back();
            for (const auto& piece : pieces) {
                Check(piece.dates_.size() >= 2 && valuationDate >= piece.dates_.front() && valuationDate < piece.dates_.back(), route + "/coverage",
                      "explicit nodes covering V and a strictly later horizon");
                finalDate = std::min(finalDate, piece.dates_.back());
            }
            Vector_<Date_> result{valuationDate, finalDate};
            for (const auto& piece : pieces)
                for (const auto& date : piece.dates_)
                    if (date > valuationDate && date < finalDate)
                        result.push_back(date);
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }
    } // namespace

    Handle_<XccyGaussianCurveData_>
    SnapshotXccyGaussianCurve(const DiscountCurve_& curve, const Date_& valuationDate, const String_& currency, const String_& route) {
        Vector_<NativePiece_> pieces;
        CapturePieces(curve, currency, route, &pieces);
        const auto dates = SnapshotDates(pieces, valuationDate, route);
        Vector_<String_> labels;
        Vector_<> values;
        for (const auto& piece : pieces) {
            labels.Append(piece.labels_);
            values.Append(piece.values_);
        }
        Matrix_<> jacobian(static_cast<int>(dates.size()), static_cast<int>(values.size()), 0.);
        Vector_<> logDF(dates.size(), 0.);
        size_t offset = 0;
        for (const auto& piece : pieces) {
            const auto anchor = Weights(piece, valuationDate);
            for (size_t row = 1; row < dates.size(); ++row) {
                const auto weights = Weights(piece, dates[row]);
                for (size_t col = 0; col < weights.size(); ++col) {
                    const double derivative = weights[col] - anchor[col];
                    jacobian(static_cast<int>(row), static_cast<int>(offset + col)) = derivative;
                    logDF[row] += derivative * piece.values_[col];
                }
            }
            offset += piece.values_.size();
        }
        return Handle_<XccyGaussianCurveData_>(new XccyGaussianCurveData_(curve.Name(), currency, route, dates, logDF, labels, values, jacobian));
    }
} // namespace Dal
