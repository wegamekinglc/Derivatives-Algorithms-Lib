//
// Created by wegam on 2023/1/22.
//

#pragma once

#include <dal/utilities/environment.hpp>
#include <dal/storage/bag.hpp>


namespace Dal {
    class Auditor_ : public Environment_::Entry_ {
    public:
        virtual void Notice(const String_& key, const Handle_<Storable_>& value) const = 0;
        [[nodiscard]] virtual Vector_<Handle_<Storable_>> Find(const String_& key) const = 0;
    };

    // wider interface seen by the auditor's parent
    class AuditorImp_ : public Auditor_ {
        std::shared_ptr<Bag_> mine_;

    public:
        enum {
            PASSIVE,
            READING,
            READING_EXCLUSIVE, // avoid vast memory use
            SHOWING,
        } mode_;

        void Notice(const String_& key, const Handle_<Storable_>& value) const override;
        [[nodiscard]] Vector_<Handle_<Storable_>> Find(const String_& key) const override;
    };

    namespace Environment {
        void AuditBase(_ENV, const String_& key, const Handle_<Storable_>& value);

        template <class T_> void Audit(_ENV, const String_& key, const Handle_<T_>& value) {
            AuditBase(_env, key, handle_cast<Storable_>(value));
        }

        template <class T_> struct Recall_ {
            const Environment_* env_;
            const String_ key_;
            Handle_<T_>* value_;
            Recall_(_ENV, String_&& key, Handle_<T_>* value) : env_(_env), key_(std::move(key)), value_(value) {}
            void operator()(const Entry_& env) const {
                if (auto audit = dynamic_cast<const Auditor_*>(&env)) {
                    auto fh = audit->Find(key_);
                    for (const auto& h : fh) {
                        Handle_<T_> temp = handle_cast<T_>(h);
                        if (!temp.Empty() && temp != *value_) {
                            REQUIRE(value_->Empty(), "conflicting recollections");
                            *value_ = temp;
                        }
                    }
                }
            }
        };

        template <class T_> void Recall(_ENV, const String_& key, Handle_<T_>* value) {
            REQUIRE(value && value->Empty(), "value should no be empty");
            auto func = Recall_<T_>(_env, key, value);
            Iterate(_env, func);
        }
    }
}

