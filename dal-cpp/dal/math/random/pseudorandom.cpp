//
// Created by wegam on 2020/12/19.
//

#include <cstdint>
#include <limits>

#include <dal/math/random/pseudorandom.hpp>
#include <dal/math/specialfunctions.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/host.hpp>
#include <dal/platform/platform.hpp>
#include <dal/platform/strict.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    namespace {
        size_t DrawCount(size_t nPaths, size_t nDim) {
            REQUIRE(nDim == 0 || nPaths <= std::numeric_limits<size_t>::max() / nDim, "Random path offset exceeds draw-count range");
            return nPaths * nDim;
        }
    } // namespace

    void PseudoRandom_::SkipTo(size_t nPaths) {
        SkipUniformDraws(DrawCount(nPaths / 2, NDim()));
        anti_ = false;
        if (nPaths & 1)
            FillUniform(&cache_);
    }

    void PseudoRandom_::SkipNormalTo(size_t nPaths) {
        SkipUniformDraws(DrawCount(nPaths, NDim()));
        anti_ = false;
    }

    void PseudoRandom_::FillUniform(Vector_<>* devs) {
        FillUniformWith([this]() { return NextUniform(); }, devs);
    }

    namespace {
        namespace RWT {
            template <class F_>
            FORCE_INLINE void Fill(const F_& nextUniform, bool precise, Vector_<>::iterator dst_begin, Vector_<>::iterator dst_end) {
                for (auto pn = dst_begin; pn != dst_end; ++pn) {
                    const double f = nextUniform();
                    *pn = InverseNCDF(f, precise, precise);
                }
            }
        } // namespace RWT
    } // namespace

    void PseudoRandom_::FillNormal(Vector_<>* deviates) {
        RWT::Fill([this]() { return NextUniform(); }, precise_, deviates->begin(), deviates->end());
    }

    namespace {
        // Generators similar to Knuth's IRN55, with shuffling
        template <int M_, int L_, int S_> struct ShuffledIRN_ : public PseudoRandom_ {
            static const int DE_NOM = 1 << 30;

            Vector_<unsigned> irn_, shuffle_;
            int irl_;
            const int seed_;
            size_t nDraws_ = 0;

            unsigned IRN() {
                if (--irl_ < 0)
                    irl_ = M_ - 1;
                const int pLoc = (irl_ + L_) % M_;
                irn_[irl_] += irn_[pLoc];
                irn_[irl_] %= DE_NOM;
                return irn_[irl_];
            }
            double DrawUniform() {
                static const double MUL = 0.5 / DE_NOM;
                const unsigned irn = IRN();
                const int sLoc = irn % S_;
                int ret_val = shuffle_[sLoc];
                shuffle_[sLoc] = irn;
                return MUL * (2 * ret_val + 1); // avoid 0.0 and 1.0
            }

            double NextUniform() override {
                ++nDraws_;
                return DrawUniform();
            }

            void FillNormal(Vector_<>* deviates) override {
                // Count once per path to avoid a bookkeeping store on every normal draw.
                RWT::Fill([this]() { return DrawUniform(); }, precise_, deviates->begin(), deviates->end());
                nDraws_ += deviates->size();
            }

            explicit ShuffledIRN_(int seed, size_t nDim = 1, bool precise = false)
                : PseudoRandom_(nDim, precise), seed_(seed), irn_(M_), shuffle_(S_), irl_(0) {
                Reset();
            }

            void Reset() {
                irl_ = 0;
                nDraws_ = 0;
                const unsigned MASK = 0x1F2E3D4C;
                const unsigned MUL = 17;
                irn_[0] = seed_;
                for (int ii = 1; ii < M_; ++ii)
                    irn_[ii] = ((MUL * irn_[ii - 1]) % DE_NOM) ^ MASK;
                for (int ii = 0; ii < S_; ++ii)
                    shuffle_[ii] = IRN();
            }

            [[nodiscard]] std::unique_ptr<PseudoRandom_> Branch(int iChild) const override {
                return std::make_unique<ShuffledIRN_<M_, L_, S_>>(irn_[0] ^ irn_[1]);
            }

            [[nodiscard]] std::unique_ptr<Random_> Clone() const override { return std::make_unique<ShuffledIRN_>(*this); }

            void SkipUniformDraws(size_t nDraws) override {
                if (nDraws < nDraws_)
                    Reset();
                for (size_t draw = nDraws_; draw < nDraws; ++draw)
                    DrawUniform();
                nDraws_ = nDraws;
            }
        };

        constexpr int64_t m1_ = 4294967087;
        constexpr int64_t m2_ = 4294944443;
        constexpr int64_t a12_ = 1403580;
        constexpr int64_t a13_ = 810728;
        constexpr int64_t a21_ = 527612;
        constexpr int64_t a23_ = 1370589;
        constexpr double m1p1_ = 4294967088.0;

        struct MRG32k32a_ : public PseudoRandom_ {
            const int64_t a_, b_;
            int64_t xn_, xn1_, xn2_, yn_, yn1_, yn2_;

            explicit MRG32k32a_(const unsigned& a = 12345, const unsigned& b = 12346, size_t nDim = 1, bool precise = false)
                : PseudoRandom_(nDim, precise), a_(a), b_(b) {
                Reset();
            }

            void Reset() {
                xn_ = xn1_ = xn2_ = a_;
                yn_ = yn1_ = yn2_ = b_;
            }

            double DrawUniform() {
                // Products fit in 53 bits, preserving the old exact-integer double stream.
                int64_t x = (a12_ * xn1_ - a13_ * xn2_) % m1_;
                if (x < 0)
                    x += m1_;
                xn2_ = xn1_;
                xn1_ = xn_;
                xn_ = x;

                int64_t y = (a21_ * yn_ - a23_ * yn2_) % m2_;
                if (y < 0)
                    y += m2_;
                yn2_ = yn1_;
                yn1_ = yn_;
                yn_ = y;

                const double u = static_cast<double>(x > y ? x - y : x - y + m1_) / m1p1_;
                return u;
            }

            double NextUniform() override { return DrawUniform(); }

            void FillUniform(Vector_<>* deviates) override {
                FillUniformWith([this]() { return DrawUniform(); }, deviates);
            }

            void FillNormal(Vector_<>* deviates) override {
                RWT::Fill([this]() { return DrawUniform(); }, precise_, deviates->begin(), deviates->end());
            }

            [[nodiscard]] std::unique_ptr<PseudoRandom_> Branch(int iChild) const override { return std::make_unique<MRG32k32a_>(); }

            [[nodiscard]] std::unique_ptr<Random_> Clone() const override { return std::make_unique<MRG32k32a_>(*this); }

            void SkipUniformDraws(size_t nPoints) override {
                Reset();

                static constexpr uint64_t m1l = m1_;
                static constexpr uint64_t m2l = m2_;

                uint64_t ab[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                uint64_t bb[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                uint64_t ai[3][3] = {{0, a12_, m1_ - a13_}, {1, 0, 0}, {0, 1, 0}};
                uint64_t bi[3][3] = {{a21_, 0, m2_ - a23_}, {1, 0, 0}, {0, 1, 0}};

                while (nPoints > 0) {
                    if (nPoints & 1) {
                        MPrd(ab, ai, m1l, ab);
                        MPrd(bb, bi, m2l, bb);
                    }

                    MPrd(ai, ai, m1l, ai);
                    MPrd(bi, bi, m2l, bi);
                    nPoints >>= 1;
                }

                uint64_t x0[3] = {static_cast<uint64_t>(xn_), static_cast<uint64_t>(xn1_), static_cast<uint64_t>(xn2_)};
                uint64_t y0[3] = {static_cast<uint64_t>(yn_), static_cast<uint64_t>(yn1_), static_cast<uint64_t>(yn2_)};

                uint64_t temp[3];
                VPrd(ab, x0, m1l, temp);

                xn_ = static_cast<int64_t>(temp[0]);
                xn1_ = static_cast<int64_t>(temp[1]);
                xn2_ = static_cast<int64_t>(temp[2]);

                VPrd(bb, y0, m2l, temp);

                yn_ = static_cast<int64_t>(temp[0]);
                yn1_ = static_cast<int64_t>(temp[1]);
                yn2_ = static_cast<int64_t>(temp[2]);
            }

        private:
            static void MPrd(const uint64_t lhs[3][3], const uint64_t rhs[3][3], uint64_t mod, uint64_t result[3][3]) {
                uint64_t temp[3][3];

                for (size_t j = 0; j < 3; j++) {
                    for (size_t k = 0; k < 3; k++) {
                        uint64_t s = 0;
                        for (size_t l = 0; l < 3; l++) {
                            uint64_t tmpNum = lhs[j][l] * rhs[l][k];
                            tmpNum %= mod;
                            s += tmpNum;
                            s %= mod;
                        }
                        temp[j][k] = s;
                    }
                }

                for (int j = 0; j < 3; j++) {
                    for (int k = 0; k < 3; k++) {
                        result[j][k] = temp[j][k];
                    }
                }
            }

            static void VPrd(const uint64_t lhs[3][3], const uint64_t rhs[3], uint64_t mod, uint64_t result[3]) {
                for (size_t j = 0; j < 3; j++) {
                    uint64_t s = 0;
                    for (size_t l = 0; l < 3; l++) {
                        uint64_t tmpNum = lhs[j][l] * rhs[l];
                        tmpNum %= mod;
                        s += tmpNum;
                        s %= mod;
                    }
                    result[j] = s;
                }
            }
        };
    } // namespace

#include <dal/auto/MG_RNGType_enum.inc>

    std::unique_ptr<PseudoRandom_> New(const RNGType_& type, int seed, size_t nDim, bool precise) {
        if (type == RNGType_("IRN"))
            return std::make_unique<ShuffledIRN_<55, 31, 128>>(seed, nDim, precise);
        if (type == RNGType_("MRG32"))
            return std::make_unique<MRG32k32a_>(static_cast<unsigned>(seed), static_cast<unsigned>(seed) + 1U, nDim, precise);
        THROW("RNG type is not recognized");
    }

#include <dal/auto/MG_PseudoRSG_v1_Read.inc>
#include <dal/auto/MG_PseudoRSG_v1_Write.inc>

    void PseudoRSG_::Write(Archive::Store_& dst) const { PseudoRSG_v1::XWrite(dst, name_, seed_, ndim_, precise_); }
} // namespace Dal
