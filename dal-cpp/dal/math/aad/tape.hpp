/*
 * Modified by wegamekinglc on 2020/12/13.
 * Written by Antoine Savine in 2018
 * This code is the strict IP of Antoine Savine
 * License to use and alter this code for personal and commercial applications
 * is freely granted to any person or company who purchased a copy of the book
 * Modern Computational Finance: AAD and Parallel Simulations
 * Antoine Savine
 * Wiley, 2018
 * As long as this comment is preserved at the top of the file
 */

#pragma once

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS) && (defined(DAL_USE_XAD_AAD) || defined(DAL_USE_CODIPACK_AAD) || defined(DAL_USE_ADEPT_AAD))
#error DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS requires the native AAD backend
#endif

#if !defined(DAL_USE_XAD_AAD) && !defined(DAL_USE_CODIPACK_AAD) && !defined(DAL_USE_ADEPT_AAD)

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
#include <atomic>
#include <limits>
#endif

#include <dal/math/aad/blocklist.hpp>
#include <dal/math/aad/node.hpp>

namespace Dal::AAD {
    class Number_;
    constexpr size_t BLOCK_SIZE = 16384;
    constexpr size_t ADJ_SIZE = 32768;
    constexpr size_t DATA_SIZE = 65536;

    class Tape_ {
    public:
        explicit Tape_(bool = true) : multi_(false), numAdj_(1), pad_{} {}

#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        Tape_(const Tape_&) = delete;
        Tape_& operator=(const Tape_&) = delete;
        Tape_(Tape_&&) = delete;
        Tape_& operator=(Tape_&&) = delete;
#endif

        using Iterator_ = BlockList_<TapNode_, BLOCK_SIZE>::Iterator_;

        bool multi_;
        size_t numAdj_;
        BlockList_<double, ADJ_SIZE> adjointsMulti_;
        BlockList_<double, DATA_SIZE> ders_;
        BlockList_<double*, DATA_SIZE> argPtrs_;
        BlockList_<TapNode_, BLOCK_SIZE> nodes_;
        char pad_[64];

        friend auto SetNumResultsForAAD(bool, size_t);
        friend struct NumResultsResetterForAAD_;
        friend class Number_;
        friend void Clear(Tape_& tape);
        friend void Mark(Tape_& tape);
        friend void RewindToMark(Tape_& tape);
        friend void Rewind(Tape_& tape);
        friend void PropagateMarkToStart(Tape_& tape);
        friend void PropagateToStart(Tape_& tape);
        friend void PropagateToMark(Tape_& tape);
        friend void ZeroAdjoints(Tape_& tape);

        template <size_t N_> TapNode_* RecordNode() { return AllocateNode<N_>(); }

    private:
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
        struct NodeBinding_ {
            std::uint64_t owner_ = 0;
            std::uint64_t epoch_ = 0;
            std::uint64_t ordinal_ = 0;
            std::uint64_t generation_ = 0;
            bool multi_ = false;
            size_t width_ = 0;
        };

        friend struct NativeLifetimeTestAccess_;
        static std::uint64_t ClaimLifetimeIdentity(std::atomic<std::uint64_t>* next);
        static std::uint64_t NewLifetimeIdentity();
        const std::uint64_t lifetimeIdentity_ = NewLifetimeIdentity();
        std::uint64_t lifetimeEpoch_ = 1;
        std::uint64_t lifetimeGeneration_ = 0;
        std::uint64_t liveNodes_ = 0;
        std::uint64_t markedNodes_ = 0;
        bool lifetimeFailed_ = false;
        bool graphMulti_ = false;
        size_t graphWidth_ = 1;

        [[noreturn]] void RejectLifetime(const char* operation, const char* constraint, const NodeBinding_* binding = nullptr) const;
        void RequireLiveGraph(const char* operation) const;
        void CheckNodeAllocation() const;
        void BeginLifetimeReset();
        void ValidateBinding(const NodeBinding_& binding, const TapNode_* node, const char* operation) const;
        void ValidateLiveSlot(const NodeBinding_& binding, const TapNode_* node, const char* operation) const;
        [[nodiscard]] NodeBinding_ CaptureBinding(const TapNode_* node) const {
            return {lifetimeIdentity_, lifetimeEpoch_, node->lifetimeOrdinal_, node->lifetimeGeneration_, multi_, numAdj_};
        }
#endif

        template <size_t N_> FORCE_INLINE TapNode_* AllocateNode() {
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            CheckNodeAllocation();
            try {
#endif
                TapNode_* node = nodes_.EmplaceBack(N_);
                if (multi_) {
                    node->pAdjoints_ = adjointsMulti_.EmplaceBackMulti(numAdj_);
                    std::fill_n(node->pAdjoints_, numAdj_, 0.0);
                }

                if constexpr (static_cast<bool>(N_)) {
                    node->pDerivatives_ = ders_.EmplaceBackMulti<N_>();
                    node->pAdjPtrs_ = argPtrs_.EmplaceBackMulti<N_>();
                }
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
                if (liveNodes_ == 0) {
                    graphMulti_ = multi_;
                    graphWidth_ = numAdj_;
                }
                node->lifetimeOrdinal_ = liveNodes_++;
                node->lifetimeGeneration_ = ++lifetimeGeneration_;
#endif
                return node;
#if defined(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS)
            } catch (...) {
                lifetimeFailed_ = true;
                throw;
            }
#endif
        }

    };

