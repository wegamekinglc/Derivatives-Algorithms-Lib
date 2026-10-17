//
// Created by Codex on 2026/10/06.
//

#include <gtest/gtest.h>

#include <memory>
#include <new>

#include <dal/math/buffercapacity.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/model/hybrid.hpp>

using namespace Dal;

namespace {
    class alignas(64) AlignedModel_ final : public AAD::BlackScholes_<double> {
    public:
        AlignedModel_() : AAD::BlackScholes_<double>(100.0, 0.2) {}
    };
} // namespace

TEST(BufferModelCapacityTest, TestModelObjectAndParameterStorageAreAdmittedBeforeCreation) {
    using Model_ = AAD::BlackScholes_<double>;
    const auto payload = sizeof(Model_) + 4 * sizeof(double*);
    BufferCapacityBudget_ budget(payload);
    {
        BufferCapacityScope_ scope(&budget);
        auto model = std::make_unique<Model_>(100.0, 0.2);
        ASSERT_EQ(budget.CapacityBytes(), payload);
        ASSERT_EQ(model->Parameters().size(), 4);
        model.reset();
        ASSERT_EQ(budget.CapacityBytes(), 0);
    }
    BufferCapacityBudget_ insufficient(payload - 1);
    {
        BufferCapacityScope_ scope(&insufficient);
        ASSERT_THROW(std::make_unique<Model_>(100.0, 0.2), Exception_);
        ASSERT_EQ(insufficient.CapacityBytes(), 0);
    }
}

TEST(BufferModelCapacityTest, TestAlignedVirtualDeletionAndCallerOwnedPlacement) {
    const auto payload = sizeof(AlignedModel_) + 4 * sizeof(double*);
    BufferCapacityBudget_ budget(payload);
    BufferCapacityScope_ scope(&budget);
    std::unique_ptr<AAD::Model_<double>> model = std::make_unique<AlignedModel_>();
    ASSERT_EQ(reinterpret_cast<std::uintptr_t>(model.get()) % alignof(AlignedModel_), 0);
    ASSERT_EQ(budget.CapacityBytes(), payload);
    model.reset();
    ASSERT_EQ(budget.CapacityBytes(), 0);
    alignas(AlignedModel_) std::byte storage[sizeof(AlignedModel_)];
    auto* placed = new (storage) AlignedModel_;
    ASSERT_EQ(budget.CapacityBytes(), 4 * sizeof(double*));
    placed->~AlignedModel_();
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferModelCapacityTest, TestConstructorFailureRefundsObjectAndNestedArrays) {
    using Model_ = AAD::BlackScholes_<double>;
    BufferCapacityBudget_ budget(sizeof(Model_) + 4 * sizeof(double*));
    BufferCapacityScope_ scope(&budget);
    ASSERT_THROW(std::make_unique<Model_>(-1.0, 0.2), Exception_);
    ASSERT_EQ(budget.CapacityBytes(), 0);
    auto recovery = std::make_unique<Model_>(100.0, 0.2);
    ASSERT_EQ(budget.CapacityBytes(), sizeof(Model_) + 4 * sizeof(double*));
}

TEST(BufferModelCapacityTest, TestHybridComponentObjectAndParametersShareTheLimit) {
    const HybridBSEquityData_ data("equity", "EQ[ABC]", "USD", "equity-factor", 100.0, 0.2, 0.0);
    using Component_ = AAD::HybridBSEquity_<double>;
    const auto payload = sizeof(Component_) + 3 * sizeof(double*);
    BufferCapacityBudget_ budget(payload);
    BufferCapacityScope_ scope(&budget);
    std::unique_ptr<AAD::HybridComponent_<double>> component = std::make_unique<Component_>(data);
    ASSERT_EQ(budget.CapacityBytes(), payload);
    ASSERT_EQ(component->Parameters().size(), 3);
    ASSERT_THROW(std::make_unique<Component_>(data), Exception_);
    component.reset();
    ASSERT_EQ(budget.CapacityBytes(), 0);
}

TEST(BufferModelCapacityTest, TestNothrowAllocationRemainsAvailableForAlignedModels) {
    BufferCapacityBudget_ insufficient(0);
    {
        BufferCapacityScope_ scope(&insufficient);
        ASSERT_EQ(new (std::nothrow) AlignedModel_, nullptr);
        ASSERT_EQ(insufficient.CapacityBytes(), 0);
    }
    const auto payload = sizeof(AlignedModel_) + 4 * sizeof(double*);
    BufferCapacityBudget_ budget(payload);
    BufferCapacityScope_ scope(&budget);
    std::unique_ptr<AAD::Model_<double>> model(new (std::nothrow) AlignedModel_);
    ASSERT_NE(model, nullptr);
    ASSERT_EQ(budget.CapacityBytes(), payload);
    model.reset();
    ASSERT_EQ(budget.CapacityBytes(), 0);
}
