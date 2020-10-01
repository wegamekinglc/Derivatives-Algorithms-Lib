//
// Created by wegamekinglc on 2020/5/2.
//

#pragma once

#include <functional>

namespace Dal {
    template<class A_, class R_> std::function<R_(A_)> AsFunctor(R_(*func)(A_)) { return std::function<R_(A_)>(func); }
}