//
// Created by wegam on 2020/12/19.
//

#include <dal/math/random/random.hpp>
#include <dal/math/vectors.hpp>
#include <dal/platform/host.hpp>
#include <cmath>

namespace Dal {
    // derived classes can call this explicitly
    void Random_::FillUniform(Vector_<>* devs) {
        for (auto&& ud : *devs)
            ud = NextUniform();
    }

    namespace {
        namespace RWT {
            // Marsaglia's rectangle-wedge-tail algorithm, based on Knuth implementation
            static const double D[31] = {
                0.0,          0.0,          0.0,          0.0,          0.0,          0.0,          0.0,
                0.0,          0.0,          0.0,          0.0,          0.0,          0.0,          0.0,
                0.0,          0.0,          0.5050335007, 0.7729568318, 0.8764243173, 0.9392112429, 0.9860868156,
                0.9951545013, 0.9867480142, 0.9792113586, 0.9722739162, 0.9657523400, 0.9595309729, 0.9535340961,
                0.9477102649, 0.9420234020, 0.9364475249};
            static const double E[31] = {
                0.0,          0.0,          0.0,          0.0,          0.0,          0.0,          0.0,
                0.0,          0.0,          0.0,          0.0,          0.0,          0.0,          0.0,
                0.0,          0.0,          25.0,         12.5,         8.3333333333, 6.25,         5.0,
                4.0637731069, 3.3677961409, 2.8582959135, 2.4694553648, 2.1631696640, 1.9158499112, 1.7121118654,
                1.5414940825, 1.3966346593, 1.2722024279};
            static const double P[32] = {
                0.0,          0.8487410443, 0.9699899979, 0.8550231215, 0.9942754672, 0.9951625205, 0.9327422986,
                0.9233994671, 0.7273661575, 1.0,          0.6910843714, 0.4540747884, 0.2866499878, 0.1738620062,
                0.1013177803, 0.0567276597, 0.0672735098, 0.1605108070, 0.2355403454, 0.2854029087, 0.3075794736,
                0.3038922909, 0.2795217494, 0.2414883115, 0.1970555059, 0.1524486512, 0.1121116518, 0.0785256947,
                0.0524616474, 0.0334682128, 0.0204066391, 0.0863979023};
            static const double Q[16] = {0.0,          0.2356431344, 0.2061876931, 0.2339118030,
                                         0.2011514983, 0.2009721989, 0.2144214970, 0.2165909849,
                                         0.2749646762, 0.2,          0.2894002647, 0.4404560771,
                                         0.6977150132, 1.1503375831, 1.9739871865, 3.5256169769};
            static const double S[17] = {0.0, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4,
                                         1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0};
            static const double Y[32] = {
                0.0,          -0.922235758, -5.864444728,  -0.579530323, -33.13734925, -39.54384419, -2.573637156,
                -1.610947892, 0.666415357,  Dal::INF, 0.352574032,  -0.166350547, 0.919632724,  0.357909694,
                -0.022548077, 0.187972157,  0.585574869,   0.961759887,  -0.061622701, 0.120122007,  1.311158187,
                0.312688141,  1.12240661,   0.536325751,   0.75091678,   0.564026097,  0.174746453,  0.382956509,
                -0.01107325,  0.393074576,  0.195833651,   0.781086317};
            static const double Z[32] = {
                0.2,          1.3222357584, 6.6644447279,  1.3795303233, 34.9373492509, 41.3438441882, 2.9736371558,
                2.6109478918, 0.7335846430, Dal::INF, 0.6474259683, 0.3663505472,  0.2803672763,  0.2420903063,
                0.2225480772, 0.2120278433, 0.2144251312,  0.2382401128, 0.2616227015,  0.2798779934,  0.2888418127,
                0.2873118590, 0.2775933900, 0.2636742492,  0.2490832199, 0.2359739033,  0.2252535472,  0.2170434909,
                0.2110732504, 0.2069254241, 0.2041663490,  0.2189136830};

            template <class SRC_>
            FORCE_INLINE void Fill(SRC_* src, Vector_<>::iterator dst_begin, Vector_<>::iterator dst_end) {
                for (auto pn = dst_begin; pn != dst_end; ++pn) {
                    double f = 64.0 * src->NextUniform();
                    double sign = (f > 32.0 ? 1.0 : -1.0);
                    int j = int(f);
                    f -= j;
                    j &= 31;
                    // f is uniform in [0, 1), j in {0..31}
                    if (f >= P[j]) // 60.55%
                        *pn = Y[j] + f * Z[j];
                    else if (j < 16) // 31.28%
                        *pn = S[j] + f * Q[j];
                    else if (j < 31) { // 7.90%
                        double u, v;
                        do { // loop c. 1.056 times
                            u = src->NextUniform();
                            v = src->NextUniform();
                            if (u > v)
                                std::swap(u, v);
                            *pn = S[j - 15] + 0.2 * u;
                            if (v <= D[j]) // In triangle
                                break;
                        } // full rejection test:  1.00%
                        while ((std::exp(0.5 * (Square(S[j - 14]) - Square(*pn))) - 1.0) * E[j] + u < v);
                    } else { // "Supertail" case; 0.27%
                        do { // loop 1.094 times
                            const double u = src->NextUniform();
                            *pn = std::sqrt(9.0 - 2.0 * std::log(u));
                        } while (*pn * src->NextUniform() >= 3.0);
                    }
                    *pn *= sign;
                }
            }
        } // namespace RWT

        // Generators similar to Knuth's IRN55, with shuffling
        template <int M_, int L_, int S_> struct ShuffledIRN_ : Random_ {
            static const int DENOM = 1 << 30;

            Vector_<unsigned> irn_, shuffle_;
            int irl_;

            unsigned IRN() {
                if (--irl_ < 0)
                    irl_ = M_ - 1;
                const int pLoc = (irl_ + L_) % M_;
                irn_[irl_] += irn_[pLoc];
                irn_[irl_] %= DENOM;
                return irn_[irl_];
            }
            double NextUniform() override {
                static const double MUL = 0.5 / DENOM;
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
                    irn_[ii] = ((MUL * irn_[ii - 1]) % DENOM) ^ MASK;
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
