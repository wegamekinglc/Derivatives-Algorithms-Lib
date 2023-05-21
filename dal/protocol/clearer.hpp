//
// Created by wegam on 2022/1/29.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/protocol/collateraltype.hpp>

/*IF--------------------------------------------------------------------------
enumeration Clearer
   Identifies a clearinghouse
switchable
alternative CME
alternative LCH
method CollateralType_ Collateral() const;
-IF-------------------------------------------------------------------------*/

namespace Dal {
    class String_;
#include <dal/auto/MG_Clearer_enum.hpp>

} // namespace Dal