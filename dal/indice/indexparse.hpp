//
// Created by wegam on 2022/1/20.
//

#pragma once

#include <dal/platform/platform.hpp>

namespace Dal {
    class Index_;
    class String_;

    namespace Index {
        Index_* Parse(const String_& name);
        using parser_t = Index_* (*)(const String_&);
        void RegisterParser(parser_t func);
        Handle_<Index_> Clone(const Index_&);
    }
}
