//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

#include <dal/math/aad/sample.hpp>
#include <dal/math/vectors.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>

namespace Dal::AAD {
    template <class T_ = double> class Product_ {
        inline static const Vector_<String_>& DefaultAssetNames() {
            static Vector_<String_> defaultAssetNames_ = {"spot"};
            return defaultAssetNames_;
        }

    protected:
        Vector_<Time_> timeLine_;
        Vector_<SampleDef_> defLine_;
        Vector_<String_> labels_;

    public:
        /*
         * access to the products time line
         * along with the sample definitions (def line)
         */
        [[nodiscard]] const Vector_<Time_>& TimeLine() const { return timeLine_; }
        [[nodiscard]] const Vector_<SampleDef_>& DefLine() const { return defLine_; }
        /*
         * labels of all payoffs in the products and
         * compute payoffs given a path (on the products time line)
         */
        [[nodiscard]] const Vector_<String_>& PayoffLabels() const { return labels_; }

        /*
         * number and names of underlying assets, default = 1 and "spot"
         */

        [[nodiscard]] virtual size_t NumAssets() const { return 1; }
        [[nodiscard]] virtual const Vector_<String_>& AssetNames() const { return DefaultAssetNames(); }
        template <class C_> inline void Payoffs(const Scenario_<T_>& path, C_ payoffs) const {
            return PayoffsImpl(path, payoffs);
        }
        virtual std::unique_ptr<Product_<T_>> Clone() const = 0;
        virtual ~Product_() = default;

    protected:
        virtual void PayoffsImpl(const Scenario_<T_>& path, typename Matrix_<T_>::Row_* payoffs) const = 0;
        virtual void PayoffsImpl(const Scenario_<T_>& path, Vector_<T_>* payoffs) const = 0;
    };
} // namespace Dal