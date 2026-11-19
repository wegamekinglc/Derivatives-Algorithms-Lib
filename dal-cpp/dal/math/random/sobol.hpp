//
// Created by wegam on 2020/12/26.
//

#pragma once

#include <cstdint>

#include <dal/storage/archive.hpp>
#include <dal/math/random/quasirandom.hpp>

/*IF--------------------------------------------------------------------------
storable SobolRSG
        sobol quasi-random number generator
version 1
&members
name is ?string
i_path is number
n_dim is number
precise is boolean
polish is boolean
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class SobolRSG_;
    namespace Detail {
        BASE_EXPORT void CheckSobolCapacity(const SobolRSG_& generator, size_t numPaths);
    } // namespace Detail

    std::unique_ptr<SequenceSet_> NewSobol(int size, size_t iPath, bool precise = false, bool polish = false);
    //  A reproducible random digital shift (one 32-bit XOR mask per coordinate).
    std::unique_ptr<SequenceSet_> NewDigitallyShiftedSobol(int size, size_t iPath, uint64_t key, bool precise = false, bool polish = false);

    class BASE_EXPORT SobolRSG_: public Storable_ {
        friend void Detail::CheckSobolCapacity(const SobolRSG_& generator, size_t numPaths);

        std::unique_ptr<SequenceSet_> rsg_;
        double i_path_;
        double ndim_;
        bool precise_;
        bool polish_;
    public:
        SobolRSG_(const String_& name, double iPath, double nDim = 1, bool precise = false, bool polish = false);
        void Write(Archive::Store_& dst) const override;
        void FillUniform(Vector_<>* deviates) const;
        void FillNormal(Vector_<>* deviates) const;
        [[nodiscard]] size_t NDim() const;
    };
} // namespace Dal
