#include <iostream>
#include <sstream>

#include <dal/platform/platform.hpp>
#include <dal/script/event.hpp>
#include <dal/script/visitor/debugger.hpp>

int main() {
    const Dal::Script::ScriptProduct_ empty({}, {});
    std::ostringstream emptyTree;
    empty.DebugTree(emptyTree);
    if (!emptyTree.str().empty())
        return 1;
    const Dal::Script::ScriptProduct_ product(
        {Dal::Cell_(Dal::Date_(2030, 1, 2))},
        {"x = -spot() + SQRT(4) * 3 / 2 ^ 2 IF spot() >= 1;0.1 AND spot() > 0 OR spot() = 0 THEN x = MAX(x, 1) ELSE x PAYS MIN(x, 2) END"});
    for (const bool ascii : {false, true}) {
        const auto& style = Dal::Script::TreeStyle(ascii);
        for (const auto* symbol : {style.tee, style.elbow, style.pipe, style.blank, style.plus, style.minus, style.times,
                                  style.over, style.power, style.negate, style.logS, style.expS, style.sqrtS, style.maxS,
                                  style.minS, style.eqS, style.gtS, style.geS, style.andS, style.orS, style.notS,
                                  style.trueS, style.falseS, style.assignS, style.paysS, style.thenS, style.elseS, style.condS,
                                  style.eventS, style.dotS, style.lAng, style.rAng, style.epsS})
            std::cout << symbol << '\n';
        for (const int width : {1, 20, 125})
            product.DebugTree(std::cout, ascii, width);
    }
}
