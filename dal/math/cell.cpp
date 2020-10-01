//
// Created by wegamekinglc on 2020/5/2.
//

#include <dal/math/cell.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal {
    String_ Cell::OwnString(const Cell_& src) {
        switch(src.type_) {
        case Cell::Type_::STRING:
            return src.s_;
        }
        THROW("Cell must contain s string value");
    }
}