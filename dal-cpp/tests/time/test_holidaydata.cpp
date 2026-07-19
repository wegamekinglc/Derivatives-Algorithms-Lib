#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include <dal/platform/platform.hpp>
#include <dal/string/strings.hpp>
#include <dal/time/date.hpp>
#include <dal/time/holidaydata.hpp>

using namespace Dal;

namespace {
    String_ ConcurrentCenterName(int run, int batch, int writer) {
        return String_("CONCURRENT_HOLIDAY_") + String::FromInt(run) + "_" + String::FromInt(batch) + "_" + String::FromInt(writer);
    }

    Vector_<Date_> LargeHolidaySet() {
        Vector_<Date_> result;
        for (Date_ date(2020, 1, 1); date <= Date::Maximum(); ++date) {
            if (!Date::IsWeekEnd(date))
                result.push_back(date);
        }
        return result;
    }
} // namespace

TEST(HolidayDataTest, TestConcurrentAddCenterPreservesAllCenters) {
    constexpr int NUM_BATCHES = 4;
    constexpr int NUM_WRITERS = 32;
    static std::atomic<int> nextRun{0};
    const int run = nextRun.fetch_add(1);
    const Vector_<Date_> holidays = LargeHolidaySet();

    for (int batch = 0; batch < NUM_BATCHES; ++batch) {
        std::mutex gateMutex;
        std::condition_variable gateCv;
        int ready = 0;
        bool start = false;
        std::vector<std::exception_ptr> errors(NUM_WRITERS);
        std::vector<std::thread> writers;
        writers.reserve(NUM_WRITERS);

        for (int writer = 0; writer < NUM_WRITERS; ++writer) {
            writers.emplace_back([&, writer]() {
                {
                    std::unique_lock<std::mutex> lock(gateMutex);
                    ++ready;
                    gateCv.notify_all();
                    gateCv.wait(lock, [&]() { return start; });
                }
                try {
                    Holidays::AddCenter(ConcurrentCenterName(run, batch, writer), holidays);
                } catch (...) {
                    errors[writer] = std::current_exception();
                }
            });
        }

        {
            std::unique_lock<std::mutex> lock(gateMutex);
            gateCv.wait(lock, [&]() { return ready == NUM_WRITERS; });
            start = true;
        }
        gateCv.notify_all();

        for (auto& writer : writers)
            writer.join();

        for (const auto& error : errors)
            ASSERT_FALSE(error);
        for (int writer = 0; writer < NUM_WRITERS; ++writer) {
            const String_ name = ConcurrentCenterName(run, batch, writer);
            Handle_<HolidayCenterData_> center;
            ASSERT_NO_THROW(center = Holidays::OfCenter(name));
            ASSERT_TRUE(center);
            ASSERT_EQ(center->center_, name);
            ASSERT_EQ(center->holidays_, holidays);
        }
    }
}
