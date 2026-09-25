//
// Created by Codex on 2026/9/25.
//

#pragma once

#include <dal/script/node.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Script {
    template <class T_> const T_& ReadVectorEntry(const Vector_<T_>& values, size_t entry, const String_& context) {
        REQUIRE2(entry < values.size(), "VectorIndexOutOfRange: " + context, ScriptError_);
        return values[entry];
    }

    template <class T_> void WriteVectorEntry(Vector_<T_>* values, size_t entry, const T_& value) {
        if (values->size() <= entry)
            values->Resize(entry + 1);
        (*values)[entry] = value;
    }

    template <class T_> T_ ReduceVectorValues(const Vector_<T_>& values, NodeVectorReduce_::Kind_ kind, const String_& context) {
        if (kind == NodeVectorReduce_::Kind_::Sum) {
            T_ result(0.0);
            for (const auto& value : values)
                result += value;
            return result;
        }
        REQUIRE2(!values.empty(), "EmptyVectorReduction: " + context, ScriptError_);
        T_ result = values.front();
        if (kind == NodeVectorReduce_::Kind_::Average) {
            for (size_t i = 1; i < values.size(); ++i)
                result += values[i];
            result /= static_cast<double>(values.size());
        } else if (kind == NodeVectorReduce_::Kind_::Minimum) {
            for (size_t i = 1; i < values.size(); ++i)
                if (values[i] < result)
                    result = values[i];
        } else {
            for (size_t i = 1; i < values.size(); ++i)
                if (values[i] > result)
                    result = values[i];
        }
        return result;
    }
} // namespace Dal::Script
