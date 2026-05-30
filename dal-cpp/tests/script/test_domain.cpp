//
// Created by wegam on 2026/05/30.
//

#include <gtest/gtest.h>
#include <cmath>
#include <dal/platform/platform.hpp>
#include <dal/script/visitor/domain.hpp>

using namespace Dal;
using namespace Dal::Script;

TEST(ScriptTest, TestBoundRealAccessors) {
    Bound_ b(3.5);
    ASSERT_FALSE(b.IsInfinite());
    ASSERT_FALSE(b.IsPlusInf());
    ASSERT_FALSE(b.IsMinusInf());
    ASSERT_NEAR(b.Val(), 3.5, 1e-10);

    Bound_ d;
    ASSERT_TRUE(d.IsZero());
    ASSERT_NEAR(d.Val(), 0.0, 1e-10);
}

TEST(ScriptTest, TestBoundPlusInfinity) {
    Bound_ b(Bound_::plusInfinity_);
    ASSERT_TRUE(b.IsInfinite());
    ASSERT_TRUE(b.IsPlusInf());
    ASSERT_FALSE(b.IsMinusInf());
    ASSERT_TRUE(b.IsPositive());
    ASSERT_TRUE(b.IsPositive(true));
    ASSERT_FALSE(b.IsZero());
}

TEST(ScriptTest, TestBoundMinusInfinity) {
    Bound_ b(Bound_::minusInfinity_);
    ASSERT_TRUE(b.IsInfinite());
    ASSERT_TRUE(b.IsMinusInf());
    ASSERT_FALSE(b.IsPlusInf());
    ASSERT_TRUE(b.IsNegative());
    ASSERT_TRUE(b.IsNegative(true));
    ASSERT_FALSE(b.IsZero());
}

TEST(ScriptTest, TestBoundZeroSignSemantics) {
    Bound_ z(0.0);
    ASSERT_TRUE(z.IsPositive(false));
    ASSERT_FALSE(z.IsPositive(true));
    ASSERT_TRUE(z.IsNegative(false));
    ASSERT_FALSE(z.IsNegative(true));
    ASSERT_TRUE(z.IsZero());
}

TEST(ScriptTest, TestBoundAssignment) {
    Bound_ b(1.0);
    b = 5.0;
    ASSERT_NEAR(b.Val(), 5.0, 1e-10);
    ASSERT_FALSE(b.IsInfinite());

    b = Bound_::plusInfinity_;
    ASSERT_TRUE(b.IsPlusInf());

    b = Bound_::minusInfinity_;
    ASSERT_TRUE(b.IsMinusInf());

    Bound_ c(3.0);
    b = c;
    ASSERT_EQ(b, Bound_(3.0));
}

TEST(ScriptTest, TestBoundComparison) {
    ASSERT_TRUE(Bound_(1.0) == Bound_(1.0));
    ASSERT_TRUE(Bound_(1.0) != Bound_(2.0));
    ASSERT_TRUE(Bound_(1.0) < Bound_(2.0));
    ASSERT_TRUE(Bound_(2.0) > Bound_(1.0));
    ASSERT_TRUE(Bound_(2.0) <= Bound_(2.0));
    ASSERT_TRUE(Bound_(2.0) >= Bound_(2.0));
    ASSERT_FALSE(Bound_(2.0) < Bound_(2.0));
}

TEST(ScriptTest, TestBoundComparisonWithInfinities) {
    Bound_ pInf(Bound_::plusInfinity_);
    Bound_ mInf(Bound_::minusInfinity_);
    ASSERT_TRUE(mInf < Bound_(0.0));
    ASSERT_TRUE(Bound_(0.0) < pInf);
    ASSERT_TRUE(mInf < pInf);
    ASSERT_TRUE(pInf == Bound_(Bound_::plusInfinity_));
    ASSERT_TRUE(mInf == Bound_(Bound_::minusInfinity_));
    ASSERT_TRUE(pInf > mInf);
}

TEST(ScriptTest, TestBoundNegation) {
    ASSERT_EQ(-Bound_(3.0), Bound_(-3.0));
    ASSERT_TRUE((-Bound_(Bound_::plusInfinity_)).IsMinusInf());
    ASSERT_TRUE((-Bound_(Bound_::minusInfinity_)).IsPlusInf());
}

