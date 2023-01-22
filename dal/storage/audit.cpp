//
// Created by wegam on 2023/1/22.
//

#include <dal/platform/strict.hpp>
#include <dal/storage/audit.hpp>
#include <dal/utilities/functionals.hpp>

namespace Dal {
    namespace {
        struct ShowToAuditor_ {
            String_ key_;
            const Handle_<Storable_>& value_;
            ShowToAuditor_(const String_& key, const Handle_<Storable_>& value) : key_(key), value_(value) {}
            void operator()(const Environment::Entry_& ee) const {
                if (const auto pa = dynamic_cast<const Auditor_*>(&ee))
                    pa->Notice(key_, value_);
            }
        };
    }	// leave local

    void AuditorImp_::Notice(const String_& key, const Handle_<Storable_>& value) const {
        switch (mode_) {
        case READING_EXCLUSIVE:
            mine_->contents_.erase(key); // and fall through
        case READING:
            mine_->contents_.insert(make_pair(key, value));
            break;
        case PASSIVE:
        case SHOWING:
            THROW("not handled for this mode");
        }
    }

    Vector_<Handle_<Storable_>> AuditorImp_::Find(const String_& key) const {
        static const auto SECOND = GetSecond(*mine_->contents_.begin());
        Vector_<Handle_<Storable_>> ret_val;
        if (mode_ == SHOWING) {
            auto range = mine_->contents_.equal_range(key);
            std::transform(range.first, range.second, back_inserter(ret_val), SECOND);
        }
        return ret_val;
    }

    void Environment::AuditBase(_ENV, const String_& key, const Handle_<Storable_>& value) {
        ShowToAuditor_ f(key, handle_cast<Storable_>(value));
        Environment::Iterate(_env, f);
    }
}