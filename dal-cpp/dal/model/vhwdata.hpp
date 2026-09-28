//
// Created by Codex on 2026/9/28.
//

#pragma once

#include <algorithm>
#include <cmath>

#include <dal/platform/platform.hpp>

#include <dal/math/matrix/matrixs.hpp>
#include <dal/model/base.hpp>
#include <dal/storage/archive.hpp>
#include <dal/time/date.hpp>
#include <dal/time/periodlength.hpp>

/*IF--------------------------------------------------------------------------
storable VHWCurveData
    Dated log-discount-factor snapshot for a one-currency VHW model
version 1
&members
name is ?string
evaluationDate is date
currency is string
nodeDates is date[]
discountLogDF is number[]
projectionTenors is string[]
projectionLogDF is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable VHWVolData
    Piecewise-constant g and H for a VHW model
version 1
&members
name is ?string
gKnotDates is date[]
gValues is number[]
hKnotDates is date[]
hValues is number[]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable VHWModelData
    One-factor Vasicek-Hull-White model data
version 1
&members
name is ?string
curve is handle VHWCurveData
vol is handle VHWVolData
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct VHWCurveData_ : Storable_ {
        Date_ evaluationDate_;
        String_ currency_;
        Vector_<Date_> nodeDates_;
        Vector_<> discountLogDF_;
        Vector_<String_> projectionTenors_;
        Matrix_<> projectionLogDF_;

        VHWCurveData_(const String_& name,
                      const Date_& evaluationDate,
                      const String_& currency,
                      const Vector_<Date_>& nodeDates,
                      const Vector_<>& discountLogDF,
                      const Vector_<String_>& projectionTenors,
                      const Matrix_<>& projectionLogDF)
            : Storable_("VHWCurveData", name), evaluationDate_(evaluationDate), currency_(currency), nodeDates_(nodeDates),
              discountLogDF_(discountLogDF), projectionTenors_(projectionTenors), projectionLogDF_(projectionLogDF) {
            REQUIRE(evaluationDate_.IsValid() && !currency_.empty(), "InvalidVHWCurve: valid evaluation date and currency are required");
            REQUIRE(nodeDates_.size() >= 2 && nodeDates_.size() == discountLogDF_.size() && nodeDates_.front() == evaluationDate_,
                    "InvalidVHWCurve: curve nodes must start at the evaluation date and match logDF values");
            REQUIRE(projectionLogDF_.Rows() == static_cast<int>(projectionTenors_.size()) &&
                        (projectionTenors_.empty() || projectionLogDF_.Cols() == static_cast<int>(nodeDates_.size())),
                    "InvalidVHWCurve: projection rows must match tenors and curve nodes");
            REQUIRE(discountLogDF_.front() == 0.0, "InvalidVHWCurve: discount anchor logDF must be zero");
            for (size_t i = 0; i < nodeDates_.size(); ++i) {
                REQUIRE(i == 0 || nodeDates_[i] > nodeDates_[i - 1], "InvalidVHWCurve: node dates must be strictly increasing");
                REQUIRE(std::isfinite(discountLogDF_[i]), "InvalidVHWCurve: non-finite discount logDF");
            }
            for (size_t row = 0; row < projectionTenors_.size(); ++row) {
                const PeriodLength_ tenor(projectionTenors_[row]);
                REQUIRE(tenor.Months() > 0, "InvalidVHWCurve: projection tenor must have positive months");
                for (size_t prior = 0; prior < row; ++prior)
                    REQUIRE(PeriodLength_(projectionTenors_[prior]).Months() != tenor.Months(), "InvalidVHWCurve: duplicate projection tenor");
                REQUIRE(projectionLogDF_(static_cast<int>(row), 0) == 0.0, "InvalidVHWCurve: projection anchor logDF must be zero");
                for (size_t col = 0; col < nodeDates_.size(); ++col)
                    REQUIRE(std::isfinite(projectionLogDF_(static_cast<int>(row), static_cast<int>(col))),
                            "InvalidVHWCurve: non-finite projection logDF");
            }
        }
        void Write(Archive::Store_& dst) const override;
    };

    struct VHWVolData_ : Storable_ {
        Vector_<Date_> gKnotDates_;
        Vector_<> gValues_;
        Vector_<Date_> hKnotDates_;
        Vector_<> hValues_;

        VHWVolData_(const String_& name,
                    const Vector_<Date_>& gKnotDates,
                    const Vector_<>& gValues,
                    const Vector_<Date_>& hKnotDates,
                    const Vector_<>& hValues)
            : Storable_("VHWVolData", name), gKnotDates_(gKnotDates), gValues_(gValues), hKnotDates_(hKnotDates), hValues_(hValues) {
            REQUIRE(!gKnotDates_.empty() && gKnotDates_.size() == gValues_.size() && !hKnotDates_.empty() && hKnotDates_.size() == hValues_.size(),
                    "InvalidVHWVol: g and H knots must match their values");
            for (size_t i = 0; i < gKnotDates_.size(); ++i) {
                REQUIRE(gKnotDates_[i].IsValid() && (i == 0 || gKnotDates_[i] > gKnotDates_[i - 1]),
                        "InvalidVHWVol: g knots must be strictly increasing");
                REQUIRE(std::isfinite(gValues_[i]) && gValues_[i] >= 0.0, "InvalidVHWVol: g must be finite and nonnegative");
            }
            for (size_t i = 0; i < hKnotDates_.size(); ++i) {
                REQUIRE(hKnotDates_[i].IsValid() && (i == 0 || hKnotDates_[i] > hKnotDates_[i - 1]),
                        "InvalidVHWVol: H knots must be strictly increasing");
                REQUIRE(std::isfinite(hValues_[i]) && hValues_[i] > 0.0, "InvalidVHWVol: H must be finite and positive");
            }
        }
        void Write(Archive::Store_& dst) const override;
    };

    struct VHWModelData_ : ModelData_ {
        Handle_<VHWCurveData_> curve_;
        Handle_<VHWVolData_> vol_;

        VHWModelData_(const String_& name, const Handle_<VHWCurveData_>& curve, const Handle_<VHWVolData_>& vol)
            : ModelData_("VHWModelData", name), curve_(curve), vol_(vol) {
            REQUIRE(curve_ && vol_, "InvalidVHWModel: curve and volatility data are required");
            REQUIRE(vol_->gKnotDates_.front() == curve_->evaluationDate_ && vol_->hKnotDates_.front() == curve_->evaluationDate_,
                    "InvalidVHWModel: g and H must start at the curve evaluation date");
        }
        void Write(Archive::Store_& dst) const override;

    private:
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
