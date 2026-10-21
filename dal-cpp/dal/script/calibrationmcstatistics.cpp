#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/calibrationmc_internal.hpp>

namespace Dal::CalibrationMcInternal {
    namespace {
        using namespace CalibrationInternal;

        Vector_<> CoefficientVector(const CalibrationAffineCoefficients_& c, size_t n, const CalibrationDiagnostic_& d) {
            Check(c.da_.size() == n && c.db_.size() == n, "InvalidCalibrationPlan", "/mc/coefficients", "matching native partial axes", "", d);
            Vector_<> x{c.b_, c.a_};
            for (size_t j = 0; j < n; ++j) {
                x.push_back(c.db_[j]);
                x.push_back(c.da_[j]);
            }
            for (double value : x)
                Check(std::isfinite(value), "CalibrationNumericalFailure", "/mc/coefficients", "finite paired batch coefficients", "", d);
            return x;
        }
        Vector_<> Means(const Vector_<Vector_<>>& rows) {
            Vector_<> result(rows[0].size(), 0.);
            for (const auto& row : rows)
                for (size_t j = 0; j < row.size(); ++j)
                    result[j] += row[j] / rows.size();
            return result;
        }
        Matrix_<> Covariance(const Vector_<Vector_<>>& rows, const Vector_<>& means, const CalibrationDiagnostic_& d) {
            const int n = static_cast<int>(means.size());
            Matrix_<> covariance(n, n, 0.);
            for (const auto& row : rows)
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        covariance(i, j) += (row[i] - means[i]) * (row[j] - means[j]) / (rows.size() - 1);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Check(std::isfinite(covariance(i, j)), "CalibrationNumericalFailure", "/mc/covariance", "finite batch covariance", "", d);
            return covariance;
        }
        double ProjectedError(const Vector_<Vector_<>>& rows, const Vector_<>& means, const Vector_<>& weights) {
            double variance = 0.;
            for (const auto& row : rows) {
                double influence = 0.;
                for (size_t j = 0; j < weights.size(); ++j)
                    influence += weights[j] * (row[j] - means[j]);
                variance += influence * influence / (static_cast<double>(rows.size()) * (rows.size() - 1));
            }
            return std::sqrt(variance);
        }
        void AffineInputs(size_t batches, int sign, double minimum, double sigma, const CalibrationDiagnostic_& d) {
            Check(batches >= 2 && (sign == 1 || sign == -1), "InvalidCalibrationPlan", "/mc/affine", "paired batches and declared direction", "", d);
            Check(std::isfinite(minimum) && minimum > 0. && std::isfinite(sigma) && sigma > 0., "InvalidCalibrationPlan", "/mc/affine",
                  "positive annuity threshold and sigma", "", d);
        }
        void QuotientGradients(const Vector_<Vector_<>>& rows,
                               const Vector_<>& means,
                               CalibrationMcAffineEstimate_* result,
                               const CalibrationDiagnostic_& d) {
            const double b = means[0], a = means[1];
            for (size_t j = 0; j < result->value_.derivatives_.size(); ++j) {
                const double db = means[2 + 2 * j], da = means[3 + 2 * j];
                result->meanCoefficients_.db_[j] = db;
                result->meanCoefficients_.da_[j] = da;
                result->value_.derivatives_[j] = -db / a + b * da / (a * a);
                Vector_<> weights(means.size(), 0.);
                weights[0] = da / (a * a);
                weights[1] = db / (a * a) - 2. * b * da / (a * a * a);
                weights[2 + 2 * j] = -1. / a;
                weights[3 + 2 * j] = b / (a * a);
                result->gradientStandardError_.push_back(ProjectedError(rows, means, weights));
                Check(std::isfinite(result->value_.derivatives_[j]) && std::isfinite(result->gradientStandardError_[j]),
                      "CalibrationNumericalFailure", "/mc/affine_gradient", "finite quotient gradient and uncertainty", "", d);
            }
        }
    } // namespace

