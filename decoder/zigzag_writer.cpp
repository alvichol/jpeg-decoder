#include "zigzag_writer.h"

ZigZagWriter::ZigZagWriter(int (&matrix)[8][8]) : matrix_(matrix) {
}

void ZigZagWriter::Write(int val) {
    matrix_[i_][j_] = val;
    if (mode_ == 0) {
        j_++;
        mode_ = reverse_ ? ((mode_ + 3) % 4) : ((mode_ + 1) % 4);
    } else if (mode_ == 1) {
        i_++;
        j_--;
        if (j_ == 0 && i_ == 7) {
            reverse_ = true;
        }
        if (j_ == 0 && !reverse_) {
            mode_ = (mode_ + 1) % 4;
        } else if (i_ == 7 && reverse_) {
            mode_ = (mode_ + 3) % 4;
        }
    } else if (mode_ == 2) {
        i_++;
        mode_ = reverse_ ? ((mode_ + 3) % 4) : ((mode_ + 1) % 4);
    } else {
        i_--;
        j_++;
        if (i_ == 0 && !reverse_) {
            mode_ = (mode_ + 1) % 4;
        } else if (j_ == 7 && reverse_) {
            mode_ = (mode_ + 3) % 4;
        }
    }
}
