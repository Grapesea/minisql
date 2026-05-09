#include "storage/disk_manager.h"

#include <sys/stat.h>

#include <filesystem>
#include <stdexcept>

#include "glog/logging.h"
#include "page/bitmap_page.h"

DiskManager::DiskManager(const std::string &db_file) : file_name_(db_file) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
  // directory or file does not exist
  if (!db_io_.is_open()) {
    db_io_.clear();
    // create a new file
    std::filesystem::path p = db_file;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    db_io_.open(db_file, std::ios::binary | std::ios::trunc | std::ios::out);
    db_io_.close();
    // reopen with original mode
    db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_io_.is_open()) {
      throw std::exception();
    }
  }
  ReadPhysicalPage(META_PAGE_ID, meta_data_);
}

void DiskManager::Close() {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  WritePhysicalPage(META_PAGE_ID, meta_data_);
  if (!closed) {
    db_io_.flush();
    db_io_.close();
    closed = true;
  }
}

void DiskManager::ReadPage(page_id_t logical_page_id, char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  ReadPhysicalPage(MapPageId(logical_page_id), page_data);
}

void DiskManager::WritePage(page_id_t logical_page_id, const char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  WritePhysicalPage(MapPageId(logical_page_id), page_data);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::AllocatePage() {
  // ASSERT(false, "Not implemented yet.");
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  auto *meta = reinterpret_cast<DiskFileMetaPage *>(meta_data_);

  uint32_t exts = meta->GetExtentNums();
  page_id_t allocate_id;
  
  for (uint32_t i = 0; i < exts; i++){ // find the first free page 
    char page_data[PAGE_SIZE] = {0};
    page_id_t bitmap_physical_page_id = i * (BITMAP_SIZE + 1) + 1;
    // get the i-th extent metadata bitmap data.
    ReadPhysicalPage(bitmap_physical_page_id, page_data); 
    
    auto* bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(page_data);
    uint32_t page_offset = 0;

    if (bitmap->AllocatePage(page_offset)){ // allocation succeeds
      WritePhysicalPage(bitmap_physical_page_id, page_data);
      meta->extent_used_page_[i]++;
      meta->num_allocated_pages_++;
      WritePhysicalPage(META_PAGE_ID, meta_data_);
      allocate_id = i * BITMAP_SIZE + page_offset; // logical addr
      return allocate_id;
    }
  }
  // If no more free pages in extent, calculate out the addr of newly built extent 
  uint32_t new_bitmap_addr = 1 + exts * (BITMAP_SIZE + 1); // physical addr
  char bitmap_data[PAGE_SIZE] = {0};
  auto* bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE>*>(bitmap_data);

  // write specific data 
  uint32_t page_offset = 0;
  bool ok = bitmap->AllocatePage(page_offset);
  ASSERT(ok, "New bitmap page should have free slot.");
  
  WritePhysicalPage(new_bitmap_addr, bitmap_data); 

  meta->num_extents_++;
  meta->extent_used_page_[exts]++;
  meta->num_allocated_pages_++;
  WritePhysicalPage(META_PAGE_ID, meta_data_);
  allocate_id = exts * BITMAP_SIZE + page_offset;
  return allocate_id;
}

/**
 * TODO: Student Implement
 */
void DiskManager::DeAllocatePage(page_id_t logical_page_id) {
  // ASSERT(false, "Not implemented yet.");
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  auto *meta = reinterpret_cast<DiskFileMetaPage *>(meta_data_);
  char bitmap_data[PAGE_SIZE] = {0};

  // read the bitmap data.
  uint32_t index = logical_page_id / BITMAP_SIZE;
  uint32_t offset = logical_page_id % BITMAP_SIZE;
  page_id_t bitmap_physical_page_id = 1 + index * (1 + BITMAP_SIZE); 
  ReadPhysicalPage(bitmap_physical_page_id, bitmap_data); // read the bitmap
  
  // deallocate on the bitmap.
  auto* bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(bitmap_data);

  if (bitmap->DeAllocatePage(offset)){
    WritePhysicalPage(bitmap_physical_page_id, bitmap_data);
    // decrement 2 params, Caution: extents numbers shouldn't decrement.
    meta->extent_used_page_[index]--;
    meta->num_allocated_pages_--;
    WritePhysicalPage(META_PAGE_ID, meta_data_);
  }
}

/**
 * TODO: Student Implement
 */
bool DiskManager::IsPageFree(page_id_t logical_page_id) {
  page_id_t physical_id = MapPageId(logical_page_id);
  uint32_t index = physical_id / (1+BITMAP_SIZE);

  page_id_t physical_id_bitmap = index * (BITMAP_SIZE + 1) + 1;
  uint32_t offset = physical_id - physical_id_bitmap - 1;

  char bitmap_page_data[PAGE_SIZE];
  ReadPhysicalPage(physical_id_bitmap, bitmap_page_data);
  auto* bitmap = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(bitmap_page_data);

  return bitmap->IsPageFree(offset);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::MapPageId(page_id_t logical_page_id) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");

  uint32_t extent_id = logical_page_id / BITMAP_SIZE;
  uint32_t page_offset = logical_page_id % BITMAP_SIZE;

  return 1 + extent_id * (BITMAP_SIZE + 1) + 1 + page_offset;
}

int DiskManager::GetFileSize(const std::string &file_name) {
  struct stat stat_buf;
  int rc = stat(file_name.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void DiskManager::ReadPhysicalPage(page_id_t physical_page_id, char *page_data) {
  int offset = physical_page_id * PAGE_SIZE;
  // check if read beyond file length
  if (offset >= GetFileSize(file_name_)) {
#ifdef ENABLE_BPM_DEBUG
    LOG(INFO) << "Read less than a page" << std::endl;
#endif
    memset(page_data, 0, PAGE_SIZE);
  } else {
    // set read cursor to offset
    db_io_.seekp(offset);
    db_io_.read(page_data, PAGE_SIZE);
    // if file ends before reading PAGE_SIZE
    int read_count = db_io_.gcount();
    if (read_count < PAGE_SIZE) {
#ifdef ENABLE_BPM_DEBUG
      LOG(INFO) << "Read less than a page" << std::endl;
#endif
      memset(page_data + read_count, 0, PAGE_SIZE - read_count);
    }
  }
}

void DiskManager::WritePhysicalPage(page_id_t physical_page_id, const char *page_data) {
  size_t offset = static_cast<size_t>(physical_page_id) * PAGE_SIZE;
  // set write cursor to offset
  db_io_.seekp(offset);
  db_io_.write(page_data, PAGE_SIZE);
  // check for I/O error
  if (db_io_.bad()) {
    LOG(ERROR) << "I/O error while writing";
    return;
  }
  // needs to flush to keep disk file in sync
  // db_io_.flush(); // Modified here, to reduce I/O write/read.
}