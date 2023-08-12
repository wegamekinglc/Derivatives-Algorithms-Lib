//
// Created by wegam on 2023/7/21.
//

#include <iostream>
#include <thread>
#include <dal/platform/strict.hpp>
#include <dal/platform/initall.hpp>
#include <dal/time/calendars/init.hpp>
#include <dal/currency/init.hpp>
#include <dal/indice/parser/init.hpp>


namespace Dal {

    bool RegisterAll_::init_ = false;
    std::mutex RegisterAll_::mutex_;

    RegisterAll_::RegisterAll_() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            std::cout << "starting DAL with: " << std::thread::hardware_concurrency() << " threads." << std::endl;
            std::cout << "starting initialization global data ..." << std::endl;
            Calendars_::Init();
            CcyFacts_::Init();
            IndexParsers_::Init();
            init_ = true;
            std::cout << "finished initialization global data." << std::endl;
        }
    }

}
