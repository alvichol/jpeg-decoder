#include <cstdint>
#include <fftw3.h>
#include <cmath>
#include <iostream>
#include "jpeg_decoder.h"
#include "zigzag_writer.h"

Sector JpegDecoder::ParseMarker() {
    char bytes[2];
    is_.read(bytes, 2);
    if (is_.gcount() < 2) {
        throw std::runtime_error("Unexpected EOF");
    }
    if (static_cast<unsigned char>(bytes[0]) != 0xff) {
        return Sector::UNDEF;
    }
    switch (static_cast<unsigned char>(bytes[1])) {
        case 0x00:
            return Sector::SKIP;
        case 0xd8:
            return Sector::SOI;
        case 0xc0:
            return Sector::SOF0;
        case 0xc4:
            return Sector::DHT;
        case 0xdb:
            return Sector::DQT;
        case 0xda:
            return Sector::SOS;
        case 0xe0:
        case 0xe1:
        case 0xe2:
        case 0xe3:
        case 0xe4:
        case 0xe5:
        case 0xe6:
        case 0xe7:
        case 0xe8:
        case 0xe9:
        case 0xea:
        case 0xeb:
        case 0xec:
        case 0xed:
        case 0xee:
        case 0xef:
            return Sector::APP;
        case 0xfe:
            return Sector::COM;
        case 0xd9:
            return Sector::EOI;
        default:
            return Sector::UNDEF;
    }
}

int JpegDecoder::Parse1Byte() {
    char bytes[1];
    is_.read(bytes, 1);
    if (is_.gcount() == 0) {
        throw std::runtime_error("Unexpected EOF");
    }
    return static_cast<uint8_t>(bytes[0]);
}

int JpegDecoder::Parse2Bytes() {
    char bytes[2];
    is_.read(bytes, 2);
    if (is_.gcount() < 2) {
        throw std::runtime_error("Unexpected EOF");
    }
    return static_cast<uint8_t>(bytes[0]) * 256 + static_cast<uint8_t>(bytes[1]);
}

int JpegDecoder::ParseLength() {
    int length = Parse2Bytes();
    length -= 2;
    if (length < 0) {
        throw std::runtime_error("Invalid sector length");
    }
    return length;
}

void JpegDecoder::ParseCOM() {
    int length = ParseLength();
    std::string comm;
    char buffer[128];
    while (length > 0) {
        int cnt = std::min(128, length);
        is_.read(buffer, cnt);
        if (is_.gcount() < cnt) {
            throw std::runtime_error("Unexpected EOF");
        }
        comm.insert(comm.size(), buffer, cnt);
        length -= cnt;
    }
    result_.SetComment(comm);
}

void JpegDecoder::ParseSOF0() {
    int length = ParseLength();
    int precision = Parse1Byte();
    if (precision != 8) {
        throw std::runtime_error("Unsupported format 1");
    }
    height_ = Parse2Bytes();
    width_ = Parse2Bytes();
    int channels = Parse1Byte();
    if (length != (channels == 3 ? 15 : 9)) {
        throw std::runtime_error("Unsupported format 2");
    }
    if (channels != 3 && channels != 1) {
        throw std::runtime_error("Unsupported format 3");
    }
    int max_h = 1, max_v = 1;
    for (int i = 0; i < channels; i++) {
        int id = Parse1Byte();
        if (id != i + 1) {
            throw std::runtime_error("Unsupported format 4");
        }
        int hv = Parse1Byte();
        int table_id = Parse1Byte();
        if (table_id > 1) {
            throw std::runtime_error("Unsupported format 5");
        }
        max_h = std::max(max_h, hv / 16);
        max_v = std::max(max_v, hv % 16);
        channels_[i] = {hv / 16, hv % 16, table_id};
    }
    for (int i = 0; i < channels; i++) {
        channels_[i].horizontal_ = max_h / channels_[i].horizontal_;
        channels_[i].vertical_ = max_v / channels_[i].vertical_;
    }
    monochrome_ = (channels == 1);
    if (monochrome_) {
        mcu_height_ = 8;
        mcu_width_ = 8;
    } else {
        if (channels_[1].horizontal_ != channels_[2].horizontal_) {
            throw std::runtime_error("Unsupported format 6");
        }
        if (channels_[1].horizontal_ == 1) {
            mcu_width_ = 8;
        } else if (channels_[1].horizontal_ == 2) {
            mcu_width_ = 16;
        } else {
            throw std::runtime_error("Unsupported format 7");
        }
        if (channels_[1].vertical_ != channels_[2].vertical_) {
            throw std::runtime_error("Unsupported format 8");
        }
        if (channels_[1].vertical_ == 1) {
            mcu_height_ = 8;
        } else if (channels_[1].vertical_ == 2) {
            mcu_height_ = 16;
        } else {
            throw std::runtime_error("Unsupported format 9");
        }
    }
}

