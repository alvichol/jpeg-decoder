#include <stdexcept>
#include <iostream>
#include "huffman_tree.h"

bool HuffmanTree::AddImpl(int idx, int len, int val) {
    if (len == 0) {
        if (val_[idx] == -1) {
            val_[idx] = val;
            return true;
        }
        return false;
    }
    int left_son = left_[idx];
    int right_son = right_[idx];
    if (left_son == -1) {
        val_.push_back(-1);
        left_.push_back(-1);
        right_.push_back(-1);
        left_son = val_.size() - 1;
        left_[idx] = left_son;
    }
    if (val_[left_son] == -1 && AddImpl(left_son, len - 1, val)) {
        return true;
    }
    if (right_son == -1) {
        val_.push_back(-1);
        left_.push_back(-1);
        right_.push_back(-1);
        right_son = val_.size() - 1;
        right_[idx] = right_son;
    }
    if (val_[right_son] == -1 && AddImpl(right_son, len - 1, val)) {
        return true;
    }
    return false;
}

bool HuffmanTree::IsEmpty() const {
    return left_.empty();
}

void HuffmanTree::Add(int len, int val) {
    if (IsEmpty()) {
        val_.push_back(-1);
        left_.push_back(-1);
        right_.push_back(-1);
    }
    if (!AddImpl(0, len, val)) {
        throw std::runtime_error("Pizdec exception");
    }
}

HuffmanTree::Iterator HuffmanTree::Begin() const {
    return HuffmanTree::Iterator(val_, left_, right_);
}

HuffmanTree::Iterator::Iterator(const std::vector<int>& val, const std::vector<int>& left,
                                const std::vector<int>& right)
    : val_(val), left_(left), right_(right) {
    if (val_.empty()) {
        throw std::runtime_error("Huffman decoding failed 0");
    }
}

void HuffmanTree::Iterator::Increment(bool bit) {
    int next_idx;
    if (bit == 0) {
        next_idx = left_[pos_];
    } else {
        next_idx = right_[pos_];
    }
    if (next_idx == -1) {
        throw std::runtime_error("Huffman decoding failed " + std::to_string(pos_) + " " +
                                 std::to_string(val_.size()));
    }
    pos_ = next_idx;
}

int HuffmanTree::Iterator::GetValue() {
    return val_[pos_];
}
