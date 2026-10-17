//
// Created by Codex on 2026/10/8.
//

#include <dal/platform/platform.hpp>

#include <algorithm>
#include <utility>

#include <dal/math/matrix/physicalrowerrorinternal.hpp>
#include <dal/math/pde/sampledthetastep.hpp>

namespace Dal::PDE {
    SampledThetaStepPullback_& SampledThetaStepPullback_::operator=(const SampledThetaStepPullback_& source) {
        if (this != &source) {
            SampledThetaStepPullback_ captured(source);
            *this = std::move(captured);
        }
        return *this;
    }

    namespace {
        using SampledThetaDetail::Product;
        using SampledThetaDetail::Quotient;
        using SampledThetaDetail::Sum;
        constexpr const char* ASSEMBLY_RANGE = "Sampled theta-step assembly loses numerical range";
        constexpr const char* RISK_RANGE = "Sampled theta-step adjoint contraction loses numerical range";

        template <class Values_> void RequireFinite(const Values_& values, const char* message) {
            for (double value : values)
                REQUIRE(std::isfinite(value), message);
        }

        void RequireUnitInterval(double value, const char* message) { REQUIRE(std::isfinite(value) && value >= 0.0 && value <= 1.0, message); }

        void ValidateGrid(const Vector_<>& x) {
            REQUIRE(x.size() >= 3, "Sampled theta-step grid requires at least three locations");
            REQUIRE(x.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "Sampled theta-step grid exceeds supported matrix dimensions");
            RequireFinite(x, "Sampled theta-step grid locations must be finite");
            for (size_t row = 1; row < x.size(); ++row)
                REQUIRE(x[row] > x[row - 1], "Sampled theta-step grid locations must be strictly increasing");
        }

        void ValidateCoefficients(const SampledThetaStepInputs_& inputs) {
            const size_t rows = inputs.x_.size() - 2;
            REQUIRE(inputs.rates_.size() == rows && inputs.drifts_.size() == rows && inputs.variances_.size() == rows,
                    "Sampled theta-step coefficients require one entry per interior location");
            RequireFinite(inputs.rates_, "Sampled theta-step rates must be finite");
            RequireFinite(inputs.drifts_, "Sampled theta-step drifts must be finite");
            RequireFinite(inputs.variances_, "Sampled theta-step variances must be finite");
            for (double variance : inputs.variances_)
                REQUIRE(variance >= 0.0, "Sampled theta-step variances must be nonnegative");
        }

        void ValidateValues(const SampledThetaStepInputs_& inputs) {
            REQUIRE(static_cast<size_t>(inputs.oldValues_.Rows()) == inputs.x_.size() && inputs.oldValues_.Cols() > 0,
                    "Sampled theta-step old values require grid-size rows and positive layers");
            RequireFinite(inputs.oldValues_, "Sampled theta-step old values must be finite");
            if (inputs.externalBoundaries_[0] || inputs.externalBoundaries_[1] || !inputs.externalValues_.Empty()) {
                REQUIRE(inputs.externalValues_.Rows() == 2 && inputs.externalValues_.Cols() == inputs.oldValues_.Cols(),
                        "Sampled theta-step external values require two rows and the old-value layer count");
            }
            RequireFinite(inputs.externalValues_, "Sampled theta-step external values must be finite");
        }

        void ValidatePolicy(const SampledThetaStepInputs_& inputs, const LinearSolveAccuracyPolicy_& policy, double tolerance) {
            REQUIRE(std::isfinite(inputs.dt_) && inputs.dt_ > 0.0, "Sampled theta-step dt must be finite and positive");
            RequireUnitInterval(inputs.theta_, "Sampled theta-step theta must be finite and in [0,1]");
            RequireUnitInterval(policy.forwardBackwardErrorLimit_, "Sampled theta-step forward backward-error limit must be finite and in [0,1]");
            RequireUnitInterval(policy.transposeBackwardErrorLimit_, "Sampled theta-step transpose backward-error limit must be finite and in [0,1]");
            REQUIRE(std::isfinite(tolerance) && tolerance > 0.0 && tolerance < 1.0,
                    "Sampled theta-step relative pivot tolerance must be finite and strictly between zero and one");
        }

        void RequireErrors(const Vector_<>& errors, double limit, const char* message) {
            for (double error : errors)
                REQUIRE(error <= limit, message);
        }
    } // namespace