void JpegDecoder::ParseDQT() {
    int length = ParseLength();
    for (int i = 0; i < 2; i++) {
        int info = Parse1Byte();
        length--;
        int byte_len = info / 16 + 1, id = info % 16;
        if (length < byte_len * 64) {
            throw std::runtime_error("Unsupported format 10");
        }
        if (id > 1 || byte_len > 2) {
            throw std::runtime_error("Unsupported format 11");
        }
        if (q_id_ != id) {
            throw std::runtime_error("Wrong DQT id");
        }
        q_id_ += (1 << id);
        ZigZagWriter writer(qtables_[id]);
        for (int k = 0; k < 64; k++) {
            int elem;
            if (byte_len == 1) {
                elem = Parse1Byte();
            } else {
                elem = Parse2Bytes();
            }
            writer.Write(elem);
        }
        if (length == byte_len * 64) {
            length -= byte_len * 64;
            break;
        }
        length -= byte_len * 64;
    }
    if (length > 0) {
        throw std::runtime_error("Invalid length");
    }
}

void JpegDecoder::ParseDHT() {
    int length = ParseLength();
    for (int k = 0; k < 4; k++) {
        int info = Parse1Byte();
        length--;
        int id = info % 16, type = info / 16;
        if (id > 1 || type > 1) {
            throw std::runtime_error("Unsupported format 12");
        }
        if (!dht_[type][id].IsEmpty()) {
            throw std::runtime_error("Wrong DHT id");
        }
        if (length < 16) {
            throw std::runtime_error("Unsupported format 13");
        }
        int codes = 0;
        int cnt[16];
        for (int i = 0; i < 16; i++) {
            int val = Parse1Byte();
            cnt[i] = val;
            codes += val;
        }
        length -= 16;
        if (length < codes) {
            throw std::runtime_error("Invalid length");
        }
        for (int len = 1; len <= 16; len++) {
            for (int i = 0; i < cnt[len - 1]; i++) {
                int val = Parse1Byte();
                dht_[type][id].Add(len, val);
            }
        }
        length -= codes;
        if (length == 0) {
            break;
        }
    }
    if (length > 0) {
        throw std::runtime_error("Invalid length");
    }
}

