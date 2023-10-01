//
// Created by wegamekinglc on 2020/5/1.
//

#pragma once

#include <dal/string/strings.hpp>

namespace Dal {
    namespace Archive {
        class Store_;
    }

    class BASE_EXPORT Storable_ : noncopyable {
    public:
        const String_ type_;
        const String_ name_;
        virtual ~Storable_() = default;
        Storable_(const char* type, const String_& name) : type_(type), name_(name) {}
        virtual void Write(Archive::Store_& dst) const = 0;
        [[nodiscard]] const String_& Name() const { return name_;}
        [[nodiscard]] const String_& Type() const {return type_;}
    };
} // namespace Dal
