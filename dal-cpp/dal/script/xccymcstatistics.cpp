#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/batchstatistics_internal.hpp>
#include <dal/script/calibrationmc_internal.hpp>
#include <dal/script/xccymc_internal.hpp>

namespace Dal::XccyMcInternal {
    namespace {
        using namespace CalibrationInternal;
        Vector_<> Row(const CalibrationAffineCoefficients_& c) {
            Vector_<> result{c.b_, c.a_};
            for (size_t j = 0; j < c.db_.size(); ++j) {
                result.push_back(c.db_[j]);
                result.push_back(c.da_[j]);
            }
            for (const auto value : result)
                Check(std::isfinite(value), "CalibrationNumericalFailure", "/execution/coefficients", "finite native coefficient estimates");
            return result;
        }
        double Error(const Vector_<Vector_<>>& rows, const Vector_<>& means, const Vector_<>& weights) {
            const double error = BatchStatisticsInternal::ProjectedError(rows, means, weights);
            if (!std::isfinite(error))
                Fail("CalibrationNumericalFailure", "/execution/covariance", "finite projected variance");
            return error;
        }
        void PriceSummary(const Vector_<Vector_<>>& rows, const Vector_<>& means, double coupon, XccyMcResult_* result) {
            auto& estimate = result->estimate_;
            const size_t n = estimate.parameterLabels_.size();
            result->coefficients_ = {means[1], means[0], Vector_<>(n), Vector_<>(n)};
            estimate.mean_ = means[0] + coupon * means[1];
            Vector_<> weights(means.size(), 0.);
            weights[0] = 1.;
            weights[1] = coupon;
            estimate.standardError_ = Error(rows, means, weights);
            for (size_t j = 0; j < n; ++j) {
                const double db = means[2 + 2 * j], da = means[3 + 2 * j];
                result->coefficients_.db_[j] = db;
                result->coefficients_.da_[j] = da;
                estimate.gradient_.push_back(db + coupon * da);
                weights = Vector_<>(means.size(), 0.);
                weights[2 + 2 * j] = 1.;
                weights[3 + 2 * j] = coupon;
                estimate.gradientStandardError_.push_back(Error(rows, means, weights));
                Check(std::isfinite(estimate.gradient_.back()) && std::isfinite(estimate.gradientStandardError_.back()),
                      "CalibrationNumericalFailure", "/derivatives/estimate", "finite native partial and gradient standard error");
            }
        }
        void ParSummary(const Vector_<CalibrationAffineCoefficients_>& coefficients,
                        const CalibrationMeasureSpec_& measure,
                        CalibrationMcResult_* estimate) {
            estimate->affine_ = CalibrationMcInternal::AffineEstimate(coefficients, *measure.annuitySign_, *measure.minAbsAnnuity_, 3.);
            estimate->mean_ = estimate->affine_->value_.value_;
            estimate->standardError_ = estimate->affine_->standardError_;
            estimate->gradient_ = estimate->affine_->value_.derivatives_;
            estimate->gradientStandardError_ = estimate->affine_->gradientStandardError_;
            for (auto& batch : estimate->batches_) {
                batch.value_ = 0.;
                batch.derivatives_.clear();
            }
        }
        String_ ParameterUnit(const String_& label, const XccyGaussianMarket_& market) {
            const auto text = Bytes(label);
            if (text.rfind("fxspot:", 0) == 0)
                return String_(market.pair_.domestic_.String()) + "/" + market.pair_.foreign_.String();
            if (text.rfind("sigma:" + Bytes(String_(market.pair_.foreign_.String()) + "/" + market.pair_.domestic_.String()) + ":", 0) == 0)
                return "year^-1/2";
            if (text.rfind("sigma:", 0) == 0)
                return "year^-3/2";
            if (text.rfind("forward:", 0) == 0)
                return "year^-1";
            return "Dimensionless";
        }
    } // namespace

