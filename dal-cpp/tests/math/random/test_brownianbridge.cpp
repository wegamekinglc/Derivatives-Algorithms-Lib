//
// Created by wegam on 2022/10/30.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <dal/math/random/brownianbridge.hpp>
#include <dal/math/random/sobol.hpp>

using namespace Dal;

namespace {
    class FixedBridgeRandom_ : public Random_ {
        Vector_<> values_;

    public:
        explicit FixedBridgeRandom_(Vector_<> values) : values_(std::move(values)) {}
        void FillUniform(Vector_<>* deviates) override { *deviates = values_; }
        void FillNormal(Vector_<>* deviates) override { *deviates = values_; }
        void SkipTo(size_t) override {}
        [[nodiscard]] std::unique_ptr<Random_> Clone() const override { return std::make_unique<FixedBridgeRandom_>(*this); }
        [[nodiscard]] size_t NDim() const override { return values_.size(); }
    };
} // namespace

TEST(RandomTest, TestBrownBridgeFillNormal) {
    int ndim = 10;
    auto bw = std::make_unique<BrownianBridge_>(std::unique_ptr<Random_>(NewSobol(ndim, 1024)));
    int n_paths = pow(2, 24);
    Vector_<> deviates;

    Vector_<> means(ndim, 0.);
    Vector_<> vars(ndim, 0.);

    for (int i = 0; i < n_paths; ++i) {
        bw->FillNormal(&deviates);
        for (int j = 0; j < ndim; ++j) {
            means[j] += deviates[j];
            vars[j] += Square(deviates[j]);
        }
    }

    for (auto k = 0; k < ndim; ++k) {
        means[k] /= n_paths;
        vars[k] /= n_paths;
        ASSERT_NEAR(means[k], 0.0, 1e-3);
        ASSERT_NEAR(vars[k], 1.0, 1e-3);
    }
}

TEST(RandomTest, TestFactorBridgeUsesFactorMajorInputsAndTimeMajorOutputs) {
    BrownianBridge_ first(std::make_unique<FixedBridgeRandom_>(Vector_<>{0.4, -0.2, 0.7}));
    BrownianBridge_ second(std::make_unique<FixedBridgeRandom_>(Vector_<>{-0.3, 0.9, -0.5}));
    FactorBrownianBridge_ combined(std::make_unique<FixedBridgeRandom_>(Vector_<>{0.4, -0.2, 0.7, -0.3, 0.9, -0.5}), 2);
    Vector_<> firstOutput;
    Vector_<> secondOutput;
    Vector_<> combinedOutput;
    first.FillNormal(&firstOutput);
    second.FillNormal(&secondOutput);
    combined.FillNormal(&combinedOutput);
    ASSERT_EQ(combinedOutput.size(), 6);
    for (size_t step = 0; step < 3; ++step) {
        ASSERT_DOUBLE_EQ(combinedOutput[step * 2], firstOutput[step]);
        ASSERT_DOUBLE_EQ(combinedOutput[step * 2 + 1], secondOutput[step]);
    }
    auto copy = combined.Clone();
    Vector_<> copyOutput;
    copy->FillNormal(&copyOutput);
    ASSERT_EQ(copyOutput, combinedOutput);
    FactorBrownianBridge_ oneFactor(std::make_unique<FixedBridgeRandom_>(Vector_<>{0.4, -0.2, 0.7}), 1);
    oneFactor.FillNormal(&copyOutput);
    ASSERT_EQ(copyOutput, firstOutput);
    ASSERT_THROW((FactorBrownianBridge_(std::make_unique<FixedBridgeRandom_>(Vector_<>{0.1, 0.2, 0.3}), 2)), Exception_);
}

TEST(RandomTest, TestFactorBridgeSobolSkipMatchesSequentialPaths) {
    FactorBrownianBridge_ sequential(std::unique_ptr<Random_>(NewSobol(6, 0)), 2);
    FactorBrownianBridge_ skipped(std::unique_ptr<Random_>(NewSobol(6, 0)), 2);
    Vector_<> expected;
    Vector_<> actual;
    for (size_t path = 0; path < 18; ++path)
        sequential.FillNormal(&expected);
    skipped.SkipTo(17);
    skipped.FillNormal(&actual);
    ASSERT_EQ(actual, expected);
}
