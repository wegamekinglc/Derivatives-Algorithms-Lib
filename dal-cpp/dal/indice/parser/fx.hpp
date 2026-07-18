//
// Created by wegam on 2023/1/24.
//

#pragma once

namespace Dal {
    class Index_;
    class String_;
    namespace Index {
        std::unique_ptr<Index_> FxParser(const String_&);
    } // namespace Index
} // namespace Dal
