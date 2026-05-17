#include <config.hpp>
#include <cassert>
#include <cstdio>

static const char* TEST_TOML = R"(
[server]
host = "127.0.0.1"
port = 8080

[database]
enabled = true
timeout = 3.5
connection_limit = 100

[plugins]
names = ["auth", "logger", "metrics"]
versions = [1, 2, 3]

[empty_section]
enabled = false
)";

int main() {
    auto cfg = tangbase::Config::parse_string(TEST_TOML);
    (void)cfg;

    // String
    auto host = cfg.get_string("server.host");
    assert(host.has_value());
    assert(*host == "127.0.0.1");

    // Int
    auto port = cfg.get_int("server.port");
    assert(port.has_value());
    assert(*port == 8080);

    // Bool
    auto enabled = cfg.get_bool("database.enabled");
    assert(enabled.has_value());
    assert(*enabled == true);

    // Double
    auto timeout = cfg.get_double("database.timeout");
    assert(timeout.has_value());
    assert(*timeout > 3.4 && *timeout < 3.6);

    // Array by index
    auto plugin0 = cfg.get_string("plugins.names[0]");
    assert(plugin0.has_value());
    assert(*plugin0 == "auth");

    auto ver2 = cfg.get_int("plugins.versions[2]");
    assert(ver2.has_value());
    assert(*ver2 == 3);

    // Default value
    auto missing = cfg.get_string_or("server.nonexistent", "default_val");
    assert(missing == "default_val");

    auto missing_int = cfg.get_int_or("server.missing", 999);
    assert(missing_int == 999);

    // Non-existent path
    assert(!cfg.get_string("nonexistent.key").has_value());
    assert(!cfg.get_string("server.host[0]").has_value());

    std::printf("Config tests passed.\n");
    return 0;
}