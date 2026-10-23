#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <algorithm>
#include <cmath>
#include <set>

#include <dal/calibration/validation_internal.hpp>
#include <dal/model/xccygaussiandata.hpp>
#include <dal/storage/json.hpp>

namespace Dal {
#include <dal/auto/MG_XccyGaussianCurveData_v1_Read.inc>
#include <dal/auto/MG_XccyGaussianCurveData_v1_Write.inc>
#include <dal/auto/MG_XccyGaussianDynamics_v1_Read.inc>
#include <dal/auto/MG_XccyGaussianDynamics_v1_Write.inc>
#include <dal/auto/MG_XccyGaussianMarket_v1_Read.inc>
#include <dal/auto/MG_XccyGaussianMarket_v1_Write.inc>
#include <dal/auto/MG_XccyGaussianModelData_v1_Read.inc>
#include <dal/auto/MG_XccyGaussianModelData_v1_Write.inc>

    namespace {
        using CalibrationInternal::Check;
        void PlanCheck(bool valid, const String_& field, const String_& expected, const String_& actual = "") {
            Check(valid, "InvalidCalibrationPlan", "/model/" + field, expected, actual);
        }
        void ContextCheck(bool valid, const String_& field, const String_& expected, const String_& actual = "") {
            Check(valid, "CalibrationContextMismatch", "/model/market/" + field, expected, actual);
        }

        const CurrencyPair_& RequiredPair(const XccyGaussianMarketSpec_& spec) {
            ContextCheck(spec.pair_.has_value(), "pair", "explicit domestic/foreign currency pair");
            ContextCheck(spec.pair_->domestic_.String() && spec.pair_->foreign_.String(), "pair", "two valid distinct currencies");
            return *spec.pair_;
        }

        void ValidateCurveNodes(const XccyGaussianCurveData_& curve) {
            PlanCheck(!curve.currency_.empty() && !curve.route_.empty(), "market/curve/route", "explicit currency and route");
            PlanCheck(curve.nodeDates_.size() >= 2 && curve.nodeDates_.size() == curve.logDF_.size(), "market/curve/nodes",
                      "matching dated logDF nodes");
            PlanCheck(curve.logDF_.front() == 0., "market/curve/anchor", "normalized zero logDF at V");
            for (size_t i = 0; i < curve.nodeDates_.size(); ++i) {
                PlanCheck(curve.nodeDates_[i].IsValid() && (i == 0 || curve.nodeDates_[i] > curve.nodeDates_[i - 1]), "market/curve/nodes",
                          "valid, strictly increasing dates");
                const double discount = std::exp(curve.logDF_[i]);
                PlanCheck(std::isfinite(discount) && discount > 0., "market/curve/logDF", "positive finite discounts");
            }
        }

        void ValidateNativeMap(const XccyGaussianCurveData_& curve) {
            PlanCheck(curve.nativeLabels_.size() == curve.nativeValues_.size() &&
                          curve.snapshotJacobian_.Rows() == static_cast<int>(curve.nodeDates_.size()) &&
                          curve.snapshotJacobian_.Cols() == static_cast<int>(curve.nativeValues_.size()),
                      "market/curve/snapshotJacobian", "one row per snapshot node and one column per native coordinate");
            std::set<String_> labels;
            for (size_t i = 0; i < curve.nativeValues_.size(); ++i) {
                PlanCheck(!curve.nativeLabels_[i].empty() && labels.insert(curve.nativeLabels_[i]).second, "market/curve/nativeLabels",
                          "unique native labels");
                PlanCheck(std::isfinite(curve.nativeValues_[i]), "market/curve/nativeValues", "finite native values");
            }
            for (int i = 0; i < curve.snapshotJacobian_.Rows(); ++i) {
                long double reconstructed = 0.L;
                for (int j = 0; j < curve.snapshotJacobian_.Cols(); ++j) {
                    PlanCheck(std::isfinite(curve.snapshotJacobian_(i, j)), "market/curve/snapshotJacobian", "finite native derivative weights");
                    PlanCheck(i != 0 || curve.snapshotJacobian_(i, j) == 0., "market/curve/snapshotJacobian", "fixed zero-risk anchor row");
                    reconstructed += static_cast<long double>(curve.snapshotJacobian_(i, j)) * curve.nativeValues_[j];
                }
                PlanCheck(std::abs(reconstructed - curve.logDF_[i]) <=
                              64.L * std::numeric_limits<double>::epsilon() * std::max(1.L, std::abs(reconstructed)),
                          "market/curve/snapshotJacobian", "native coordinate map reproducing the normalized snapshot");
            }
        }