void JpegDecoder::ParseSOS() {
    if (mcu_height_ == -1) {
        throw std::runtime_error("No SOF0 was parsed");
    }
    int length = ParseLength();
    if (length != (monochrome_ ? 6 : 10)) {
        throw std::runtime_error("Unsupported format 14");
    }
    int cnt = Parse1Byte();
    if (cnt != (monochrome_ ? 1 : 3)) {
        throw std::runtime_error("Unsupported format 15");
    }
    int dc_idx[3];
    int ac_idx[3];
    for (int k = 0; k < (monochrome_ ? 1 : 3); k++) {
        int num = Parse1Byte();
        if (num != k + 1) {
            throw std::runtime_error("Unsupported format 16");
        }
        int info = Parse1Byte();
        dc_idx[k] = info / 16;
        ac_idx[k] = info % 16;
        if (dc_idx[k] > 1 || ac_idx[k] > 1) {
            throw std::runtime_error("Wrong AC/DC table id");
        }
    }
    int b1 = Parse1Byte();
    int b2 = Parse1Byte();
    int b3 = Parse1Byte();
    if (b1 != 0 || b2 != 63 || b3 != 0) {
        throw std::runtime_error("Unsupported format");
    }
    int i = 0, j = 0;
    int blocks_in_line = (width_ + mcu_width_ - 1) / mcu_width_;
    int blocks_in_col = (height_ + mcu_height_ - 1) / mcu_height_;
    int v_blocks = (mcu_height_ / 8), h_blocks = (mcu_width_ / 8);
    y_img_.resize(blocks_in_col * v_blocks, std::vector<Block>(blocks_in_line * h_blocks));
    if (!monochrome_) {
        cb_img_.resize(blocks_in_col, std::vector<Block>(blocks_in_line));
        cr_img_.resize(blocks_in_col, std::vector<Block>(blocks_in_line));
    }
    BitReader reader(is_);
    int last_dc_y = 0, last_dc_cb = 0, last_dc_cr = 0;
    while (i < blocks_in_col) {
        int temp_i = i * v_blocks, temp_j = j * h_blocks;
        ParseMatrix(reader, y_img_[temp_i][temp_j].table_, dc_idx[0], ac_idx[0]);
        y_img_[temp_i][temp_j].table_[0][0] += last_dc_y;
        last_dc_y = y_img_[temp_i][temp_j].table_[0][0];
        if (h_blocks == 2) {
            ParseMatrix(reader, y_img_[temp_i][temp_j + 1].table_, dc_idx[0], ac_idx[0]);
            y_img_[temp_i][temp_j + 1].table_[0][0] += last_dc_y;
            last_dc_y = y_img_[temp_i][temp_j + 1].table_[0][0];
        }
        if (v_blocks == 2) {
            ParseMatrix(reader, y_img_[temp_i + 1][temp_j].table_, dc_idx[0], ac_idx[0]);
            y_img_[temp_i + 1][temp_j].table_[0][0] += last_dc_y;
            last_dc_y = y_img_[temp_i + 1][temp_j].table_[0][0];
        }
        if (h_blocks * v_blocks == 4) {
            ParseMatrix(reader, y_img_[temp_i + 1][temp_j + 1].table_, dc_idx[0], ac_idx[0]);
            y_img_[temp_i + 1][temp_j + 1].table_[0][0] += last_dc_y;
            last_dc_y = y_img_[temp_i + 1][temp_j + 1].table_[0][0];
        }
        if (!monochrome_) {
            ParseMatrix(reader, cb_img_[i][j].table_, dc_idx[1], ac_idx[1]);
            cb_img_[i][j].table_[0][0] += last_dc_cb;
            last_dc_cb = cb_img_[i][j].table_[0][0];
            ParseMatrix(reader, cr_img_[i][j].table_, dc_idx[2], ac_idx[2]);
            cr_img_[i][j].table_[0][0] += last_dc_cr;
            last_dc_cr = cr_img_[i][j].table_[0][0];
        }
        j++;
        if (j == blocks_in_line) {
            j = 0;
            i++;
        }
    }
}

JpegDecoder::JpegDecoder(std::istream& is) : is_(is) {
}

void JpegDecoder::ParseMatrix(BitReader& reader, int (&matrix)[8][8], int dc_idx, int ac_idx) {
    int dc = 0;
    auto start = dht_[0][dc_idx].Begin();
    while (start.GetValue() == -1) {
        start.Increment(reader.Read());
    }
    if (start.GetValue() != 0) {
        dc = reader.ReadN(start.GetValue());
    }
    ZigZagWriter writer(matrix);
    writer.Write(dc);
    int idx = 1;
    while (idx < 64) {
        auto it = dht_[1][ac_idx].Begin();
        while (it.GetValue() == -1) {
            it.Increment(reader.Read());
        }
        int zero_cnt = 64 - idx;
        int ac = -1;
        if (it.GetValue() != 0) {
            zero_cnt = it.GetValue() / 16;
            int coef_len = it.GetValue() % 16;
            ac = reader.ReadN(coef_len);
        }
        for (int k = 0; k < zero_cnt; k++) {
            writer.Write(0);
            idx++;
        }
        if (idx != 64) {
            writer.Write(ac);
            idx++;
        }
    }
}

void JpegDecoder::ParseAPP() {
    int length = ParseLength();
    char buffer[128];
    while (length > 0) {
        int cnt = std::min(128, length);
        is_.read(buffer, cnt);
        if (is_.gcount() < cnt) {
            throw std::runtime_error("Unexpected EOF");
        }
        length -= cnt;
    }
}

