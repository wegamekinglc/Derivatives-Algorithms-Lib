#pragma once

#include <memory>

#include <dal/indice/indexparse.hpp>

namespace Dal::Index {
    std::unique_ptr<Index_> IRParser(const String_& name);
    std::unique_ptr<Index_> OvernightParser(const Vector_<String_>& fields);
} // namespace Dal::Index
