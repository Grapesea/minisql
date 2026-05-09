#include "page/b_plus_tree_leaf_page.h"

#include <algorithm>

#include "index/generic_key.h"

#define pairs_off (data_)
#define pair_size (GetKeySize() + sizeof(RowId))
#define key_off 0
#define val_off GetKeySize()
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * TODO: Student Implement
 */
/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size
 * 未初始化next_page_id
 */
void LeafPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetSize(0);
  SetKeySize(key_size);
  SetMaxSize(max_size);
  SetNextPageId(INVALID_PAGE_ID); // It seems that using INVALID_PAGE_ID is true.
}

/**
 * Helper methods to set/get next page id
 */
page_id_t LeafPage::GetNextPageId() const {
  return next_page_id_;
}

void LeafPage::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
}

/**
 * TODO: Student Implement
 */
/**
 * Helper method to find the first index i so that pairs_[i].first >= key
 * NOTE: This method is only used when generating index iterator
 * 二分查找
 */
int LeafPage::KeyIndex(const GenericKey *key, const KeyManager &KM) {
  int l = 0, r = GetSize();
  while (l < r){
    int mid = l + ((r - l) >> 1);
    if (KM.CompareKeys(KeyAt(mid), key) < 0){
      l = mid + 1;
    } else{
      r = mid;
    }
  }
  return l;
}

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
GenericKey *LeafPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void LeafPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

RowId LeafPage::ValueAt(int index) const {
  return *reinterpret_cast<const RowId *>(pairs_off + index * pair_size + val_off);
}

void LeafPage::SetValueAt(int index, RowId value) {
  *reinterpret_cast<RowId *>(pairs_off + index * pair_size + val_off) = value;
}

void *LeafPage::PairPtrAt(int index) {
  return KeyAt(index);
}

void LeafPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(RowId)));
}
/*
 * Helper method to find and return the key & value pair associated with input
 * "index"(a.k.a. array offset)
 */
std::pair<GenericKey *, RowId> LeafPage::GetItem(int index) { return {KeyAt(index), ValueAt(index)}; }

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert key & value pair into leaf page ordered by key
 * @return page size after insertion
 */

 // memcpy函数： memcpy(dest, key, size)，将size大小的key cpoy给dest.

int LeafPage::Insert(GenericKey *key, const RowId &value, const KeyManager &KM) {
  int key_index = KeyIndex(key, KM);
  int cur_size = GetSize();
  if (key_index < cur_size && KM.CompareKeys(KeyAt(key_index), key) == 0) {
    return cur_size;
  }
  if (key_index < cur_size) {
    memmove(PairPtrAt(key_index + 1), PairPtrAt(key_index), (cur_size - key_index) * pair_size);
  }
  SetKeyAt(key_index, key);
  SetValueAt(key_index, value);
  IncreaseSize(1);
  return GetSize();
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/*
 * Remove half of key & value pairs from this page to "recipient" page
 */
void LeafPage::MoveHalfTo(LeafPage *recipient) {
  int cur_size = GetSize();
  int move_num = cur_size / 2;
  int start_index = cur_size - move_num;
  recipient->CopyNFrom(PairPtrAt(start_index), move_num);
  SetSize(start_index);
  recipient->SetNextPageId(GetNextPageId());
  SetNextPageId(recipient->GetPageId());
}

/*
 * Copy starting from items, and copy {size} number of elements into me.
 */
void LeafPage::CopyNFrom(void *src, int size) {
  int cur_size = GetSize();
  PairCopy(PairPtrAt(cur_size), src, size);
  IncreaseSize(size);
}

/*****************************************************************************
 * LOOKUP
 **********************************************************s*******************/
/*
 * For the given key, check to see whether it exists in the leaf page. If it
 * does, then store its corresponding value in input "value" and return true.
 * If the key does not exist, then return false
 */
bool LeafPage::Lookup(const GenericKey *key, RowId &value, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  int cur = GetSize();
  if (index < cur && KM.CompareKeys(KeyAt(index), key) == 0) { 
    // Find the exact position and compare, return true only if values equal. 
    value = ValueAt(index);
    return true;
  }
  return false;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * First look through leaf page to see whether delete key exist or not. If
 * existed, perform deletion, otherwise return immediately.
 * NOTE: store key&value pair continuously after deletion
 * @return  page size after deletion
 */
int LeafPage::RemoveAndDeleteRecord(const GenericKey *key, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  int cur_size = GetSize();
  // 1. key not found.
  if (index >= cur_size || KM.CompareKeys(KeyAt(index), key) != 0) {
    return cur_size;
  }
  // 2. move forward
  if (index < cur_size - 1) {
    memmove(PairPtrAt(index), PairPtrAt(index + 1), (cur_size - index - 1) * pair_size);
  }
  IncreaseSize(-1);
  return GetSize();
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all key & value pairs from this page to "recipient" page. Don't forget
 * to update the next_page id in the sibling page
 */
void LeafPage::MoveAllTo(LeafPage *recipient) {
  int cur_size = GetSize();
  recipient->CopyNFrom(PairPtrAt(0), cur_size);
  recipient->SetNextPageId(GetNextPageId());
  SetSize(0);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to "recipient" page.
 *
 */
void LeafPage::MoveFirstToEndOf(LeafPage *recipient) {
  GenericKey *first_key = KeyAt(0);
  RowId val = ValueAt(0);
  recipient->CopyLastFrom(first_key, val);
  memmove(PairPtrAt(0), PairPtrAt(1), (GetSize() - 1) * pair_size); // delete the first element in this page.
  
  // recipient->IncreaseSize(1); // this line is absolutely misleading.
  IncreaseSize(-1);
}

/*
 * Copy the item into the end of my item list. (Append item to my array)
 */
void LeafPage::CopyLastFrom(GenericKey *key, const RowId value) {
  int cur_size = GetSize();
  SetKeyAt(cur_size, key);
  SetValueAt(cur_size, value);
  IncreaseSize(1);
}

// 不得不说，这几个函数的命名让人迷惑具体作用，我看了半天也没读懂究竟在说什么.

/*
 * Remove the last key & value pair from this page to the front of "recipient" page.
 */
void LeafPage::MoveLastToFrontOf(LeafPage *recipient) {
  int last = GetSize() - 1;
  GenericKey* last_key = KeyAt(last);
  RowId val = ValueAt(last);
  recipient->CopyFirstFrom(last_key, val);

  IncreaseSize(-1);
}

/*
 * Insert item at the front of my items. Move items accordingly.
 *
 */
void LeafPage::CopyFirstFrom(GenericKey *key, const RowId value) {
  int cur_size = GetSize();
  memmove(PairPtrAt(1), PairPtrAt(0), cur_size * pair_size); // move 1 backwards
  SetKeyAt(0, key);
  SetValueAt(0, value);
  IncreaseSize(1);
}
