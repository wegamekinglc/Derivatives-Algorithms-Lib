//
// Created by wegam on 2022/1/23.
//

#pragma once

namespace Dal {
    class Index_;
    class String_;
    namespace Index {
        std::unique_ptr<Index_> EquityParser(const String_&);
    } // namespace Index
} // namespace Dal