void JpegDecoder::Parse() {
    Sector sect = ParseMarker();
    if (sect != Sector::SOI) {
        throw std::runtime_error("Unsupported format 17");
    }
    bool sof0 = false;
    while (true) {
        bool end = false;
        sect = ParseMarker();
        switch (sect) {
            case Sector::SOI:
                throw std::runtime_error("Unexpected marker");
            case Sector::SOF0:
                if (sof0) {
                    throw std::runtime_error("Unexpected marker");
                }
                ParseSOF0();
                sof0 = true;
                break;
            case Sector::DHT:
                ParseDHT();
                break;
            case Sector::DQT:
                ParseDQT();
                break;
            case Sector::APP:
                ParseAPP();
                break;
            case Sector::COM:
                ParseCOM();
                break;
            case Sector::SOS:
                ParseSOS();
                break;
            case Sector::EOI:
                end = true;
                break;
            case Sector::SKIP:
                break;
            case Sector::UNDEF:
                throw std::runtime_error("Unexpected marker");
        }
        if (end) {
            break;
        }
    }
    if (!sof0) {
        throw std::runtime_error("No sectors");
    }
    if (monochrome_ && q_id_ != 1) {
        throw std::runtime_error("No sectors");
    }
    if (!monochrome_ && q_id_ != 3) {
        throw std::runtime_error("No sectors");
    }
}

void JpegDecoder::Calculate() {
    for (auto& v : y_img_) {
        for (auto& matrix : v) {
            ProcessMatrix(matrix.table_, qtables_[channels_[0].table_id_]);
        }
    }

    if (!monochrome_) {
        for (auto& v : cb_img_) {
            for (auto& matrix : v) {
                ProcessMatrix(matrix.table_, qtables_[channels_[1].table_id_]);
            }
        }

        for (auto& v : cr_img_) {
            for (auto& matrix : v) {
                ProcessMatrix(matrix.table_, qtables_[channels_[2].table_id_]);
            }
        }
    }

    result_.SetSize(width_, height_);
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int y_val = y_img_[y / 8][x / 8].table_[y % 8][x % 8];
            int cb_val = 0, cr_val = 0;
            if (monochrome_) {
                result_.SetPixel(y, x, {y_val, y_val, y_val});
                continue;
            }
            int new_y = y / channels_[1].vertical_, new_x = x / channels_[1].horizontal_;
            cb_val = cb_img_[new_y / 8][new_x / 8].table_[new_y % 8][new_x % 8];
            cr_val = cr_img_[new_y / 8][new_x / 8].table_[new_y % 8][new_x % 8];
            int r = std::lround(y_val + 1.402 * (cr_val - 128));
            int g = std::lround(y_val - 0.34414 * (cb_val - 128) - 0.71414 * (cr_val - 128));
            int b = std::lround(y_val + 1.772 * (cb_val - 128));
            r = std::min(std::max(r, 0), 255);
            g = std::min(std::max(g, 0), 255);
            b = std::min(std::max(b, 0), 255);
            result_.SetPixel(y, x, {r, g, b});
        }
    }
}

void JpegDecoder::ProcessMatrix(int (&matrix)[8][8], const int (&q_matrix)[8][8]) {
    double input_matrix[8][8];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            matrix[i][j] *= q_matrix[i][j];
            input_matrix[i][j] = matrix[i][j];
        }
    }

    for (int i = 0; i < 8; i++) {
        input_matrix[i][0] *= 1.41421356;
    }

    for (int j = 0; j < 8; j++) {
        input_matrix[0][j] *= 1.41421356;
    }

    double output_matrix[8][8];
    fftw_plan plan = fftw_plan_r2r_2d(8, 8, &input_matrix[0][0], &output_matrix[0][0], FFTW_REDFT01,
                                      FFTW_REDFT01, 0);
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            matrix[i][j] = std::lround(output_matrix[i][j] / 16.);
            matrix[i][j] = std::min(std::max(0, matrix[i][j] + 128), 255);
        }
    }
}

Image JpegDecoder::Decode() {
    Parse();
    Calculate();
    return result_;
}
