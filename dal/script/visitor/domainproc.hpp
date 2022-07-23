//
// Created by wegam on 2022/7/23.
//

#pragma once

#include <dal/script/visitor.hpp>
#include <dal/script/visitor/domain.hpp>
#include <dal/math/vectors.hpp>


namespace Dal::Script {
    class DomainProcessor_ : public Visitor_ {
        const bool fuzzy_;
        Vector_<Domain_> domains_;
    };
}