TEST(ScriptTest, TestBoundMultiplicationReal) {
    ASSERT_EQ(Bound_(2.0) * Bound_(3.0), Bound_(6.0));
    ASSERT_EQ(Bound_(-2.0) * Bound_(3.0), Bound_(-6.0));
}

TEST(ScriptTest, TestBoundMultiplicationWithInfinity) {
    Bound_ pInf(Bound_::plusInfinity_);
    Bound_ mInf(Bound_::minusInfinity_);

    ASSERT_TRUE((pInf * Bound_(2.0)).IsPlusInf());
    ASSERT_TRUE((pInf * Bound_(-2.0)).IsMinusInf());
    ASSERT_TRUE((mInf * Bound_(2.0)).IsMinusInf());
    ASSERT_TRUE((mInf * Bound_(-2.0)).IsPlusInf());
    ASSERT_TRUE((pInf * mInf).IsMinusInf());
    ASSERT_TRUE((pInf * pInf).IsPlusInf());

    ASSERT_TRUE((pInf * Bound_(0.0)).IsPlusInf());
    ASSERT_TRUE((Bound_(0.0) * mInf).IsMinusInf());
}

TEST(ScriptTest, TestBoundWrite) {
    ASSERT_EQ(Bound_(Bound_::plusInfinity_).Write(), "+INF");
    ASSERT_EQ(Bound_(Bound_::minusInfinity_).Write(), "-INF");
}

TEST(ScriptTest, TestIntervalSingleton) {
    Interval_ i(4.0);
    double val;
    ASSERT_TRUE(i.IsSingleton(&val));
    ASSERT_NEAR(val, 4.0, 1e-10);
    ASSERT_FALSE(i.IsContinuous());
    ASSERT_EQ(i.Left(), Bound_(4.0));
    ASSERT_EQ(i.Right(), Bound_(4.0));
    ASSERT_FALSE(i.IsInfinite());
}

TEST(ScriptTest, TestIntervalZeroSingleton) {
    Interval_ i(0.0);
    ASSERT_TRUE(i.IsZero());
    ASSERT_TRUE(i.IsSingleton());
}

TEST(ScriptTest, TestIntervalContinuous) {
    Interval_ i(Bound_(1.0), Bound_(2.0));
    ASSERT_FALSE(i.IsSingleton());
    ASSERT_TRUE(i.IsContinuous());
    ASSERT_FALSE(i.IsZero());
}

TEST(ScriptTest, TestIntervalInconsistentBoundsThrow) {
    ASSERT_THROW(Interval_(Bound_(2.0), Bound_(1.0)), Dal::Exception_);
    ASSERT_THROW(Interval_(Bound_(Bound_::plusInfinity_), Bound_(1.0)), Dal::Exception_);
    ASSERT_THROW(Interval_(Bound_(1.0), Bound_(Bound_::minusInfinity_)), Dal::Exception_);
}

TEST(ScriptTest, TestIntervalSignChecks) {
    Interval_ pos(Bound_(1.0), Bound_(2.0));
    ASSERT_TRUE(pos.IsPositive());
    ASSERT_TRUE(pos.IsPositive(true));
    ASSERT_FALSE(pos.IsNegative());
    ASSERT_TRUE(pos.IsPosOrNeg());

    Interval_ neg(Bound_(-2.0), Bound_(-1.0));
    ASSERT_TRUE(neg.IsNegative());
    ASSERT_TRUE(neg.IsNegative(true));
    ASSERT_FALSE(neg.IsPositive());

    Interval_ straddle(Bound_(-1.0), Bound_(1.0));
    ASSERT_FALSE(straddle.IsPositive(true));
    ASSERT_FALSE(straddle.IsNegative(true));
    ASSERT_FALSE(straddle.IsPosOrNeg(true));
}

TEST(ScriptTest, TestIntervalInfinite) {
    Interval_ i(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_));
    ASSERT_TRUE(i.IsInfinite());
    ASSERT_TRUE(i.Includes(0.0));
    ASSERT_TRUE(i.Includes(1e20));
}