    CalibrationMcAffineEstimate_
    AffineEstimate(const Vector_<CalibrationAffineCoefficients_>& batches, int sign, double minimum, double sigma, const CalibrationDiagnostic_& d) {
        AffineInputs(batches.size(), sign, minimum, sigma, d);
        const size_t n = batches[0].da_.size();
        Vector_<Vector_<>> rows;
        for (const auto& c : batches)
            rows.push_back(CoefficientVector(c, n, d));
        const auto means = Means(rows);
        CalibrationMcAffineEstimate_ result;
        result.annuitySign_ = sign;
        result.minAbsAnnuity_ = minimum;
        result.coefficientCovariance_ = Covariance(rows, means, d);
        const double b = means[0], a = means[1];
        Vector_<> weights(means.size(), 0.);
        weights[1] = 1.;
        result.annuityStandardError_ = ProjectedError(rows, means, weights);
        Check(sign * a > minimum && std::abs(a) - sigma * result.annuityStandardError_ > minimum, "InvalidCalibrationAnnuity", "/mc/annuity",
              "correct direction and denominator confidence above threshold", NumberText(a), d);
        result.meanCoefficients_ = {a, b, Vector_<>(n), Vector_<>(n)};
        result.value_ = {-b / a, Vector_<>(n)};
        weights[0] = -1. / a;
        weights[1] = b / (a * a);
        result.standardError_ = ProjectedError(rows, means, weights);
        QuotientGradients(rows, means, &result, d);
        Check(std::isfinite(result.value_.value_) && std::isfinite(result.standardError_), "CalibrationNumericalFailure", "/mc/affine",
              "finite quotient and uncertainty", "", d);
        return result;
    }
} // namespace Dal::CalibrationMcInternal

namespace Dal {
    namespace {
        using namespace CalibrationInternal;

