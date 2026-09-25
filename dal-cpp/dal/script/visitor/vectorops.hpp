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

    template <class T_> T_ SumVectorValues(const Vector_<T_>& values) {
        T_ result(0.0);
        for (const auto& value : values)
            result += value;
        return result;
    }

    template <class T_> T_ ExtremeVectorValue(const Vector_<T_>& values, bool minimum) {
        T_ result = values.front();
        for (size_t i = 1; i < values.size(); ++i)
            if (minimum ? values[i] < result : values[i] > result)
                result = values[i];
        return result;
    }

    template <class T_> T_ ReduceVectorValues(const Vector_<T_>& values, NodeVectorReduce_::Kind_ kind, const String_& context) {
        if (kind == NodeVectorReduce_::Kind_::Sum)
            return SumVectorValues(values);
        REQUIRE2(!values.empty(), "EmptyVectorReduction: " + context, ScriptError_);
        if (kind == NodeVectorReduce_::Kind_::Average)
            return SumVectorValues(values) / static_cast<double>(values.size());
        return ExtremeVectorValue(values, kind == NodeVectorReduce_::Kind_::Minimum);
    }
} // namespace Dal::Script
