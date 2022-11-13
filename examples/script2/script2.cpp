#include <dal/utilities/timer.hpp>
#include <dal/script/visitor.hpp>
#include <dal/script/visitor/evaluator.hpp>
#include <dal/script/visitor/debugger.hpp>
#include <iomanip>

using namespace Dal;
using namespace Dal::Script;

int main() {
    const int n = 100000000;
    Timer_ timer;
    ScriptNode_ const10 = MakeBaseNode<NodeConst_>(20.0);
    ScriptNode_ const20 = MakeBaseNode<NodeConst_>(30.0);
    ScriptNode_ const30 = MakeBaseNode<NodeConst_>(40.0);
    ScriptNode_ expr10 = BuildBinary<NodePlus_>(const10, const20);
    ScriptNode_ expr20 = BuildBinary<NodeMinus_>(expr10, const30);
    Evaluator_<double> visitor20(1);

    timer.Reset();
    for(int i = 0; i < n; ++i) {
        visitor20.Init();
        visitor20.Visit(expr20);
    }
    std::cout << std::setprecision(8)  << "\tElapsed: " << timer.Elapsed<milliseconds>() << " ms" << std::endl;


    return 0;
}