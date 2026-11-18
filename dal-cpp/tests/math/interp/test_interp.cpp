//
// Created by wegam on 2020/11/10.
//

#include <gtest/gtest.h>
#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/json.hpp>
#include <dal/storage/splat.hpp>

using Dal::Vector_;
using Dal::String_;
using Dal::Interp::NewLinear;
using Dal::Interp1_;
using Dal::Handle_;
using Dal::Storable_;

TEST(InterpTest, TestInterpLinearImplXWithSmooth) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    auto calculated = Dal::InterpLinearImplX<>(x, f, (x[2] + x[3]) / 2.0);
    auto expected = (f[2] + f[3]) / 2.;
    ASSERT_DOUBLE_EQ(calculated, expected);
}

TEST(InterpTest, TestActiveQueryAndOrdinateGradients) {
    struct Case_ {
        Vector_<> x_, y_;
        double query_, value_, slope_;
        Vector_<> weights_;
        bool finiteDifference_;
    };
    const Vector_<Case_> cases{
        {{0., 1., 2.}, {2., 4., 10.}, .5, 3., 2., {.5, .5, 0.}, true},
        {{0., 1., 2.}, {2., 4., 10.}, 1.5, 7., 6., {0., .5, .5}, true},
        {{0., 1., 2.}, {2., 4., 10.}, -1., 2., 0., {1., 0., 0.}, true},
        {{0., 1., 2.}, {2., 4., 10.}, 3., 10., 0., {0., 0., 1.}, true},
        {{0., 1., 2.}, {2., 4., 10.}, 0., 2., 0., {1., 0., 0.}, false},
        {{0., 1., 2.}, {2., 4., 10.}, 1., 4., 0., {0., 1., 0.}, false},
        {{0., 1., 2.}, {2., 4., 10.}, 2., 10., 0., {0., 0., 1.}, false},
        {{0.}, {2.}, .5, 2., 0., {1.}, true},
        {{0., 1., 1., 2.}, {2., 4., 9., 15.}, 1., 4., 0., {0., 1., 0., 0.}, false},
        {{0., 1., 1., 2.}, {2., 4., 9., 15.}, 1.5, 12., 6., {0., 0., .5, .5}, true},
    };
    for (const auto& row : cases) {
        SCOPED_TRACE(row.query_);
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ query;
        Vector_<Dal::AAD::Number_> values(row.y_.size());
        recording.RegisterInput(query, row.query_);
        for (size_t i = 0; i < values.size(); ++i)
            recording.RegisterInput(values[i], row.y_[i]);
        recording.StartRecording();
        auto output = Dal::InterpLinearImplX(row.x_, values, query);
        recording.FinishRecording();
        recording.ClearAdjoints();
        Dal::AAD::NativeOperations_::SetSeed(output, 1.);
        recording.Reverse();
        ASSERT_DOUBLE_EQ(Dal::Value(output), row.value_);
        ASSERT_DOUBLE_EQ(Dal::InterpLinearImplX(row.x_, row.y_, row.query_), row.value_);
        ASSERT_DOUBLE_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(query), row.slope_);
        for (size_t i = 0; i < values.size(); ++i)
            ASSERT_DOUBLE_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(values[i]), row.weights_[i]);
        if (row.finiteDifference_) {
            const double h = 1e-6;
            const double fd = (Dal::InterpLinearImplX(row.x_, row.y_, row.query_ + h) -
                               Dal::InterpLinearImplX(row.x_, row.y_, row.query_ - h)) / (2. * h);
            ASSERT_NEAR(fd, row.slope_, 1e-8);
        }
        recording.Close();
    }
}

TEST(InterpTest, TestInterp1LinearWithUnorderedX) {
    Vector_<> x = {2., 1., 3., 4., 5.};
    Vector_<> f = {3.5, 2.5, 1.7, 2.8, 3.6};

    ASSERT_THROW(Handle_<Interp1_> interp(NewLinear("interp", x, f)), Dal::Exception_);
}

TEST(InterpTest, TestNewLinear) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};
    Handle_<Interp1_> interp(NewLinear("interp", x, f));

    ASSERT_EQ(interp->name_, "interp");
    ASSERT_DOUBLE_EQ(f[2], (*interp)(x[2]));
    ASSERT_DOUBLE_EQ(f[0], (*interp)(x[0] - 1.));
    ASSERT_DOUBLE_EQ(f[4], (*interp)(x[4] + 1.));
    ASSERT_DOUBLE_EQ((f[2] + f[3]) / 2., (*interp)((x[2] + x[3]) / 2.));
}

TEST(InterpTest, TestInterp1LinearSplatSerialization) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(NewLinear("interp", x, f));
    auto dst = Dal::Splat(*src);
    Handle_<Storable_> rtn = Dal::UnSplat(dst, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(InterpTest, TestInterp1LinearJsonSerialization) {
    Vector_<> x = {1., 2., 3., 4., 5.};
    Vector_<> f = {2.5, 3.5, 1.7, 2.8, 3.6};

    Handle_<Interp1_> src(NewLinear("interp", x, f));
    auto json = Dal::JSON::WriteString(*src);
    Handle_<Storable_> rtn = Dal::JSON::ReadString(json, true);
    Handle_<Interp1_> val(std::dynamic_pointer_cast<const Interp1_>(rtn));

    ASSERT_TRUE(val.get() != nullptr);
    ASSERT_DOUBLE_EQ((*src)(2.5), (*val)(2.5));
}

TEST(InterpTest, TestNewLinearRejectsEmpty) {
    Vector_<> x;
    Vector_<> f;
    ASSERT_THROW((void)Handle_<Interp1_>(NewLinear("interp", x, f)), Dal::Exception_);
}
