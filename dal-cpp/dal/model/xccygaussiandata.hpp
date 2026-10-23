#pragma once

#include <limits>

#include <dal/curve/curveblock.hpp>
#include <dal/curve/xccyinstrument.hpp>
#include <dal/model/base.hpp>
#include <dal/storage/archive.hpp>

/*IF--------------------------------------------------------------------------
storable XccyGaussianCurveData
    Owning normalized log-linear discount snapshot and native coordinate map
version 1
&members
name is ?string
currency is string
route is string
nodeDates is date[]
logDF is number[]
nativeLabels is string[]
nativeValues is number[]
snapshotJacobian is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable XccyGaussianMarket
    Frozen domestic collateral cross-currency market
version 1
&members
name is ?string
valuationDate is date
domesticCurrency is string
foreignCurrency is string
collateralCurrency is string
fxSpot is number
discountRoute is string
domestic is handle XccyGaussianCurveData
foreign is handle XccyGaussianCurveData
basis is handle XccyGaussianCurveData
projections is +handle XccyGaussianCurveData
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable XccyGaussianDynamics
    Explicit dated short-rate and FX dynamics with named correlation
version 1
&members
name is ?string
domesticA is number
foreignA is number
bucketEndDates is date[]
domesticSigma is number[]
foreignSigma is number[]
fxSigma is number[]
factorNames is string[]
correlation is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable XccyGaussianModelData
    Immutable domestic collateral Gaussian cross-currency model data
version 1
&members
name is ?string
market is handle XccyGaussianMarket
dynamics is handle XccyGaussianDynamics
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct XccyGaussianMarketSpec_ {
        DateTime_ valuationTime_;
        std::optional<CurrencyPair_> pair_;
        Ccy_ collateralCurrency_;
        double fxSpot_ = std::numeric_limits<double>::quiet_NaN();
        Handle_<CurveBlock_> domesticBlock_;
        Handle_<CurveBlock_> foreignBlock_;
        Handle_<DiscountCurve_> basis_;
        String_ discountRoute_;
    };

    struct XccyGaussianDynamicsSpec_ {
        double domesticA_ = std::numeric_limits<double>::quiet_NaN();
        double foreignA_ = std::numeric_limits<double>::quiet_NaN();
        Vector_<Date_> bucketEndDates_;
        Vector_<> domesticSigma_, foreignSigma_, fxSigma_;
        Vector_<String_> factorNames_;
        Matrix_<> correlation_;
    };

    struct XccyGaussianModelSpec_ {
        XccyGaussianMarketSpec_ market_;
        XccyGaussianDynamicsSpec_ dynamics_;
    };

    struct XccyGaussianCurveData_ final : Storable_ {
        const String_ currency_, route_;
        const Vector_<Date_> nodeDates_;
        const Vector_<> logDF_;
        const Vector_<String_> nativeLabels_;
        const Vector_<> nativeValues_;
        const Matrix_<> snapshotJacobian_;

        XccyGaussianCurveData_(const String_& name,
                               const String_& currency,
                               const String_& route,
                               const Vector_<Date_>& nodeDates,
                               const Vector_<>& logDF,
                               const Vector_<String_>& nativeLabels,
                               const Vector_<>& nativeValues,
                               const Matrix_<>& snapshotJacobian);
        [[nodiscard]] double LogDF(const Date_& date) const;
        void Write(Archive::Store_& dst) const override;
    };

    [[nodiscard]] Handle_<XccyGaussianCurveData_>
    SnapshotXccyGaussianCurve(const DiscountCurve_& curve, const Date_& valuationDate, const String_& currency, const String_& route);

    struct XccyGaussianMarket_ final : Storable_ {
        const DateTime_ valuationTime_;
        const CurrencyPair_ pair_;
        const Ccy_ collateralCurrency_;
        const double fxSpot_;
        const String_ discountRoute_;
        const Handle_<XccyGaussianCurveData_> domestic_, foreign_, basis_;
        const Vector_<Handle_<XccyGaussianCurveData_>> projections_;

        XccyGaussianMarket_(const String_& name, const XccyGaussianMarketSpec_& spec);
        XccyGaussianMarket_(const String_& name,
                            const Date_& valuationDate,
                            const String_& domesticCurrency,
                            const String_& foreignCurrency,
                            const String_& collateralCurrency,
                            double fxSpot,
                            const String_& discountRoute,
                            const Handle_<XccyGaussianCurveData_>& domestic,
                            const Handle_<XccyGaussianCurveData_>& foreign,
                            const Handle_<XccyGaussianCurveData_>& basis,
                            const Vector_<Handle_<XccyGaussianCurveData_>>& projections);
        [[nodiscard]] const XccyGaussianCurveData_& Projection(const String_& currency, const String_& tenor) const;
        [[nodiscard]] Vector_<String_> FactorNames() const;
        void Write(Archive::Store_& dst) const override;
    };

    struct XccyGaussianDynamics_ final : Storable_ {
        const double domesticA_, foreignA_;
        const Vector_<Date_> bucketEndDates_;
        const Vector_<> domesticSigma_, foreignSigma_, fxSigma_;
        const Vector_<String_> factorNames_;
        const Matrix_<> correlation_;

        XccyGaussianDynamics_(const String_& name, const XccyGaussianDynamicsSpec_& spec);
        XccyGaussianDynamics_(const String_& name,
                              double domesticA,
                              double foreignA,
                              const Vector_<Date_>& bucketEndDates,
                              const Vector_<>& domesticSigma,
                              const Vector_<>& foreignSigma,
                              const Vector_<>& fxSigma,
                              const Vector_<String_>& factorNames,
                              const Matrix_<>& correlation);
        void Write(Archive::Store_& dst) const override;
    };

    struct XccyGaussianModelData_ final : ModelData_ {
        const Handle_<XccyGaussianMarket_> market_;
        const Handle_<XccyGaussianDynamics_> dynamics_;

        XccyGaussianModelData_(const String_& name, const XccyGaussianModelSpec_& spec);
        XccyGaussianModelData_(const String_& name, const Handle_<XccyGaussianMarket_>& market, const Handle_<XccyGaussianDynamics_>& dynamics);
        [[nodiscard]] String_ Identity() const;
        void RequireSameGeometry(const XccyGaussianModelData_& candidate) const;
        void Write(Archive::Store_& dst) const override;

    private:
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
