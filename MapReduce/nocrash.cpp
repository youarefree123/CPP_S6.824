#include "nocrash.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cstring>


void maybeCrash() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 999);

    if (false && distrib(gen) < 500) {
        // crash!
        exit(1);
    }
}

std::vector<KeyValue> mapTask(std::string_view filename, std::string_view contents) {
    maybeCrash();

    std::vector<KeyValue> kva;
    kva.push_back(KeyValue{"a", std::string{filename}});
    kva.push_back(KeyValue{"b", std::to_string(filename.length())});
    kva.push_back(KeyValue{"c", std::to_string(contents.length())});
    kva.push_back(KeyValue{"d", "xyzzy"});
    return kva;
}

std::string reduceTask(std::string_view key, const std::vector<std::string>& values) {
    maybeCrash();

    // sort values to ensure deterministic output.
    std::vector<std::string> vv(values.size());
    std::copy(values.begin(), values.end(), vv.begin());
    std::sort(vv.begin(), vv.end());

    std::string val = "";
    for (const auto& v : vv) {
        val += v + " ";
    }
    val.erase(val.length() - 1); // remove the last whitespace
    return val;
}