    SampledThetaStepPullback_::SampledThetaStepPullback_(const SampledThetaStepInputs_& inputs,
                                                         const LinearSolveAccuracyPolicy_& policy,
                                                         double relativePivotTolerance)
        : policy_(policy), dt_(inputs.dt_), theta_(inputs.theta_), implicitMultiplier_(0.0), explicitMultiplier_(0.0),
          externalBoundaries_(inputs.externalBoundaries_) {
        ValidateGrid(inputs.x_);
        ValidateCoefficients(inputs);
        ValidateValues(inputs);
        ValidatePolicy(inputs, policy, relativePivotTolerance);
        implicitMultiplier_ = Product(dt_, theta_, ASSEMBLY_RANGE);
        explicitMultiplier_ = Product(dt_, 1.0 - theta_, ASSEMBLY_RANGE);
        oldValues_ = inputs.oldValues_;
        BuildGenerator(inputs);
        auto rhs = BuildRhs(inputs);
        if (theta_ == 0.0) {
            solution_ = std::move(rhs);
            forwardBackwardErrors_ = Vector_<>(oldValues_.Cols(), 0.0);
        } else {
            BuildFactors(relativePivotTolerance);
            solution_ = factors_.Solve(rhs, false);
            forwardBackwardErrors_ = BackwardErrors(false, rhs, solution_);
        }
        RequireErrors(forwardBackwardErrors_, policy_.forwardBackwardErrorLimit_, "Sampled theta-step forward backward error exceeds its limit");
    }

    void SampledThetaStepPullback_::BuildGenerator(const SampledThetaStepInputs_& inputs) {
        const int rows = inputs.x_.size() - 2;
        dx_ = Matrix_<>(rows, 3);
        dxx_ = Matrix_<>(rows, 3);
        generator_ = Matrix_<>(rows, 3);
        for (int row = 0; row < rows; ++row) {
            const double left = Sum(inputs.x_[row + 1], -inputs.x_[row], ASSEMBLY_RANGE);
            const double right = Sum(inputs.x_[row + 2], -inputs.x_[row + 1], ASSEMBLY_RANGE);
            const double width = Sum(left, right, ASSEMBLY_RANGE);
            const double leftWidth = Product(left, width, ASSEMBLY_RANGE);
            const double rightWidth = Product(right, width, ASSEMBLY_RANGE);
            const double leftRight = Product(left, right, ASSEMBLY_RANGE);
            dx_(row, 0) = Quotient(-right, leftWidth, ASSEMBLY_RANGE);
            dx_(row, 1) = Quotient(Sum(right, -left, ASSEMBLY_RANGE), leftRight, ASSEMBLY_RANGE);
            dx_(row, 2) = Quotient(left, rightWidth, ASSEMBLY_RANGE);
            dxx_(row, 0) = Quotient(2.0, leftWidth, ASSEMBLY_RANGE);
            dxx_(row, 1) = Quotient(-2.0, leftRight, ASSEMBLY_RANGE);
            dxx_(row, 2) = Quotient(2.0, rightWidth, ASSEMBLY_RANGE);
            const double halfVariance = Product(0.5, inputs.variances_[row], ASSEMBLY_RANGE);
            for (int local = 0; local < 3; ++local) {
                generator_(row, local) = Sum(Product(inputs.drifts_[row], dx_(row, local), ASSEMBLY_RANGE),
                                             Product(halfVariance, dxx_(row, local), ASSEMBLY_RANGE), ASSEMBLY_RANGE);
            }
            generator_(row, 1) = Sum(generator_(row, 1), -inputs.rates_[row], ASSEMBLY_RANGE);
        }
    }

    double SampledThetaStepPullback_::ImplicitEntry(int row, int column) const {
        const double identity = row == column ? 1.0 : 0.0;
        if (row == 0 || row == oldValues_.Rows() - 1)
            return identity;
        return Sum(identity, -Product(implicitMultiplier_, generator_(row - 1, column - row + 1), ASSEMBLY_RANGE), ASSEMBLY_RANGE);
    }

    double SampledThetaStepPullback_::ExplicitEntry(int row, int local) const {
        return Sum(local == 1 ? 1.0 : 0.0, Product(explicitMultiplier_, generator_(row - 1, local), ASSEMBLY_RANGE), ASSEMBLY_RANGE);
    }

    void SampledThetaStepPullback_::BuildFactors(double tolerance) {
        const int n = oldValues_.Rows();
        Vector_<> lower(n - 1), diagonal(n), upper(n - 1);
        for (int row = 0; row < n; ++row)
            diagonal[row] = ImplicitEntry(row, row);
        for (int row = 0; row < n - 1; ++row) {
            lower[row] = ImplicitEntry(row + 1, row);
            upper[row] = ImplicitEntry(row, row + 1);
        }
        factors_ = SampledThetaDetail::TridiagonalFactors_(std::move(lower), std::move(diagonal), std::move(upper), tolerance);
    }

    Matrix_<> SampledThetaStepPullback_::BuildRhs(const SampledThetaStepInputs_& inputs) const {
        const int n = oldValues_.Rows();
        Matrix_<> rhs(n, oldValues_.Cols());
        for (int layer = 0; layer < rhs.Cols(); ++layer) {
            for (int side = 0; side < 2; ++side) {
                const int row = side == 0 ? 0 : n - 1;
                rhs(row, layer) = externalBoundaries_[side] ? inputs.externalValues_(side, layer) : oldValues_(row, layer);
            }
            for (int row = 1; row < n - 1; ++row)
                for (int local = 0; local < 3; ++local)
                    rhs(row, layer) =
                        Sum(rhs(row, layer), Product(ExplicitEntry(row, local), oldValues_(row + local - 1, layer), ASSEMBLY_RANGE), ASSEMBLY_RANGE);
        }
        return rhs;
    }

