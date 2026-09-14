//
// Created by Codex on 2026/9/13.
//

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <dal/curve/tapeguard.hpp>
#include <dal/model/factory.hpp>

using namespace Dal;

namespace {
    Vector_<Handle_<ModelData_>> ParameterModels(const Vector_<>& values) {
        return {
            Handle_<ModelData_>(new BSModelData_("", values[0], values[1], values[2], values[3])),
            Handle_<ModelData_>(new DupireModelData_("", values[0], values[2], values[3], {80.0, 160.0}, {0.0, 1.0}, Matrix_<>(2, 2, values[1])))};
    }

    Vector_<> InvalidValues(const String_& label) {
        Vector_<> result{std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
        if (label == "spot") {
            result.push_back(0.0);
            result.push_back(-123.0);
        } else if (label != "rate" && label != "div" && label != "repo")
            result.push_back(-0.2);
        return result;
    }

    template <class F_> void AssertInvalidParameter(const F_& action) {
        try {
            action();
            FAIL() << "invalid current parameters must be rejected";
        } catch (const Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("InvalidModelParameter:"), std::string::npos);
        }
    }

    double MutatedValue(const String_& label) {
        if (label == "spot")
            return 124.0;
        if (label == "rate")
            return -0.03;
        return label == "div" || label == "repo" ? -0.01 : 0.25;
    }

    void AssertCloneRisks(const AAD::Model_<AAD::Number_>& model, double expected) {
        double parallelVega = 0.0;
        for (size_t i = 0; i < model.Parameters().size(); ++i) {
            const auto& label = model.ParameterLabels()[i];
            const double risk = AAD::Adjoint(*model.Parameters()[i]);
            if (label == "spot")
                ASSERT_NEAR(risk, expected / 125.0, 1.0e-10);
            else if (label == "rate")
                ASSERT_NEAR(risk, 0.0, 1.0e-10);
            else if (label == "div" || label == "repo")
                ASSERT_NEAR(risk, -expected, 1.0e-10);
            else
                parallelVega += risk;
        }
        ASSERT_NEAR(parallelVega, expected * (0.4 - 0.25), 1.0e-10);
    }
} // namespace

TEST(ModelTest, TestConstructorParameterDomains) {
    const Vector_<String_> labels{"spot", "vol", "rate", "div"};
    for (size_t parameter = 0; parameter < labels.size(); ++parameter) {
        for (const double value : InvalidValues(labels[parameter])) {
            SCOPED_TRACE(::testing::Message() << "parameter=" << parameter << "; value=" << value);
            Vector_<> values{123.0, 0.2, 0.03, 0.01};
            values[parameter] = value;
            for (const auto& data : ParameterModels(values))
                ASSERT_NO_FATAL_FAILURE(AssertInvalidParameter([&] { static_cast<void>(CreateModel<double>(data)); }));
        }
    }
    for (const auto& data : ParameterModels({124.0, 0.0, -0.03, -0.01}))
        ASSERT_NO_THROW(static_cast<void>(CreateModel<double>(data)));
}

TEST(ModelTest, TestAadLiveParameterDomains) {
    const TapeGuard_ guard(AAD::Tape());
    for (const auto& data : ParameterModels({123.0, 0.2, 0.03, 0.01})) {
        auto model = CreateModel<AAD::Number_>(data);
        for (size_t parameter = 0; parameter < model->Parameters().size(); ++parameter) {
            const double original = AAD::Value(*model->Parameters()[parameter]);
            for (const double value : InvalidValues(model->ParameterLabels()[parameter])) {
                SCOPED_TRACE(::testing::Message() << "parameter=" << parameter << "; value=" << value);
                *model->Parameters()[parameter] = value;
                for (const double time : {0.0, 1.0}) {
                    const Vector_<> timeline{time};
                    Vector_<AAD::SampleDef_> definitions(1);
                    definitions[0].numeraire_ = false;
                    model->Allocate(timeline, definitions);
                    ASSERT_NO_FATAL_FAILURE(AssertInvalidParameter([&] { model->Init(timeline, definitions); }));
                }
            }
            *model->Parameters()[parameter] = original;
        }
    }
}

TEST(ModelTest, TestAadValidLiveParametersAndCloneRisks) {
    for (const auto& data : ParameterModels({123.0, 0.2, 0.03, 0.01})) {
        const TapeGuard_ guard(AAD::Tape());
        auto original = CreateModel<AAD::Number_>(data);
        for (size_t i = 0; i < original->Parameters().size(); ++i)
            *original->Parameters()[i] = MutatedValue(original->ParameterLabels()[i]);
        auto model = original->Clone();
        ASSERT_EQ(model->ParameterLabels(), original->ParameterLabels());
        for (size_t i = 0; i < model->Parameters().size(); ++i) {
            ASSERT_NE(model->Parameters()[i], original->Parameters()[i]);
            ASSERT_EQ(AAD::Value(*model->Parameters()[i]), AAD::Value(*original->Parameters()[i]));
        }
        *model->Parameters()[0] = 125.0;
        ASSERT_EQ(AAD::Value(*original->Parameters()[0]), 124.0);

        const Vector_<> timeline{1.0};
        const Vector_<AAD::SampleDef_> definitions(1);
        model->Allocate(timeline, definitions);
        AAD::Scenario_<AAD::Number_> path;
        AAD::AllocatePath(definitions, path);
        AAD::InitializePath(path);
        AAD::Rewind(*AAD::Tape());
        for (auto* parameter : model->Parameters())
            AAD::PutOnTape(*parameter);
        AAD::NewRecording(*AAD::Tape());
        model->Init(timeline, definitions);
        ASSERT_EQ(model->SimDim(), 1);
        model->GeneratePath({0.4}, &path);
        AAD::Number_ value = path[0].spot_ / path[0].numeraire_;
        const double expected = 125.0 * std::exp(0.01 - 0.5 * 0.25 * 0.25 + 0.25 * 0.4);
        ASSERT_NEAR(AAD::Value(value), expected, 1.0e-10);
        AAD::Adjoint(value) = 1.0;
        AAD::PropagateToStart(*AAD::Tape());
        ASSERT_NO_FATAL_FAILURE(AssertCloneRisks(*model, expected));
    }
}
