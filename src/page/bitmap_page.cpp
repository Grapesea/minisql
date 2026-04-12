#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) { // 这里的offset没有什么用，过程中会被修改.
  if (this->page_allocated_ >= this->GetMaxSupportedSize() || next_free_page_ == -1)
    return false;  // Already full, allocation failed.
  page_offset = next_free_page_; // page_offset is the the parameter to be modified.
  page_allocated_ ++; // page_allocated increases

  uint32_t byte_id = page_offset / 8;
  uint8_t bit_id = page_offset % 8;
  bytes[byte_id] |= (1 << bit_id); // modify specific bit as allocated

  size_t max = this->GetMaxSupportedSize();

  for (uint32_t i = 1; i <= max; i++){  // find next free page
    uint32_t next = (page_offset + i) % max;
    if (IsPageFree(next)){
      next_free_page_ = next;
      break;
    } 
  }

  if (next_free_page_ == page_offset){
    next_free_page_ = -1;
  }
  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  if (page_offset >= GetMaxSupportedSize()){
    return false;
  }
  uint32_t byte_id = page_offset / 8;
  uint8_t bit_id = page_offset % 8;
  auto valid = (bytes[byte_id] >> bit_id) % 2; // this page is not allocated.
  if (page_allocated_ == 0 || valid == 0)    // No allocated page or this page is not allocated.
    return false;
  
  page_allocated_ --;
  if (page_offset < next_free_page_){
    next_free_page_ = page_offset;
  }
  unsigned char res = 0xFF - (1 << bit_id);
  bytes[byte_id] &= res; // making corresponding bit 0
  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  return (((bytes[page_offset / 8] >> (page_offset % 8)) & 1U) == 0); // 0 --> free; 1 --> allocated
}

template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  return (((bytes[byte_index] >> bit_index) & 1U) == 0);
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;