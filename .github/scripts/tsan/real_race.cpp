//
// Created by Codex on 2026/10/16.
//

#include <iostream>
#include <thread>

int main() {
    // Intentional race: the runtime check must still report unsynchronized writes.
    int counter = 0;
    std::thread first([&counter] { ++counter; });
    std::thread second([&counter] { ++counter; });
    first.join();
    second.join();
    std::cout << counter << '\n';
}