        Handle_<XccyGaussianCurveData_> CaptureDiscount(const Handle_<CurveBlock_>& block, const DateTime_& valuationTime, const Ccy_& currency) {
            ContextCheck(block && block->ccy_ == currency, "block/currency", currency.String());
            const auto route = block->DiscountCurves().find(CollateralType_("OIS"));
            ContextCheck(route != block->DiscountCurves().end() && route->second, "block/OIS", "explicit OIS discount route");
            return SnapshotXccyGaussianCurve(*route->second, valuationTime.Date(), currency.String(), "OIS");
        }

        Handle_<XccyGaussianCurveData_> CaptureBasis(const XccyGaussianMarketSpec_& spec) {
            const auto& pair = RequiredPair(spec);
            ContextCheck(static_cast<bool>(spec.basis_), "basis", "explicit domestic-denominated basis curve, including a unit curve for zero basis");
            return SnapshotXccyGaussianCurve(*spec.basis_, spec.valuationTime_.Date(), pair.domestic_.String(),
                                             String_("BASIS:") + pair.foreign_.String() + "/" + pair.domestic_.String());
        }

        Vector_<Handle_<XccyGaussianCurveData_>> CaptureProjections(const XccyGaussianMarketSpec_& spec) {
            Vector_<Handle_<XccyGaussianCurveData_>> result;
            for (const auto& block : {spec.domesticBlock_, spec.foreignBlock_}) {
                ContextCheck(static_cast<bool>(block), "projection/block", "explicit owning curve block");
                for (const auto& entry : block->ForwardCurves()) {
                    ContextCheck(static_cast<bool>(entry.second), "projection/route", "explicit projection curve");
                    const int months = entry.first.Months();
                    PlanCheck(months == 3 || months == 6, "market/projections/tenor", "canonical 3M or 6M route", entry.first.String());
                    result.push_back(
                        SnapshotXccyGaussianCurve(*entry.second, spec.valuationTime_.Date(), block->ccy_.String(), String::FromInt(months) + "M"));
                }
            }
            return result;
        }

        void ValidateMarketCurve(const Handle_<XccyGaussianCurveData_>& curve, const String_& currency, const String_& route, const Date_& date) {
            ContextCheck(curve && curve->currency_ == currency && curve->route_ == route, route, "matching currency and explicit route");
            ContextCheck(curve->nodeDates_.front() == date, route + "/valuationDate", Date::ToString(date),
                         Date::ToString(curve->nodeDates_.front()));
        }

        void ValidateMarket(const XccyGaussianMarket_& market) {
            Check(market.valuationTime_.IsValid() && market.valuationTime_.Frac() == 0., "UnsupportedCalibrationFixingTime",
                  "/model/market/valuationTime", "exact midnight valuation", DateTime::ToString(market.valuationTime_));
            ContextCheck(market.collateralCurrency_ == market.pair_.domestic_, "collateralCurrency", market.pair_.domestic_.String());
            PlanCheck(std::isfinite(market.fxSpot_) && market.fxSpot_ > 0., "market/fxSpot", "positive finite domestic per foreign spot");
            PlanCheck(!market.discountRoute_.empty(), "market/discountRoute", "explicit discount route identity");
            const Date_ date = market.valuationTime_.Date();
            ValidateMarketCurve(market.domestic_, market.pair_.domestic_.String(), "OIS", date);
            ValidateMarketCurve(market.foreign_, market.pair_.foreign_.String(), "OIS", date);
            ValidateMarketCurve(market.basis_, market.pair_.domestic_.String(),
                                String_("BASIS:") + market.pair_.foreign_.String() + "/" + market.pair_.domestic_.String(), date);
            std::set<std::pair<String_, String_>> routes;
            for (const auto& curve : market.projections_) {
                ContextCheck(curve && (curve->currency_ == market.pair_.domestic_.String() || curve->currency_ == market.pair_.foreign_.String()),
                             "projection/currency", "one of the two source currencies");
                ValidateMarketCurve(curve, curve->currency_, curve->route_, date);
                PlanCheck(routes.emplace(curve->currency_, curve->route_).second, "market/projections", "unique currency/tenor routes");
                PlanCheck(curve->route_ == "3M" || curve->route_ == "6M", "market/projections/tenor", "canonical 3M or 6M route", curve->route_);
            }
        }

