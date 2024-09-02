//
// Created by wegam on 2023/7/21.
//

#include "dal/concurrency/threadpool.hpp"

#include <dal/currency/init.hpp>
#include <dal/indice/parser/init.hpp>
#include <dal/math/aad/aad.hpp>
#include <dal/platform/initall.hpp>
#include <dal/platform/strict.hpp>
#include <dal/time/calendars/init.hpp>
#include <iostream>
#include <thread>

namespace Dal {

    bool RegisterAll_::init_ = false;
    std::mutex RegisterAll_::mutex_{};

    RegisterAll_::RegisterAll_() {
        std::lock_guard<std::mutex> l(mutex_);
        if (!init_) {
            ThreadPool_::GetInstance()->Start(0, true);
            std::cout << "starting DAL with: " << ThreadPool_::GetInstance()->NumThreads() << " threads." << std::endl;
            std::cout << "use AAD framework: " << "AADET" << std::endl;

            std::cout << "starting initialization global data ..." << std::endl;
            Calendars_::Init();
            CcyFacts_::Init();
            IndexParsers_::Init();

            std::cout << "stating initialization global tape ..." << std::endl;
            static AAD::Tape_ tape;
            AAD::Number_::SetTape(tape);

            init_ = true;
            std::cout << "finished initialization all the global information." << std::endl;
        }
    }

}
