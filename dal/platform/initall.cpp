//
// Created by wegam on 2023/7/21.
//

#include <dal/time/calendars/init.hpp>
#include <dal/currency/init.hpp>
#include <dal/indice/parser/init.hpp>


namespace Dal {

    void InitAll() {
        Calendars_::Init();
        CcyFacts_::Init();
        IndexParsers_::Init();
    }
}
