//
// Created by wegam on 2026/7/19.
//

#include <gtest/gtest.h>

#include <dal/platform/platform.hpp>
#include <dal/model/factory.hpp>
#include <dal/storage/archive.hpp>

using namespace Dal;

namespace {
    class UnsupportedModelData_ : public ModelData_ {
    public:
        UnsupportedModelData_() : ModelData_("UnsupportedModelData_", "unsupported") {}

        void Write(Archive::Store_&) const override {}

    private:
        std::unique_ptr<ModelData_> MutantModel(const String_*, const Slide_*) const override {
            return std::make_unique<UnsupportedModelData_>();
        }
    };
} // namespace

TEST(ModelTest, TestCreateModelBuildsBlackScholes) {
    const Handle_<ModelData_> model_data(std::make_shared<BSModelData_>("my_model", 100.0, 0.20, 0.05, 0.01));
    std::unique_ptr<AAD::Model_<>> model = CreateModel<double>(model_data);

    auto bs = dynamic_cast<const AAD::BlackScholes_<>*>(model.get());
    ASSERT_TRUE(bs != nullptr);
    ASSERT_NEAR(bs->Spot(), 100.0, 1e-10);
    ASSERT_NEAR(bs->Vol(), 0.20, 1e-10);
    ASSERT_NEAR(bs->Rate(), 0.05, 1e-10);
    ASSERT_NEAR(bs->Div(), 0.01, 1e-10);
}

TEST(ModelTest, TestCreateModelBuildsDupire) {
    const Vector_<> spots = {90.0, 100.0, 110.0};
    const Vector_<> times = {0.25, 0.5, 1.0};
    const Matrix_<> vols(3, 3, 0.2);
    const Handle_<ModelData_> model_data(std::make_shared<DupireModelData_>("my_model", 100.0, 0.05, 0.01, spots, times, vols));
    std::unique_ptr<AAD::Model_<>> model = CreateModel<double>(model_data);

    auto dupire = dynamic_cast<const AAD::Dupire_<>*>(model.get());
    ASSERT_TRUE(dupire != nullptr);
    ASSERT_NEAR(dupire->Spot(), 100.0, 1e-10);
    ASSERT_EQ(dupire->Spots(), spots);
    ASSERT_EQ(dupire->Times(), times);
}

TEST(ModelTest, TestCreateModelRejectsUnknownModelData) {
    const Handle_<ModelData_> model_data(std::make_shared<UnsupportedModelData_>());
    ASSERT_THROW((void)CreateModel<double>(model_data), Dal::Exception_);
}