TEST(ScriptTest, TestIntervalAddition) {
    Interval_ a(Bound_(1.0), Bound_(2.0));
    Interval_ b(Bound_(3.0), Bound_(4.0));
    Interval_ c = a + b;
    ASSERT_EQ(c.Left(), Bound_(4.0));
    ASSERT_EQ(c.Right(), Bound_(6.0));

    Interval_ withInf(Bound_(Bound_::minusInfinity_), Bound_(5.0));
    Interval_ d = a + withInf;
    ASSERT_TRUE(d.Left().IsMinusInf());
    ASSERT_EQ(d.Right(), Bound_(7.0));
}

TEST(ScriptTest, TestIntervalSubtraction) {
    Interval_ a(Bound_(5.0), Bound_(7.0));
    Interval_ b(Bound_(1.0), Bound_(2.0));
    Interval_ c = a - b;
    ASSERT_EQ(c.Left(), Bound_(3.0));
    ASSERT_EQ(c.Right(), Bound_(6.0));
}

TEST(ScriptTest, TestIntervalUnaryMinus) {
    Interval_ a(Bound_(1.0), Bound_(3.0));
    Interval_ n = -a;
    ASSERT_EQ(n.Left(), Bound_(-3.0));
    ASSERT_EQ(n.Right(), Bound_(-1.0));
}

TEST(ScriptTest, TestIntervalMultiplication) {
    Interval_ a(Bound_(2.0), Bound_(3.0));
    Interval_ b(Bound_(4.0), Bound_(5.0));
    Interval_ c = a * b;
    ASSERT_EQ(c.Left(), Bound_(8.0));
    ASSERT_EQ(c.Right(), Bound_(15.0));

    Interval_ straddle(Bound_(-2.0), Bound_(3.0));
    Interval_ d = straddle * Interval_(Bound_(-1.0), Bound_(4.0));
    ASSERT_EQ(d.Left(), Bound_(-8.0));
    ASSERT_EQ(d.Right(), Bound_(12.0));
}

TEST(ScriptTest, TestIntervalMultiplicationByZero) {
    Interval_ a(Bound_(2.0), Bound_(3.0));
    Interval_ z(0.0);
    Interval_ c = a * z;
    ASSERT_TRUE(c.IsZero());
}

TEST(ScriptTest, TestIntervalInverseSingleton) {
    Interval_ a(4.0);
    Interval_ inv = a.Inverse();
    double val;
    ASSERT_TRUE(inv.IsSingleton(&val));
    ASSERT_NEAR(val, 0.25, 1e-10);
}

TEST(ScriptTest, TestIntervalInverseContinuous) {
    Interval_ a(Bound_(2.0), Bound_(4.0));
    Interval_ inv = a.Inverse();
    ASSERT_NEAR(inv.Left().Val(), 0.25, 1e-10);
    ASSERT_NEAR(inv.Right().Val(), 0.5, 1e-10);
}

TEST(ScriptTest, TestIntervalInverseZeroThrows) {
    Interval_ z(0.0);
    ASSERT_THROW(z.Inverse(), Dal::Exception_);
}

TEST(ScriptTest, TestIntervalDivision) {
    Interval_ a(Bound_(2.0), Bound_(4.0));
    Interval_ b(Bound_(2.0), Bound_(2.0));
    Interval_ c = a / b;
    ASSERT_NEAR(c.Left().Val(), 1.0, 1e-10);
    ASSERT_NEAR(c.Right().Val(), 2.0, 1e-10);
}

TEST(ScriptTest, TestIntervalMinMax) {
    Interval_ a(Bound_(1.0), Bound_(4.0));
    Interval_ b(Bound_(2.0), Bound_(3.0));

    Interval_ mn = a.IMin(b);
    ASSERT_EQ(mn.Left(), Bound_(1.0));
    ASSERT_EQ(mn.Right(), Bound_(3.0));

    Interval_ mx = a.IMax(b);
    ASSERT_EQ(mx.Left(), Bound_(2.0));
    ASSERT_EQ(mx.Right(), Bound_(4.0));
}

