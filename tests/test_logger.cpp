#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>

using namespace tangbase;

static std::string read_file(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

static int count_lines(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return 0;
    std::string line;
    int n = 0;
    while (std::getline(ifs, line)) ++n;
    return n;
}

int main() {
    std::filesystem::remove_all("test_logs");
    std::filesystem::create_directories("test_logs");

    bool ok = true;

    // Test 1: RotationOptions defaults
    {
        Logger::RotationOptions opts;
        if (opts.rotation_interval_ms != 86400000 || opts.max_files != 10) {
            std::cerr << "[FAIL] defaults" << std::endl;
            ok = false;
        } else {
            std::cout << "[PASS] defaults" << std::endl;
        }
    }

    // Test 2: basic rotation
    {
        Logger::outconsole();
        Logger::RotationOptions opts;
        opts.rotation_interval_ms = 1000;
        opts.max_files = 5;
        Logger::set_rotation("test_logs/basic/app_{}%Y-%m-%d{}.log", opts);

        log_info("test2 message");

        std::filesystem::path parent = "test_logs/basic";
        std::vector<std::filesystem::path> files;
        for (auto& e : std::filesystem::directory_iterator(parent)) {
            if (e.is_regular_file()) files.push_back(e.path());
        }

        if (files.size() == 1) {
            auto name = files[0].filename().string();
            if (name.find("app_") != std::string::npos && name.find(".log") != std::string::npos) {
                std::cout << "[PASS] basic rotation: " << name << std::endl;
            } else {
                std::cerr << "[FAIL] bad filename: " << name << std::endl;
                ok = false;
            }
        } else {
            std::cerr << "[FAIL] basic rotation: got " << files.size() << " files" << std::endl;
            ok = false;
        }
    }

    // Test 3: force rotate (same day → same file, append)
    {
        log_info("before rotate");
        Logger::rotate();
        log_info("after rotate");

        std::filesystem::path parent = "test_logs/basic";
        int count = 0;
        for (auto& e : std::filesystem::directory_iterator(parent)) {
            if (e.is_regular_file()) ++count;
        }
        // 同日轮转不会创建新文件，仍然只有 1 个
        if (count == 1) {
            std::cout << "[PASS] force rotate (same day, " << count << " file)" << std::endl;
        } else {
            std::cerr << "[FAIL] force rotate: expected 1, got " << count << std::endl;
            ok = false;
        }
    }

    // Test 4: outfile
    {
        Logger::outfile("test_logs/normal.log");
        log_info("normal message");

        if (std::filesystem::exists("test_logs/normal.log")) {
            auto content = read_file("test_logs/normal.log");
            if (content.find("normal message") != std::string::npos) {
                std::cout << "[PASS] outfile" << std::endl;
            } else {
                std::cerr << "[FAIL] outfile content" << std::endl;
                ok = false;
            }
        } else {
            std::cerr << "[FAIL] outfile file" << std::endl;
            ok = false;
        }
    }

    // Test 5: outconsole
    {
        Logger::outconsole();
        log_info("console check");
        std::cout << "[PASS] outconsole (check line above)" << std::endl;
    }

    // Test 6: format multi-arg
    {
        Logger::outfile("test_logs/format.log");
        log_info("int={} str={} bool={} double={}", 42, "hello", true, 3.14);

        auto content = read_file("test_logs/format.log");
        if (content.find("42") != std::string::npos &&
            content.find("hello") != std::string::npos &&
            content.find("true") != std::string::npos &&
            content.find("3.14") != std::string::npos) {
            std::cout << "[PASS] format multi-arg" << std::endl;
        } else {
            std::cerr << "[FAIL] format multi-arg" << std::endl;
            ok = false;
        }
    }

    // Test 7: concurrent
    {
        Logger::outfile("test_logs/concurrent.log");
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&, i] {
                for (int j = 0; j < 50; ++j) {
                    log_info("t{} msg{}", i, j);
                }
            });
        }
        for (auto& t : threads) t.join();

        int lines = count_lines("test_logs/concurrent.log");
        if (lines == 200) {
            std::cout << "[PASS] concurrent (" << lines << " lines)" << std::endl;
        } else {
            std::cerr << "[FAIL] concurrent: expected 200, got " << lines << std::endl;
            ok = false;
        }
    }

    // Test 8: update rotation (same day → single file, append)
    {
        Logger::outconsole();
        Logger::RotationOptions opts;
        opts.rotation_interval_ms = 1000;
        opts.max_files = 3;
        Logger::set_rotation("test_logs/update/app_{}%Y-%m-%d{}.log", opts);

        log_info("update 1");
        Logger::rotate();
        log_info("update 2");
        Logger::rotate();
        log_info("update 3");

        std::filesystem::path parent = "test_logs/update";
        int count = 0;
        for (auto& e : std::filesystem::directory_iterator(parent)) {
            if (e.is_regular_file()) ++count;
        }
        // 同日多次 rotate 不会创建新文件
        if (count == 1) {
            std::cout << "[PASS] update rotation (same day, " << count << " file)" << std::endl;
        } else {
            std::cerr << "[FAIL] update rotation: expected 1, got " << count << std::endl;
            ok = false;
        }
    }

    // Test 9: empty messages
    {
        Logger::outconsole();
        log_info("");
        log_debug("");
        log_warn("");
        log_error("");
        std::cout << "[PASS] empty messages" << std::endl;
    }

    std::cout << "\n=== " << (ok ? "All tests passed" : "Some tests FAILED") << " ===" << std::endl;

    std::filesystem::remove_all("test_logs");
    return ok ? 0 : 1;
}