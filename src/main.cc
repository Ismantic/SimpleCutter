#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>

#include "cut.h"

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) r += sep;
        r += v[i];
    }
    return r;
}

void LoadDict(const std::string& filename,
              std::vector<std::string>& words,
              std::vector<int>& freqs) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "cannot open: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t pos = line.find('\t');
        if (pos == std::string::npos) continue;

        std::string word = line.substr(0, pos);
        int freq = std::stoi(line.substr(pos + 1));

        words.push_back(word);
        freqs.push_back(freq);
    }
}

void repl(const std::string& dict_file) {
    std::vector<std::string> words;
    std::vector<int> freqs;

    std::cout << "loading " << dict_file << " ..." << std::endl;
    LoadDict(dict_file, words, freqs);
    std::cout << "loaded " << words.size() << " words" << std::endl;

    cut::NaiveCutter cutter;

    auto start = std::chrono::high_resolution_clock::now();
    cutter.Build(words, freqs);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "build done in " << ms.count() << "ms" << std::endl;

    std::cout << "> ";
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line == "q" || line == "quit") break;

        auto rs = cutter.Cut(line);
        std::cout << join(rs, "/") << std::endl;
        std::cout << "> ";
    }
}

// --- tests ---
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

void test_basic_cut() {
    cut::NaiveCutter cutter;
    cutter.Build({"ab", "cd", "abcd", "ef"}, {5, 5, 100, 5});

    auto rs = cutter.Cut("abcdef");
    std::string result = join(rs, "/");
    std::cout << "basic: " << result << std::endl;
    check(result == "abcd/ef", "basic cut");
}

void test_chinese_cut() {
    cut::NaiveCutter cutter;
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

void test_no_match() {
    cut::NaiveCutter cutter;
    cutter.Build({"ab", "cd"}, {10, 10});

    auto rs = cutter.Cut("xyz");
    std::cout << "no match: " << join(rs, "/") << std::endl;
    check(rs.size() == 3, "no match");
}

void run_tests() {
    test_basic_cut();
    test_chinese_cut();
    test_no_match();
    std::cout << "\nPassed: " << passed << ", Failed: " << failed << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        repl(argv[1]);
    } else {
        run_tests();
    }
    return 0;
}
