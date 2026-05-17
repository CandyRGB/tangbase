#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

int main() {
    std::filesystem::remove_all("debug_logs");
    std::filesystem::create_directories("debug_logs");

    tangbase::Logger::outconsole();
    tangbase::Logger::RotationOptions opts;
    opts.rotation_interval_ms = 1000;
    opts.max_files = 5;
    tangbase::Logger::set_rotation("debug_logs/app_{}%Y-%m-%d{}.log", opts);

    tangbase::log_info("test message");

    std::cout << "checking..." << std::endl;
    for (auto& e : std::filesystem::directory_iterator("debug_logs")) {
        std::cout << "file: " << e.path().string() << std::endl;
    }

    return 0;
}