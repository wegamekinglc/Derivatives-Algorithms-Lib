//
// Created by wegam on 2024/8/29.
//

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <new>

#include <dal/platform/platform.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/storage/json.hpp>

using namespace Dal;
using Dal::BSModelData_;

TEST(ModelTest, TestBlackScholesModelData) {
    auto model_data = BSModelData_("my_model", 100.0, 0.20, 0.05, 0.01);
    auto dst = JSON::WriteString(model_data);

    Handle_<Storable_> rtn = JSON::ReadString(dst, true);
    ASSERT_NEAR(std::dynamic_pointer_cast<const BSModelData_>(rtn)->spot_, 100.0, 1e-8);
    ASSERT_NEAR(std::dynamic_pointer_cast<const BSModelData_>(rtn)->vol_, 0.20, 1e-8);
}

TEST(ModelTest, TestBlackScholesMutantModelRename) {
    BSModelData_ model_data("my_model", 100.0, 0.20, 0.05, 0.01);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> noSlides;
    std::unique_ptr<ModelData_> renamed(base.MutantModel(String_("renamed"), noSlides));
    auto bs = dynamic_cast<const BSModelData_*>(renamed.get());
    ASSERT_TRUE(bs != nullptr);
    ASSERT_EQ(bs->Name(), String_("renamed"));
    ASSERT_NEAR(bs->spot_, 100.0, 1e-10);
    ASSERT_NEAR(bs->vol_, 0.20, 1e-10);
    ASSERT_NEAR(bs->rate_, 0.05, 1e-10);
    ASSERT_NEAR(bs->div_, 0.01, 1e-10);
}

TEST(ModelTest, TestBlackScholesMutantModelRejectsSlides) {
    BSModelData_ model_data("my_model", 100.0, 0.20, 0.05, 0.01);
    ModelData_& base = model_data;
    Vector_<Handle_<Slide_>> slides{Handle_<Slide_>(std::make_shared<Slide_>())};
    ASSERT_THROW((void)base.MutantModel(String_("renamed"), slides), Dal::Exception_);
}

TEST(ModelTest, TestBlackScholesModelDataParameterLabels) {
    const BSModelData_ model_data("my_model", 100.0, 0.20, 0.05, 0.01);
    ASSERT_EQ(model_data.parameterLabels_.size(), 4);
    ASSERT_EQ(model_data.parameterLabels_[0], String_("spot"));
    ASSERT_EQ(model_data.parameterLabels_[1], String_("vol"));
    ASSERT_EQ(model_data.parameterLabels_[2], String_("rate"));
    ASSERT_EQ(model_data.parameterLabels_[3], String_("div"));
}

TEST(ModelTest, TestBlackScholesModelParametersTrackMembers) {
    AAD::BlackScholes_<> model(100.0, 0.20, 0.05, 0.01);
    ASSERT_EQ(model.NumParams(), 4);
    ASSERT_EQ(model.Parameters().size(), 4);
    ASSERT_EQ(model.ParameterLabels().size(), 4);
    ASSERT_EQ(model.ParameterLabels()[0], String_("spot"));
    ASSERT_EQ(model.ParameterLabels()[1], String_("vol"));
    ASSERT_EQ(model.ParameterLabels()[2], String_("rate"));
    ASSERT_EQ(model.ParameterLabels()[3], String_("div"));

    ASSERT_NEAR(model.Spot(), 100.0, 1e-10);
    ASSERT_NEAR(model.Vol(), 0.20, 1e-10);
    ASSERT_NEAR(model.Rate(), 0.05, 1e-10);
    ASSERT_NEAR(model.Div(), 0.01, 1e-10);

    // Parameters() exposes the live members, so writes through it move the model
    *model.Parameters()[0] = 110.0;
    *model.Parameters()[1] = 0.25;
    ASSERT_NEAR(model.Spot(), 110.0, 1e-10);
    ASSERT_NEAR(model.Vol(), 0.25, 1e-10);
}

TEST(ModelTest, TestBlackScholesModelDefaultsRateAndDivToZero) {
    const AAD::BlackScholes_<> model(100.0, 0.20);
    ASSERT_NEAR(model.Rate(), 0.0, 1e-14);
    ASSERT_NEAR(model.Div(), 0.0, 1e-14);
}

TEST(ModelTest, TestBlackScholesModelCloneIsIndependent) {
    AAD::BlackScholes_<> model(100.0, 0.20, 0.05, 0.01);
    std::unique_ptr<AAD::Model_<>> cloned(model.Clone());

    *cloned->Parameters()[0] = 50.0;
    ASSERT_NEAR(model.Spot(), 100.0, 1e-10);
    ASSERT_NEAR(model.Vol(), 0.20, 1e-10);

    auto bs_clone = dynamic_cast<const AAD::BlackScholes_<>*>(cloned.get());
    ASSERT_TRUE(bs_clone != nullptr);
    ASSERT_NEAR(bs_clone->Spot(), 50.0, 1e-10);
    ASSERT_NEAR(bs_clone->Vol(), 0.20, 1e-10);
    ASSERT_NEAR(bs_clone->Rate(), 0.05, 1e-10);
    ASSERT_NEAR(bs_clone->Div(), 0.01, 1e-10);
}

