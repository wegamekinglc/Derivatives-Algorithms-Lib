//
// Created by wegam on 2022/12/9.
//

#pragma once
#include <optional>
#include <dal/platform//platform.hpp>
#include <dal/utilities/noncopyable.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/protocol/accrualperiod.hpp>


namespace Dal {
    namespace Payment {
        class Tag_ : noncopyable {
        public:
            virtual ~Tag_();
        };
        const Handle_<Tag_>& Null();

        namespace Amount {
            class Tag_: noncopyable {
            public:
                virtual ~Tag_();
            };

        }

        namespace Default {
            class Tag_: noncopyable {
            public:
                virtual ~Tag_();
            };

        }

        struct DefaultPeriod_ {
            Date_ start;
            Date_ end;
            bool recoverable_;
        };

        struct Conditions_ {
            enum class Exercise_: char {
                UNCONDITIONAL,
                ON_EXERCISE,
                ON_BARRIER_HIT,
                ON_CONTINUATION
            } exerciseCondition_;

            enum class Credit_: char {
                RISKLESS,
                ON_SURVIVAL,
                ON_DEFAULT
            } creditCondition_;

            DefaultPeriod_ defaultPeriod_;
            Conditions_();
        };

        struct Info_ {
            String_ description_;
            DateTime_ knownTime_;
            Conditions_ conditions_;
            std::optional<AccrualPeriod_> period_;
            Info_(const String_& des = String_(),
                  const DateTime_& known = DateTime::Minimum(),
                  const Conditions_& conditions = Conditions_(),
                  const AccrualPeriod_* accrual = nullptr);
        };
    }
}
