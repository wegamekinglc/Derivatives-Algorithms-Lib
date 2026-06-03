//
// Created by wegam on 2022/1/20.
//

#pragma once

namespace Dal {
    class Index_;
    class String_;

    namespace Index {
        Index_* Parse(const String_& name);
        using parser_t = Index_* (*)(const String_&);
        void RegisterParser(const String_& name, parser_t func);
        Handle_<Index_> Clone(const Index_&);
    } // namespace Index
} // namespace Dal
