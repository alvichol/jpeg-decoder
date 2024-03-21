#pragma once

#include <istream>
#include <memory>
#include <vector>
#include "bit_reader.h"
#include "huffman_tree.h"
#include "../image.h"

enum class Sector { SOI, SOF0, DHT, DQT, APP, COM, SOS, EOI, SKIP, UNDEF };

struct ChannelInfo {
    int horizontal_;
    int vertical_;
    int table_id_;
};

using Table = int[8][8];

struct Block {
    int table_[8][8];
};

class JpegDecoder {
public:
    Image Decode();

private:
    void Parse();
    Sector ParseMarker();
    int Parse1Byte();
    int Parse2Bytes();
    int ParseLength();
    void ParseCOM();
    void ParseSOF0();
    void ParseDQT();
    void ParseDHT();
    void ParseSOS();
    void ParseAPP();
    void ParseMatrix(BitReader& reader, int (&matrix)[8][8], int dc_idx, int ac_idx);
    void Calculate();
    void ProcessMatrix(int (&matrix)[8][8], const int (&q_matrix)[8][8]);

public:
    JpegDecoder(std::istream& is);

private:
    std::istream& is_;
    int height_ = -1;
    int width_ = -1;
    int mcu_height_ = -1;
    int mcu_width_ = -1;
    bool monochrome_ = false;
    ChannelInfo channels_[3];
    Table qtables_[2];
    int q_id_ = 0;
    HuffmanTree dht_[2][2];
    std::vector<std::vector<Block>> y_img_, cb_img_, cr_img_;
    Image result_;
};