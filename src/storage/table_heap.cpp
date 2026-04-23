#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
bool TableHeap::InsertTuple(Row &row, Txn *txn) { 
  // If the tuple is too large (>= page_size), return false.
  if (row.GetSerializedSize(schema_) >= PAGE_SIZE) return false;

  page_id_t pi = first_page_id_;
  while (pi != INVALID_PAGE_ID) {
    auto *page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(pi));
    if (page == nullptr) return false;
    
    // 1. try to insert in the current page directly.
    page->WLatch(); // add a write latch
    bool inserted = page->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);
    if (inserted) {
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
      return true;
    }

    // 2. get next valid page and continue the loop.
    page_id_t next_pi = page->GetNextPageId();
    if (next_pi != INVALID_PAGE_ID) {
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);
      pi = next_pi;
      continue;
    }

    // 3. All above failed, try to malloc a new page in the buffer pool manager.
    page_id_t new_page_id = INVALID_PAGE_ID; 
    auto *new_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(new_page_id));
    if (new_page == nullptr) { // failed to insert eventually.
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);
      return false;
    }

    // set parameters for the current page and the newly inserted page.
    new_page->WLatch();
    new_page->Init(new_page_id, page->GetTablePageId(), log_manager_, txn);
    page->SetNextPageId(new_page_id);
    bool new_inserted = new_page->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);

    new_page->WUnlatch();
    page->WUnlatch();
    buffer_pool_manager_->UnpinPage(new_page_id, true);
    buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
    return new_inserted;
  }

  return false; // 这个函数真难写，基本上所有bug都在这个地方
}

bool TableHeap::MarkDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  // If the page could not be found, then abort the recovery.
  if (page == nullptr) {
    return false;
  }
  // Otherwise, mark the tuple as deleted.
  page->WLatch();
  page->MarkDelete(rid, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
  return true;
}

/**
 * TODO: Student Implement
 */
bool TableHeap::UpdateTuple(Row &row, const RowId &rid, Txn *txn) { 
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if (page == nullptr) return false;
  page->WLatch();
  Row old_row(rid);

  uint32_t errtype = 0;
  bool f = page->UpdateTuple(row, &old_row, schema_, txn, lock_manager_, log_manager_, &errtype);
  switch (errtype){
      case 3:
        std::cout << "The slot number is invalid, abort." << std::endl;
        break;
      case 4:
       std::cout << "The tuple is deleted, abort." << std::endl;
        break;
      case 5:
        std::cout << "There is not enough space to update, we need to update via delete followed by an insert (not enough space)." << std::endl;
        break;
      default: // In other cases, assertion would throw error messages.
        break;
  }
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), f); 
  // if updated, unpin and mark it dirty.

  return f; 
}

/**
 * TODO: Student Implement
 */
void TableHeap::ApplyDelete(const RowId &rid, Txn *txn) {
  // Step1: Find the page which contains the tuple.
  // Step2: Delete the tuple from the page.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if (page == nullptr) return; // not found.
  
  page->WLatch();
  page->ApplyDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

void TableHeap::RollbackDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  assert(page != nullptr);
  // Rollback to delete.
  page->WLatch();
  page->RollbackDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

/**
 * TODO: Student Implement
 */
bool TableHeap::GetTuple(Row *row, Txn *txn) { 
  if (row == nullptr) {
    return false;
  }
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(row->GetRowId().GetPageId()));
  if (page == nullptr) return false;
  page->RLatch();
  bool f = page->GetTuple(row, schema_, txn, lock_manager_);
  page->RUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);

  return f; 
}

void TableHeap::DeleteTable(page_id_t page_id) {
  if (page_id != INVALID_PAGE_ID) {
    auto temp_table_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));  // 删除table_heap
    if (temp_table_page->GetNextPageId() != INVALID_PAGE_ID)
      DeleteTable(temp_table_page->GetNextPageId());
    buffer_pool_manager_->UnpinPage(page_id, false);
    buffer_pool_manager_->DeletePage(page_id);
  } else {
    DeleteTable(first_page_id_);
  }
}

/**
 * TODO: Student Implement
 */
TableIterator TableHeap::Begin(Txn *txn) { 
  page_id_t page_id = first_page_id_;
  while (page_id != INVALID_PAGE_ID) {
    auto *page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));
    if (page == nullptr) return End();

    RowId first_rid;
    bool f = page->GetFirstTupleRid(&first_rid);
    page_id_t next_page_id = page->GetNextPageId();
    buffer_pool_manager_->UnpinPage(page_id, false);
    if (f) return TableIterator(this, first_rid, txn);
    page_id = next_page_id;
  }

  return End();
}

/**
 * TODO: Student Implement
 */
TableIterator TableHeap::End() { 
  return TableIterator(this, INVALID_ROWID, nullptr);
  // 我想复杂了……突然意识到只要是个invalid就行了.
}
