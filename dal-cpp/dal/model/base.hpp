//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/indice/index/equity.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/storage/storable.hpp>
#include <dal/string/strings.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    class Index_;

    struct ModelIndexBinding_ {
        String_ assetName_;
        String_ indexName_;
    };

    namespace AAD {
        inline bool IsPlainEquity(const Index_& index) {
            const auto* equity = dynamic_cast<const Index::Equity_*>(&index);
            return equity && typeid(index) == typeid(Index::Equity_) && equity->Name() == Index::Equity_(equity->eqName_).Name();
        }

        template <class T_ = double> class Model_ {
            inline static const Vector_<String_>& DefaultAssetNames() {
                static Vector_<String_> defaultAssetNames_ = {"spot"};
                return defaultAssetNames_;
            }

            void ValidateSampleOutputs(const SampleDef_& definition, String_* indexName) const {
                REQUIRE(definition.indexNames_.size() <= 1, "UnsupportedModelObservation: one EQ output per sample");
                for (const auto& name : definition.indexNames_) {
                    const Handle_<Index_> index(Index::Parse(name));
                    REQUIRE(index && SupportsIndex(*index), "UnsupportedModelObservation: " + name);
                    REQUIRE(indexName->empty() || *indexName == index->Name(), "UnsupportedModelObservation: multiple future indices");
                    *indexName = index->Name();
                }
            }

        public:
            [[nodiscard]] virtual bool SupportsIndex(const Index_& index) const { return false; }

            void ValidateTimeline(const Vector_<>& timeline, const Vector_<SampleDef_>& definitions) const {
                REQUIRE(!timeline.empty() && timeline.size() == definitions.size(), "InvalidModelTimeline: sample definitions must match dates");
                String_ indexName;
                for (size_t i = 0; i < timeline.size(); ++i) {
                    REQUIRE(std::isfinite(timeline[i]) && timeline[i] >= 0.0 && (i == 0 || timeline[i] > timeline[i - 1]),
                            "InvalidModelTimeline: times must be nonnegative, finite and strictly increasing");
                    ValidateSampleOutputs(definitions[i], &indexName);
                }
            }

            [[nodiscard]] virtual size_t NumAssets() const { return 1; }

            [[nodiscard]] virtual const Vector_<String_>& AssetNames() const { return DefaultAssetNames(); }

            virtual void Allocate(const Vector_<>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

            virtual void Init(const Vector_<>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

            [[nodiscard]] virtual size_t SimDim() const = 0;

            virtual void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>* path) const = 0;

            virtual std::unique_ptr<Model_<T_>> Clone() const = 0;

            virtual ~Model_() = default;

            [[nodiscard]] virtual const Vector_<T_*>& Parameters() const = 0;
            [[nodiscard]] virtual const Vector_<String_>& ParameterLabels() const = 0;

            [[nodiscard]] size_t NumParams() const { return Parameters().size(); }
        };
    } // namespace AAD

    class Slide_ {
    public:
        virtual ~Slide_() = default;
    };

    struct ModelData_: public Storable_ {
        Vector_<String_> parameterLabels_;

        ModelData_(const String_& type, const String_& name): Storable_(type.c_str(), name) {}
        [[nodiscard]] std::unique_ptr<ModelData_> MutantModel(const String_& newName, const Vector_<Handle_<Slide_> >& slides) const {
            REQUIRE(slides.empty(), "slides are not supported for ModelData");
            return MutantModel(&newName, nullptr);
        }

    private:
        virtual std::unique_ptr<ModelData_> MutantModel(const String_* newName, const Slide_* slide) const = 0;
    };

} // namespace Dal
