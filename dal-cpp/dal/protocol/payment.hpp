//
// Created by wegam on 2022/12/9.
//

#pragma once
#include <optional>
#include <dal/utilities/noncopyable.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/protocol/accrualperiod.hpp>
#include <dal/currency/currency.hpp>

/*IF--------------------------------------------------------------------------
enumeration ExerciseCondition
    Condition under which a payment is made
switchable
alternative UNCONDITIONAL
alternative ON_EXERCISE
alternative ON_BARRIER_HIT
alternative ON_CONTINUATION
-IF-------------------------------------------------------------------------*/

namespace Dal {
#include <dal/auto/MG_ExerciseCondition_enum.hpp>

    namespace Payment {
        class Tag_ : noncopyable {
        public:
            virtual ~Tag_() = default;
        };
        const Handle_<Tag_>& Null();

        namespace Amount {
            class Tag_: noncopyable {
            public:
                virtual ~Tag_() = default;
            };

        } // namespace Amount

        struct Conditions_ {
            ExerciseCondition_ exerciseCondition_;

            Conditions_();
        };

        struct Info_ {
            String_ description_;
            DateTime_ knownTime_;
            Conditions_ conditions_;
            std::optional<AccrualPeriod_> period_;
            explicit Info_(String_  des = String_(),
                           const DateTime_& known = DateTime::Minimum(),
                           const Conditions_& conditions = Conditions_(),
                           const AccrualPeriod_* accrual = nullptr);
        };
    } // namespace Payment

    struct Payment_ {
        DateTime_ eventTime_;
        Ccy_ ccy_;
        Date_ date_;
        String_ stream_;
        Payment::Info_ tag_;
        Date_ commitDate_;
        Payment_() = default;
        Payment_(const Payment_& src) = default;
        Payment_(const DateTime_& et,
                 const Ccy_& ccy,
                 const Date_& dt,
                 String_ s,
                 Payment::Info_  tag,
                 const Date_& cd = Date::Minimum());
    };

    class NodeValue_: noncopyable {
    public:
        virtual ~NodeValue_() = default;
        virtual void operator+(double amount) = 0;
    };

    class NodeValues_: noncopyable {
        virtual ~NodeValues_() = default;
        virtual NodeValue_& operator[](const Payment::Tag_& tag) = 0;
        inline NodeValue_& operator[](const Handle_<Payment::Tag_>& tag) {
            return operator[](*tag);
        }

        virtual double& operator[](const Payment::Amount::Tag_& tag) = 0;
        inline double& operator[](const Handle_<Payment::Amount::Tag_>& tag) {
            return operator[](*tag);
        }
    };
} // namespace Dal
