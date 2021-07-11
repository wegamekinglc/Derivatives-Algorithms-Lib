//
// Created by wegam on 2021/7/11.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/aad/aad.hpp>

namespace Dal {
    /*
     * Scenarios
     */

    struct SampleDef_ {
        bool numeraire_ = true;

        struct RateDef_ {
            Time_ start_;
            Time_ end_;
            String_ curve_;

            RateDef_(const Time_& s, const Time_& e, const String_& c)
                :start_(s), end_(e), curve_(c) {}
        };

        Vector_<Time_> discountMats_;
        Vector_<RateDef_> liborDefs_;
        Vector_<Vector_<Time_>> forwardMats_;
    };

    template <class T_>
    struct Sample_ {
        T_ numeraire;
        Vector_<T_> discounts_;
        Vector_<T_> libors_;
        Vector_<Vector_<T_>> forwards_;

        void Allocate(const SampleDef_& data) {
            discounts_.Resize(data.discountMats_.size());
            libors_.Resize(data.liborDefs_.size());
            forwards_.Resize(data.forwardMats_.size());
            for (size_t i = 0; i < forwards_.size(); ++i)
                forwards_[i].Resize(data.forwardMats_[i].size());
        }

        void Initialize() {
            numeraire = T_(1.0);
            std::fill(discounts_.begin(), discounts_.end(), T_(1.0));
            std::fill(libors_.begin(), libors_.end(), T_(1.0));
            for (auto& forward: forwards_)
                std::fill(forward.begin(), forward.end(), T_(1.0));
        }
    };

    template <class T_>
    using Scenario_ = Vector_<Sample_<T_>>;

    template <class T_>
    inline void AllocatePath(const Vector_<SampleDef_>& defLine, Scenario_<T_>& path) {
        path.Resize(defLine.size());
        for (size_t i = 0; i < defLine.size(); ++i)
            path[i].Allocate(defLine[i]);
    }

    template <class T_>
    inline void InitializePath(Scenario_<T_>& path) {
        for (auto& s: path)
            s.Initialize();
    }

    /*
     * Products
     */

    template <class T_>
    class Product_ {
        inline static const Vector_<String_> defaultAssetNames_ = {"spot"};

    public:
        /*
         * access to the product time line
         * along with the sample definitions (def line)
         */
        virtual const Vector_<Time_>& TimeLine() const = 0;
        virtual const Vector_<SampleDef_>& DefLine() const = 0;

        /*
         * number and names of underlying assets, default = 1 and "spot"
         */

        virtual const size_t NumAssets() const {
            return 1;
        }

        virtual const Vector_<String_>& AssetNames() const {
            return defaultAssetNames_;
        }

        /*
         * labels of all payoffs in the product and
         * compute payoffs given a path (on the product time line)
         */
        virtual const Vector_<String_>& PayoffLabels() const = 0;
        virtual void Payoffs(const Scenario_<T_>& path,
                             Vector_<T_>& payoffs) const =0;
        virtual std::unique_ptr<Product_<T_>>& Clone() const = 0;
        virtual ~Product_() = default;
    };

    /*
     * models
     */

    template <class T_>
    class Model_ {
        inline static const Vector_<String_> defaultAssetNames_ = {"spot"};

    public:
        virtual const size_t NumAssets() const { return 1; }

        virtual const Vector_<String_>& AssetNames() const { return defaultAssetNames_; }

        virtual void Allocate(const Vector_<Time_>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        virtual void Init(const Vector_<Time_>& prdTimeLine, const Vector_<SampleDef_>& prdDefLine) = 0;

        virtual size_t SimDim() const = 0;

        virtual void GeneratePath(const Vector_<>& gaussVec, Scenario_<T_>& path) const = 0;

        virtual std::unique_ptr<Model_<T_>> Clone() const = 0;

        virtual ~Model_() = default;

        virtual const Vector_<T_*>& Parameters() = 0;
        virtual const Vector_<String_>& ParameterLabels() const = 0;

        size_t NumParams() const { return const_cast<Model_*>(this)->Parameters().size(); }

        void PutParametersOnTape();
    };

    /*
     * random number generators
     */

    class RNG_ {
    public:
        virtual void Init(const size_t simDim) = 0;
        virtual void NextU(Vector_<>& uVec) = 0;
        virtual void NextG(Vector_<>& gaussVec) = 0;

        virtual std::unique_ptr<RNG_> Clone() const = 0;
        virtual ~RNG_() = default;
        virtual void SkipTo(const unsigned b) = 0;
    };

    /*
     * Template algorithms
     * check compatibility of model and product
     * At the moment, only check that assets are the same in both cases
     * may be easily extended in the future
     */

    template <class T_>
    inline bool CheckCompatibility(
        const Product_<T_>& prd,
        const Model_<T_>& mdl
        ) {
        return prd.AssetNames() == mdl.AssetNames();
    }

    Vector_<Vector_<>> McSimulation(
        const Product_<double>& prd,
        const Model_<double>& mdl,
        const RNG_& rng,
        const size_t nPath
        ) {
        REQUIRE(CheckCompatibility(prd, mdl), "model and product are not compatible");
        auto cMdl = mdl.Clone();
        auto cRng = rng.Clone();

        const size_t nPay = prd.PayoffLabels().size();
        Vector_<Vector_<>> results(nPath, Vector_<>(nPay));

        cMdl->Allocate(prd.TimeLine(), prd.DefLine());
        cMdl->Init(prd.TimeLine(), prd.DefLine());
        cRng->Init(cMdl->SimDim());
        Vector_<> gaussVec(cMdl->SimDim());
        Scenario_<double> path;
        AllocatePath(prd.DefLine(), path);
        InitializePath(path);

        for (size_t i =0; i < nPath; ++i) {
            cRng->NextG(gaussVec);
            cMdl->GeneratePath(gaussVec, path);
            prd.Payoffs(path, results[i]);
        }
        return results;
    }
}