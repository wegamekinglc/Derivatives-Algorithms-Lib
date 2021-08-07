//
// Created by wegamekinglc on 2021/8/7.
//

#include <dal/platform/strict.hpp>
#include <dal/math/aad/models/base.hpp>
#include <dal/math/aad/number.hpp>

namespace Dal {
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