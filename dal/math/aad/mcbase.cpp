//
// Created by wegam on 2021/7/11.
//

#include <dal/platform/strict.hpp>
#include <dal/math/aad/mcbase.hpp>

namespace Dal {
    Time_  SYSTEM_TIME = 0.0;

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