#ifndef DALM_BIT_VECTOR_H_
#define DALM_BIT_VECTOR_H_

namespace DALM {

template <bool DefaultBit>
class BitVector {
  class _Ref {
   private:
    uint64_t *pointer_;
    size_t ctz_;
   public:
    explicit _Ref(uint64_t* pointer, size_t ctz) : pointer_(pointer), ctz_(ctz) {}
    explicit operator bool() const {
      return *pointer_ & (1ull << ctz_);
    }
    _Ref& operator=(bool bit) {
      if (bit)
        *pointer_ |= (1ull << ctz_);
      else
        *pointer_ &= compl (1ull << ctz_);
      return *this;
    }
  };

 public:
  static constexpr uint64_t kDefaultWord = DefaultBit ? -1ull : 0;

  BitVector() : size_(0) {}
  _Ref operator[](size_t index) {
    return _Ref(&container_[index / 64], index % 64);
  }
  uint64_t* ptr_at(size_t word_index) {
    return &container_[word_index];
  }
  uint64_t word_at(size_t word_index) const {
    return word_index < container_.size() ? container_[word_index] : kDefaultWord;
  }
  uint64_t bits64_from(size_t index) const {
    auto insets = index%64;
    if (insets == 0) {
      return word_at(index/64);
    } else {
      return (word_at(index/64) >> insets) | (word_at(index/64+1) << (64-insets));
    }
  };
  void resize(size_t new_size, bool bit) {
    container_.resize((new_size-1)/64+1, bit ? -1ull : 0);
    if (new_size > size_ and bit) {
      *ptr_at(size_/64) |= -1ull << size_%64;
      if (new_size % 64 > 0) {
        container_[new_size/64] &= -1ull >> (64 - new_size%64);
      }
    }
    size_ = new_size;
  }

 private:
  size_t size_;
  std::vector<uint64_t> container_;
};

}

#endif //DALM_BIT_VECTOR_H_
