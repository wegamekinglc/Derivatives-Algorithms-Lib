//
// Created by wegamekinglc on 2020/5/1.
//

#pragma once

#include <dal/platform/platform.hpp>
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
    };
} // namespace Dal