TEST(ScriptTest, TestIntervalIncludes) {
    Interval_ a(Bound_(1.0), Bound_(5.0));
    ASSERT_TRUE(a.Includes(3.0));
    ASSERT_TRUE(a.Includes(1.0));
    ASSERT_TRUE(a.Includes(5.0));
    ASSERT_FALSE(a.Includes(6.0));
    ASSERT_FALSE(a.Includes(0.0));

    Interval_ inner(Bound_(2.0), Bound_(4.0));
    ASSERT_TRUE(a.Includes(inner));
    ASSERT_TRUE(inner.IsIncludedIn(a));
    ASSERT_FALSE(inner.Includes(a));
}

TEST(ScriptTest, TestIntervalComparison) {
    Interval_ a(Bound_(1.0), Bound_(2.0));
    Interval_ b(Bound_(1.0), Bound_(3.0));
    Interval_ c(Bound_(2.0), Bound_(3.0));

    ASSERT_TRUE(a == a);
    ASSERT_TRUE(a < b);
    ASSERT_TRUE(b < c);
    ASSERT_TRUE(c > a);
    ASSERT_TRUE(a <= a);
    ASSERT_TRUE(a >= a);
}

TEST(ScriptTest, TestIntervalAdjacent) {
    Interval_ a(Bound_(1.0), Bound_(2.0));
    Interval_ b(Bound_(2.0), Bound_(3.0));
    ASSERT_EQ(a.IsAdjacent(b), 1u);
    ASSERT_EQ(b.IsAdjacent(a), 2u);

    Interval_ c(Bound_(5.0), Bound_(6.0));
    ASSERT_EQ(a.IsAdjacent(c), 0u);
}

TEST(ScriptTest, TestIntervalIntersect) {
    Interval_ a(Bound_(1.0), Bound_(4.0));
    Interval_ b(Bound_(3.0), Bound_(6.0));
    Interval_ sect;
    ASSERT_TRUE(Intersect(a, b, &sect));
    ASSERT_EQ(sect.Left(), Bound_(3.0));
    ASSERT_EQ(sect.Right(), Bound_(4.0));

    Interval_ c(Bound_(10.0), Bound_(12.0));
    ASSERT_FALSE(Intersect(a, c));
}

TEST(ScriptTest, TestIntervalMerge) {
    Interval_ a(Bound_(1.0), Bound_(4.0));
    Interval_ b(Bound_(3.0), Bound_(6.0));
    Interval_ merged;
    ASSERT_TRUE(Merge(a, b, &merged));
    ASSERT_EQ(merged.Left(), Bound_(1.0));
    ASSERT_EQ(merged.Right(), Bound_(6.0));

    Interval_ c(Bound_(10.0), Bound_(12.0));
    ASSERT_FALSE(Merge(a, c));
}

TEST(ScriptTest, TestIntervalWrite) {
    ASSERT_EQ(Interval_(2.0).Write(), "{2}");
}

TEST(ScriptTest, TestDomainEmpty) {
    Domain_ d;
    ASSERT_TRUE(d.IsEmpty());
    ASSERT_EQ(d.Size(), 0u);
    ASSERT_TRUE(d.MinBound().IsMinusInf());
    ASSERT_TRUE(d.MaxBound().IsPlusInf());
}

TEST(ScriptTest, TestDomainSingleton) {
    Domain_ d(3.0);
    ASSERT_FALSE(d.IsEmpty());
    ASSERT_EQ(d.Size(), 1u);
    ASSERT_TRUE(d.IsDiscrete());
    ASSERT_FALSE(d.IsContinuous());
    double val;
    ASSERT_TRUE(d.IsConstant(&val));
    ASSERT_NEAR(val, 3.0, 1e-10);
    ASSERT_EQ(d.MinBound(), Bound_(3.0));
    ASSERT_EQ(d.MaxBound(), Bound_(3.0));
}

