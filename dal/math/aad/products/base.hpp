//
// Created by wegamekinglc on 2021/8/7.
//

#pragma once

namespace Dal {
    template <class T_ = double>
    class Product_ {
        inline static const Vector_<String_> defaultAssetNames_ = {"spot"};

    public:
        /*
         * access to the products time line
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
         * labels of all payoffs in the products and
         * compute payoffs given a path (on the products time line)
         */
        virtual const Vector_<String_>& PayoffLabels() const = 0;
        virtual void Payoffs(const Scenario_<T_>& path,
                             Vector_<T_>* payoffs) const =0;
        virtual std::unique_ptr<Product_<T_>>& Clone() const = 0;
        virtual ~Product_() = default;
    };
}