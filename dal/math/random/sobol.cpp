//
// Created by wegam on 2020/12/26.
//

#include <dal/platform/platform.hpp>
#include <dal/math/random/quasirandom.hpp>
#include <dal/math/random/sobol.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/math/matrixs.hpp>
#include <dal/platform/strict.hpp>

namespace Dal {
    namespace {
        constexpr int N_BITS = 30;
        constexpr auto XOR = [](int i, int j) -> int { return i ^ j; };

        // precomputed primitive polynomials
        constexpr unsigned int KNOWN_PRIMITIVE[] = {0x0000002, 0x0000003, 0x00000005, 0x0000000A};
        constexpr int N_KNOWN = sizeof(KNOWN_PRIMITIVE) / sizeof(KNOWN_PRIMITIVE[0]);

        Matrix_<int> Directions(int size) {
            REQUIRE(size < N_KNOWN, "Not enough primitive polynomials available to generate Sobol sequences");
            // body is left as an exercise to the reader, unless the author gets here first
            return Matrix_<int>(N_BITS, size);
        }

        double ScaleTo01(int state) {
            static constexpr double MUL = 0.5 / (1L << (N_BITS - 1));
            return MUL * state;
        }

        struct SobolSet_ : QuasiRandom::SequenceSet_ {
            Matrix_<int> directions_; // index as [i_bit][i_seq]
            int iPath_;
            Vector_<int> state_;

            explicit SobolSet_(int i_path) : iPath_(i_path) {}

        public:
            [[nodiscard]] int Size() const override {
                return static_cast<int>(state_.size());
            }
            void Next(Vector_<>* dst) override;
            SobolSet_* TakeAway(int sub_size) override;
        };

        void SobolSet_::Next(Vector_<>* dst) {
            dst->Resize(Size()); // usually no-op
            ++iPath_;
            REQUIRE(iPath_ != 0, "Next loop may not terminate"); // so next loop can terminate
            int k = 0;
            for (int j = iPath_; !(j & 1); j >>= 1, ++k)
                ;
            REQUIRE(k < directions_.Rows(), "Exceeding the maximum directions number");
            Transform(&state_, directions_.Row(k), XOR);
            Transform(state_, ScaleTo01, dst);
        }

        SobolSet_* SobolSet_::TakeAway(int sub_size) {
            REQUIRE(sub_size > 0 && sub_size <= Size(), "Invalid sequence sub_size");
            std::unique_ptr<SobolSet_> retval(new SobolSet_(iPath_));
            if (sub_size == Size()) {
                // give away all our contents
                directions_.Swap(&retval->directions_);
                state_.Swap(&retval->state_);
            } else {
                // give it the back of our state/direction
                const int mySize = Size() - sub_size;
                Matrix_<int> myDir(N_BITS, mySize);
                retval->directions_.Resize(N_BITS, sub_size);
                for (int ii = 0; ii < N_BITS; ++ii) {
                    copy(directions_.Row(ii).begin(), directions_.Row(ii).begin() + mySize, myDir.Row(ii).begin());
                    copy(directions_.Row(ii).begin() + mySize, directions_.Row(ii).end(),
                         retval->directions_.Row(ii).begin());
                }
                directions_.Swap(&myDir);
                retval->state_.Assign(state_.begin() + mySize, state_.end());
                state_.Resize(mySize);
            }
            return retval.release();
        }
    } // namespace

    QuasiRandom::SequenceSet_* QuasiRandom::NewSobol(int size, int i_path) {
        std::unique_ptr<SobolSet_> seq(new SobolSet_(i_path));
        seq->directions_ = Directions(size);
        // initialize state from directions
        Fill(&seq->state_, 0);
        for (int jj = 0, ip = i_path; ip; ++jj, ip >>= 1) {
            if ((ip ^ (ip >> 1)) & 1) // the Gray code of iPath_ has a 1<<jj bit
                Transform(&seq->state_, seq->directions_.Row(jj), XOR);
        }
        return seq.release();
    }
} // namespace Dal
