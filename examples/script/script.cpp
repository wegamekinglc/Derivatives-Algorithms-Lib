//
// Created by wegam on 2020/12/21.
//

#include <dal/script/node.hpp>
#include <dal/script/visitor/debugger.hpp>

using namespace std;
using namespace Dal;

int main() {
    auto lhs = Script::MakeBaseNode<Script::NodeVar_>("x");
    auto rhs = Script::MakeBaseNode<Script::NodeVar_>("y");
    auto root = Script::BuildBinary<Script::NodePlus_>(lhs, rhs);

    Script::Debugger_ debugger;
    root->AcceptVisitor(&debugger);
    std::cout << debugger.String() << std::endl;

    return 0;
}