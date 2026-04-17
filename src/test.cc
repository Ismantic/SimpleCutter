#include <iostream>
#include <string>
#include <vector>

#include "cut.h"

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) r += sep;
        r += v[i];
    }
    return r;
}

int passed = 0;
int failed = 0;

void check(bool cond, const std::string& name) {
    if (cond) {
        passed++;
    } else {
        failed++;
        std::cout << "FAIL: " << name << std::endl;
    }
}

void test_chinese_cut() {
    cut::Cutter cutter;
    cutter.Build(
        {"\xe5\x8d\x97\xe4\xba\xac",
         "\xe5\x8d\x97\xe4\xba\xac\xe5\xb8\x82",
         "\xe5\xb8\x82\xe9\x95\xbf",
         "\xe9\x95\xbf\xe6\xb1\x9f",
         "\xe5\xa4\xa7\xe6\xa1\xa5",
         "\xe6\xb1\x9f\xe5\xa4\xa7\xe6\xa1\xa5",
         "\xe9\x95\xbf\xe6\xb1\x9f\xe5\xa4\xa7\xe6\xa1\xa5"},
        {100, 80, 50, 90, 60, 30, 120});

    auto rs = cutter.Cut(
        "\xe5\x8d\x97\xe4\xba\xac\xe5\xb8\x82"
        "\xe9\x95\xbf\xe6\xb1\x9f\xe5\xa4\xa7\xe6\xa1\xa5");
    std::string result = join(rs, "/");
    std::cout << "chinese: " << result << std::endl;
    check(rs.size() >= 2 && rs.size() <= 4, "chinese cut");
}

void test_no_dict() {
    cut::Cutter cutter;
    // No Build() — Han should be split into individual characters
    auto rs = cutter.Cut("你好世界");
    std::string result = join(rs, "/");
    std::cout << "no dict: " << result << std::endl;
    check(result == "你/好/世/界", "no dict");
}

void test_mixed_cut() {
    cut::Cutter cutter;
    cutter.Build(
        {"他", "是", "英国", "英国人", "人"},
        {50, 40, 80, 100, 30});

    auto rs = cutter.Cut("他是英国人Tom");
    std::string result = join(rs, "/");
    std::cout << "mixed: " << result << std::endl;
    check(result == "他/是/英国人/Tom", "mixed cut");
}

void test_punct() {
    cut::Cutter cutter;
    cutter.Build({"你好", "世界"}, {50, 50});

    auto rs = cutter.Cut("你好，世界！");
    std::string result = join(rs, "/");
    std::cout << "punct: " << result << std::endl;
    check(result == "你好/，/世界/！", "punct cut");
}

int main() {
    test_chinese_cut();
    test_no_dict();
    test_mixed_cut();
    test_punct();
    std::cout << "\nPassed: " << passed << ", Failed: " << failed << std::endl;
    return failed > 0 ? 1 : 0;
}
