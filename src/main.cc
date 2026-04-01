#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <string>
#include <chrono>

#include "count.h"
#include "cut.h"
#include "segment.h"

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

cut::NaiveCutter BuildCutter(const std::string& dict_file) {
    std::vector<std::string> words;
    std::vector<int> freqs;

    std::cerr << "loading " << dict_file << " ..." << std::endl;
    LoadDict(dict_file, words, freqs);
    std::cerr << "loaded " << words.size() << " words" << std::endl;

    cut::NaiveCutter cutter;

    auto start = std::chrono::high_resolution_clock::now();
    cutter.Build(words, freqs);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cerr << "build done in " << ms.count() << "ms" << std::endl;

    return cutter;
}

void repl(const std::string& dict_file) {
    auto cutter = BuildCutter(dict_file);

    std::cout << "> ";
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line == "q" || line == "quit") break;

        auto rs = cutter.Cut(line);
        std::cout << join(rs, "/") << std::endl;
        std::cout << "> ";
    }
}

void pipe_mode(const std::string& dict_file) {
    auto cutter = BuildCutter(dict_file);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "\n";
            continue;
        }
        auto rs = cutter.Cut(line);
        std::cout << join(rs, " ") << "\n";
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

void LoadWords(const std::string& filename, std::vector<std::string>& words) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "cannot open: " << filename << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        // Take first field, delimited by tab or space
        size_t pos = line.find('\t');
        if (pos == std::string::npos) {
            pos = line.find(' ');
        }
        if (pos != std::string::npos) {
            words.push_back(line.substr(0, pos));
        } else {
            words.push_back(line);
        }
    }
}

void segment_mode(const std::string& dict_file,
                  const std::string& input_file,
                  const std::string& output_file) {
    std::vector<std::string> words;
    std::cerr << "loading " << dict_file << " ..." << std::endl;
    LoadWords(dict_file, words);
    std::cerr << "loaded " << words.size() << " words" << std::endl;

    cut::Segmenter seg;
    seg.Build(words);

    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "cannot open: " << input_file << std::endl;
        return;
    }
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "cannot open: " << output_file << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            out << "\n";
            continue;
        }
        auto rs = seg.Cut(line);
        out << join(rs, " ") << "\n";
    }
    std::cerr << "done" << std::endl;
}

void cut_mode(const std::string& dict_file,
              const std::string& input_file,
              const std::string& output_file) {
    auto cutter = BuildCutter(dict_file);

    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "cannot open: " << input_file << std::endl;
        return;
    }
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "cannot open: " << output_file << std::endl;
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            out << "\n";
            continue;
        }
        auto rs = cutter.Cut(line);
        out << join(rs, " ") << "\n";
    }
    std::cerr << "done" << std::endl;
}

void count_mode(const std::string& dict_file,
                const std::string& input_file,
                const std::string& output_file) {
    // Load dictionary whitelist if provided
    std::set<std::string> whitelist;
    if (!dict_file.empty()) {
        std::vector<std::string> dict_words;
        LoadWords(dict_file, dict_words);
        whitelist.insert(dict_words.begin(), dict_words.end());
        std::cerr << "dict filter: " << whitelist.size() << " words" << std::endl;
    }

    std::ifstream in(input_file);
    if (!in.is_open()) {
        std::cerr << "cannot open: " << input_file << std::endl;
        return;
    }

    cut::Counter counter;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        // Split by space
        std::vector<std::string> words;
        size_t start = 0;
        while (start < line.size()) {
            size_t end = line.find(' ', start);
            if (end == std::string::npos) end = line.size();
            if (end > start) {
                std::string w = line.substr(start, end - start);
                if (whitelist.empty() || whitelist.count(w)) {
                    words.push_back(std::move(w));
                }
            }
            start = end + 1;
        }
        counter.Add(words);
    }

    std::vector<std::string> words;
    std::vector<int> freqs;
    counter.Export(words, freqs);

    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "cannot open: " << output_file << std::endl;
        return;
    }
    for (size_t i = 0; i < words.size(); ++i) {
        out << words[i] << "\t" << freqs[i] << "\n";
    }
    std::cerr << "counted " << words.size() << " words" << std::endl;
}

int main(int argc, char* argv[]) {
    // iscut --dict dict.file --segment input.file output.file
    // iscut --count output.file count.file
    // iscut --dict dict.file (repl)
    // iscut dict.file --pipe
    // iscut dict.file  (repl)
    // iscut  (tests)

    std::string mode;
    std::string dict_file;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dict" && i + 1 < argc) {
            dict_file = argv[++i];
        } else if (a == "--segment" || a == "--cut" || a == "--count" || a == "--pipe") {
            mode = a;
        } else {
            args.push_back(a);
        }
    }

    if (mode == "--segment") {
        if (dict_file.empty() || args.size() != 2) {
            std::cerr << "usage: iscut --dict dict.file --segment input.file output.file" << std::endl;
            return 1;
        }
        segment_mode(dict_file, args[0], args[1]);
    } else if (mode == "--cut") {
        if (dict_file.empty() || args.size() != 2) {
            std::cerr << "usage: iscut --dict dict.file --cut input.file output.file" << std::endl;
            return 1;
        }
        cut_mode(dict_file, args[0], args[1]);
    } else if (mode == "--count") {
        if (args.size() != 2) {
            std::cerr << "usage: iscut [--dict dict.file] --count input.file output.file" << std::endl;
            return 1;
        }
        count_mode(dict_file, args[0], args[1]);
    } else if (mode == "--pipe") {
        std::string df = dict_file.empty() && args.size() == 1 ? args[0] : dict_file;
        if (df.empty()) {
            std::cerr << "usage: iscut --dict dict.file --pipe" << std::endl;
            return 1;
        }
        pipe_mode(df);
    } else if (!dict_file.empty() && args.empty()) {
        repl(dict_file);
    } else if (!args.empty()) {
        repl(args[0]);
    } else {
        run_tests();
    }
    return 0;
}
