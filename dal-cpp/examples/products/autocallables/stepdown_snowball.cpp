#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Step-down snowball";
    terms.knockOut_ = 1.05;
    terms.stepDown_ = 0.015;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
