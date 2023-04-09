//
// Created by wegam on 2020/12/26.
//

#pragma once

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
-IF-------------------------------------------------------------------------*/

namespace Dal {
    SequenceSet_* NewSobol(int size, size_t i_path, bool precise = false);

    class BASE_EXPORT SobolRSG_: public Storable_ {
        std::unique_ptr<SequenceSet_> rsg_;
        double i_path_;
        double ndim_;
        bool precise_;
    public:
        SobolRSG_(const String_& name, double i_path, double n_dim = 1, bool precise = false)
            : Storable_("SobolRSG", name), i_path_(i_path), ndim_(n_dim), precise_(precise) {
            rsg_.reset(NewSobol(static_cast<int>(ndim_), static_cast<size_t>(i_path_), precise));
        }
        void Write(Archive::Store_& dst) const override;
        void FillUniform(Vector_<>* deviates) const;
        void FillNormal(Vector_<>* deviates) const;
        [[nodiscard]] size_t NDim() const;
    };
} // namespace Dal