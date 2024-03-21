#include <fstream>
#include <iostream>
#include "../decoder.h"
#include "jpeg_decoder.h"

Image Decode(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios_base::binary);
    JpegDecoder decoder(stream);
    return decoder.Decode();
}
