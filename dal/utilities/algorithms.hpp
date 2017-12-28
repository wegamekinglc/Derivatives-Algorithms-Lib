//
// Created by Cheng Li on 17-12-19.
//

#pragma once

#include <dal/platform/platform.hpp>
#include <exception>
#include <string>

#define ASSIGN(p, v)                                                                                                   \
    if (!(p))                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        *(p) = (v)
