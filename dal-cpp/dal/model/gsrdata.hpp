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
storable GSRCurveData
    Dated log-discount-factor snapshot for a one-currency GSR model
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
storable GSRVolData
    Piecewise-constant g and H for a GSR model
version 1
&members
name is ?string
gKnotDates is date[]
gValues is number[]
hKnotDates is date[]
hValues is number[]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable GSRModelData
    One-factor Gaussian Short Rate model data
version 1
&members
name is ?string
curve is handle GSRCurveData
vol is handle GSRVolData
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct GSRCurveData_ : Storable_ {
        Date_ evaluationDate_;
        String_ currency_;
        Vector_<Date_> nodeDates_;
        Vector_<> discountLogDF_;
        Vector_<String_> projectionTenors_;
        Matrix_<> projectionLogDF_;

        GSRCurveData_(const String_& name,
                      const Date_& evaluationDate,
                      const String_& currency,
                      const Vector_<Date_>& nodeDates,
                      const Vector_<>& discountLogDF,
                      const Vector_<String_>& projectionTenors,
                      const Matrix_<>& projectionLogDF)
            : Storable_("GSRCurveData", name), evaluationDate_(evaluationDate), currency_(currency), nodeDates_(nodeDates),
              discountLogDF_(discountLogDF), projectionTenors_(projectionTenors), projectionLogDF_(projectionLogDF) {
            ValidateNodes();
            ValidateProjectionRows();
        }
        void Write(Archive::Store_& dst) const override;

    private:
        void ValidateNodes() const {
            REQUIRE(evaluationDate_.IsValid() && !currency_.empty(), "InvalidGSRCurve: valid evaluation date and currency are required");
            REQUIRE(nodeDates_.size() >= 2 && nodeDates_.size() == discountLogDF_.size() && nodeDates_.front() == evaluationDate_,
                    "InvalidGSRCurve: curve nodes must start at the evaluation date and match logDF values");
            REQUIRE(discountLogDF_.front() == 0.0, "InvalidGSRCurve: discount anchor logDF must be zero");
            for (size_t i = 0; i < nodeDates_.size(); ++i) {
                REQUIRE(i == 0 || nodeDates_[i] > nodeDates_[i - 1], "InvalidGSRCurve: node dates must be strictly increasing");
                REQUIRE(std::isfinite(discountLogDF_[i]), "InvalidGSRCurve: non-finite discount logDF");
            }
        }

        void ValidateProjectionRow(size_t row) const {
            const PeriodLength_ tenor(projectionTenors_[row]);
            REQUIRE(tenor.Months() > 0, "InvalidGSRCurve: projection tenor must have positive months");
            for (size_t prior = 0; prior < row; ++prior)
                REQUIRE(PeriodLength_(projectionTenors_[prior]).Months() != tenor.Months(), "InvalidGSRCurve: duplicate projection tenor");
            REQUIRE(projectionLogDF_(static_cast<int>(row), 0) == 0.0, "InvalidGSRCurve: projection anchor logDF must be zero");
            for (size_t col = 0; col < nodeDates_.size(); ++col)
                REQUIRE(std::isfinite(projectionLogDF_(static_cast<int>(row), static_cast<int>(col))),
                        "InvalidGSRCurve: non-finite projection logDF");
        }

        void ValidateProjectionRows() const {
            REQUIRE(projectionLogDF_.Rows() == static_cast<int>(projectionTenors_.size()) &&
                        (projectionTenors_.empty() || projectionLogDF_.Cols() == static_cast<int>(nodeDates_.size())),
                    "InvalidGSRCurve: projection rows must match tenors and curve nodes");
            for (size_t row = 0; row < projectionTenors_.size(); ++row)
                ValidateProjectionRow(row);
        }
    };

    struct GSRVolData_ : Storable_ {
        Vector_<Date_> gKnotDates_;
        Vector_<> gValues_;
        Vector_<Date_> hKnotDates_;
        Vector_<> hValues_;

        GSRVolData_(const String_& name,
                    const Vector_<Date_>& gKnotDates,
                    const Vector_<>& gValues,
                    const Vector_<Date_>& hKnotDates,
                    const Vector_<>& hValues)
            : Storable_("GSRVolData", name), gKnotDates_(gKnotDates), gValues_(gValues), hKnotDates_(hKnotDates), hValues_(hValues) {
            ValidateSizes();
            ValidateGKnots();
            ValidateHKnots();
        }
        void Write(Archive::Store_& dst) const override;

    private:
        void ValidateSizes() const {
            REQUIRE(!gKnotDates_.empty() && gKnotDates_.size() == gValues_.size() && !hKnotDates_.empty() && hKnotDates_.size() == hValues_.size(),
                    "InvalidGSRVol: g and H knots must match their values");
        }

        void ValidateGKnots() const {
            for (size_t i = 0; i < gKnotDates_.size(); ++i) {
                REQUIRE(gKnotDates_[i].IsValid() && (i == 0 || gKnotDates_[i] > gKnotDates_[i - 1]),
                        "InvalidGSRVol: g knots must be strictly increasing");
                REQUIRE(std::isfinite(gValues_[i]) && gValues_[i] >= 0.0, "InvalidGSRVol: g must be finite and nonnegative");
            }
        }

        void ValidateHKnots() const {
            for (size_t i = 0; i < hKnotDates_.size(); ++i) {
                REQUIRE(hKnotDates_[i].IsValid() && (i == 0 || hKnotDates_[i] > hKnotDates_[i - 1]),
                        "InvalidGSRVol: H knots must be strictly increasing");
                REQUIRE(std::isfinite(hValues_[i]) && hValues_[i] > 0.0, "InvalidGSRVol: H must be finite and positive");
            }
        }
    };

    struct GSRModelData_ : ModelData_ {
        Handle_<GSRCurveData_> curve_;
        Handle_<GSRVolData_> vol_;

        GSRModelData_(const String_& name, const Handle_<GSRCurveData_>& curve, const Handle_<GSRVolData_>& vol)
            : ModelData_("GSRModelData", name), curve_(curve), vol_(vol) {
            REQUIRE(curve_ && vol_, "InvalidGSRModel: curve and volatility data are required");
            REQUIRE(vol_->gKnotDates_.front() == curve_->evaluationDate_ && vol_->hKnotDates_.front() == curve_->evaluationDate_,
                    "InvalidGSRModel: g and H must start at the curve evaluation date");
        }
        void Write(Archive::Store_& dst) const override;

    private:
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
