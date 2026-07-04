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
polish is boolean
-IF-------------------------------------------------------------------------*/

namespace Dal {
    SequenceSet_* NewSobol(int size, size_t iPath, bool precise = true, bool polish = false);

    class BASE_EXPORT SobolRSG_: public Storable_ {
        std::unique_ptr<SequenceSet_> rsg_;
        double i_path_;
        double ndim_;
        bool precise_;
        bool polish_;
    public:
        SobolRSG_(const String_& name, double iPath, double nDim = 1, bool precise = true, bool polish = false)
            : Storable_("SobolRSG", name), i_path_(iPath), ndim_(nDim), precise_(precise), polish_(polish) {
            rsg_.reset(NewSobol(static_cast<int>(ndim_), static_cast<size_t>(i_path_), precise, polish));
        }
        void Write(Archive::Store_& dst) const override;
        void FillUniform(Vector_<>* deviates) const;
        void FillNormal(Vector_<>* deviates) const;
        [[nodiscard]] size_t NDim() const;
    };
} // namespace Dal
