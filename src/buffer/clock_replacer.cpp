#include "buffer/clock_replacer.h"

CLOCKReplacer::CLOCKReplacer(size_t num_pages)
    : capacity_(num_pages), hand_(0), curr_size_(0), in_replacer_(num_pages, false), ref_bit_(num_pages, false) {}

CLOCKReplacer::~CLOCKReplacer() = default;

bool CLOCKReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> guard(latch_);
  if (curr_size_ == 0 || capacity_ == 0) {
    return false;
  }

  size_t scanned = 0;
  while (scanned < capacity_ * 2) {
    if (in_replacer_[hand_]) {
      if (ref_bit_[hand_]) {
        ref_bit_[hand_] = false;
      } else {
        *frame_id = static_cast<frame_id_t>(hand_);
        in_replacer_[hand_] = false;
        curr_size_--;
        hand_ = (hand_ + 1) % capacity_;
        return true;
      }
    }
    hand_ = (hand_ + 1) % capacity_;
    scanned++;
  }
  return false;
}

void CLOCKReplacer::Pin(frame_id_t frame_id) {
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= capacity_) {
    return;
  }

  std::lock_guard<std::mutex> guard(latch_);
  const auto fid = static_cast<size_t>(frame_id);
  if (!in_replacer_[fid]) {
    return;
  }

  in_replacer_[fid] = false;
  ref_bit_[fid] = false;
  curr_size_--;
}

void CLOCKReplacer::Unpin(frame_id_t frame_id) {
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= capacity_) {
    return;
  }

  std::lock_guard<std::mutex> guard(latch_);
  const auto fid = static_cast<size_t>(frame_id);
  if (in_replacer_[fid]) {
    return;
  }

  in_replacer_[fid] = true;
  ref_bit_[fid] = true;
  curr_size_++;
}

size_t CLOCKReplacer::Size() {
  std::lock_guard<std::mutex> guard(latch_);
  return curr_size_;
}
