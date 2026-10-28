#pragma once

#include <map>
#include <memory>

#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>
#include <dal/utilities/facts.hpp>

namespace Dal::FactInternal {
    template <class K_, class V_> class StoredFact_ final : public OneFact_<K_, V_> {
        Handle_<V_> background_;
        std::map<K_, Handle_<V_>> specific_;

        class Writer_ final : public OneFact_<K_, V_>::Writer_ {
            StoredFact_* fact_;

        public:
            explicit Writer_(StoredFact_* fact) : fact_(fact) {}
            void SetDefault(const V_& value) override { fact_->background_.reset(new V_(value)); }
            void operator()(const K_& key, const V_& value) override { fact_->specific_[key].reset(new V_(value)); }
        };
        std::unique_ptr<Writer_> writer_;

    public:
        StoredFact_() : writer_(std::make_unique<Writer_>(this)) {}
        [[nodiscard]] const V_& operator()(const K_& key) const override {
            const auto found = specific_.find(key);
            if (found != specific_.end())
                return *found->second;
            REQUIRE(background_, "no default for '" + String_(key.String()) + "'");
            return *background_;
        }
        [[nodiscard]] typename OneFact_<K_, V_>::Writer_& XWrite() const override { return *writer_; }
    };
} // namespace Dal::FactInternal
