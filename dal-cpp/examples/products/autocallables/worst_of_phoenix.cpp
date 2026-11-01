#include "common.hpp"

int main(int argc, char* argv[]) {
    Dal::Examples::Autocallables::Case_ terms;
    terms.title_ = "Two-asset worst-of Phoenix";
    terms.phoenix_ = true;
    terms.worstOf_ = true;
    return Dal::Examples::Autocallables::Run(argc, argv, terms);
}
