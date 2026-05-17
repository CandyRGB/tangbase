# tangbase — C++ 基础库

tangbase 是一个纯头文件的 C++ 基础库，定位为"快速搭建项目基础"。所有组件均无依赖、互不干扰，直接引入即可使用。

**要求：C++17 或更高版本**

---

## 目录结构

```
src/
  assert.hpp       — 断言与 panic
  config.hpp       — TOML 配置读取
  datetime.hpp     — 日期时间工具
  logger.hpp       — 日志输出
  result.hpp       — Result / Option 类型
  sync.hpp         — 同步原语（Latch、信号量）
  thread_pool.hpp  — 线程池
  util/
    file.hpp       — 文件读写
    format.hpp     — 格式化字符串
    string.hpp     — 字符串工具
    toml.hpp       — TOML 解析器
```

---

## 组件概览

### assert.hpp — 断言与 panic

```cpp
#include <assert.hpp>

tangbase::panic("something went wrong");  // 打印到 stderr，终止程序

TB_CHECK(true);                            // 编译期断言，条件为 false 则编译失败

TB_ASSERT(x > 0, "x must be positive");    // 运行时断言，Debug 和 Release 均生效
```

**注意**：`TB_ASSERT` 在 Release 模式下不会禁用，慎用于正常业务流程。

---

### result.hpp — Result / Option 类型

类似 Rust 的错误处理模式，用于替代异常。

```cpp
#include <result.hpp>

using tangbase::Result;
using tangbase::Option;

// Result<T, E> — 表示成功（T）或失败（E）
Result<int, std::error_code> divide(int a, int b) {
    if (b == 0) return Result<int, std::error_code>::err(
        std::make_error_code(std::errc::invalid_argument));
    return Result<int, std::error_code>::ok(a / b);
}

auto r = divide(10, 2);
if (r.is_ok()) {
    std::cout << *r.ok() << "\n";       // 成功值
}
if (r.is_err()) {
    std::cout << *r.err() << "\n";      // 错误值
}

// 组合器
auto r2 = r.map([](int v) { return v * 2; });       // 变换成功值
auto r3 = r.map_err([](auto e) { return -1; });      // 变换错误值

// and_then / or_else — 链式调用，适合 early return 风格
auto r4 = r.and_then([](int v) -> Result<int, int> {
    if (v > 100) return Result<int, int>::err(999);
    return Result<int, int>::ok(v + 1);
});

// unwrap_or — 获取成功值，失败时返回默认值
int val = r.unwrap_or(0);

// Option<T> — 别名 std::optional<T>
Option<int> opt = 42;
if (opt.has_value()) std::cout << *opt << "\n";
std::cout << opt.value_or(99) << "\n";   // 99
```

---

### sync.hpp — 同步原语

**Latch** — 一次性 barrier，所有线程到达后门打开

```cpp
#include <sync.hpp>

tangbase::Latch latch(3);  // 等待 3 次 arrive()

std::thread t1([&] { latch.arrive(); });
std::thread t2([&] { latch.arrive(); });
std::thread t3([&] { latch.arrive(); });

t1.join(); t2.join(); t3.join();
latch.wait();  // 不阻塞，立即通过
```

**BinarySemaphore** — 二态信号量（初始 available=true 时相当于互斥锁）

```cpp
tangbase::BinarySemaphore sem(true);

sem.acquire();          // 获取，阻塞直到 available
bool ok = sem.try_acquire();  // 非阻塞尝试，返回 true/false
sem.release();          // 释放，唤醒一个等待者
```

**CountingSemaphore** — N 计数信号量，用于限制并发资源数

```cpp
tangbase::CountingSemaphore sem(5, 5);  // 最多 5 个并发

sem.acquire();          // 获取一个名额，阻塞直到 count > 0
bool ok = sem.try_acquire();  // 非阻塞
sem.release();          // 归还名额
```

---

### thread_pool.hpp — 线程池

```cpp
#include <thread_pool.hpp>

tangbase::ThreadPool pool(4);  // 4 个工作线程，默认用 CPU 核心数

// enqueue 返回 std::optional<std::future<T>>
auto f = pool.enqueue([] { return 42; });
if (f) std::cout << f->get() << "\n";  // 打印 42

// 成员函数作为任务
struct Task { int run(int x) { return x * 2; } };
Task t;
auto f2 = pool.enqueue(&Task::run, &t, 10);
if (f2) std::cout << f2->get() << "\n";  // 打印 20

pool.shutdown();  // 停止接受新任务
pool.join();     // 等待所有任务完成
```

**注意**：
- shutdown 后 enqueue 返回 `std::nullopt`，已入队的任务会继续执行
- 任务抛出的异常会被捕获，不会导致工作线程终止
- 任务参数被移动到堆上，线程池不持有引用

---

### logger.hpp — 日志

