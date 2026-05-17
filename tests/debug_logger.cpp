#include "logger.hpp"
#include <iostream>
#include <filesystem>
int main() {
    std::filesystem::create_directories("test_logs/simple");
    std::cout << "after create_directories" << std::endl;
    tangbase::Logger::outconsole();
    std::cout << "after outconsole" << std::endl;
    tangbase::Logger::RotationOptions opts;
    opts.rotation_interval_ms = 1000;
    opts.max_files = 5;
    std::cout << "before set_rotation" << std::endl;
    tangbase::Logger::set_rotation("test_logs/simple/app_%Y-%m-%d.log", opts);
    std::cout << "after set_rotation" << std::endl;
    tangbase::log_info("test message");
    std::cout << "after log_info" << std::endl;
    std::cout << std::flush;
    return 0;
}
