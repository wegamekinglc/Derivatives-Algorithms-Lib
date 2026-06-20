//
// Created by dal-implementer on 2026/6/20.
//

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/jointcalibration.hpp>
#include <dal/curve/piecewiselinear.hpp>
#include <dal/curve/ycimp.hpp>
#include <dal/curve/ycpwlf.hpp>
#include <dal/currency/currency.hpp>
#include <dal/protocol/collateraltype.hpp>
#include <dal/protocol/rateconvention.hpp>
#include <dal/time/date.hpp>
#include <dal/time/daybasis.hpp>
#include <dal/time/holidays.hpp>
#include <dal/time/periodlength.hpp>

using namespace Dal;

// The joint AAD analytic Jacobian is backend-neutral: the templated machinery (Tape::DiscountPWLF_,
// Tape::JointCurveBlock_, Tape::JointRate_) compiles and produces a correct Jacobian under every
// AAD backend (native, XAD, CoDiPack, Adept) via the Dal::AAD facade. Every test below runs on
// every backend; there is no skip machinery.
//
// AC11 is the cheapest falsifier for the templated PWL arithmetic (critique S8): the double
// specialization Tape::DiscountPWLF_<double>::operator() must match the existing anonymous-namespace
// double DiscountPWLF_ (ycimp.cpp:63-66) element-wise across all four IntegralTo branches.

namespace {
    // Build a PWL forward with a DISCONTINUITY at every knot (fLeft != fRight), so the
    // fLeftT_[ii] + fRightT_[ii-1] segment indexing from PiecewiseLinear_::Sofar is exercised.
    // Returns the templated curve and a parallel double DiscountPWLF_ for byte-for-byte comparison.
    struct PwlPair_ {
        Handle_<DiscountCurve_> refCurve;                 // anonymous-namespace double DiscountPWLF_
        std::shared_ptr<Tape::DiscountPWLF_<double>> tCurve; // templated double specialization
        Vector_<Date_> knots;
    };

    PwlPair_ BuildDiscontinuousPwlPair() {
        const Date_ today(2024, 1, 15);
        const Vector_<Date_> knots = {
            Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2024, 10, 15),
            Date_(2025, 1, 15), Date_(2025, 7, 15), Date_(2026, 1, 15),
            Date_(2027, 1, 15), Date_(2029, 1, 15),
        };
        const int nKnots = static_cast<int>(knots.size());
        Vector_<> fLeft(nKnots);
        Vector_<> fRight(nKnots);
        // Distinct fLeft/fRight at every knot so the discontinuity is non-trivial.
        for (int k = 0; k < nKnots; ++k) {
            fLeft[k] = 0.02 + 0.001 * k + 0.0007 * (k % 3);
            fRight[k] = 0.025 + 0.0013 * k - 0.0005 * (k % 4);
        }
        const PiecewiseLinear_ pw(knots, fLeft, fRight);
        PwlPair_ retval;
        retval.knots = knots;
        retval.refCurve = Handle_<DiscountCurve_>(NewDiscountPWLF("ref", "USD", pw));
        retval.tCurve = std::make_shared<Tape::DiscountPWLF_<double>>("templ", "USD", knots, fLeft, fRight);
        return retval;
    }
} // namespace

TEST(JointAnalyticJacobianTest, TestTemplatedPwlByteForByte) {
    // AC11: the templated Tape::DiscountPWLF_<double>::operator()(from, to) matches the existing
    // anonymous-namespace double DiscountPWLF_ (ycimp.cpp:63-66) element-wise to 1e-15 across query
    // intervals that hit all four IntegralTo branches: below first knot, beyond last knot, on a
    // knot, in-range partial trapezoid.
    const PwlPair_ p = BuildDiscontinuousPwlPair();
    const Date_ beforeFirst = Date_(2024, 2, 15);    // branch 1: below first knot
    const Date_ onKnot0 = p.knots.front();           // branch 3: exactly on knot 0
    const Date_ onKnotMid = p.knots[4];              // branch 3: exactly on a middle knot
    const Date_ inRange1 = Date_(2024, 8, 20);       // branch 4: interior partial trapezoid
    const Date_ inRange2 = Date_(2025, 10, 1);       // branch 4: interior partial trapezoid (later segment)
    const Date_ beyondLast = Date_(2030, 6, 15);     // branch 2: flat-forward extrapolation past last knot

    const Vector_<Date_> queries = {beforeFirst, onKnot0, onKnotMid, inRange1, inRange2, beyondLast};
    for (int i = 0; i < static_cast<int>(queries.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(queries.size()); ++j) {
            const Date_ from = queries[i];
            const Date_ to = queries[j];
            const double ref = (*p.refCurve)(from, to);
            const double got = (*p.tCurve)(from, to);
            ASSERT_NEAR(got, ref, 1e-15) << "Mismatch at from=" << Date::ToString(from) << ", to=" << Date::ToString(to)
                                         << " (ref=" << ref << ", got=" << got << ")";
        }
    }
}
