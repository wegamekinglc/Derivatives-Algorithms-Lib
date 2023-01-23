//
// Created by wegam on 2023/1/24.
//

#include <dal/platform/strict.hpp>
#include <dal/string/strings.hpp>
#include <dal/indice/indexparse.hpp>
#include <dal/indice/parser/equity.hpp>
#include <dal/indice/parser/init.hpp>

namespace Dal {

    bool IndexParsers_::init_ = false;

    void IndexParsers_::Init() {
        if (!init_) {
            Index::RegisterParser("EQ", Index::EquityParser);
            init_ = true;
        }
    }
} // namespace Dal