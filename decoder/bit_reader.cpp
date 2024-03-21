#include <cstdint>
#include <iostream>
#include "bit_reader.h"

BitReader::BitReader(std::istream& is) : is_(is) {
}
bool BitReader::Read() {
    if (last_byte_ == -1) {
        char bytes[1];
        is_.read(bytes, 1);
        if (is_.gcount() == 0) {
            throw std::runtime_error("Unexpected EOF");
        }
        last_byte_ = static_cast<uint8_t>(bytes[0]);
        if (last_byte_ == 0xff) {
            is_.read(bytes, 1);
            if (is_.gcount() == 0 || static_cast<uint8_t>(bytes[0]) != 0) {
                throw std::runtime_error("Unexpected EOF");
            }
        }
    }
    bool ans = last_byte_ & (1 << (7 - idx_));
    idx_++;
    if (idx_ == 8) {
        last_byte_ = -1;
        idx_ = 0;
    }
    return ans;
}

int BitReader::ReadN(int n) {
    int val = 0;
    bool begin = true, first_zero = false;
    for (int i = 0; i < n; i++) {
        bool bit = Read();
        if (!bit && begin) {
            first_zero = true;
        }
        begin = false;
        val *= 2;
        val += bit;
    }
    if (first_zero) {
        val = val - (1 << n) + 1;
    }
    return val;
}
