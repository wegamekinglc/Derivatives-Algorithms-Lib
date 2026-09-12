//
// Created by Codex on 2026/9/13.
//

#pragma once

#include <dal/indice/fixingsnapshot.hpp>
#include <dal/utilities/exceptions.hpp>

namespace Dal::Detail {
    class SnapshotFixingError_ : public Exception_ {
        FixingRequest_ request_;

    public:
        SnapshotFixingError_(const std::exception& error, const FixingRequest_& request)
            : Exception_(__FILE__, __LINE__, __func__, error.what()), request_(request) {}
        [[nodiscard]] const FixingRequest_& Request() const { return request_; }
    };
} // namespace Dal::Detail
