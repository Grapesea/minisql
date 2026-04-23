#include "storage/table_iterator.h"

#include "common/macros.h"
#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
TableIterator::TableIterator(TableHeap *table_heap, RowId rid, Txn *txn)
    : table_heap_(table_heap), rid_(rid), txn_(txn), row_(rid) {}

TableIterator::TableIterator(const TableIterator &other):
  table_heap_(other.table_heap_),
  rid_(other.rid_),
  txn_(other.txn_),
  row_(other.rid_){}
// 这里遇到一个拷贝bug，稍微修了一会，row_作为一个缓存最好不要直接存row，而是先存rid_，解引用的时候再改成row.

TableIterator::~TableIterator() {}

bool TableIterator::operator==(const TableIterator &itr) const {
  return (table_heap_ == itr.table_heap_ && rid_ == itr.rid_);
}

bool TableIterator::operator!=(const TableIterator &itr) const {
  return (table_heap_ != itr.table_heap_ || !(rid_ == itr.rid_));
  // Overloading for RowId '==' exists but '!=' doesn't.
}

const Row &TableIterator::operator*() {
  ASSERT(rid_.Get() != INVALID_ROWID.Get(), "Dereference end iterator.");
  row_.SetRowId(rid_);
  if (table_heap_ != nullptr) {
    table_heap_->GetTuple(&row_, txn_);
  }
  return row_;
}

Row *TableIterator::operator->() {
  operator*(); // renew row_ before derefence.
  return &row_;
}

TableIterator &TableIterator::operator=(const TableIterator &itr) noexcept {
  if (this == &itr) {
    return *this;
  }
  table_heap_ = itr.table_heap_;
  rid_ = itr.rid_;
  txn_ = itr.txn_;
  row_.SetRowId(rid_);
  return *this;
}

// ++iter
TableIterator &TableIterator::operator++() {
  if (table_heap_ == nullptr || rid_ == INVALID_ROWID) {
    return *this;
  }
  auto *buffer_pool_manager = table_heap_->buffer_pool_manager_;
  page_id_t page_id = rid_.GetPageId();

  auto *page = reinterpret_cast<TablePage *>(buffer_pool_manager->FetchPage(page_id));
  if (page == nullptr) {
    rid_ = INVALID_ROWID;
    row_.SetRowId(INVALID_ROWID);
    return *this;
  }

  RowId next_rid;
  if (page->GetNextTupleRid(rid_, &next_rid)) {
    buffer_pool_manager->UnpinPage(page_id, false);
    rid_ = next_rid;
    row_.SetRowId(rid_);
    return *this;
  }

  page_id_t next_page_id = page->GetNextPageId();
  buffer_pool_manager->UnpinPage(page_id, false);
  while (next_page_id != INVALID_PAGE_ID) {
    auto *next_page = reinterpret_cast<TablePage *>(buffer_pool_manager->FetchPage(next_page_id));
    if (next_page == nullptr) {
      rid_ = INVALID_ROWID;
      row_.SetRowId(INVALID_ROWID);
      return *this;
    }

    RowId first_rid;
    if (next_page->GetFirstTupleRid(&first_rid)) {
      buffer_pool_manager->UnpinPage(next_page_id, false);
      rid_ = first_rid;
      row_.SetRowId(rid_);
      return *this;
    }
    page_id_t candidate = next_page->GetNextPageId();
    buffer_pool_manager->UnpinPage(next_page_id, false);
    next_page_id = candidate;
  }

  rid_ = INVALID_ROWID;
  row_.SetRowId(INVALID_ROWID);
  return *this;
}

// iter++
TableIterator TableIterator::operator++(int) { 
  TableIterator tmp(*this);
  ++(*this);
  return TableIterator(tmp); // explicit ctor, must be initialized in this way.
}
