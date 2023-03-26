//
// Created by wegam on 2023/3/26.
//

#pragma once

#include <map>
#include <dal/math/vectors.hpp>
#include <dal/storage/storable.hpp>

namespace Dal {
    class YCComponent_ : public Storable_ {
    public:
        YCComponent_(const char *type, const String_ &name) : Storable_(type, name) {}

        // poll all components  (including this)
        virtual void Poll(Vector_<const YCComponent_ *> *) const = 0; // state which ones exist
        virtual void Poll(std::map<const YCComponent_ *, Handle_<YCComponent_>> *) const = 0; // state which ones we have handles for
        // clone, using new base curves in place of old
        using substitutions_t = std::map<const YCComponent_*, Handle_<YCComponent_>>;
        virtual YCComponent_ *Clone(const String_ &new_name, const substitutions_t &base_changes) const = 0;
    };

    template<class T_, class B_ = T_>
    class CurveWithBase_ : public T_ {
    protected:
        Handle_<B_> base_;

        CurveWithBase_(const String_ &name, const Handle_<B_> &base) : T_(name), base_(base) {}

        Handle_<B_> NewBase(const YCComponent_::substitutions_t &base_changes) const {
            auto pb = base_changes.find(base_.get());
            return pb == base_changes.end() ? base_ : handle_cast<B_>(pb->second);
        }

    public:
        void Poll(Vector_<const YCComponent_ *> *all) const override {
            all->push_back(this);
            if (base_)
                base_->Poll(all);
        }

        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>> *all) const override {
            if (base_) {
                (*all)[base_.get()] = handle_cast<YCComponent_>(base_);
                base_->Poll(all);
            }
        }
    };
}