        CalibrationDiagnostic_ ValidationDiagnostic(const CalibrationMcResult_& result) {
            CalibrationDiagnostic_ d;
            d.stage_ = CalibrationStage_::Value_::Validate;
            d.measureId_ = result.measureId_;
            return d;
        }
        void Nonnegative(double value, const String_& field, const CalibrationDiagnostic_& d) {
            Check(std::isfinite(value) && value >= 0., "InvalidCalibrationPlan", field, "finite nonnegative indicator", "", d);
        }
        void Positive(double value, const String_& field, const CalibrationDiagnostic_& d) {
            Check(std::isfinite(value) && value > 0., "InvalidCalibrationPlan", field, "finite positive setting", "", d);
        }
        void AnnuityConfidence(const CalibrationMcResult_& value, double sigma, const String_& field, const CalibrationDiagnostic_& d) {
            if (!value.affine_)
                return;
            Positive(sigma, "/precision/sigma", d);
            const auto& estimate = *value.affine_;
            const double a = estimate.meanCoefficients_.a_;
            Check(estimate.annuitySign_ * a > estimate.minAbsAnnuity_ &&
                      std::abs(a) - sigma * estimate.annuityStandardError_ > estimate.minAbsAnnuity_,
                  "InvalidCalibrationAnnuity", field, "denominator confidence at requested validation sigma", "", d);
        }
        void PrecisionSettings(const CalibrationMcPrecision_& p, size_t n, const CalibrationDiagnostic_& d) {
            Positive(p.errorScale_, "/precision/error_scale", d);
            Positive(p.sigma_, "/precision/sigma", d);
            Check(p.numericalErrorFraction_ > 0. && p.numericalErrorFraction_ < 1. && p.gradientErrorFraction_ > 0. && p.gradientErrorFraction_ < 1.,
                  "InvalidCalibrationPlan", "/precision/fractions", "fractions strictly between zero and one", "", d);
            Nonnegative(p.epsilonValue_, "/precision/epsilon_value", d);
            Check(p.epsilonValue_ < p.numericalErrorFraction_ * p.errorScale_, "InvalidCalibrationPlan", "/precision/epsilon_value",
                  "numerical floor below frozen error budget", "", d);
            Check(p.parameterScales_.size() == n && p.epsilonGradient_.size() == n, "InvalidCalibrationPlan", "/precision/gradient",
                  "one frozen parameter scale and floor per native gradient", "", d);
            for (size_t j = 0; j < n; ++j) {
                Positive(p.parameterScales_[j], "/precision/parameter_scale", d);
                Nonnegative(p.epsilonGradient_[j], "/precision/epsilon_gradient", d);
                const double budget = p.gradientErrorFraction_ * p.errorScale_ / p.parameterScales_[j];
                Check(std::isfinite(budget) && p.epsilonGradient_[j] < budget, "InvalidCalibrationPlan", "/precision/epsilon_gradient",
                      "gradient floor below frozen native-unit budget", "", d);
            }
        }
        void CheckGradientEstimate(const CalibrationMcResult_& value, const CalibrationMcReference_& reference, const CalibrationDiagnostic_& d) {
            const size_t n = value.parameterLabels_.size();
            Check(value.gradient_.size() == n && value.gradientStandardError_.size() == n && reference.gradient_.size() == n &&
                      reference.gradientStandardError_.size() == n && reference.finiteDifferenceError_.size() == n,
                  "InvalidCalibrationPlan", "/validation/gradient", "matching native gradient and independent uncertainty axes", "", d);
            for (size_t j = 0; j < n; ++j) {
                Check(std::isfinite(value.gradient_[j]) && std::isfinite(reference.gradient_[j]), "CalibrationNumericalFailure",
                      "/validation/gradient", "finite native gradients", "", d);
                Nonnegative(value.gradientStandardError_[j], "/validation/gradient_standard_error", d);
                Nonnegative(reference.gradientStandardError_[j], "/reference/gradient_standard_error", d);
                Nonnegative(reference.finiteDifferenceError_[j], "/reference/finite_difference_error", d);
            }
        }
        void CheckEstimate(const CalibrationMcResult_& value, const CalibrationMcReference_& reference, const CalibrationDiagnostic_& d) {
            Check(value.validation_ && value.batches_.size() >= 2, "InvalidCalibrationPlan", "/validation", "independent validation batch result", "",
                  d);
            Check(std::isfinite(value.mean_) && std::isfinite(reference.value_), "CalibrationNumericalFailure", "/validation/mean", "finite means",
                  "", d);
            Nonnegative(value.standardError_, "/validation/standard_error", d);
            Nonnegative(reference.standardError_, "/reference/standard_error", d);
            Nonnegative(reference.numericalError_, "/reference/numerical_error", d);
            CheckGradientEstimate(value, reference, d);
        }
        void GradientValidation(const CalibrationMcResult_& value,
                                const CalibrationMcReference_& r,
                                const CalibrationMcPrecision_& p,
                                CalibrationMcValidation_* result) {
            result->gradientValidationRequested_ = !value.gradient_.empty();
            result->gradientParityPassed_ = true;
            result->gradientPrecisionPassed_ = true;
            for (size_t j = 0; j < value.gradient_.size(); ++j) {
                const double se = std::hypot(value.gradientStandardError_[j], r.gradientStandardError_[j]);
                const double indicator = p.sigma_ * se + r.finiteDifferenceError_[j];
                result->gradientDeltaStandardError_.push_back(se);
                result->gradientPrecisionIndicators_.push_back(indicator);
                result->gradientParityPassed_ &= std::abs(value.gradient_[j] - r.gradient_[j]) <= indicator + p.epsilonGradient_[j];
                result->gradientPrecisionPassed_ &= indicator <= p.gradientErrorFraction_ * p.errorScale_ / p.parameterScales_[j];
            }
        }
        void SameAxes(const CalibrationMcResult_& fit, const CalibrationMcResult_& validation, const CalibrationDiagnostic_& d) {
            Check(Bytes(fit.instrumentIdentity_) == Bytes(validation.instrumentIdentity_) &&
                      Bytes(fit.contextIdentity_) == Bytes(validation.contextIdentity_) && Bytes(fit.measureId_) == Bytes(validation.measureId_) &&
                      Bytes(fit.mappingIdentity_) == Bytes(validation.mappingIdentity_),
                  "CalibrationContextMismatch", "/validation/identity", "same instrument, measure, context and per-observation mapping", "", d);
            Check(fit.parameterLabels_.size() == validation.parameterLabels_.size(), "InvalidCalibrationPlan", "/validation/parameter_labels",
                  "same native axes", "", d);
            for (size_t j = 0; j < fit.parameterLabels_.size(); ++j)
                Check(Bytes(fit.parameterLabels_[j]) == Bytes(validation.parameterLabels_[j]), "InvalidCalibrationPlan",
                      "/validation/parameter_labels", "same exact native axes", "", d);
        }
        void SeparateStreams(const CalibrationMcResult_& fit, const CalibrationMcResult_& validation, const CalibrationDiagnostic_& d) {
            Check(!fit.validation_ && fit.batches_.size() >= 2, "InvalidCalibrationPlan", "/fit", "fixed fit batch result", "", d);
            SameAxes(fit, validation, d);
            for (const auto& a : fit.ranges_)
                for (const auto& b : validation.ranges_)
                    Check(a.firstPath_ + a.pathCount_ <= b.firstPath_ || b.firstPath_ + b.pathCount_ <= a.firstPath_, "InvalidCalibrationPlan",
                          "/validation/ranges", "nonoverlapping fit and validation normal-path ranges", "", d);
        }
    } // namespace

