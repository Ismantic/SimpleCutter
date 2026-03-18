#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fstream>
#include <string>
#include <vector>

#include "cut.h"

namespace py = pybind11;

class PyCutter {
public:
    explicit PyCutter(const std::string& dict_path) {
        std::vector<std::string> words;
        std::vector<int> freqs;

        std::ifstream file(dict_path);
        if (!file.is_open()) {
            throw std::runtime_error("cannot open: " + dict_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('\t');
            if (pos == std::string::npos) continue;
            words.push_back(line.substr(0, pos));
            freqs.push_back(std::stoi(line.substr(pos + 1)));
        }

        cutter_.Build(words, freqs);
    }

    std::vector<std::string> cut(const std::string& sentence) {
        return cutter_.Cut(sentence);
    }

private:
    cut::NaiveCutter cutter_;
};

PYBIND11_MODULE(_ismacut, m) {
    m.doc() = "IsmaCut: Chinese word segmentation based on Double-Array Trie";

    py::class_<PyCutter>(m, "Cutter")
        .def(py::init<const std::string&>(), py::arg("dict_path"))
        .def("cut", &PyCutter::cut, py::arg("sentence"));
}
