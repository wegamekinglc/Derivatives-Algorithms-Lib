#include <dal/platform/platform.hpp>

#include <dal/calibration/validation_internal.hpp>
#include <dal/script/batchstatistics_internal.hpp>
#include <dal/script/xccymcbasket_internal.hpp>

namespace Dal::XccyMcBasketInternal {
    namespace {
        using namespace CalibrationInternal;
        void Finite(const Vector_<>& row) {
            for (const double value : row)
                Check(std::isfinite(value), "CalibrationNumericalFailure", "/execution/coefficients", "finite complete coefficient row");
        }
        void CoefficientAxes(const XccyMcBasketExecutionHeader_& h,
                             const XccyMcBasketMemberAllocation_& m,
                             const String_& currency,
                             Vector_<XccyMcBasketAxis_>* axes) {
            const String_ bUnit = currency, aUnit = currency + "/decimal-spread";
            axes->push_back({m.memberId_, "B", bUnit});
            axes->push_back({m.memberId_, "A", aUnit});
            for (size_t j = 0; j < h.settings_.parameterLabels_.size(); ++j) {
                const auto& label = h.settings_.parameterLabels_[j];
                const auto& unit = h.parameterUnits_[j];
                axes->push_back({m.memberId_, "dB/" + label, bUnit + "/(" + unit + ")"});
                axes->push_back({m.memberId_, "dA/" + label, aUnit + "/(" + unit + ")"});
            }
        }
        void PriceJacobian(double coupon, int row, int col, size_t q, Matrix_<>* jacobian) {
            (*jacobian)(row, col) = 1.;
            (*jacobian)(row, col + 1) = coupon;
            for (size_t j = 0; j < q; ++j) {
                (*jacobian)(row + 1 + static_cast<int>(j), col + 2 + 2 * static_cast<int>(j)) = 1.;
                (*jacobian)(row + 1 + static_cast<int>(j), col + 3 + 2 * static_cast<int>(j)) = coupon;
            }
        }
        void ParJacobian(const CalibrationAffineCoefficients_& c, int row, int col, size_t q, Matrix_<>* jacobian) {
            const double a = c.a_, b = c.b_;
            (*jacobian)(row, col) = -1. / a;
            (*jacobian)(row, col + 1) = b / (a * a);
            for (size_t j = 0; j < q; ++j) {
                const int r = row + 1 + static_cast<int>(j), k = col + 2 + 2 * static_cast<int>(j);
                (*jacobian)(r, col) = c.da_[j] / (a * a);
                (*jacobian)(r, col + 1) = c.db_[j] / (a * a) - 2. * b * c.da_[j] / (a * a * a);
                (*jacobian)(r, k) = -1. / a;
                (*jacobian)(r, k + 1) = b / (a * a);
            }
        }
        void OutputAxes(const XccyMcBasketExecutionHeader_& h,
                        const XccyMcInternal::Member_& member,
                        const String_& id,
                        Vector_<XccyMcBasketAxis_>* axes) {
            const auto& measure = member.admitted_.Measure();
            axes->push_back({id, measure.id_, measure.unit_.Token()});
            for (size_t j = 0; j < h.settings_.parameterLabels_.size(); ++j)
                axes->push_back(
                    {id, "d" + measure.id_ + "/" + h.settings_.parameterLabels_[j], measure.unit_.Token() + "/(" + h.parameterUnits_[j] + ")"});
        }
        Matrix_<> Influences(const XccyMcBasketSnapshot_& result) {
            const auto& j = result.outputJacobian_;
            Matrix_<> influences(static_cast<int>(result.coefficientRows_.size()), j.Rows(), 0.);
            for (int b = 0; b < influences.Rows(); ++b)
                for (int i = 0; i < j.Rows(); ++i) {
                    double value = 0.;
                    for (int k = 0; k < j.Cols(); ++k)
                        value += j(i, k) * (result.coefficientRows_[b][k] - result.coefficientMeans_[k]);
                    influences(b, i) = value;
                    Check(std::isfinite(influences(b, i)), "CalibrationNumericalFailure", "/execution/covariance", "finite full Jacobian influence");
                }
            return influences;
        }
        Matrix_<> OutputCovariance(const XccyMcBasketSnapshot_& result) {
            const auto influences = Influences(result);
            Matrix_<> covariance(influences.Cols(), influences.Cols(), 0.);
            for (int i = 0; i < covariance.Rows(); ++i)
                for (int j = 0; j < covariance.Cols(); ++j) {
                    double value = 0.;
                    for (int b = 0; b < influences.Rows(); ++b)
                        value += influences(b, i) * influences(b, j);
                    covariance(i, j) = value / (static_cast<double>(influences.Rows()) * (influences.Rows() - 1));
                    Check(std::isfinite(covariance(i, j)), "CalibrationNumericalFailure", "/execution/covariance", "finite output covariance");
                }
            return covariance;
        }
        void JointRows(XccyMcBasketSnapshot_* result) {
            const size_t batches = static_cast<size_t>(result->allocation_.batches_);
            result->coefficientRows_ = Vector_<Vector_<>>(batches);
            for (size_t i = 0; i < result->members_.size(); ++i) {
                const auto& member = result->members_[i];
                Check(member.completedEstimate_ && member.batches_.size() == batches, "CalibrationContextMismatch", "/sampling/ranges",
                      "complete equal batch counts in original member order");
                CoefficientAxes(result->header_, result->allocation_.members_[i], result->model_->market_->pair_.domestic_.String(),
                                &result->coefficientAxes_);
                for (size_t b = 0; b < batches; ++b) {
                    Check(member.batches_[b].ordinal_ == b, "CalibrationContextMismatch", "/sampling/ranges", "same original batch ordinal");
                    for (const auto value : member.batches_[b].row_)
                        result->coefficientRows_[b].push_back(value);
                }
            }
        }
    } // namespace
    Vector_<> Row(const CalibrationAffineCoefficients_& c) {
        Check(c.db_.size() == c.da_.size(), "CalibrationContextMismatch", "/derivatives", "complete matching coefficient axes");
        Vector_<> row{c.b_, c.a_};
        for (size_t j = 0; j < c.db_.size(); ++j) {
            row.push_back(c.db_[j]);
            row.push_back(c.da_[j]);
        }
        Finite(row);
        return row;
    }
    void JointStatistics(const Vector_<XccyMcInternal::Member_>& members, XccyMcBasketSnapshot_* result) {
        JointRows(result);
        result->coefficientMeans_ = BatchStatisticsInternal::Means(result->coefficientRows_, result->coefficientAxes_.size());
        Finite(result->coefficientMeans_);
        result->coefficientCovariance_ = BatchStatisticsInternal::Covariance(result->coefficientRows_, result->coefficientMeans_,
                                                                             "/execution/covariance", "finite full covariance");
        result->meanCovariance_ = result->coefficientCovariance_;
        for (int i = 0; i < result->meanCovariance_.Rows(); ++i)
            for (int j = 0; j < result->meanCovariance_.Cols(); ++j)
                result->meanCovariance_(i, j) /= result->coefficientRows_.size();
        const size_t q = result->header_.settings_.parameterLabels_.size();
        result->outputJacobian_ = Matrix_<>(static_cast<int>(members.size() * (q + 1)), static_cast<int>(result->coefficientAxes_.size()), 0.);
        for (size_t i = 0; i < members.size(); ++i) {
            const int r = static_cast<int>(i * (q + 1)), col = static_cast<int>(i * (2 + 2 * q));
            OutputAxes(result->header_, members[i], result->allocation_.members_[i].memberId_, &result->outputAxes_);
            if (members[i].admitted_.Measure().kind_ == CalibrationMeasureKind_::Value_::Price)
                PriceJacobian(members[i].coupon_, r, col, q, &result->outputJacobian_);
            else
                ParJacobian(result->members_[i].completedEstimate_->coefficients_, r, col, q, &result->outputJacobian_);
        }
        result->outputCovariance_ = OutputCovariance(*result);
    }
} // namespace Dal::XccyMcBasketInternal
