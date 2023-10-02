//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/storage/storable.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/string/strings.hpp>

namespace Dal::AAD {

    template <class T_ = double> class Model_ {
        inline static const Vector_<String_>& DefaultAssetNames() {
            static Vector_<String_> defaultAssetNames_ = {"spot"};
            return defaultAssetNames_;
        }

    public:
        [[nodiscard]] virtual const size_t NumAssets() const { return 1; }

        [[nodiscard]] virtual const Vector_<String_>& AssetNames() const { return DefaultAssetNames(); }

        virtual void Allocate(const Vector_<>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        virtual void Init(const Vector_<>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        [[nodiscard]] virtual size_t SimDim() const = 0;

        virtual void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const = 0;

        virtual std::unique_ptr<Model_<T_>> Clone() const = 0;

        virtual ~Model_() = default;

        [[nodiscard]] virtual const Vector_<T_*>& Parameters() const = 0;
        [[nodiscard]] virtual const Vector_<String_>& ParameterLabels() const = 0;

        [[nodiscard]] size_t NumParams() const { return const_cast<Model_*>(this)->Parameters().size(); }

    };

    struct ModelData_: public Storable_ {
        ModelData_(const String_& type, const String_& name): Storable_(type.c_str(), name) {}
    };
} // namespace Dal