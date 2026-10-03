//
// Created by Codex on 2026/10/2.
//

#include <algorithm>
#include <cmath>

#include <dal/model/gsrslvdata.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    namespace {
        void ValidateAxis(const Vector_<>& axis, bool time) {
            REQUIRE(!axis.empty(), "InvalidGSRLeverage: axes must be nonempty");
            for (size_t i = 0; i < axis.size(); ++i)
                REQUIRE(std::isfinite(axis[i]) && (!time || axis[i] >= 0.0) && (i == 0 || axis[i] > axis[i - 1]),
                        "InvalidGSRLeverage: axes must be finite and strictly increasing; times must be nonnegative");
        }
    } // namespace

#include <dal/auto/MG_GSRLeverageData_v1_Read.inc>
#include <dal/auto/MG_GSRLeverageData_v1_Write.inc>
#include <dal/auto/MG_GSRSLVModelData_v1_Read.inc>
#include <dal/auto/MG_GSRSLVModelData_v1_Write.inc>

    GSRLeverageData_::GSRLeverageData_(const String_& name, const Vector_<>& rateShifts, const Vector_<>& times, const Matrix_<>& values)
        : Storable_("GSRLeverageData", name), rateShifts_(rateShifts), times_(times), values_(values) {
        ValidateAxis(rateShifts_, false);
        ValidateAxis(times_, true);
        REQUIRE(values_.Rows() == static_cast<int>(rateShifts_.size()) && values_.Cols() == static_cast<int>(times_.size()),
                "InvalidGSRLeverage: value rows must match rate shifts and columns must match times");
        for (int row = 0; row < values_.Rows(); ++row)
            for (int col = 0; col < values_.Cols(); ++col)
                REQUIRE(std::isfinite(values_(row, col)) && values_(row, col) > 0.0,
                        "InvalidGSRLeverage: leverage values must be finite and strictly positive");
    }

    GSRSLVModelData_::GSRSLVModelData_(const String_& name,
                                       const Handle_<MultiFactorGSRModelData_>& gaussian,
                                       const Handle_<GSRLeverageData_>& leverage,
                                       double kappa,
                                       double volOfVol,
                                       const Vector_<>& varianceCorrelations,
                                       double maxStep)
        : ModelData_("GSRSLVModelData", name), gaussian_(gaussian), leverage_(leverage), kappa_(kappa), volOfVol_(volOfVol),
          varianceCorrelations_(varianceCorrelations), maxStep_(maxStep) {
        REQUIRE(gaussian_ && leverage_, "InvalidGSRSLVModel: Gaussian model and leverage data are required");
        REQUIRE(std::isfinite(kappa_) && kappa_ >= 0.0 && std::isfinite(volOfVol_) && volOfVol_ >= 0.0,
                "InvalidGSRSLVVariance: kappa and volOfVol must be finite and nonnegative");
        REQUIRE(std::isfinite(maxStep_) && maxStep_ > 0.0, "InvalidGSRSLVStep: maxStep must be finite and positive");
        const size_t n = gaussian_->vol_->factorNames_.size();
        if (varianceCorrelations_.empty())
            varianceCorrelations_ = Vector_<>(n, 0.0);
        REQUIRE(varianceCorrelations_.size() == n, "InvalidGSRSLVCorrelation: one variance correlation per rate factor is required");
        static_cast<void>(AAD::CovarianceFactor(DriverCorrelation()));
    }

    Matrix_<> GSRSLVModelData_::DriverCorrelation() const {
        const int n = static_cast<int>(varianceCorrelations_.size());
        Matrix_<> correlation(n + 1, n + 1, 0.0);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j)
                correlation(i, j) = gaussian_->vol_->correlations_(i, j);
            correlation(i, n) = correlation(n, i) = varianceCorrelations_[i];
        }
        correlation(n, n) = 1.0;
        return correlation;
    }

    Matrix_<> GSRSLVModelData_::FactorCorrelations() const {
        const auto drivers = DriverCorrelation();
        const int n = drivers.Rows();
        Matrix_<> factors(n + 1, n + 1, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                factors(i, j) = drivers(i, j);
        factors(n, n) = 1.0;
        return factors;
    }

    Vector_<> GSRSLVModelData_::BreakpointTimes() const {
        const auto& curve = *gaussian_->curve_;
        Vector_<> times;
        const auto pushAll = [&](const Vector_<Date_>& dates) {
            for (const auto& date : dates)
                times.push_back((date - curve.evaluationDate_) / DAYS_PER_YEAR);
        };
        pushAll(gaussian_->vol_->gKnotDates_);
        pushAll(gaussian_->vol_->hKnotDates_);
        for (const double time : leverage_->times_)
            times.push_back(time);
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        return times;
    }

    Vector_<String_> GSRSLVModelData_::RiskLabels() const {
        auto labels = gaussian_->curve_->RiskLabels();
        labels.Append(gaussian_->vol_->RiskLabels());
        labels.push_back("kappa");
        labels.push_back("volOfVol");
        for (int row = 0; row < leverage_->values_.Rows(); ++row)
            for (int col = 0; col < leverage_->values_.Cols(); ++col)
                labels.push_back("leverage:" + String::FromInt(row) + ":" + String::FromInt(col));
        return labels;
    }

    void GSRLeverageData_::Write(Archive::Store_& dst) const { GSRLeverageData_v1::XWrite(dst, name_, rateShifts_, times_, values_); }
    void GSRSLVModelData_::Write(Archive::Store_& dst) const {
        GSRSLVModelData_v1::XWrite(dst, name_, gaussian_, leverage_, kappa_, volOfVol_, varianceCorrelations_, maxStep_);
    }
    std::unique_ptr<ModelData_> GSRSLVModelData_::MutantModel(const String_* newName, const Slide_* slide) const {
        REQUIRE(!slide, "slides are not supported for GSRSLVModelData");
        return std::make_unique<GSRSLVModelData_>(*newName, gaussian_, leverage_, kappa_, volOfVol_, varianceCorrelations_, maxStep_);
    }
} // namespace Dal
