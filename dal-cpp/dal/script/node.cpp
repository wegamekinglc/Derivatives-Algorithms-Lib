#include <utility>

#include <dal/platform/platform.hpp>
#include <dal/script/node.hpp>
#include <dal/script/visitor/all.hpp>

namespace Dal::Script {
    namespace {
        template <class Extremum_> void ReleaseFoldedArguments(Extremum_& node) {
            if (node.arguments_.empty())
                return;

            // A variadic extremum can own a long chain of binary left operands.
            auto& left = node.arguments_.front();
            while (auto* child = dynamic_cast<Extremum_*>(left.get())) {
                if (child->arguments_.empty())
                    break;
                auto released = std::move(left);
                left = std::move(released->arguments_.front());
            }
        }
    } // namespace

    NodeMax_::~NodeMax_() { ReleaseFoldedArguments(*this); }
    NodeMin_::~NodeMin_() { ReleaseFoldedArguments(*this); }
} // namespace Dal::Script