        long double Determinant(const long double b[3][3]) {
            return b[0][0] * (b[1][1] * b[2][2] - b[1][2] * b[2][1]) - b[0][1] * (b[1][0] * b[2][2] - b[1][2] * b[2][0]) +
                   b[0][2] * (b[1][0] * b[2][1] - b[1][1] * b[2][0]);
        }

        double MinimumEigenvalue(const Matrix_<>& matrix) {
            const long double off = matrix(0, 1) * matrix(0, 1) + matrix(0, 2) * matrix(0, 2) + matrix(1, 2) * matrix(1, 2);
            if (off == 0.L)
                return 1.;
            const long double p = std::sqrt(off / 3.L);
            long double b[3][3]{};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    b[i][j] = (matrix(i, j) - (i == j ? 1.L : 0.L)) / p;
            const long double angle = std::acos(std::clamp(Determinant(b) / 2.L, -1.L, 1.L)) / 3.L;
            return static_cast<double>(1.L + 2.L * p * std::cos(angle + 2.L * std::acos(-1.L) / 3.L));
        }

        void ValidateCorrelation(const XccyGaussianDynamics_& dynamics) {
            const auto& matrix = dynamics.correlation_;
            PlanCheck(matrix.Rows() == 3 && matrix.Cols() == 3 && dynamics.factorNames_.size() == 3, "dynamics/correlation", "named 3x3 correlation");
            PlanCheck(std::set<String_>(dynamics.factorNames_.begin(), dynamics.factorNames_.end()).size() == 3, "dynamics/correlation/names",
                      "three unique economic Brownian names");
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    PlanCheck(std::isfinite(matrix(i, j)) && matrix(i, j) == matrix(j, i), "dynamics/correlation", "finite symmetric correlation");
                    PlanCheck(i != j || matrix(i, j) == 1., "dynamics/correlation", "unit diagonal");
                }
            const double minimum = MinimumEigenvalue(matrix);
            PlanCheck(minimum > 1e-10, "dynamics/correlation", "minimum eigenvalue > 1e-10", CalibrationInternal::NumberText(minimum));
        }

        void ValidateBuckets(const XccyGaussianDynamics_& dynamics) {
            const size_t count = dynamics.bucketEndDates_.size();
            PlanCheck(count > 0, "dynamics/buckets", "nonempty explicit bucket ends");
            for (const auto* values : {&dynamics.domesticSigma_, &dynamics.foreignSigma_, &dynamics.fxSigma_})
                PlanCheck(count == values->size(), "dynamics/buckets", "matching three volatility vectors");
            for (size_t i = 0; i < count; ++i) {
                PlanCheck(dynamics.bucketEndDates_[i].IsValid() && (i == 0 || dynamics.bucketEndDates_[i] > dynamics.bucketEndDates_[i - 1]),
                          "dynamics/bucketEndDates", "valid, strictly increasing dates");
                for (const double value : {dynamics.domesticSigma_[i], dynamics.foreignSigma_[i], dynamics.fxSigma_[i]})
                    PlanCheck(std::isfinite(value) && value >= 0., "dynamics/sigma", "finite nonnegative explicitly supplied volatility");
            }
        }

        bool SameNativeMap(const Matrix_<>& first, const Matrix_<>& second) {
            if (first.Rows() != second.Rows() || first.Cols() != second.Cols())
                return false;
            for (int i = 0; i < first.Rows(); ++i)
                for (int j = 0; j < first.Cols(); ++j)
                    if (first(i, j) != second(i, j))
                        return false;
            return true;
        }