```cpp
#include <logger.hpp>

// 输出到控制台（默认）
tangbase::Logger log(tangbase::Target::cout);
log.info("server started on port {}", 8080);

// 输出到文件（追加模式，文件不存在会创建）
tangbase::Logger flog(tangbase::Target::file, "app.log");
flog.debug("debug message");
flog.error("something failed: {}", err_code);

// 也支持直接传字符串（不做格式化）
log.info("plain message");
```

日志格式：`[LEVEL] [timestamp] message`

输出示例：
```
[INFO ] [2026-05-14 10:30:00] server started on port 8080
[ERROR] [2026-05-14 10:30:05] something failed: 500
```

---

### config.hpp — TOML 配置读取

```cpp
#include <config.hpp>

// 从文件加载
auto cfg = tangbase::Config::parse_file("config.toml");

// 或从字符串解析
auto cfg2 = tangbase::Config::parse_string(R"(
[server]
host = "127.0.0.1"
port = 8080

[database]
enabled = true
timeout = 3.5

[plugins]
names = ["auth", "logger", "metrics"]
)");

// 读取各类型值
auto host = cfg.get_string("server.host");         // std::optional<std::string>
auto port = cfg.get_int("server.port");            // std::optional<int64_t>
auto timeout = cfg.get_double("database.timeout"); // std::optional<double>
auto enabled = cfg.get_bool("database.enabled");   // std::optional<bool>

// 数组访问
auto name0 = cfg.get_string("plugins.names[0]");   // "auth"

// 带默认值（路径不存在时返回默认值）
auto missing = cfg.get_string_or("server.nonexistent", "default");
int port2 = cfg.get_int_or("server.missing", 80);
```

路径语法：`section.key` 或 `section.array[n]`，支持嵌套如 `a.b.c[0].d`。

---

### datetime.hpp — 日期时间

```cpp
#include <datetime.hpp>

auto now = tangbase::DateTime::now();

// 预定义格式
std::cout << now.to_iso8601() << "\n";   // 2026-05-14T10:30:00
std::cout << now.to_datetime() << "\n";  // 2026-05-14 10:30:00
std::cout << now.to_date() << "\n";      // 2026-05-14
std::cout << now.to_time() << "\n";      // 10:30:00

// 自定义格式
std::cout << now.format("%Y/%m/%d %H:%M") << "\n";  // 2026/05/14 10:30

// 相对时间
std::cout << now.ago() << "\n";  // "0s ago"

// 从 time_t 构造
time_t ts = 1747200000;
tangbase::DateTime dt(ts);
std::cout << dt.to_datetime() << "\n";
```

---

### util/format.hpp — 格式化

Python 风格的 `{}` 占位符格式化，不依赖 C++20。

```cpp
#include <util/format.hpp>

std::string s = tangbase::format("Hello {}! Value={}", "world", 42);
// "Hello world! Value=42"

// 转义大括号
std::string s2 = tangbase::format("{{literal}} {}");  // "{literal} value"

// 支持类型：int, long, long long, unsigned, unsigned long,
//          float, double, bool, char, const char*, std::string_view

// 抛出 tangbase::format_error（继承 std::exception）
try {
    auto bad = tangbase::format("{} {}", 1);  // 参数不足
} catch (const tangbase::format_error& e) {
    std::cout << e.what() << "\n";
}
```

---

### util/string.hpp — 字符串工具

```cpp
#include <util/string.hpp>
using namespace tangbase::util;

std::string s = "  hello world  ";

trim(s);              // "hello world"（两端去空白）
trim_start(s);        // "hello world  "（去左端）
trim_end(s);          // "  hello world"（去右端）

split("a,b,c", ',');  // ["a", "b", "c"]
split_any("a,b;c", ",;");  // ["a", "b", "c"]

replace_once("aaa", "aa", "b");   // "ba"
replace_all("aaa", "a", "b");     // "bbb"

to_lower("Hello");    // "hello"
to_upper("Hello");    // "HELLO"

starts_with("hello world", "hello");  // true
ends_with("file.txt", ".txt");        // true
is_blank("   ");                       // true
```

---

### util/file.hpp — 文件读写

```cpp
#include <util/file.hpp>

// 读取文件
auto content = tangbase::util::read_file("data.txt");
if (content.is_ok()) {
    std::string data = *content.ok();
    // ...
} else {
    std::error_code ec = *content.err();
    // 处理错误
}

// 写文件（覆盖）
std::error_code ec = tangbase::util::write_file("out.txt", "hello");
if (ec) { /* 处理错误 */ }

// 追加
ec = tangbase::util::append_file("out.txt", "\nmore");
```

---

## 测试

所有测试位于 `tests/` 目录，运行方式：

```bash
cmake -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

| 测试文件 | 对应模块 |
|---------|---------|
| `test_result.cpp` | result.hpp |
| `test_thread_pool.cpp` | thread_pool.hpp |
| `test_sync.cpp` | sync.hpp |
| `test_assert.cpp` | assert.hpp |
| `test_config.cpp` | config.hpp / util/toml.hpp |
| `test_util.cpp` | util/string.hpp, util/file.hpp, datetime.hpp |