    CalibrationMcValidation_
    ValidateCalibrationMc(const CalibrationMcResult_& value, const CalibrationMcReference_& reference, const CalibrationMcPrecision_& precision) {
        const auto d = ValidationDiagnostic(value);
        PrecisionSettings(precision, value.parameterLabels_.size(), d);
        CheckEstimate(value, reference, d);
        AnnuityConfidence(value, precision.sigma_, "/validation/annuity", d);
        CalibrationMcValidation_ result;
        result.deltaStandardError_ = std::hypot(value.standardError_, reference.standardError_);
        result.precisionIndicator_ = precision.sigma_ * result.deltaStandardError_ + reference.numericalError_;
        const double delta = std::abs(value.mean_ - reference.value_);
        result.errorIndicator_ = delta + result.precisionIndicator_;
        const double budget = precision.numericalErrorFraction_ * precision.errorScale_;
        result.parityPassed_ = delta <= result.precisionIndicator_ + precision.epsilonValue_;
        result.precisionPassed_ = result.precisionIndicator_ <= budget;
        result.valueErrorPassed_ = result.errorIndicator_ <= budget;
        GradientValidation(value, reference, precision, &result);
        result.numericalValidationPassed_ = result.parityPassed_ && result.precisionPassed_ && result.valueErrorPassed_ &&
                                            result.gradientParityPassed_ && result.gradientPrecisionPassed_;
        result.budgetExhausted_ = !result.numericalValidationPassed_;
        result.terminationReason_ = result.numericalValidationPassed_ ? "NumericalValidationPassed" : "NumericalValidationFailed";
        Vector_<CalibrationDiagnostic_> errors;
        if (!result.numericalValidationPassed_) {
            auto failure = d;
            failure.code_ = "NumericalValidationFailed";
            failure.message_ = "Frozen Monte Carlo error/precision budget not met";
            failure.field_ = "/validation";
            errors.push_back(std::move(failure));
        }
        result.report_.reset(new CalibrationReport_(CalibrationStage_::Value_::Validate, {}, std::move(errors)));
        return result;
    }
    CalibrationMcValidation_
    ValidateCalibrationMc(const CalibrationMcResult_& fit, const CalibrationMcResult_& validation, const CalibrationMcPrecision_& precision) {
        SeparateStreams(fit, validation, ValidationDiagnostic(validation));
        AnnuityConfidence(fit, precision.sigma_, "/fit/annuity", ValidationDiagnostic(validation));
        return ValidateCalibrationMc(
            validation, {fit.mean_, fit.standardError_, 0., fit.gradient_, fit.gradientStandardError_, Vector_<>(fit.gradient_.size(), 0.)},
            precision);
    }
} // namespace Dal
