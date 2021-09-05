//
// Created by wegam on 2020/12/19.
//

#include <cmath>
#include <dal/math/random/random.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/host.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    // derived classes can call this explicitly
    void Random_::FillUniform(Vector_<>* devs) {
        for (auto&& ud : *devs)
            if (anti_) {
                ud = (1.0 - cache_);
                anti_ = false;
            } else {
                ud = NextUniform();
                cache_ = ud;
                anti_ = true;
            }
    }

    namespace {
        namespace RWT {
            template <class SRC_>
            FORCE_INLINE void Fill(SRC_* src, Vector_<>::iterator dst_begin, Vector_<>::iterator dst_end) {
                for (auto pn = dst_begin; pn != dst_end; ++pn) {
                    double f = src->NextUniform();
                    *pn = InverseNCDF(f);
                }
            }
        } // namespace RWT

        // Generators similar to Knuth's IRN55, with shuffling
        template <int M_, int L_, int S_> struct ShuffledIRN_ : Random_ {
            static const int DE_NOM = 1 << 30;

            Vector_<unsigned> irn_, shuffle_;
            int irl_;
            const int seed_;

            unsigned IRN() {
                if (--irl_ < 0)
                    irl_ = M_ - 1;
                const int pLoc = (irl_ + L_) % M_;
                irn_[irl_] += irn_[pLoc];
                irn_[irl_] %= DE_NOM;
                return irn_[irl_];
            }
            double NextUniform() override {
                static const double MUL = 0.5 / DE_NOM;
                const unsigned irn = IRN();
                const int sLoc = irn % S_;
                int ret_val = shuffle_[sLoc];
                shuffle_[sLoc] = irn;
                return MUL * (2 * ret_val + 1); // avoid 0.0 and 1.0
            }

            void FillUniform(Vector_<>* deviates) override { Random_::FillUniform(deviates); }
            void FillNormal(Vector_<>* deviates) override { RWT::Fill(this, deviates->begin(), deviates->end()); }

            explicit ShuffledIRN_(int seed) : seed_(seed), irn_(M_), shuffle_(S_), irl_(0) {
                const unsigned MASK = 0x1F2E3D4C;
                const unsigned MUL = 17;
                // initialize IRN_
                irn_[0] = seed;
                for (int ii = 1; ii < M_; ++ii)
                    irn_[ii] = ((MUL * irn_[ii - 1]) % DE_NOM) ^ MASK;
                // initialize shuffle_
                for (int ii = 0; ii < S_; ++ii)
                    shuffle_[ii] = IRN();
            }

            [[nodiscard]] Random_* Branch(int i_child) const override {
                return new ShuffledIRN_<M_, L_, S_>(irn_[0] ^ irn_[1]);
            }

            Random_* Clone() const override { return new ShuffledIRN_(seed_); }

            void SkipTo(size_t n_points) override {}
        };

        constexpr const double m1_ = 4294967087;
        constexpr const double m2_ = 4294944443;
        constexpr const double a12_ = 1403580;
        constexpr const double a13_ = 810728;
        constexpr const double a21_ = 527612;
        constexpr const double a23_ = 1370589;
        constexpr const double m1p1_ = 4294967088;

        struct MRG32k32a_ : public Random_ {
            const double a_, b_;
            double xn_, xn1_, xn2_, yn_, yn1_, yn2_;

            MRG32k32a_(const unsigned& a = 12345, const unsigned& b = 12346) : a_(a), b_(b) { Reset(); }

            void Reset() {
                // Reset state
                xn_ = xn1_ = xn2_ = a_;
                yn_ = yn1_ = yn2_ = b_;
            }

            double NextUniform() override {
                double x = a12_ * xn1_ - a13_ * xn2_;
                // Modulus
                x -= long(x / m1_) * m1_;
                if (x < 0)
                    x += m1_;
                // Update
                xn2_ = xn1_;
                xn1_ = xn_;
                xn_ = x;

                // Same for Y
                double y = a21_ * yn_ - a23_ * yn2_;
                y -= long(y / m2_) * m2_;
                if (y < 0)
                    y += m2_;
                yn2_ = yn1_;
                yn1_ = yn_;
                yn_ = y;

                // Uniform
                const double u = x > y ? (x - y) / m1p1_ : (x - y + m1_) / m1p1_;
                return u;
            }

            void FillUniform(Vector_<>* deviates) override { Random_::FillUniform(deviates); }
            void FillNormal(Vector_<>* deviates) override { RWT::Fill(this, deviates->begin(), deviates->end()); }

            [[nodiscard]] Random_* Branch(int i_child) const override { return new MRG32k32a_(); }

            Random_* Clone() const override {
                return new MRG32k32a_(static_cast<unsigned>(a_), static_cast<unsigned>(b_));
            }

            void SkipTo(size_t n_points) override {
                Reset();

                if (n_points < 0)
                    return;

                if (n_points & 1)
                    n_points = (n_points - 1) / 2;
                else
                    n_points /= 2;

                static constexpr size_t m1l = static_cast<size_t>(m1_);
                static constexpr size_t m2l = static_cast<size_t>(m2_);

                size_t ab[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                size_t bb[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                size_t ai[3][3] = {
                    {0, static_cast<size_t>(a12_), static_cast<size_t>(m1_ - a13_)}, {1, 0, 0}, {0, 1, 0}};
                size_t bi[3][3] = {
                    {static_cast<size_t>(a21_), 0, static_cast<size_t>(m2_ - a23_)}, {1, 0, 0}, {0, 1, 0}};

                while (n_points > 0) {
                    if (n_points & 1) {
                        MPrd(ab, ai, m1l, ab);
                        MPrd(bb, bi, m2l, bb);
                    }

                    MPrd(ai, ai, m1l, ai);
                    MPrd(bi, bi, m2l, bi);
                    n_points >>= 1;
                }

                size_t x0[3] = {static_cast<size_t>(xn_), static_cast<size_t>(xn1_), static_cast<size_t>(xn2_)};
                size_t y0[3] = {static_cast<size_t>(yn_), static_cast<size_t>(yn1_), static_cast<size_t>(yn2_)};

                size_t temp[3];
                VPrd(ab, x0, m1l, temp);

                xn_ = static_cast<double>(temp[0]);
                xn1_ = static_cast<double>(temp[1]);
                xn2_ = static_cast<double>(temp[2]);

                VPrd(bb, y0, m2l, temp);

                yn_ = static_cast<double>(temp[0]);
                yn1_ = static_cast<double>(temp[1]);
                yn2_ = static_cast<double>(temp[2]);
            }

        private:
            //  Matrix product with modulus
            static void MPrd(const size_t lhs[3][3], const size_t rhs[3][3], const size_t& mod, size_t result[3][3]) {
                // Result go to temp, in case result points to lhs or rhs
                size_t temp[3][3];

                for (size_t j = 0; j < 3; j++) {
                    for (size_t k = 0; k < 3; k++) {
                        size_t s = 0;
                        for (size_t l = 0; l < 3; l++) {
                            //	Apply modulus to innermost product
                            size_t tmpNum = lhs[j][l] * rhs[l][k];
                            //	Apply mod
                            tmpNum %= mod;
                            //	Result
                            s += tmpNum;
                            //	Reapply mod
                            s %= mod;
                        }
                        //  Store result in temp
                        temp[j][k] = s;
                    }
                }

                //	Now product is done, copy temp to result
                for (int j = 0; j < 3; j++) {
                    for (int k = 0; k < 3; k++) {
                        result[j][k] = temp[j][k];
                    }
                }
            }

            static void VPrd(const size_t lhs[3][3],
                             const size_t rhs[3],
                             const size_t& mod,
                             size_t result[3]) {
                for (size_t j = 0; j < 3; j++) {
                    size_t s = 0;
                    for (size_t l = 0; l < 3; l++) {
                        size_t tmpNum = lhs[j][l] * rhs[l];
                        tmpNum %= mod;
                        s += tmpNum;
                        s %= mod;
                    }
                    result[j] = s;
                }
            }
        };
    } // namespace

#include <dal/auto/MG_RNGType_enum.cpp>

    Random_* New(const RNGType_& type, int seed) {
        Random_* ret;
        if (type == RNGType_("IRN"))
            ret = new ShuffledIRN_<55, 31, 128>(seed);
        else if (type == RNGType_("MRG32"))
            ret = new MRG32k32a_(seed, seed + 1);
        return ret;
    }
} // namespace Dal
