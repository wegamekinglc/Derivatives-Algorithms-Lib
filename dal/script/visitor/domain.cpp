//
// Created by wegam on 2022/7/31.
//

#include <dal/math/operators.hpp>
#include <algorithm>
#include <dal/script/visitor/domain.hpp>
#include <sstream>
#include <stdexcept>

namespace Dal::Script {

    const Bound_::PlusInfinity Bound_::plusInfinity_;
    const Bound_::MinusInfinity Bound_::minusInfinity_;
} // namespace Dal::Script
