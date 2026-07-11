//
// Created by Codex on 2026/7/11.
//

#include <gtest/gtest.h>

#include <string>
#include <utility>

#include <dal/math/stacks.hpp>
#include <dal/platform/platform.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

TEST(StackTest, TestDynamicStackLifoAndGrowth) {
    Stack_<int, 2> stack;
    stack.Push(10);
    stack.Push(20);
    stack.Push(30);

    ASSERT_EQ(stack.Size(), 3);
    ASSERT_GE(stack.Capacity(), 3);
    ASSERT_EQ(stack.Top(), 30);
    ASSERT_EQ(stack[0], 30);
    ASSERT_EQ(stack[2], 10);
    ASSERT_EQ(stack.TopAndPop(), 30);
    ASSERT_EQ(stack.Top(), 20);
    ASSERT_EQ(stack.Size(), 2);

    stack.Pop(2);
    ASSERT_TRUE(stack.IsEmpty());
}

TEST(StackTest, TestDynamicStackBounds) {
    Stack_<int, 1> stack;
    ASSERT_THROW(stack.Top(), Exception_);
    ASSERT_THROW(stack.TopAndPop(), Exception_);
    ASSERT_THROW(stack.Pop(), Exception_);
    ASSERT_THROW(stack.Pop(1), Exception_);
    ASSERT_THROW(stack[0], Exception_);

    stack.Push(7);
    ASSERT_THROW(stack[1], Exception_);
    stack.Clear();
    stack.Push(11);
    ASSERT_EQ(stack.Top(), 11);
}

TEST(StackTest, TestDynamicStackSelfAliasAtCapacity) {
    Stack_<std::string, 2> stack;
    stack.Push("first");
    stack.Push("second");

    stack.Push(stack[1]);

    ASSERT_EQ(stack.Size(), 3);
    ASSERT_EQ(stack[0], "first");
    ASSERT_EQ(stack[1], "second");
    ASSERT_EQ(stack[2], "first");
}

TEST(StackTest, TestDynamicStackRvalueSelfAliasAtCapacity) {
    Stack_<std::string, 2> stack;
    stack.Push("first");
    stack.Push("second");

    stack.Push(std::move(stack[1]));

    ASSERT_EQ(stack.Size(), 3);
    ASSERT_EQ(stack.Top(), "first");
    ASSERT_EQ(stack[1], "second");
    stack[2] = "reused";
    ASSERT_EQ(stack[2], "reused");
    ASSERT_EQ(stack.Size(), 3);
}

TEST(StackTest, TestStaticStackBounds) {
    StaticStack_<int, 2> stack;
    ASSERT_THROW(stack.Top(), Exception_);
    ASSERT_THROW(stack.TopAndPop(), Exception_);
    ASSERT_THROW(stack.Pop(), Exception_);
    ASSERT_THROW(stack.Pop(1), Exception_);
    ASSERT_THROW(stack[0], Exception_);
    ASSERT_THROW(stack.PopAndTop(), Exception_);

    stack.Push(10);
    stack.Push(20);
    ASSERT_EQ(stack.Top(), 20);
    ASSERT_EQ(stack[1], 10);
    ASSERT_THROW(stack.Push(30), Exception_);
    ASSERT_EQ(stack.TopAndPop(), 20);
    ASSERT_EQ(stack.Top(), 10);
    stack.Pop();
    ASSERT_TRUE(stack.IsEmpty());

    stack.Push(10);
    stack.Push(20);
    ASSERT_EQ(stack.PopAndTop(), 10);
    ASSERT_EQ(stack.Size(), 1);
}

TEST(StackTest, TestStaticStackEmptyScalarCopyAndAssignment) {
    const StaticStack_<double, 2> emptyDoubles;
    const StaticStack_<double, 2> copiedDoubles(emptyDoubles);
    StaticStack_<double, 2> assignedDoubles;
    assignedDoubles = emptyDoubles;

    const StaticStack_<bool, 2> emptyBools;
    const StaticStack_<bool, 2> copiedBools(emptyBools);
    StaticStack_<bool, 2> assignedBools;
    assignedBools = emptyBools;

    ASSERT_TRUE(copiedDoubles.IsEmpty());
    ASSERT_TRUE(assignedDoubles.IsEmpty());
    ASSERT_TRUE(copiedBools.IsEmpty());
    ASSERT_TRUE(assignedBools.IsEmpty());

    assignedDoubles.Push(1.25);
    assignedBools.Push(true);
    ASSERT_DOUBLE_EQ(assignedDoubles.Top(), 1.25);
    ASSERT_TRUE(assignedBools.Top());
}
