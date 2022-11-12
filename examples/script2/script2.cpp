#include <dal/utilities/timer.hpp>
#include <dal/script/experimental/visitor.hpp>
#include <dal/script/visitor/evaluator.hpp>
#include <dal/script/experimental/visitor/evaluator.hpp>
#include <dal/script/visitor/debugger.hpp>
//#include <dal/script/experimental/visitor/debugger.hpp>
#include <iomanip>

using namespace Dal;
using namespace Dal::Script;

int main() {
    std::unique_ptr<ScriptNode_> const1 = MakeBaseNode<NodeConst_>(20.0);
    std::unique_ptr<ScriptNode_> const2 = MakeBaseNode<NodeConst_>(30.0);
    std::unique_ptr<ScriptNode_> const3 = MakeBaseNode<NodeConst_>(40.0);
    std::unique_ptr<ScriptNode_> expr1 = BuildBinary<NodePlus_>(const1, const2);
    std::unique_ptr<ScriptNode_> expr2 = BuildBinary<NodeMinus_>(expr1, const3);
    Evaluator_<double> visitor2(1);
    Timer_ timer;
    timer.Reset();
    for(int i = 0; i < 100000000; ++i) {
        visitor2.Init();
        expr2->AcceptVisitor(&visitor2);
    }
    std::cout << std::setprecision(8)  << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;

    std::unique_ptr<Experimental::ScriptNode_> const10 = Experimental::MakeBaseNode<Experimental::NodeConst_>(20.0);
    std::unique_ptr<Experimental::ScriptNode_> const20 = Experimental::MakeBaseNode<Experimental::NodeConst_>(30.0);
    std::unique_ptr<Experimental::ScriptNode_> const30 = Experimental::MakeBaseNode<Experimental::NodeConst_>(40.0);
    std::unique_ptr<Experimental::ScriptNode_> expr10 = Experimental::BuildBinary<Experimental::NodePlus_>(const10, const20);
    std::unique_ptr<Experimental::ScriptNode_> expr20 = Experimental::BuildBinary<Experimental::NodeMinus_>(expr10, const30);
    Experimental::Evaluator_<double> visitor20(1);

    timer.Reset();
    for(int i = 0; i < 100000000; ++i) {
        visitor20.Init();
        visitor20.Visit(expr20);
    }
    std::cout << std::setprecision(8)  << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;


    return 0;
}