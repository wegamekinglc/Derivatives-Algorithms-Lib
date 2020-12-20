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

#include <dal/platform/strict.hpp>
#include <list>
#include <iterator>
#include <array>
#include <cstring>

namespace Dal {

    template <class T_, size_t BLOCK_SIZE_>
    class BlockList_ {
    private:
        std::list<std::array<T_, BLOCK_SIZE_>> data_;
        using list_iter = decltype(data_.begin());
        using block_iter = decltype(data_.back().begin());

        list_iter curr_block_;
        list_iter last_block_;

        block_iter next_space_;
        block_iter last_space_;

        list_iter marked_block_;
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
        BlockList_() { NewBlock();}

        void Clear() {
            data_.clear();
            NewBlock();
        }

        void Rewind() {
            curr_block_ = data_.begin();
            next_space_ = curr_block_->begin();
            last_space_ = curr_block_->end();
        }

        void Memset(unsigned char val) {
            for (auto& arr : data_)
                std::memset(&arr[0], val, BLOCK_SIZE_ * sizeof(T_));
        }

        template <typename ...Args_> T_* EmplaceBack(Args_&& ...args) {
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

        template <size_t N_>
        T_* EmplaceBackMulti() {
            if (std::distance(next_space_, last_space_) < static_cast<int>(N_))
                NewBlock();
            auto old_next = next_space_;
            next_space_ += N_;
            return &*old_next;
        }

        T_* EmplaceBackMulti(const size_t& n) {
            if (std::distance(next_space_, last_space_) < static_cast<int>(n))
                NewBlock();
            auto old_next = next_space_;
            next_space_ += n;
            return &*old_next;
        }

        void SetMark() {
            marked_block_ = curr_block_;
            marked_space_ = next_space_;
        }

        void RewindToMark() {
            curr_block_ = marked_block_;
            next_space_ = marked_space_;
            last_space_ = curr_block_->end();
        }

        class Iterator_ {
        private:
            list_iter curr_block_;
            block_iter curr_space_;
            block_iter first_space_;
            block_iter last_space_;

        public:
            using difference_type = std::ptrdiff_t;
            using reference = T_&;
            using pointer = T_*;
            using value_type = T_;
            using iterator_category = std::bidirectional_iterator_tag;

            Iterator_() {}

            Iterator_(list_iter cb, block_iter cs, block_iter fs, block_iter ls)
                :curr_block_(cb), curr_space_(cs), first_space_(fs), last_space_(ls) {}

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

            T_& operator*() {
                return *curr_space_;
            }

            const T_& operator*() const {
                return *curr_space_;
            }

            T_* operator->() {
                return &*curr_space_;
            }

            const T_* operator->() const {
                return &*curr_space_;
            }

            bool operator==(const Iterator_& rhs) {
                return curr_block_ == rhs.curr_block_ && curr_space_ == rhs.curr_space_;
            }

            bool operator!=(const Iterator_& rhs) {
                return curr_block_ != rhs.curr_block_ || curr_space_ != rhs.curr_space_;
            }
        };

        Iterator_ Begin() {
            return Iterator_(data_.begin(), data_.begin()->begin(), data_.begin()->begin(), data_.begin()->end());
        }

        Iterator_ End() {
            return Iterator_(curr_block_, next_space_, curr_block_->begin(), curr_block_->end());
        }

        Iterator_ Mark() {
            return Iterator_(marked_block_, marked_space_,
                             marked_block_->begin(), marked_block_->end());
        }

        Iterator_ Find(const T_* const element) {
            Iterator_ it = End();
            Iterator_ b = Begin();

            while (it != b) {
                --it;
                if (&*it == element) return it;
            }

            if (&*it == element)
                return it;

            return End();
        }
    };
}
