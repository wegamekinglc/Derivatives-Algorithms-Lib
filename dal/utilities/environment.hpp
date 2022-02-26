//
// Created by wegam on 2019/11/4.
//

#pragma once

#include <dal/math/vectors.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/algorithms.hpp>
#include <dal/utilities/noncopyable.hpp>

namespace Dal {
    namespace Environment {
        struct Entry_ : noncopyable {
            virtual ~Entry_() = default;
        };
    } // namespace Environment

    class Environment_ : noncopyable {
    public:
        virtual ~Environment_() = default;
        using Entry_ = Environment::Entry_;

        struct IterImp_ : noncopyable {
            virtual ~IterImp_() = default;
            virtual bool Valid() const = 0;
            virtual IterImp_* Next() const = 0;
            virtual const Entry_& operator*() const = 0;
        };

        struct Iterator_ {
            Handle_<IterImp_> imp_;
            explicit Iterator_(IterImp_* orphan) : imp_(orphan) {}
            bool IsValid() const { return imp_.get() != nullptr && imp_.get()->Valid(); }
            void operator++();
            const Entry_& operator*() const { return **imp_; }
        };

        virtual IterImp_* XBegin() const = 0;
        Iterator_ Begin() const { return Iterator_(XBegin()); }
    };

    namespace Environment {
        template <typename F_> void Iterate(const Environment_* env, F_ func) {
            if (env)
                for (auto pe = env->Begin(); pe.IsValid(); ++pe)
                    func(*pe);
        }

        template <typename T_> Vector_<const T_*> Collect(const Environment_* env) {
            /*
             * we do not own the returned pointers.
             */
            Vector_<const T_*> ret_val;
            Iterate(env, [&](const Entry_& h) {
                if (DYN_PTR(t, const T_, &h))
                    ret_val.push_back(t);
            });
            return ret_val;
        }

        template <typename F_> auto Find(const Environment_* env, F_& func) -> decltype(func(*env->Begin())) {
            using ret_type = decltype(func(*env->Begin()));
            if (env) {
                for (auto pe = env->Begin(); pe.IsValid(); ++pe)
                    if (auto ret = func(*pe))
                        return ret;
            }
            return ret_type(0);
        }

        template <typename T_> const T_* Find(const Environment_* env) {
            auto func = [](const Entry_& e) { return dynamic_cast<const T_*>(&e); };
            return Find<decltype(func)>(env, func);
        }

        class Base_ : public Environment_ {
            Vector_<Handle_<Entry_>> vals_;
            struct MyIter_ : IterImp_ {
                const Vector_<Handle_<Entry_>>& all_;
                using iterator_ = Vector_<Handle_<Entry_>>::const_iterator;
                iterator_ me_;
                MyIter_(const Vector_<Handle_<Entry_>>& all, const Vector_<Handle_<Entry_>>::const_iterator& me)
                    : all_(all), me_(me) {}
                bool Valid() const override { return me_ != all_.end(); }
                IterImp_* Next() const override { return new MyIter_(all_, Dal::Next(me_)); }
                const Entry_& operator*() const override { return **me_; }
            };

        public:
            explicit Base_(const Vector_<Handle_<Entry_>>& vals = Vector_<Handle_<Entry_>>()) : vals_(vals) {}
            MyIter_* XBegin() const override { return new MyIter_(vals_, vals_.begin()); }
        };

        class XDecorated_ : public Environment_ {
            const Environment_*& theEnv_;
            const Environment_* parent_;
            const Entry_& val_;

            struct I1 : IterImp_ {
                const Environment_* parent_;
                const Entry_& val_;
                I1(const Environment_* parent, const Entry_& val) : parent_(parent), val_(val) {}
                bool Valid() const override { return true; }
                IterImp_* Next() const override { return parent_ ? parent_->XBegin() : nullptr; }
                const Entry_& operator*() const override { return val_; }
            };

        public:
            XDecorated_(const Environment_*& theEnv, const Entry_& val) : theEnv_(theEnv), val_(val) {
                parent_ = theEnv_;
                theEnv_ = this;
            }
            ~XDecorated_() override { theEnv_ = parent_; }

            IterImp_* XBegin() const override { return new I1(parent_, val_); }
        };
    } // namespace Environment
} // namespace Dal

#define _ENV const Dal::Environment_* _env
#define XX_ENV_ADD(u, e) Dal::Environment::XDecorated_ __xee##u(_env, e)
#define X_ENV_ADD(u, e) XX_ENV_ADD(u, e)
#define ENV_ADD(e) X_ENV_ADD(__COUNTER__, e)
#define ENV_SEED(e) _ENV = nullptr; ENV_ADD(e)