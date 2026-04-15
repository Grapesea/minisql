#include "buffer/lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages): 
  capacity(num_pages){}

LRUReplacer::~LRUReplacer() = default;

/**
 * TODO: Student Implement
 */
bool LRUReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> guard(locks);
  
  if (this->Size() == 0){
    return false;
  }
  frame_id_t top = this->frames.back();
  this->frames.pop_back();
  mapping.erase(top);
  *frame_id = top;
  // pop out the top element and assign the value to `frame_id` pointer.

  return true;
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(locks);
  // <C++ 20 standard>: `contains`, so I have to use count() instead.
  if (!mapping.count(frame_id)){
    return;
  }
  mapping.erase(frame_id); // if a page gets pinned, it must be deleted from replacer_
  frames.remove(frame_id);
  return;
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(locks);

  if (mapping.count(frame_id)){
    return;
  }
  if (this->Size() == capacity){
    frame_id_t pop = frames.back();
    mapping.erase(pop);
    frames.pop_back();
  }
  frames.push_front(frame_id);
  mapping[frame_id] = 1;
  return;
}

/**
 * TODO: Student Implement
 */
size_t LRUReplacer::Size() {
  return frames.size();
}