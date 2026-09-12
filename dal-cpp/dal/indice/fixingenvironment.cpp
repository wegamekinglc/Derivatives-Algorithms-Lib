//
// Created by Codex on 2026/9/13.
//

#include <dal/indice/fixingsnapshot.hpp>
#include <dal/indice/index.hpp>
#include <dal/platform/platform.hpp>

namespace Dal {
    Handle_<Environment_> SnapshotFixingEnvironment(const MarketFixingSnapshot_& snapshot, const Vector_<FixingRequest_>& requests) {
        MarketFixingSnapshot_::values_t values;
        for (const auto& dependency : HistoricalFixingDependencies(requests)) {
            const auto history = snapshot.Values().find(dependency.indexName_);
            if (history == snapshot.Values().end())
                continue;
            const auto value = history->second.find(dependency.fixingTime_);
            if (value != history->second.end())
                values[dependency.indexName_].emplace(*value);
        }
        auto access = std::make_unique<FixingsAccess_>();
        for (const auto& history : values)
            access->fixings_.emplace(history.first, Handle_<Fixings_>(new Fixings_(history.first, history.second)));
        Vector_<Handle_<Environment_::Entry_>> entries;
        entries.push_back(Handle_<Environment_::Entry_>(std::move(access)));
        return Handle_<Environment_>(new Environment::Base_(entries));
    }
} // namespace Dal
