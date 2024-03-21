#pragma once

#include <vector>

struct HuffmanTree {
    struct Iterator {
        void Increment(bool bit);
        int GetValue();

    private:
        Iterator(const std::vector<int>& val, const std::vector<int>& left,
                 const std::vector<int>& right);

        const std::vector<int>& val_;
        const std::vector<int>& left_;
        const std::vector<int>& right_;
        int pos_ = 0;

        friend HuffmanTree;
    };

    bool IsEmpty() const;
    void Add(int len, int val);
    Iterator Begin() const;

public:  // todo
    bool AddImpl(int idx, int len, int val);

    std::vector<int> val_;
    std::vector<int> left_;
    std::vector<int> right_;
};