    void Summarize(XccyMcResult_* result, const PreparedCalibrationInstrument_& admitted, double coupon) {
        auto& estimate = result->estimate_;
        const size_t n = estimate.parameterLabels_.size();
        Vector_<CalibrationAffineCoefficients_> coefficients;
        Vector_<Vector_<>> rows;
        for (const auto& batch : estimate.batches_) {
            coefficients.push_back(*batch.coefficients_);
            rows.push_back(Row(coefficients.back()));
        }
        const auto means = BatchStatisticsInternal::Means(rows, 2 + 2 * n);
        result->coefficientCovariance_ = BatchStatisticsInternal::Covariance(rows, means, "/execution/covariance", "finite full paired covariance");
        PriceSummary(rows, means, coupon, result);
        if (admitted.Measure().kind_ != CalibrationMeasureKind_::Value_::Price)
            ParSummary(coefficients, admitted.Measure(), &estimate);
        estimate.quoteJacobian_ = Matrix_<>(1, static_cast<int>(n));
        for (size_t j = 0; j < n; ++j)
            estimate.quoteJacobian_(0, static_cast<int>(j)) = estimate.gradient_[j];
        Check(std::isfinite(estimate.mean_) && std::isfinite(estimate.standardError_), "CalibrationNumericalFailure", "/execution/estimate",
              "finite estimate and batch standard error");
    }

    Handle_<CalibrationReport_> Report(const XccyMcResult_& result,
                                       const PreparedCalibrationInstrument_& admitted,
                                       const XccyGaussianModelData_& model,
                                       const XccyMcSettings_& settings) {
        CalibrationReportSnapshot_ snapshot;
        snapshot.summary_.domain_ = "XCCYGaussianMonteCarlo";
        snapshot.summary_.status_ = "Estimate";
        snapshot.summary_.contextIdentity_ = result.estimate_.contextIdentity_;
        snapshot.summary_.evaluations_ = static_cast<int>(result.estimate_.engineRuns_);
        if (settings.aad_) {
            snapshot.quoteJacobian_ = result.estimate_.quoteJacobian_;
            for (const auto& label : result.estimate_.parameterLabels_) {
                CalibrationReportParameter_ parameter;
                parameter.id_ = label;
                parameter.unit_ = ParameterUnit(label, *model.market_);
                snapshot.parameters_.push_back(parameter);
            }
        }
        CalibrationReportPlan_ plan;
        plan.measureId_ = admitted.Measure().id_;
        plan.ruleId_ = result.ruleId_;
        plan.backend_ = "ScriptMC";
        plan.derivativeMethod_ = settings.aad_ ? "AAD" : "None";
        plan.sourceIdentity_ = admitted.Instrument()->Definition()->SourceIdentity();
        plan.definitionIdentity_ = admitted.Instrument()->Definition()->DefinitionIdentity();
        plan.bindingIdentity_ = admitted.Instrument()->BindingIdentity();
        plan.compilerIdentity_ = admitted.CompilerIdentity();
        plan.contextIdentity_ = result.estimate_.contextIdentity_;
        plan.planIdentity_ = result.estimate_.planIdentity_;
        snapshot.plans_.push_back(plan);
        CalibrationReportObservation_ observation;
        observation.instrument_ = admitted.Instrument();
        observation.representation_ = "Script";
        observation.measureId_ = admitted.Measure().id_;
        observation.unit_ = admitted.Measure().unit_.Token();
        observation.backend_ = plan.backend_;
        observation.derivativeMethod_ = plan.derivativeMethod_;
        observation.planIdentity_ = result.estimate_.planIdentity_;
        observation.modelValue_ = result.estimate_.mean_;
        observation.standardError_ = result.estimate_.standardError_;
        snapshot.observations_.push_back(observation);
        snapshot.execution_.valuationDate_ = model.market_->valuationTime_.Date();
        snapshot.execution_.modelIdentity_ = model.Identity();
        snapshot.execution_.modelTimeBasis_ = "ACT365F";
        snapshot.execution_.numeraireConvention_ = result.mapping_.numeraire_;
        snapshot.execution_.simulation_ = CalibrationReportSimulation_{false, settings.compiled_, 0., "Precise"};
        CalibrationReportSampling_ sampling{"PseudoRandom", settings.streamVersion_, result.mapping_.identity_, "mrg32", "Precise",
                                            settings.seed_, settings.batches_,       settings.pathsPerBatch_,   {},      {}};
        for (const auto& range : result.reservation_.ranges_) {
            sampling.firstPaths_.push_back(static_cast<double>(range.firstPath_));
            sampling.pathCounts_.push_back(static_cast<double>(range.pathEnd_ - range.firstPath_));
        }
        snapshot.execution_.sampling_ = std::move(sampling);
        return Handle_<CalibrationReport_>(new CalibrationReport_(CalibrationStage_::Value_::Evaluate, {}, {}, snapshot));
    }
} // namespace Dal::XccyMcInternal
