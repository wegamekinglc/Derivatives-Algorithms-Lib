//
// Created by wegam on 2020/10/25.
//

#pragma once

#include <map>
#include <dal/storage/archive.hpp>

namespace Dal {
    class Interp1_: public Storable_ {
    public:
        Interp1_(const String_& name);
        virtual double operator()(double x) const = 0;
        virtual bool IsInBounds(double x) const {return true;}
    };
}

/*IF--------------------------------------------------------------------------
storable Interp1Linear
        Linear interpolator on known values in one dimension
version 1
&members
name is ?string
x is number[]
f is number[]
-IF-------------------------------------------------------------------------*/