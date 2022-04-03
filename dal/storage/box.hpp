//
// Created by wegam on 2022/4/3.
//

// a Storable_ holding a concrete datum (a matrix of cells)

#pragma once

#include <dal/math/cell.hpp>
#include <dal/math/matrix/matrixs.hpp>
#include <dal/storage/storable.hpp>

/*IF--------------------------------------------------------------------------
storable Box
        Holder for a data block
&members
name is ?string
contents is cell[][]
        Data in the box
-IF-------------------------------------------------------------------------*/

namespace Dal {

    struct Box_ : Storable_ {
        const Matrix_<Cell_> contents_;
        Box_(const String_& name, const Matrix_<Cell_>& data) : Storable_("Box", name), contents_(data) {}
        void Write(Archive::Store_& dst) const override;
    };
}