TEST(ScriptTest, TestDomainAddDisjointSingletons) {
    Domain_ d;
    d.AddSingleton(1.0);
    d.AddSingleton(3.0);
    ASSERT_EQ(d.Size(), 2u);
    ASSERT_TRUE(d.IsDiscrete());

    auto singletons = d.GetSingletons();
    ASSERT_EQ(singletons.size(), 2u);
    ASSERT_NEAR(singletons[0], 1.0, 1e-10);
    ASSERT_NEAR(singletons[1], 3.0, 1e-10);
}

TEST(ScriptTest, TestDomainMergesOverlappingIntervals) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(1.0), Bound_(4.0)));
    d.AddInterval(Interval_(Bound_(3.0), Bound_(6.0)));
    ASSERT_EQ(d.Size(), 1u);
    ASSERT_EQ(d.MinBound(), Bound_(1.0));
    ASSERT_EQ(d.MaxBound(), Bound_(6.0));
}

TEST(ScriptTest, TestDomainKeepsDisjointIntervals) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    d.AddInterval(Interval_(Bound_(5.0), Bound_(6.0)));
    ASSERT_EQ(d.Size(), 2u);
    ASSERT_EQ(d.MinBound(), Bound_(1.0));
    ASSERT_EQ(d.MaxBound(), Bound_(6.0));
}

TEST(ScriptTest, TestDomainRealSpaceCollapse) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    d.AddInterval(Interval_(Bound_(Bound_::minusInfinity_), Bound_(Bound_::plusInfinity_)));
    ASSERT_EQ(d.Size(), 1u);
    ASSERT_TRUE(d.IsInfinite());
    ASSERT_TRUE(d.MinBound().IsMinusInf());
    ASSERT_TRUE(d.MaxBound().IsPlusInf());
}

TEST(ScriptTest, TestDomainBoolean) {
    Domain_ d;
    d.AddSingleton(0.0);
    d.AddSingleton(1.0);
    pair<double, double> vals;
    ASSERT_TRUE(d.IsBoolean(&vals));
    ASSERT_NEAR(vals.first, 0.0, 1e-10);
    ASSERT_NEAR(vals.second, 1.0, 1e-10);
}

TEST(ScriptTest, TestDomainPositiveNegative) {
    Domain_ pos;
    pos.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    pos.AddSingleton(5.0);
    ASSERT_TRUE(pos.IsPositive());
    ASSERT_FALSE(pos.IsNegative());

    Domain_ neg;
    neg.AddInterval(Interval_(Bound_(-3.0), Bound_(-1.0)));
    ASSERT_TRUE(neg.IsNegative());
    ASSERT_FALSE(neg.IsPositive());
}

TEST(ScriptTest, TestDomainGetContinuous) {
    Domain_ d;
    d.AddSingleton(0.0);
    d.AddInterval(Interval_(Bound_(2.0), Bound_(4.0)));
    Domain_ cont = d.GetContinuous();
    ASSERT_EQ(cont.Size(), 1u);
    ASSERT_TRUE(cont.IsContinuous());
    ASSERT_EQ(cont.MinBound(), Bound_(2.0));
}

TEST(ScriptTest, TestDomainAddition) {
    Domain_ a(2.0);
    Domain_ b(3.0);
    Domain_ c = a + b;
    double val;
    ASSERT_TRUE(c.IsConstant(&val));
    ASSERT_NEAR(val, 5.0, 1e-10);
}

TEST(ScriptTest, TestDomainNegation) {
    Domain_ a;
    a.AddInterval(Interval_(Bound_(1.0), Bound_(3.0)));
    Domain_ n = -a;
    ASSERT_EQ(n.MinBound(), Bound_(-3.0));
    ASSERT_EQ(n.MaxBound(), Bound_(-1.0));
}

TEST(ScriptTest, TestDomainSubtraction) {
    Domain_ a(5.0);
    Domain_ b(2.0);
    Domain_ c = a - b;
    double val;
    ASSERT_TRUE(c.IsConstant(&val));
    ASSERT_NEAR(val, 3.0, 1e-10);
}

TEST(ScriptTest, TestDomainMultiplication) {
    Domain_ a(4.0);
    Domain_ b(3.0);
    Domain_ c = a * b;
    double val;
    ASSERT_TRUE(c.IsConstant(&val));
    ASSERT_NEAR(val, 12.0, 1e-10);
}

