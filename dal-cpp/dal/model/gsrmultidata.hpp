//
// Created by Codex on 2026/10/2.
//

#pragma once

#include <algorithm>
#include <cmath>

#include <dal/math/matrix/covariance.hpp>
#include <dal/model/gsrdata.hpp>

/*IF--------------------------------------------------------------------------
storable MultiFactorGSRVolData
    Named multi-factor Gaussian rate volatility
version 1
&members
name is ?string
factorNames is string[]
gKnotDates is date[]
gValues is number[][]
hKnotDates is date[]
hValues is number[][]
correlations is number[][]
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable MultiFactorGSRModelData
    Multi-factor Gaussian short rate model
version 1
&members
name is ?string
curve is handle GSRCurveData
vol is handle MultiFactorGSRVolData
-IF-------------------------------------------------------------------------*/

namespace Dal {
    struct MultiFactorGSRVolSettings_ {
        Vector_<String_> factorNames_;
        Vector_<Date_> gKnotDates_;
        Matrix_<> gValues_;
        Vector_<Date_> hKnotDates_;
        Matrix_<> hValues_;
        Matrix_<> correlations_;
    };

    struct MultiFactorGSRVolData_ : Storable_ {
        Vector_<String_> factorNames_;
        Vector_<Date_> gKnotDates_;
        Matrix_<> gValues_;
        Vector_<Date_> hKnotDates_;
        Matrix_<> hValues_;
        Matrix_<> correlations_;

        MultiFactorGSRVolData_(const String_& name, const MultiFactorGSRVolSettings_& settings)
            : MultiFactorGSRVolData_(name,
                                     settings.factorNames_,
                                     settings.gKnotDates_,
                                     settings.gValues_,
                                     settings.hKnotDates_,
                                     settings.hValues_,
                                     settings.correlations_) {}
        MultiFactorGSRVolData_(const String_& name,
                               const Vector_<String_>& factorNames,
                               const Vector_<Date_>& gKnotDates,
                               const Matrix_<>& gValues,
                               const Vector_<Date_>& hKnotDates,
                               const Matrix_<>& hValues,
                               const Matrix_<>& correlations)
            : Storable_("MultiFactorGSRVolData", name), factorNames_(factorNames), gKnotDates_(gKnotDates), gValues_(gValues),
              hKnotDates_(hKnotDates), hValues_(hValues), correlations_(correlations) {
            REQUIRE(!factorNames_.empty(), "InvalidGSRFactors: at least one factor is required");
            for (size_t i = 0; i < factorNames_.size(); ++i) {
                REQUIRE(!factorNames_[i].empty(), "InvalidGSRFactors: factor name must be nonempty");
                REQUIRE(std::find(factorNames_.begin(), factorNames_.begin() + i, factorNames_[i]) == factorNames_.begin() + i,
                        "InvalidGSRFactors: duplicate factor " + factorNames_[i]);
            }
            ValidatePieces(gKnotDates_, gValues_, true);
            ValidatePieces(hKnotDates_, hValues_, false);
            const int n = static_cast<int>(factorNames_.size());
            REQUIRE(correlations_.Rows() == n && correlations_.Cols() == n, "InvalidGSRCorrelation: dimensions must match factor names");
            for (int i = 0; i < n; ++i)
                REQUIRE(std::isfinite(correlations_(i, i)) && std::abs(correlations_(i, i) - 1.0) <= 1e-12,
                        "InvalidGSRCorrelation: diagonal must equal one");
            static_cast<void>(AAD::CovarianceFactor(correlations_));
        }
        // Correlations among the named factors, in factorNames_ order.
        [[nodiscard]] Matrix_<> FactorCorrelations() const { return correlations_; }
        // Labels of the named-factor g/H risk parameters, factor-major as the kernel registers them.
        [[nodiscard]] Vector_<String_> RiskLabels() const {
            Vector_<String_> labels;
            for (size_t factor = 0; factor < factorNames_.size(); ++factor)
                for (const auto& date : gKnotDates_)
                    labels.push_back("g:" + factorNames_[factor] + ":" + Date::ToString(date));
            for (size_t factor = 0; factor < factorNames_.size(); ++factor)
                for (const auto& date : hKnotDates_)
                    labels.push_back("H:" + factorNames_[factor] + ":" + Date::ToString(date));
            return labels;
        }
        void Write(Archive::Store_& dst) const override;

    private:
        void ValidatePieces(const Vector_<Date_>& dates, const Matrix_<>& values, bool nonnegative) const {
            REQUIRE(!dates.empty(), "InvalidGSRVol: knot dates must be nonempty");
            REQUIRE(values.Rows() == static_cast<int>(factorNames_.size()) && values.Cols() == static_cast<int>(dates.size()),
                    "InvalidGSRVol: rows must match factors and columns must match dates");
            for (size_t i = 0; i < dates.size(); ++i) {
                REQUIRE(dates[i].IsValid() && (i == 0 || dates[i] > dates[i - 1]), "InvalidGSRVol: dates must be valid and strictly increasing");
                for (int row = 0; row < values.Rows(); ++row)
                    REQUIRE(std::isfinite(values(row, static_cast<int>(i))) && (!nonnegative || values(row, static_cast<int>(i)) >= 0.0),
                            "InvalidGSRVol: g must be finite and nonnegative; H must be finite");
            }
        }
    };

    struct MultiFactorGSRModelData_ : ModelData_ {
        Handle_<GSRCurveData_> curve_;
        Handle_<MultiFactorGSRVolData_> vol_;

        MultiFactorGSRModelData_(const String_& name, const Handle_<GSRCurveData_>& curve, const Handle_<MultiFactorGSRVolData_>& vol)
            : ModelData_("MultiFactorGSRModelData", name), curve_(curve), vol_(vol) {
            REQUIRE(curve_ && vol_, "InvalidGSRModel: curve and volatility data are required");
            REQUIRE(vol_->gKnotDates_.front() == curve_->evaluationDate_ && vol_->hKnotDates_.front() == curve_->evaluationDate_,
                    "InvalidGSRModel: g and H must start at the curve evaluation date");
        }
        void Write(Archive::Store_& dst) const override;

    private:
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const override;
    };
} // namespace Dal
