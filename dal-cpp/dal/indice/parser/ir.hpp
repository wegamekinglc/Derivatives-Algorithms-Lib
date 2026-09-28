//
// Created by Codex on 2026/9/28.
//

#pragma once

#include <memory>

#include <dal/indice/indexparse.hpp>

namespace Dal::Index {
    std::unique_ptr<Index_> IRParser(const String_& name);
} // namespace Dal::Index
