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

#include <array>
#include <cstring>
#include <iterator>
#include <list>

namespace Dal::AAD {

    template <class T_, size_t BLOCK_SIZE_> class BlockList_ {
    private:
        std::list<std::array<T_, BLOCK_SIZE_>> data_;
        using iterator = typename std::list<std::array<T_, BLOCK_SIZE_>>::iterator;
        using const_iterator = typename std::list<std::array<T_, BLOCK_SIZE_>>::const_iterator;
        using block_iter = typename std::array<T_, BLOCK_SIZE_>::iterator;
        using const_block_iter = typename std::array<T_, BLOCK_SIZE_>::const_iterator;

        iterator currBlock_;
        iterator lastBlock_;

        block_iter nextSpace_;
        block_iter lastSpace_;

        iterator markedBlock_;
        block_iter markedSpace_;

        void NewBlock() {
            data_.emplace_back();
            currBlock_ = lastBlock_ = std::prev(data_.end());
            nextSpace_ = currBlock_->begin();
            lastSpace_ = currBlock_->end();
        }

        void NextBlock() {
            if (currBlock_ == lastBlock_)
                NewBlock();
            else {
                ++currBlock_;
                nextSpace_ = currBlock_->begin();
                lastSpace_ = currBlock_->end();
            }
        }

    public:
        struct BlockPosition_ {
            iterator currBlock_;
            block_iter nextSpace_;

            BlockPosition_() = default;
            BlockPosition_(iterator curr_block, block_iter next_space): currBlock_(curr_block), nextSpace_(next_space) {}
        };

        BlockList_() { NewBlock(); }

        void Clear() {
            data_.clear();
            NewBlock();
        }

        void Rewind() {
            currBlock_ = data_.begin();
            nextSpace_ = currBlock_->begin();
            lastSpace_ = currBlock_->end();
        }

        [[nodiscard]] int Size() const {
            int count = 0;
            for(ConstIterator_ it = this->Begin(); it != this->End(); ++it)
                count += 1;
            return count;
        }

        void Memset(unsigned char val) {
            for (auto& arr : data_)
                std::memset(&arr[0], val, BLOCK_SIZE_ * sizeof(T_));
        }

        template <typename... Args_> T_* EmplaceBack(Args_&&... args) {
            if (nextSpace_ == lastSpace_)
                NextBlock();
            T_* emplaced = new (&*nextSpace_) T_(std::forward<Args_>(args)...);
            ++nextSpace_;
            return emplaced;
        }

        T_* EmplaceBack() {
            if (nextSpace_ == lastSpace_)
                NextBlock();
            auto old_next = nextSpace_;
            ++nextSpace_;
            return &*old_next;
        }

        template <size_t N_> T_* EmplaceBackMulti() {
            if (std::distance(nextSpace_, lastSpace_) < static_cast<int>(N_))
                NextBlock();
            auto old_next = nextSpace_;
            nextSpace_ += N_;
            return &*old_next;
        }

        T_* EmplaceBackMulti(const size_t& n) {
            if (std::distance(nextSpace_, lastSpace_) < static_cast<int>(n))
                NextBlock();
            auto old_next = nextSpace_;
            nextSpace_ += n;
            return &*old_next;
        }

        void SetMark() {
            if (nextSpace_ == lastSpace_)
                NextBlock();
            markedBlock_ = currBlock_;
            markedSpace_ = nextSpace_;
        }

        auto GetPosition() {
            if (nextSpace_ == lastSpace_)
                NextBlock();
            return Iterator_(currBlock_, nextSpace_, currBlock_->begin(), currBlock_->end());
        }

        auto GetPosition() const {
            if (nextSpace_ == lastSpace_)
                NextBlock();
            return ConstIterator_(currBlock_, nextSpace_, currBlock_->begin(), currBlock_->end());
        }

        auto GetZeroPosition() {
            return Iterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        auto GetZeroPosition() const {
            return ConstIterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        void RewindToMark() {
            currBlock_ = markedBlock_;
            nextSpace_ = markedSpace_;
            lastSpace_ = currBlock_->end();
        }



        class Iterator_ {
        public:
            iterator currBlock_;
            block_iter currSpace_;
            block_iter firstSpace_;
            block_iter lastSpace_;

            using difference_type = std::ptrdiff_t;
            using reference = T_&;
            using pointer = T_*;
            using value_type = T_;
            using iterator_category = std::bidirectional_iterator_tag;

            Iterator_() = default;
            Iterator_(iterator cb, block_iter cs, block_iter fs, block_iter ls)
                : currBlock_(cb), currSpace_(cs), firstSpace_(fs), lastSpace_(ls) {}

            Iterator_& operator++() {
                ++currSpace_;
                if (currSpace_ == lastSpace_) {
                    ++currBlock_;
                    firstSpace_ = currBlock_->begin();
                    lastSpace_ = currBlock_->end();
                    currSpace_ = firstSpace_;
                }
                return *this;
            }

            inline const Iterator_& operator++() const {
                return this->operator++();
            }

            Iterator_& operator--() {
                if (currSpace_ == firstSpace_) {
                    --currBlock_;
                    firstSpace_ = currBlock_->begin();
                    lastSpace_ = currBlock_->end();
                    currSpace_ = lastSpace_;
                }
                --currSpace_;
                return *this;
            }

            inline const Iterator_& operator--() const {
                return this->operator--();
            }

            T_& operator*() { return *currSpace_; }

            const T_& operator*() const { return *currSpace_; }

            T_* operator->() { return &*currSpace_; }

            const T_* operator->() const { return &*currSpace_; }

            bool operator==(const Iterator_& rhs) {
                return currBlock_ == rhs.currBlock_ && currSpace_ == rhs.currSpace_;
            }

            bool operator!=(const Iterator_& rhs) {
                return currBlock_ != rhs.currBlock_ || currSpace_ != rhs.currSpace_;
            }
        };

        class ConstIterator_ {
        public:
            const_iterator currBlock_;
            const_block_iter currSpace_;
            const_block_iter firstSpace_;
            const_block_iter lastSpace_;

            using difference_type = std::ptrdiff_t;
            using reference = const T_&;
            using pointer = const T_*;
            using value_type = T_;
            using iterator_category = std::bidirectional_iterator_tag;

            ConstIterator_() = default;
            ConstIterator_(const_iterator cb, const_block_iter cs, const_block_iter fs, const_block_iter ls)
                : currBlock_(cb), currSpace_(cs), firstSpace_(fs), lastSpace_(ls) {}

            ConstIterator_& operator++() {
                ++currSpace_;
                if (currSpace_ == lastSpace_) {
                    ++currBlock_;
                    firstSpace_ = currBlock_->begin();
                    lastSpace_ = currBlock_->end();
                    currSpace_ = firstSpace_;
                }
                return *this;
            }

            ConstIterator_& operator--() {
                if (currSpace_ == firstSpace_) {
                    --currBlock_;
                    firstSpace_ = currBlock_->begin();
                    lastSpace_ = currBlock_->end();
                    currSpace_ = lastSpace_;
                }
                --currSpace_;
                return *this;
            }

            const T_& operator*() const { return *currSpace_; }
            const T_* operator->() const { return &*currSpace_; }

            bool operator==(const ConstIterator_& rhs) {
                return currBlock_ == rhs.currBlock_ && currSpace_ == rhs.currSpace_;
            }

            bool operator!=(const ConstIterator_& rhs) {
                return currBlock_ != rhs.currBlock_ || currSpace_ != rhs.currSpace_;
            }
        };

        Iterator_ Begin() {
            return Iterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        inline Iterator_ begin() {
            return Begin();
        }

        Iterator_ End() { return Iterator_(currBlock_, nextSpace_, currBlock_->begin(), currBlock_->end()); }

        inline Iterator_ end() {
            return End();
        }

        ConstIterator_ Begin() const {
            return ConstIterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        inline ConstIterator_ begin() const {
            return Begin();
        }

        ConstIterator_ End() const {
            return ConstIterator_(currBlock_, nextSpace_, currBlock_->begin(), currBlock_->end());
        }

        inline ConstIterator_ end() const {
            return End();
        }

        Iterator_ Mark() {
            return Iterator_(markedBlock_, markedSpace_, markedBlock_->begin(), markedBlock_->end());
        }

        Iterator_ Find(const T_* const element) {
            Iterator_ it = End();
            Iterator_ b = Begin();

            while (it != b) {
                --it;
                if (&*it == element)
                    return it;
            }

            if (&*it == element)
                return it;

            return End();
        }

        void RewindTo(const Iterator_& position) {
            currBlock_ = position.currBlock_;
            nextSpace_ = position.currSpace_;
            lastSpace_ = currBlock_->end();
        }
    };
} // namespace Dal