TEST(ScriptTest, TestDomainShiftInPlace) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    d += 3.0;
    ASSERT_EQ(d.MinBound(), Bound_(4.0));
    ASSERT_EQ(d.MaxBound(), Bound_(5.0));
    d -= 1.0;
    ASSERT_EQ(d.MinBound(), Bound_(3.0));
    ASSERT_EQ(d.MaxBound(), Bound_(4.0));
}

TEST(ScriptTest, TestDomainMinMax) {
    Domain_ a;
    a.AddInterval(Interval_(Bound_(1.0), Bound_(4.0)));
    Domain_ b;
    b.AddInterval(Interval_(Bound_(2.0), Bound_(3.0)));

    Domain_ mn = a.DMin(b);
    ASSERT_EQ(mn.MinBound(), Bound_(1.0));
    ASSERT_EQ(mn.MaxBound(), Bound_(3.0));

    Domain_ mx = a.DMax(b);
    ASSERT_EQ(mx.MinBound(), Bound_(2.0));
    ASSERT_EQ(mx.MaxBound(), Bound_(4.0));
}

TEST(ScriptTest, TestDomainIncludes) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(1.0), Bound_(3.0)));
    d.AddSingleton(7.0);
    ASSERT_TRUE(d.Includes(2.0));
    ASSERT_TRUE(d.Includes(7.0));
    ASSERT_FALSE(d.Includes(5.0));
}

TEST(ScriptTest, TestDomainFuzzyZeroShortcuts) {
    Domain_ zero(0.0);
    ASSERT_TRUE(zero.CanBeZero());
    ASSERT_FALSE(zero.CanBeNonZero());
    ASSERT_TRUE(zero.ZeroIsDiscrete());
    ASSERT_FALSE(zero.ZeroIsCont());

    Domain_ around;
    around.AddInterval(Interval_(Bound_(-1.0), Bound_(1.0)));
    ASSERT_TRUE(around.CanBeZero());
    ASSERT_TRUE(around.CanBeNonZero());
    ASSERT_FALSE(around.ZeroIsDiscrete());
    ASSERT_TRUE(around.ZeroIsCont());

    Domain_ nonZero(5.0);
    ASSERT_FALSE(nonZero.CanBeZero());
    ASSERT_TRUE(nonZero.CanBeNonZero());
}

TEST(ScriptTest, TestDomainCanBePositiveNegative) {
    Domain_ pos;
    pos.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    ASSERT_TRUE(pos.CanBePositive(true));
    ASSERT_FALSE(pos.CanBeNegative(true));

    Domain_ straddle;
    straddle.AddInterval(Interval_(Bound_(-2.0), Bound_(3.0)));
    ASSERT_TRUE(straddle.CanBePositive(true));
    ASSERT_TRUE(straddle.CanBeNegative(true));

    Domain_ empty;
    ASSERT_FALSE(empty.CanBePositive(true));
    ASSERT_FALSE(empty.CanBeNegative(true));
}

TEST(ScriptTest, TestDomainSmallestPosLb) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(-2.0), Bound_(-1.0)));
    d.AddInterval(Interval_(Bound_(1.0), Bound_(3.0)));
    double res;
    ASSERT_TRUE(d.SmallestPosLb(res));
    ASSERT_NEAR(res, 1.0, 1e-10);
}

TEST(ScriptTest, TestDomainBiggestNegRb) {
    Domain_ d;
    d.AddInterval(Interval_(Bound_(-3.0), Bound_(-1.0)));
    d.AddInterval(Interval_(Bound_(1.0), Bound_(2.0)));
    double res;
    ASSERT_TRUE(d.BiggestNegRb(res));
    ASSERT_NEAR(res, -1.0, 1e-10);
}

TEST(ScriptTest, TestDomainAddDomain) {
    Domain_ a;
    a.AddSingleton(1.0);
    Domain_ b;
    b.AddSingleton(2.0);
    a.AddDomain(b);
    ASSERT_EQ(a.Size(), 2u);
    ASSERT_TRUE(a.Includes(1.0));
    ASSERT_TRUE(a.Includes(2.0));
}
