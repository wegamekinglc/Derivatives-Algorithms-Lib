//
// Created by wegam on 2022/4/3.
//

// global state; e.g., dates which define "now"

#pragma once

#include <dal/utilities/environment.hpp>

#include <mutex>
#include <optional>
#include <utility>

namespace Dal {
    class String_;
    struct Cell_;
    class Date_;
    class DateTime_;

    struct FixHistory_ {
        Vector_<pair<DateTime_, double>> vals_;
    };

    namespace Global {
        class Store_ : noncopyable {
        public:
            virtual ~Store_() = default;
            virtual void Set(const String_& name, const Matrix_<Cell_>& value) = 0;
            virtual const Matrix_<Cell_>& Get(const String_& name) = 0;
        };

        Store_& TheDateStore();
        void SetTheDateStore(Store_* orphan); // we take over the memory

        class Dates_ : public Environment::Entry_ {
        public:
            static Date_ AccountingDate();
            static Date_ EvaluationDate();
            static void SetAccountingDate(const Date_& date);
            static void SetEvaluationDate(const Date_& date);
        };

        Store_& TheFixingsStore();
        void SetTheFixingsStore(Store_* orphan); // we take over the memory

        class Fixings_ : public Environment::Entry_ {
        public:
            FixHistory_ History(const String_& index_canonical_name); // do not use synonyms or user inputs, only the
                                                                      // Name() emitted by the index
        };
    } // namespace Global

    namespace XGLOBAL {
        class ValuationMutationGuard_ {
            std::unique_lock<std::recursive_mutex> lock_;

        public:
            ValuationMutationGuard_();
            explicit ValuationMutationGuard_(std::try_to_lock_t);

            [[nodiscard]] bool OwnsLock() const { return lock_.owns_lock(); }

            ValuationMutationGuard_(const ValuationMutationGuard_&) = delete;
            ValuationMutationGuard_& operator=(const ValuationMutationGuard_&) = delete;
            ValuationMutationGuard_(ValuationMutationGuard_&&) noexcept = default;
            ValuationMutationGuard_& operator=(ValuationMutationGuard_&&) noexcept = default;
        };

        template <class T_> class ScopedOverride_ {

            using Setter_ = void (*)(const T_&);
            T_ saved_;
            Setter_ setFunc_;
            std::optional<ValuationMutationGuard_> barrier_;

            void Restore() {
                if (setFunc_) {
                    const Setter_ restore = setFunc_;
                    setFunc_ = nullptr;
                    restore(saved_);
                }
            }

            ScopedOverride_(const ScopedOverride_& rhs) = delete;
            ScopedOverride_& operator=(const ScopedOverride_& rhs) = delete;

        public:
            ScopedOverride_(ScopedOverride_&& rhs) noexcept : saved_(rhs.saved_), setFunc_(rhs.setFunc_), barrier_(std::move(rhs.barrier_)) {
                rhs.setFunc_ = nullptr;
            }
            ScopedOverride_(Setter_ set_func, const T_& saved_val) : saved_(saved_val), setFunc_(set_func) {}
            ScopedOverride_(Setter_ set_func, const T_& saved_val, ValuationMutationGuard_&& barrier)
                : saved_(saved_val), setFunc_(set_func), barrier_(std::move(barrier)) {}
            ScopedOverride_& operator=(ScopedOverride_&& rhs) = delete;
            ~ScopedOverride_() { Restore(); }
        };

        void SetAccountingDate(const Date_& dt);
        void SetEvaluationDate(const Date_& dt);
        ScopedOverride_<Date_> SetAccountingDateInScope(const Date_& dt);
        ScopedOverride_<Date_> SetEvaluationDateInScope(const Date_& dt);

        int StoreFixings // returns # stored
            (const String_& index_canonical_name,
             const FixHistory_& fixings,
             bool append = true); // if false, old fixings will be removed
    } // namespace XGLOBAL
} // namespace Dal
