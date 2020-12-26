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
        constexpr unsigned int KNOWN_PRIMITIVE[] = {0x0000002, 0x0000003, 0x00000005, 0x0000000A};
        constexpr int N_KNOWN = sizeof(KNOWN_PRIMITIVE) / sizeof(KNOWN_PRIMITIVE[0]);

        Matrix_<int> Directions(int size) {
            REQUIRE(size < N_KNOWN, "Not enough primitive polynomials available to generate Sobol sequences");
            return Matrix_<int>(N_BITS, size);
        }

        double ScaleTo01(int state) {
            static constexpr double MUL = 0.5 / (1L << (N_BITS - 1));
            return MUL * state;
        }

        struct SobolSet_ : QuasiRandom::SequenceSet_ {
            Matrix_<int> directions_;
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
            dst->Resize(Size());
            ++iPath_;
            REQUIRE(iPath_ != 0, "Next loop may not terminate");
            int k = 0;
            for (int j = iPath_; !(j & 1); j >>= 1, ++k)
                ;
            REQUIRE(k < directions_.Rows(), "Exceeding the maximum directions number");
            Transform(&state_, directions_.Row(k), XOR);
            Transform(state_, ScaleTo01, dst);
        }

        SobolSet_* SobolSet_::TakeAway(int sub_size) {
            REQUIRE(sub_size > 0 && sub_size <= Size(), "Invalid sequence sub_size");
            std::unique_ptr<SobolSet_> ret_val(new SobolSet_(iPath_));
            if (sub_size == Size()) {
                directions_.Swap(&ret_val->directions_);
                state_.Swap(&ret_val->state_);
            } else {
                const int size = Size() - sub_size;
                Matrix_<int> dir(N_BITS, size);
                ret_val->directions_.Resize(N_BITS, sub_size);
                for (int ii = 0; ii < N_BITS; ++ii) {
                    copy(directions_.Row(ii).begin(), directions_.Row(ii).begin() + size, dir.Row(ii).begin());
                    copy(directions_.Row(ii).begin() + size, directions_.Row(ii).end(),
                         ret_val->directions_.Row(ii).begin());
                }
                directions_.Swap(&dir);
                ret_val->state_.Assign(state_.begin() + size, state_.end());
                state_.Resize(size);
            }
            return ret_val.release();
        }
    } // namespace

    QuasiRandom::SequenceSet_* QuasiRandom::NewSobol(int size, int i_path) {
        std::unique_ptr<SobolSet_> seq(new SobolSet_(i_path));
        seq->directions_ = Directions(size);
        Fill(&seq->state_, 0);
        for (int jj = 0, ip = i_path; ip; ++jj, ip >>= 1) {
            if ((ip ^ (ip >> 1)) & 1)
                Transform(&seq->state_, seq->directions_.Row(jj), XOR);
        }
        return seq.release();
    }
} // namespace Dal