    // Keep three-input allocation out of model loops without duplicating the allocator.
    template <>
#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
    __attribute__((noipa))
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    inline TapNode_* Tape_::RecordNode<3>() {
        return AllocateNode<3>();
    }

} // namespace Dal::AAD
#elif defined(DAL_USE_ADEPT_AAD)
#include <adept.h>
#include <algorithm>
#include <dal/utilities/exceptions.hpp>

namespace Dal::AAD {

    struct Position_ {
        adept::uIndex statements_;
        adept::uIndex operations_;
    };

    class Tape_ : public adept::Stack {
    public:
        using adept::Stack::compute_adjoint;

        Position_ start_;
        Position_ mark_;

        explicit Tape_(bool activate = true) : adept::Stack(activate), start_(Position()), mark_(start_) {}

        [[nodiscard]] Position_ Position() const {
            return {n_statements(), n_operations()};
        }

        void reset_to(adept::uIndex nStatements, adept::uIndex nOperations) {
            n_statements_ = nStatements;
            n_operations_ = nOperations;
        }

        // Later recording windows can outgrow Adept's gradient storage while old adjoints must survive.
        // Reaches into adept::Stack protected internals; pinned to the fork wegamekinglc/Adept-2 (submodule, 1e29edc).
        void EnsureGradientCapacity() {
            if (!gradients_are_initialized()) {
                initialize_gradients();
                return;
            }
#ifdef ADEPT_STACK_STORAGE_STL
            if (gradient_.size() < max_gradient_)
                gradient_.resize(max_gradient_ + 10, 0.0);
#else
            if (max_gradient_ <= n_allocated_gradients_)
                return;
            auto* grown = new adept::Real[max_gradient_];
            if (n_allocated_gradients_ > 0)
                std::copy(gradient_, gradient_ + n_allocated_gradients_, grown);
            std::fill(grown + n_allocated_gradients_, grown + max_gradient_, 0.0);
            delete[] gradient_;
            gradient_ = grown;
            n_allocated_gradients_ = max_gradient_;
#endif
        }

        void compute_adjoint(adept::uIndex fromStatement, adept::uIndex toStatement) {
            if (!gradients_are_initialized())
                THROW("Adept gradients are not initialized");

            EnsureGradientCapacity();

            for (adept::uIndex ist = fromStatement; ist > toStatement && ist > 1; --ist) {
                const adept::uIndex statementIndex = ist - 1;
                const auto& statement = statement_[statementIndex];
                adept::Real adjoint = gradient_[statement.index];
                gradient_[statement.index] = 0.0;
                if (adjoint != 0.0) {
                    for (adept::uIndex i = statement_[statementIndex - 1].end_plus_one; i < statement.end_plus_one; ++i)
                        gradient_[index_[i]] += multiplier_[i] * adjoint;
                }
            }
        }

        void ZeroGradientArray() { initialize_gradients(); }
    };

} // namespace Dal::AAD
#elif defined(DAL_USE_XAD_AAD)
#include <XAD/XAD.hpp>

namespace Dal::AAD {

    class Tape_ {
    public:
        using tape_type = xad::adj<double>::tape_type;
        tape_type tape_;
        tape_type::position_type start_;
        tape_type::position_type mark_;

        explicit Tape_(bool activate = true) : tape_(activate), start_(tape_.getPosition()), mark_(start_) { }
    };

} // namespace Dal::AAD
#elif defined(DAL_USE_CODIPACK_AAD)
#include <codi.hpp>

namespace Dal::AAD {

    class Tape_ {
    public:
        using active_type = codi::RealReverseUnchecked;
        using tape_type = typename active_type::Tape;
        using position_type = typename tape_type::Position;

        tape_type& tape_;
        position_type start_;
        position_type mark_;

        explicit Tape_(bool activate = true) : tape_(active_type::getTape()), start_(tape_.getPosition()), mark_(start_) {
            if (activate)
                tape_.setActive();
        }
    };

} // namespace Dal::AAD

#endif

namespace Dal::AAD {
    void Clear(Tape_& tape);
    void Mark(Tape_& tape);
    void RewindToMark(Tape_& tape);
    void Rewind(Tape_& tape);
    void PropagateMarkToStart(Tape_& tape);
    void PropagateToStart(Tape_& tape);
    void PropagateToMark(Tape_& tape);
    void NewRecording(Tape_& tape);
    void Activate(Tape_& tape);
    void Deactivate(Tape_& tape);
} // namespace Dal::AAD
