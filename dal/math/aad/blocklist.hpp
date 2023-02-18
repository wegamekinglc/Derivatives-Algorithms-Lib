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

        iterator curr_block_;
        iterator last_block_;

        block_iter next_space_;
        block_iter last_space_;

        iterator marked_block_;
        block_iter marked_space_;

        void NewBlock() {
            data_.emplace_back();
            curr_block_ = last_block_ = std::prev(data_.end());
            next_space_ = curr_block_->begin();
            last_space_ = curr_block_->end();
        }

        void NextBlock() {
            if (curr_block_ == last_block_)
                NewBlock();
            else {
                ++curr_block_;
                next_space_ = curr_block_->begin();
                last_space_ = curr_block_->end();
            }
        }

    public:
        struct BlockPosition_ {
            iterator curr_block_;
            block_iter next_space_;

            BlockPosition_() = default;
            BlockPosition_(iterator curr_block, block_iter next_space): curr_block_(curr_block), next_space_(next_space) {}
        };

        BlockList_() { NewBlock(); }

        void Clear() {
            data_.clear();
            NewBlock();
        }

        void Rewind() {
            curr_block_ = data_.begin();
            next_space_ = curr_block_->begin();
            last_space_ = curr_block_->end();
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
            if (next_space_ == last_space_)
                NextBlock();
            T_* emplaced = new (&*next_space_) T_(std::forward<Args_>(args)...);
            ++next_space_;
            return emplaced;
        }

        T_* EmplaceBack() {
            if (next_space_ == last_space_)
                NextBlock();
            auto old_next = next_space_;
            ++next_space_;
            return &*old_next;
        }

        template <size_t N_> T_* EmplaceBackMulti() {
            if (std::distance(next_space_, last_space_) < static_cast<int>(N_))
                NextBlock();
            auto old_next = next_space_;
            next_space_ += N_;
            return &*old_next;
        }

        T_* EmplaceBackMulti(const size_t& n) {
            if (std::distance(next_space_, last_space_) < static_cast<int>(n))
                NextBlock();
            auto old_next = next_space_;
            next_space_ += n;
            return &*old_next;
        }

        void SetMark() {
            if (next_space_ == last_space_)
                NextBlock();
            marked_block_ = curr_block_;
            marked_space_ = next_space_;
        }

        auto GetPosition() {
            if (next_space_ == last_space_)
                NextBlock();
            return Iterator_(curr_block_, next_space_, curr_block_->begin(), curr_block_->end());
        }

        auto GetPosition() const {
            if (next_space_ == last_space_)
                NextBlock();
            return ConstIterator_(curr_block_, next_space_, curr_block_->begin(), curr_block_->end());
        }

        auto GetZeroPosition() {
            return Iterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        auto GetZeroPosition() const {
            return ConstIterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        void RewindToMark() {
            curr_block_ = marked_block_;
            next_space_ = marked_space_;
            last_space_ = curr_block_->end();
        }



        class Iterator_ {
        public:
            iterator curr_block_;
            block_iter curr_space_;
            block_iter first_space_;
            block_iter last_space_;

            using difference_type = std::ptrdiff_t;
            using reference = T_&;
            using pointer = T_*;
            using value_type = T_;
            using iterator_category = std::bidirectional_iterator_tag;

            Iterator_() = default;
            Iterator_(iterator cb, block_iter cs, block_iter fs, block_iter ls)
                : curr_block_(cb), curr_space_(cs), first_space_(fs), last_space_(ls) {}

            Iterator_& operator++() {
                ++curr_space_;
                if (curr_space_ == last_space_) {
                    ++curr_block_;
                    first_space_ = curr_block_->begin();
                    last_space_ = curr_block_->end();
                    curr_space_ = first_space_;
                }
                return *this;
            }

            inline const Iterator_& operator++() const {
                return this->operator++();
            }

            Iterator_& operator--() {
                if (curr_space_ == first_space_) {
                    --curr_block_;
                    first_space_ = curr_block_->begin();
                    last_space_ = curr_block_->end();
                    curr_space_ = last_space_;
                }
                --curr_space_;
                return *this;
            }

            inline const Iterator_& operator--() const {
                return this->operator--();
            }

            T_& operator*() { return *curr_space_; }

            const T_& operator*() const { return *curr_space_; }

            T_* operator->() { return &*curr_space_; }

            const T_* operator->() const { return &*curr_space_; }

            bool operator==(const Iterator_& rhs) {
                return curr_block_ == rhs.curr_block_ && curr_space_ == rhs.curr_space_;
            }

            bool operator!=(const Iterator_& rhs) {
                return curr_block_ != rhs.curr_block_ || curr_space_ != rhs.curr_space_;
            }
        };

        class ConstIterator_ {
        public:
            const_iterator curr_block_;
            const_block_iter curr_space_;
            const_block_iter first_space_;
            const_block_iter last_space_;

            using difference_type = std::ptrdiff_t;
            using reference = const T_&;
            using pointer = const T_*;
            using value_type = T_;
            using iterator_category = std::bidirectional_iterator_tag;

            ConstIterator_() = default;
            ConstIterator_(const_iterator cb, const_block_iter cs, const_block_iter fs, const_block_iter ls)
                : curr_block_(cb), curr_space_(cs), first_space_(fs), last_space_(ls) {}

            ConstIterator_& operator++() {
                ++curr_space_;
                if (curr_space_ == last_space_) {
                    ++curr_block_;
                    first_space_ = curr_block_->begin();
                    last_space_ = curr_block_->end();
                    curr_space_ = first_space_;
                }
                return *this;
            }

            ConstIterator_& operator--() {
                if (curr_space_ == first_space_) {
                    --curr_block_;
                    first_space_ = curr_block_->begin();
                    last_space_ = curr_block_->end();
                    curr_space_ = last_space_;
                }
                --curr_space_;
                return *this;
            }

            const T_& operator*() const { return *curr_space_; }
            const T_* operator->() const { return &*curr_space_; }

            bool operator==(const ConstIterator_& rhs) {
                return curr_block_ == rhs.curr_block_ && curr_space_ == rhs.curr_space_;
            }

            bool operator!=(const ConstIterator_& rhs) {
                return curr_block_ != rhs.curr_block_ || curr_space_ != rhs.curr_space_;
            }
        };

        Iterator_ Begin() {
            return Iterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        inline Iterator_ begin() {
            return Begin();
        }

        Iterator_ End() { return Iterator_(curr_block_, next_space_, curr_block_->begin(), curr_block_->end()); }

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
            return ConstIterator_(curr_block_, next_space_, curr_block_->begin(), curr_block_->end());
        }

        inline ConstIterator_ end() const {
            return End();
        }

        Iterator_ Mark() {
            return Iterator_(marked_block_, marked_space_, marked_block_->begin(), marked_block_->end());
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
            curr_block_ = position.curr_block_;
            next_space_ = position.curr_space_;
            last_space_ = curr_block_->end();
        }
    };
} // namespace Dal
