#pragma once
#include <istream>

struct BitReader {
    BitReader(std::istream& is);
    bool Read();
    int ReadN(int n);

private:
    std::istream& is_;
    int last_byte_ = -1;
    int idx_ = 0;
};
