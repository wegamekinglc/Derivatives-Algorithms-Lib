#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Phoenix with coupon memory";
    terms.phoenix_ = true;
    terms.memory_ = true;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