TEST(ModelTest, TestBlackScholesModelCloneHasDefinedPreAllocationState) {
    using model_t = AAD::BlackScholes_<>;
    alignas(model_t) unsigned char storage[sizeof(model_t)];
    std::memset(storage, 0xc0, sizeof(storage));
    auto* model = new (storage) model_t(100.0, 0.20, 0.05, 0.01);

    std::unique_ptr<AAD::Model_<>> cloned(model->Clone());
    model->~model_t();

    auto* blackScholesClone = dynamic_cast<const model_t*>(cloned.get());
    ASSERT_TRUE(blackScholesClone != nullptr);
    ASSERT_NEAR(blackScholesClone->Spot(), 100.0, 1e-10);
    ASSERT_NEAR(blackScholesClone->Vol(), 0.20, 1e-10);
    ASSERT_NEAR(blackScholesClone->Rate(), 0.05, 1e-10);
    ASSERT_NEAR(blackScholesClone->Div(), 0.01, 1e-10);
}

TEST(ModelTest, TestBlackScholesAllocateRejectsEmptyTimeline) {
    AAD::BlackScholes_<> model(100.0, 0.20, 0.05, 0.01);
    const Vector_<> empty_timeline;
    const Vector_<AAD::SampleDef_> definitions;
    ASSERT_THROW(model.Allocate(empty_timeline, definitions), Dal::Exception_);
}

TEST(ModelTest, TestBlackScholesDeterministicPathWithToday) {
    constexpr double spot = 100.0;
    constexpr double vol = 0.20;
    constexpr double rate = 0.05;
    constexpr double div = 0.01;

    AAD::BlackScholes_<> model(spot, vol, rate, div);
    const Vector_<> timeline{0.0, 1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(3);
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 2);

    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);

    const Vector_<> gaussian(model.SimDim(), 0.0);
    model.GeneratePath(gaussian, &path);

    // zero Gaussian increments leave the pure drift path spot * exp((rate - div - vol^2 / 2) * t)
    const double drift = rate - div - 0.5 * vol * vol;
    ASSERT_NEAR(path[0].spot_, spot, 1e-10);
    ASSERT_NEAR(path[1].spot_, spot * std::exp(drift * 1.0), 1e-10);
    ASSERT_NEAR(path[2].spot_, spot * std::exp(drift * 2.0), 1e-10);

    for (int i = 0; i < 3; ++i)
        ASSERT_NEAR(path[i].numeraire_, std::exp(rate * timeline[i]), 1e-10);
}

TEST(ModelTest, TestBlackScholesDeterministicPathWithoutToday) {
    constexpr double spot = 100.0;
    constexpr double vol = 0.20;
    constexpr double rate = 0.05;
    constexpr double div = 0.01;

    AAD::BlackScholes_<> model(spot, vol, rate, div);
    const Vector_<> timeline{1.0, 2.0};
    Vector_<AAD::SampleDef_> definitions(2);
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 2);

    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);

    const Vector_<> gaussian(model.SimDim(), 0.0);
    model.GeneratePath(gaussian, &path);

    const double drift = rate - div - 0.5 * vol * vol;
    ASSERT_NEAR(path[0].spot_, spot * std::exp(drift * 1.0), 1e-10);
    ASSERT_NEAR(path[1].spot_, spot * std::exp(drift * 2.0), 1e-10);
}

TEST(ModelTest, TestBlackScholesGeneratePathWithNonzeroGaussian) {
    constexpr double spot = 100.0;
    constexpr double vol = 0.20;
    constexpr double rate = 0.05;
    constexpr double div = 0.01;

    AAD::BlackScholes_<> model(spot, vol, rate, div);
    const Vector_<> timeline{0.0, 1.5};
    Vector_<AAD::SampleDef_> definitions(2);
    model.Allocate(timeline, definitions);
    model.Init(timeline, definitions);
    ASSERT_EQ(model.SimDim(), 1);

    AAD::Scenario_<> path;
    AAD::AllocatePath(definitions, path);
    AAD::InitializePath(path);

    const Vector_<> gaussian{1.0};
    model.GeneratePath(gaussian, &path);

    const double drift = rate - div - 0.5 * vol * vol;
    ASSERT_NEAR(path[0].spot_, spot, 1e-10);
    ASSERT_NEAR(path[1].spot_, spot * std::exp(drift * 1.5 + vol * std::sqrt(1.5)), 1e-10);
}
