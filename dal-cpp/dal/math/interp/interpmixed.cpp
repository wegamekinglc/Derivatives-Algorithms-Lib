//
// Created by dal-implementer on 2026/6/14.
//

#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/math/interp/interpmixed.hpp>
#include <dal/math/interp/interplinear.hpp>
#include <dal/math/interp/interpcubic.hpp>
#include <dal/math/interp/interp.hpp>
#include <dal/storage/archive.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        // Composite interpolator: linear on log(DF) up to cutoffYf_, natural cubic on log(DF) beyond.
        // The cutoff must be one of the knot abscissae so both sub-interpolators reproduce its value
        // (C0 continuity). The cubic domain is the tail subarray at and beyond the cutoff.
        class MixedLogDF_ : public Interp1_ {
            Vector_<> yf_;
            Vector_<> logDF_;
            double cutoffYf_;
            int cutoffIndex_;
            Handle_<Interp1_> linear_;
            Handle_<Interp1_> cubic_;

        public:
            MixedLogDF_(const String_& name,
                        const Vector_<>& yf,
                        const Vector_<>& logDF,
                        const MixedSchemeSpec_& spec)
                : Interp1_(name),
                  yf_(yf),
                  logDF_(logDF),
                  cutoffYf_(spec.cutoffYf_),
                  cutoffIndex_(-1) {
                REQUIRE(yf_.size() == logDF_.size(), "Mixed log-DF interpolator: yf and logDF must have equal length");
                REQUIRE(yf_.size() >= 4, "Mixed log-DF interpolator: need at least 4 abscissae (cutoff + 3 cubic points)");
                REQUIRE(IsMonotonic(yf_), "Mixed log-DF interpolator: yf must be strictly increasing");
                // locate the cutoff as one of the knots (largest knot at or below cutoffYf_)
                for (int i = 0; i < static_cast<int>(yf_.size()); ++i) {
                    if (yf_[i] <= cutoffYf_)
                        cutoffIndex_ = i;
                }
                REQUIRE(cutoffIndex_ >= 1, "Mixed log-DF interpolator: cutoff must lie at or beyond the second knot");
                REQUIRE(static_cast<int>(yf_.size()) - cutoffIndex_ >= 3,
                        "Mixed log-DF interpolator: cubic tail must span at least 3 knots");

                // linear head: knots [0 .. cutoffIndex_]
                Vector_<> headYf(yf_.begin(), yf_.begin() + cutoffIndex_ + 1);
                Vector_<> headLogDF(logDF_.begin(), logDF_.begin() + cutoffIndex_ + 1);
                linear_.reset(Interp::NewLinear(name + "_lin", headYf, headLogDF));

                // cubic tail: knots [cutoffIndex_ .. end]
                Vector_<> tailYf(yf_.begin() + cutoffIndex_, yf_.end());
                Vector_<> tailLogDF(logDF_.begin() + cutoffIndex_, logDF_.end());
                cubic_.reset(Interp::NewCubic(name + "_cub", tailYf, tailLogDF, spec.cubicLhs_, spec.cubicRhs_));
            }

            double operator()(double x) const override {
                if (x <= cutoffYf_)
                    return (*linear_)(x);
                return (*cubic_)(x);
            }

            [[nodiscard]] bool IsInBounds(double x) const override {
                return x >= yf_.front() && x <= yf_.back();
            }

            void Write(Archive::Store_& dst) const override {
                REQUIRE(false, "MixedLogDF_ serialization is TODO until an archive schema is added");
            }
        };
    } // namespace

    Interp1_* NewMixedLogDF(const String_& name,
                            const Vector_<>& yf,
                            const Vector_<>& logDF,
                            const MixedSchemeSpec_& spec) {
        return new MixedLogDF_(name, yf, logDF, spec);
    }
} // namespace Dal
