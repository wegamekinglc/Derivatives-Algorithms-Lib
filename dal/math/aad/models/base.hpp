//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/math/aad/number.hpp>


namespace Dal {
    template <class T_ = double> class Model_ {
        inline static const Vector_<String_> defaultAssetNames_ = {"spot"};

    public:
        virtual const size_t NumAssets() const { return 1; }

        virtual const Vector_<String_>& AssetNames() const { return defaultAssetNames_; }

        virtual void Allocate(const Vector_<Time_>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        virtual void Init(const Vector_<Time_>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        virtual size_t SimDim() const = 0;

        virtual void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const = 0;

        virtual std::unique_ptr<Model_<T_>> Clone() const = 0;

        virtual ~Model_() = default;

        virtual const Vector_<T_*>& Parameters() = 0;
        virtual const Vector_<String_>& ParameterLabels() const = 0;

        size_t NumParams() const { return const_cast<Model_*>(this)->Parameters().size(); }

        void PutParametersOnTape();
    };

    namespace {
        template <class T_>
        void PutParametersOnTapeT(const Vector_<T_*>&) {}

        template <>
        void PutParametersOnTapeT<Number_>(const Vector_<Number_*>& parameters) {
            for (Number_* param : parameters)
                param->PutOnTape();
        }
    }

    template <class T_>
    void Model_<T_>::PutParametersOnTape() {
        PutParametersOnTapeT<T_>(Parameters());
    }
}