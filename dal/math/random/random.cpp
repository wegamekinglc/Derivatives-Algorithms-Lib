//
// Created by wegam on 2020/12/19.
//

#include <dal/platform/strict.hpp>
#include <dal/math/random/random.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/host.hpp>
#include <dal/math/specialfunctions.hpp>
#include <cmath>

namespace Dal {
    // derived classes can call this explicitly
    void Random_::FillUniform(Vector_<>* devs) {
        for (auto&& ud : *devs)
            ud = NextUniform();
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

            explicit ShuffledIRN_(int seed) : irn_(M_), shuffle_(S_), irl_(0) {
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
        };
    } // namespace

    Random_* Random::New(int seed) {
        return new ShuffledIRN_<55, 31, 128>(seed);
    }
} // namespace Dal
