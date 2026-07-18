//
// Created by wegam on 2020/12/19.
//

#pragma once

#include <dal/storage/archive.hpp>
#include <dal/math/random/base.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

/*IF--------------------------------------------------------------------------
enumeration RNGType
    random number generator types
alternative IRN ShuffledIRN
alternative MRG32 MRG32k32a
-IF-------------------------------------------------------------------------*/

/*IF--------------------------------------------------------------------------
storable PseudoRSG
        pseudo random number generator
version 1
&members
name is ?string
seed is number
n_dim is number
precise is boolean
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class PseudoRandom_ : public Random_ {
        bool anti_ = false;

    protected:
        Vector_<> cache_;

    public:
        explicit PseudoRandom_(size_t nDim, bool precise = true) : cache_(nDim), precise_(precise) {}
        ~PseudoRandom_() override = default;
        virtual double NextUniform() = 0;
        void FillUniform(Vector_<>* deviates) override;
        void FillNormal(Vector_<>* deviates) override;
        [[nodiscard]] size_t NDim() const override { return cache_.size(); }
        [[nodiscard]] virtual std::unique_ptr<PseudoRandom_> Branch(int iChild) const = 0;
        const bool precise_;
    };

#include <dal/auto/MG_RNGType_enum.hpp>
    std::unique_ptr<PseudoRandom_> New(const RNGType_& type, int seed, size_t nDim = 1, bool precise = true);

    class BASE_EXPORT PseudoRSG_: public Storable_ {
        std::unique_ptr<PseudoRandom_> rsg_;
        double seed_;
        double ndim_;
        bool precise_;
    public:
        PseudoRSG_(const String_& name, double seed, double ndim = 1, bool precise = true)
            : Storable_("PseudoRSG", name),
              rsg_(New(RNGType_(name), static_cast<int>(seed), static_cast<size_t>(ndim), precise)),
              seed_(seed),
              ndim_(ndim),
              precise_(precise) {}
        void Write(Archive::Store_& dst) const override;
        void FillUniform(Vector_<>* deviates) const {
            rsg_->FillUniform(deviates);
        }
        void FillNormal(Vector_<>* deviates) const {
            rsg_->FillNormal(deviates);
        }
        [[nodiscard]] size_t NDim() const { return rsg_->NDim(); }
    };

} // namespace Dal
