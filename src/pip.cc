#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fstream>
#include <string>
#include <vector>

#include "cut.h"

namespace py = pybind11;

static void LoadDict(const std::string& path,
                     std::vector<std::string>& words,
                     std::vector<int>& freqs) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open: " + path);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        size_t pos = line.find('\t');
        if (pos == std::string::npos) continue;
        words.push_back(line.substr(0, pos));
        freqs.push_back(std::stoi(line.substr(pos + 1)));
    }
}

class PyCutter {
public:
    explicit PyCutter(const std::string& dict_path) {
        std::vector<std::string> words;
        std::vector<int> freqs;
        LoadDict(dict_path, words, freqs);
        cutter_.Build(words, freqs);
    }

    std::vector<std::string> cut(const std::string& sentence) {
        return cutter_.Cut(sentence);
    }

private:
    cut::NaiveCutter cutter_;
};

class PySemanticCutter {
public:
    PySemanticCutter(const std::string& dict_path,
                     const std::string& piece_path) {
        std::vector<std::string> words;
        std::vector<int> freqs;
        LoadDict(dict_path, words, freqs);
        cutter_.Build(words, freqs);
        if (!piece_path.empty() && !cutter_.LoadPiece(piece_path)) {
            throw std::runtime_error("cannot load piece model: " + piece_path);
        }
    }

    std::vector<std::string> cut(const std::string& sentence, bool en) {
        return cutter_.Cut(sentence, en);
    }

private:
    cut::SemanticCutter cutter_;
};

PYBIND11_MODULE(_iscut, m) {
    m.doc() = "Iscut: Chinese word segmentation and tokenization";

    py::class_<PyCutter>(m, "Cutter")
        .def(py::init<const std::string&>(), py::arg("dict_path"))
        .def("cut", &PyCutter::cut, py::arg("sentence"));

    py::class_<PySemanticCutter>(m, "SemanticCutter")
        .def(py::init<const std::string&, const std::string&>(),
             py::arg("dict_path"), py::arg("piece_path") = "")
        .def("cut", &PySemanticCutter::cut,
             py::arg("sentence"), py::arg("en") = false);
}
