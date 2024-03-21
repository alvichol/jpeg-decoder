#pragma once

struct ZigZagWriter {
    ZigZagWriter(int (&matrix)[8][8]);
    void Write(int val);

private:
    int (&matrix_)[8][8];
    int i_ = 0, j_ = 0;
    int mode_ = 0;
    bool reverse_ = false;
};