    Vector_<> SampledThetaStepPullback_::BackwardErrors(bool transpose, const Matrix_<>& rhs, const Matrix_<>& solution) const {
        const int n = oldValues_.Rows();
        Vector_<> errors(rhs.Cols(), 0.0);
        for (int layer = 0; layer < rhs.Cols(); ++layer)
            for (int row = 0; row < n; ++row) {
                const int begin = std::max(0, row - 1);
                const int end = std::min(n, row + 2);
                const double error = LinearSolveDetail::PhysicalRowBackwardError(rhs(row, layer), end - begin, [&](int entry) {
                    const int column = begin + entry;
                    const double coefficient = transpose ? ImplicitEntry(column, row) : ImplicitEntry(row, column);
                    return LinearSolveDetail::ScaledProduct_(coefficient, solution(column, layer));
                });
                errors[layer] = std::max(errors[layer], error);
            }
        return errors;
    }

    void SampledThetaStepPullback_::AccumulateInterior(int row, int layer, double lambda, SampledThetaStepAdjoints_* risk) const {
        const int coefficientRow = row - 1;
        for (int local = 0; local < 3; ++local) {
            const int column = row + local - 1;
            const double old = oldValues_(column, layer);
            const double next = solution_(column, layer);
            const double mixture = Sum(Product(theta_, next, RISK_RANGE), Product(1.0 - theta_, old, RISK_RANGE), RISK_RANGE);
            const double pointContribution = Product(lambda, mixture, RISK_RANGE);
            const double gradient = Product(dt_, pointContribution, RISK_RANGE);
            risk->oldValues_(column, layer) =
                Sum(risk->oldValues_(column, layer), Product(lambda, ExplicitEntry(row, local), RISK_RANGE), RISK_RANGE);
            risk->drifts_[coefficientRow] = Sum(risk->drifts_[coefficientRow], Product(gradient, dx_(coefficientRow, local), RISK_RANGE), RISK_RANGE);
            risk->variances_[coefficientRow] = Sum(risk->variances_[coefficientRow],
                                                   Product(gradient, Product(0.5, dxx_(coefficientRow, local), RISK_RANGE), RISK_RANGE), RISK_RANGE);
            if (local == 1)
                risk->rates_[coefficientRow] = Sum(risk->rates_[coefficientRow], -gradient, RISK_RANGE);
            const double generator = generator_(coefficientRow, local);
            risk->dt_ = Sum(risk->dt_, Product(pointContribution, generator, RISK_RANGE), RISK_RANGE);
            const double thetaContribution =
                Product(dt_, Product(lambda, Product(generator, Sum(next, -old, RISK_RANGE), RISK_RANGE), RISK_RANGE), RISK_RANGE);
            risk->theta_ = Sum(risk->theta_, thetaContribution, RISK_RANGE);
        }
    }

    void SampledThetaStepPullback_::AccumulateAdjoints(const Matrix_<>& lambda, SampledThetaStepAdjoints_* risk) const {
        for (int layer = 0; layer < lambda.Cols(); ++layer) {
            for (int row = 1; row < lambda.Rows() - 1; ++row)
                if (lambda(row, layer) != 0.0)
                    AccumulateInterior(row, layer, lambda(row, layer), risk);
            for (int side = 0; side < 2; ++side) {
                const int row = side == 0 ? 0 : lambda.Rows() - 1;
                if (externalBoundaries_[side])
                    risk->externalValues_(side, layer) = lambda(row, layer);
                else
                    risk->oldValues_(row, layer) = Sum(risk->oldValues_(row, layer), lambda(row, layer), RISK_RANGE);
            }
        }
    }

    SampledThetaStepAdjoints_ SampledThetaStepPullback_::Reverse(const Matrix_<>& solutionSeeds) const {
        REQUIRE(solutionSeeds.Rows() == oldValues_.Rows() && solutionSeeds.Cols() == oldValues_.Cols(),
                "Sampled theta-step seed shape must match the solution");
        RequireFinite(solutionSeeds, "Sampled theta-step seed entries must be finite");
        const auto lambda = theta_ == 0.0 ? solutionSeeds : factors_.Solve(solutionSeeds, true);
        auto errors = BackwardErrors(true, solutionSeeds, lambda);
        RequireErrors(errors, policy_.transposeBackwardErrorLimit_, "Sampled theta-step transpose backward error exceeds its limit");
        const int n = oldValues_.Rows(), layers = oldValues_.Cols();
        SampledThetaStepAdjoints_ risk{
            Matrix_<>(n, layers), Matrix_<>(2, layers), Vector_<>(n - 2, 0.0), Vector_<>(n - 2, 0.0), Vector_<>(n - 2, 0.0), 0.0, 0.0,
            std::move(errors)};
        AccumulateAdjoints(lambda, &risk);
        return risk;
    }
} // namespace Dal::PDE