        bool SameCurveGeometry(const XccyGaussianCurveData_& first, const XccyGaussianCurveData_& second) {
            return first.currency_ == second.currency_ && first.route_ == second.route_ && first.nodeDates_ == second.nodeDates_ &&
                   first.nativeLabels_ == second.nativeLabels_ && SameNativeMap(first.snapshotJacobian_, second.snapshotJacobian_);
        }
    } // namespace

    XccyGaussianCurveData_::XccyGaussianCurveData_(const String_& name,
                                                   const String_& currency,
                                                   const String_& route,
                                                   const Vector_<Date_>& nodeDates,
                                                   const Vector_<>& logDF,
                                                   const Vector_<String_>& nativeLabels,
                                                   const Vector_<>& nativeValues,
                                                   const Matrix_<>& snapshotJacobian)
        : Storable_("XccyGaussianCurveData", name), currency_(currency), route_(route), nodeDates_(nodeDates), logDF_(logDF),
          nativeLabels_(nativeLabels), nativeValues_(nativeValues), snapshotJacobian_(snapshotJacobian) {
        ValidateCurveNodes(*this);
        ValidateNativeMap(*this);
    }

    double XccyGaussianCurveData_::LogDF(const Date_& date) const {
        ContextCheck(date.IsValid() && date >= nodeDates_.front() && date <= nodeDates_.back(), route_ + "/coverage", "date within snapshot coverage",
                     Date::ToString(date));
        const auto found = std::lower_bound(nodeDates_.begin(), nodeDates_.end(), date);
        const size_t upper = static_cast<size_t>(found - nodeDates_.begin());
        if (*found == date)
            return logDF_[upper];
        const double weight = static_cast<double>(date - nodeDates_[upper - 1]) / (nodeDates_[upper] - nodeDates_[upper - 1]);
        return logDF_[upper - 1] * (1. - weight) + logDF_[upper] * weight;
    }

    XccyGaussianMarket_::XccyGaussianMarket_(const String_& name, const XccyGaussianMarketSpec_& spec)
        : Storable_("XccyGaussianMarket", name), valuationTime_(spec.valuationTime_), pair_(RequiredPair(spec)),
          collateralCurrency_(spec.collateralCurrency_), fxSpot_(spec.fxSpot_), discountRoute_(spec.discountRoute_),
          domestic_(CaptureDiscount(spec.domesticBlock_, spec.valuationTime_, pair_.domestic_)),
          foreign_(CaptureDiscount(spec.foreignBlock_, spec.valuationTime_, pair_.foreign_)), basis_(CaptureBasis(spec)),
          projections_(CaptureProjections(spec)) {
        ValidateMarket(*this);
    }

    XccyGaussianMarket_::XccyGaussianMarket_(const String_& name,
                                             const Date_& valuationDate,
                                             const String_& domesticCurrency,
                                             const String_& foreignCurrency,
                                             const String_& collateralCurrency,
                                             double fxSpot,
                                             const String_& discountRoute,
                                             const Handle_<XccyGaussianCurveData_>& domestic,
                                             const Handle_<XccyGaussianCurveData_>& foreign,
                                             const Handle_<XccyGaussianCurveData_>& basis,
                                             const Vector_<Handle_<XccyGaussianCurveData_>>& projections)
        : Storable_("XccyGaussianMarket", name), valuationTime_(valuationDate), pair_(Ccy_(domesticCurrency), Ccy_(foreignCurrency)),
          collateralCurrency_(collateralCurrency), fxSpot_(fxSpot), discountRoute_(discountRoute), domestic_(domestic), foreign_(foreign),
          basis_(basis), projections_(projections) {
        ValidateMarket(*this);
    }

    const XccyGaussianCurveData_& XccyGaussianMarket_::Projection(const String_& currency, const String_& tenor) const {
        for (const auto& curve : projections_)
            if (curve->currency_ == currency && curve->route_ == tenor)
                return *curve;
        CalibrationInternal::Fail("CalibrationContextMismatch", "/model/market/projection/" + currency + "/" + tenor, "explicit projection route");
    }

    Vector_<String_> XccyGaussianMarket_::FactorNames() const {
        return {String_("rate:") + pair_.domestic_.String(), String_("rate:") + pair_.foreign_.String(),
                String_("fx:") + pair_.foreign_.String() + "/" + pair_.domestic_.String()};
    }

    XccyGaussianDynamics_::XccyGaussianDynamics_(const String_& name, const XccyGaussianDynamicsSpec_& spec)
        : XccyGaussianDynamics_(name,
                                spec.domesticA_,
                                spec.foreignA_,
                                spec.bucketEndDates_,
                                spec.domesticSigma_,
                                spec.foreignSigma_,
                                spec.fxSigma_,
                                spec.factorNames_,
                                spec.correlation_) {}

    XccyGaussianDynamics_::XccyGaussianDynamics_(const String_& name,
                                                 double domesticA,
                                                 double foreignA,
                                                 const Vector_<Date_>& bucketEndDates,
                                                 const Vector_<>& domesticSigma,
                                                 const Vector_<>& foreignSigma,
                                                 const Vector_<>& fxSigma,
                                                 const Vector_<String_>& factorNames,
                                                 const Matrix_<>& correlation)
        : Storable_("XccyGaussianDynamics", name), domesticA_(domesticA), foreignA_(foreignA), bucketEndDates_(bucketEndDates),
          domesticSigma_(domesticSigma), foreignSigma_(foreignSigma), fxSigma_(fxSigma), factorNames_(factorNames), correlation_(correlation) {
        PlanCheck(std::isfinite(domesticA_) && domesticA_ >= 0. && std::isfinite(foreignA_) && foreignA_ >= 0., "dynamics/meanReversion",
                  "explicit finite nonnegative domestic and foreign mean reversions");
        ValidateBuckets(*this);
        ValidateCorrelation(*this);
    }

    XccyGaussianModelData_::XccyGaussianModelData_(const String_& name, const XccyGaussianModelSpec_& spec)
        : XccyGaussianModelData_(name,
                                 Handle_<XccyGaussianMarket_>(new XccyGaussianMarket_(name, spec.market_)),
                                 Handle_<XccyGaussianDynamics_>(new XccyGaussianDynamics_(name, spec.dynamics_))) {}

    XccyGaussianModelData_::XccyGaussianModelData_(const String_& name,
                                                   const Handle_<XccyGaussianMarket_>& market,
                                                   const Handle_<XccyGaussianDynamics_>& dynamics)
        : ModelData_("XccyGaussianModelData", name), market_(market), dynamics_(dynamics) {
        PlanCheck(market_ && dynamics_, "data", "owning market and dynamics");
        PlanCheck(dynamics_->bucketEndDates_.front() > market_->valuationTime_.Date(), "dynamics/bucketEndDates",
                  "first bucket end strictly after V");
        ContextCheck(market_->FactorNames() == dynamics_->factorNames_, "correlation/names",
                     "ordered rate:domestic, rate:foreign, fx:foreign/domestic names");
    }

    String_ XccyGaussianModelData_::Identity() const {
        return CalibrationInternal::Fingerprint(CalibrationInternal::Parse(JSON::WriteString(*this)));
    }

    void XccyGaussianModelData_::RequireSameGeometry(const XccyGaussianModelData_& candidate) const {
        const auto& first = *market_;
        const auto& second = *candidate.market_;
        ContextCheck(first.valuationTime_ == second.valuationTime_ && first.pair_ == second.pair_ && first.discountRoute_ == second.discountRoute_,
                     "refresh", "same V, pair, collateral and route");
        ContextCheck(SameCurveGeometry(*first.domestic_, *second.domestic_) && SameCurveGeometry(*first.foreign_, *second.foreign_) &&
                         SameCurveGeometry(*first.basis_, *second.basis_) && first.projections_.size() == second.projections_.size(),
                     "refresh/curves", "identical snapshot and native coordinate layouts");
        for (size_t i = 0; i < first.projections_.size(); ++i)
            ContextCheck(SameCurveGeometry(*first.projections_[i], *second.projections_[i]), "refresh/projections",
                         "same ordered projection geometry");
        ContextCheck(dynamics_->bucketEndDates_ == candidate.dynamics_->bucketEndDates_ &&
                         dynamics_->factorNames_ == candidate.dynamics_->factorNames_,
                     "refresh/dynamics", "same buckets and economic factor registry");
    }

    void XccyGaussianCurveData_::Write(Archive::Store_& dst) const {
        XccyGaussianCurveData_v1::XWrite(dst, name_, currency_, route_, nodeDates_, logDF_, nativeLabels_, nativeValues_, snapshotJacobian_);
    }
    void XccyGaussianMarket_::Write(Archive::Store_& dst) const {
        XccyGaussianMarket_v1::XWrite(dst, name_, valuationTime_.Date(), pair_.domestic_.String(), pair_.foreign_.String(),
                                      collateralCurrency_.String(), fxSpot_, discountRoute_, domestic_, foreign_, basis_, projections_);
    }
    void XccyGaussianDynamics_::Write(Archive::Store_& dst) const {
        XccyGaussianDynamics_v1::XWrite(dst, name_, domesticA_, foreignA_, bucketEndDates_, domesticSigma_, foreignSigma_, fxSigma_, factorNames_,
                                        correlation_);
    }
    void XccyGaussianModelData_::Write(Archive::Store_& dst) const { XccyGaussianModelData_v1::XWrite(dst, name_, market_, dynamics_); }

    std::unique_ptr<ModelData_> XccyGaussianModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        PlanCheck(!slide, "slide", "no implicit XCCY model slides");
        return std::make_unique<XccyGaussianModelData_>(*newName, market_, dynamics_);
    }
} // namespace Dal
