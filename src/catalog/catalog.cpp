#include "catalog/catalog.h"

void CatalogMeta::SerializeTo(char *buf) const {
  ASSERT(GetSerializedSize() <= PAGE_SIZE, "Failed to serialize catalog metadata to disk.");
  MACH_WRITE_UINT32(buf, CATALOG_METADATA_MAGIC_NUM);
  buf += 4;
  MACH_WRITE_UINT32(buf, table_meta_pages_.size());
  buf += 4;
  MACH_WRITE_UINT32(buf, index_meta_pages_.size());
  buf += 4;
  for (auto iter : table_meta_pages_) {
    MACH_WRITE_TO(table_id_t, buf, iter.first);
    buf += 4;
    MACH_WRITE_TO(page_id_t, buf, iter.second);
    buf += 4;
  }
  for (auto iter : index_meta_pages_) {
    MACH_WRITE_TO(index_id_t, buf, iter.first);
    buf += 4;
    MACH_WRITE_TO(page_id_t, buf, iter.second);
    buf += 4;
  }
}

CatalogMeta *CatalogMeta::DeserializeFrom(char *buf) {
  // check valid
  uint32_t magic_num = MACH_READ_UINT32(buf);
  buf += 4;
  ASSERT(magic_num == CATALOG_METADATA_MAGIC_NUM, "Failed to deserialize catalog metadata from disk.");
  // get table and index nums
  uint32_t table_nums = MACH_READ_UINT32(buf);
  buf += 4;
  uint32_t index_nums = MACH_READ_UINT32(buf);
  buf += 4;
  // create metadata and read value
  CatalogMeta *meta = new CatalogMeta();
  for (uint32_t i = 0; i < table_nums; i++) {
    auto table_id = MACH_READ_FROM(table_id_t, buf);
    buf += 4;
    auto table_heap_page_id = MACH_READ_FROM(page_id_t, buf);
    buf += 4;
    meta->table_meta_pages_.emplace(table_id, table_heap_page_id);
  }
  for (uint32_t i = 0; i < index_nums; i++) {
    auto index_id = MACH_READ_FROM(index_id_t, buf);
    buf += 4;
    auto index_page_id = MACH_READ_FROM(page_id_t, buf);
    buf += 4;
    meta->index_meta_pages_.emplace(index_id, index_page_id);
  }
  return meta;
}

uint32_t CatalogMeta::GetSerializedSize() const {
  // magic_num(4) + table_nums(4) + index_nums(4)
  // + table_meta_pages_.size() * (table_id(4) + page_id(4))
  // + index_meta_pages_.size() * (index_id(4) + page_id(4))
  return 12 + 8 * table_meta_pages_.size() + 8 * index_meta_pages_.size();
}

CatalogMeta::CatalogMeta() {}

/**
 * TODO: Student Implement
 */
CatalogManager::CatalogManager(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager,
                               LogManager *log_manager, bool init)
    : buffer_pool_manager_(buffer_pool_manager), lock_manager_(lock_manager), log_manager_(log_manager) {
  if (init) {
    catalog_meta_ = CatalogMeta::NewInstance();
    next_table_id_ = 0;
    next_index_id_ = 0;
  } else {
    auto page = buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
    catalog_meta_ = CatalogMeta::DeserializeFrom(page->GetData());
    buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID, false);
    next_table_id_ = catalog_meta_->GetNextTableId();
    next_index_id_ = catalog_meta_->GetNextIndexId();
    for (auto &[table_id, page_id] : *catalog_meta_->GetTableMetaPages()) {
      LoadTable(table_id, page_id);
    }
    for (auto &[index_id, page_id] : *catalog_meta_->GetIndexMetaPages()) {
      LoadIndex(index_id, page_id);
    }
  }
}

CatalogManager::~CatalogManager() {
  FlushCatalogMetaPage();
  delete catalog_meta_;
  for (auto iter : tables_) {
    delete iter.second;
  }
  for (auto iter : indexes_) {
    delete iter.second;
  }
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateTable(const string &table_name, TableSchema *schema, Txn *txn, TableInfo *&table_info) {
  if (table_names_.find(table_name) != table_names_.end()) {
    return DB_TABLE_ALREADY_EXIST;
  }
  table_id_t table_id = next_table_id_++;
  TableSchema *copied_schema = TableSchema::DeepCopySchema(schema);
  auto table_heap = TableHeap::Create(buffer_pool_manager_, copied_schema, txn, log_manager_, lock_manager_);
  page_id_t root_page_id = table_heap->GetFirstPageId();
  auto table_meta = TableMetadata::Create(table_id, table_name, root_page_id, copied_schema);
  page_id_t meta_page_id;
  buffer_pool_manager_->NewPage(meta_page_id);
  auto table_info_obj = TableInfo::Create();
  table_info_obj->Init(table_meta, table_heap);
  table_names_[table_name] = table_id;
  tables_[table_id] = table_info_obj;
  catalog_meta_->GetTableMetaPages()->emplace(table_id, meta_page_id);
  auto meta_page = buffer_pool_manager_->FetchPage(meta_page_id);
  table_meta->SerializeTo(meta_page->GetData());
  buffer_pool_manager_->UnpinPage(meta_page_id, true);
  FlushCatalogMetaPage();
  table_info = table_info_obj;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTable(const string &table_name, TableInfo *&table_info) {
  auto it = table_names_.find(table_name);
  if (it == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  return GetTable(it->second, table_info);
}

dberr_t CatalogManager::GetTables(vector<TableInfo *> &tables) const {
  for (auto &[id, table_info] : tables_) {
    tables.push_back(table_info);
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateIndex(const std::string &table_name, const string &index_name,
                                    const std::vector<std::string> &index_keys, Txn *txn, IndexInfo *&index_info,
                                    const string &index_type) {
  // Check table exists
  auto name_it = table_names_.find(table_name);
  if (name_it == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  table_id_t table_id = name_it->second;
  auto table_it = tables_.find(table_id);
  if (table_it == tables_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  // Check index doesn't already exist
  auto &table_index_names = index_names_[table_name];
  if (table_index_names.find(index_name) != table_index_names.end()) {
    return DB_INDEX_ALREADY_EXIST;
  }
  // Validate index keys exist in table schema
  auto schema = table_it->second->GetSchema();
  std::vector<uint32_t> key_map;
  for (auto &col_name : index_keys) {
    uint32_t col_index;
    if (schema->GetColumnIndex(col_name, col_index) != DB_SUCCESS) {
      return DB_COLUMN_NAME_NOT_EXIST;
    }
    key_map.push_back(col_index);
  }
  // Create index metadata
  index_id_t index_id = next_index_id_++;
  auto index_meta = IndexMetadata::Create(index_id, index_name, table_id, key_map);
  // Allocate page for index metadata
  page_id_t meta_page_id;
  buffer_pool_manager_->NewPage(meta_page_id);
  // Create IndexInfo
  auto index_info_obj = IndexInfo::Create();
  index_info_obj->Init(index_meta, table_it->second, buffer_pool_manager_);
  // Store in maps
  table_index_names[index_name] = index_id;
  indexes_[index_id] = index_info_obj;
  catalog_meta_->GetIndexMetaPages()->emplace(index_id, meta_page_id);
  // Serialize index metadata to page
  auto meta_page = buffer_pool_manager_->FetchPage(meta_page_id);
  index_meta->SerializeTo(meta_page->GetData());
  buffer_pool_manager_->UnpinPage(meta_page_id, true);
  FlushCatalogMetaPage();
  index_info = index_info_obj;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetIndex(const std::string &table_name, const std::string &index_name,
                                 IndexInfo *&index_info) const {
  auto name_it = index_names_.find(table_name);
  if (name_it == index_names_.end()) {
    return DB_INDEX_NOT_FOUND;
  }
  auto idx_it = name_it->second.find(index_name);
  if (idx_it == name_it->second.end()) {
    return DB_INDEX_NOT_FOUND;
  }
  auto info_it = indexes_.find(idx_it->second);
  if (info_it == indexes_.end()) {
    return DB_INDEX_NOT_FOUND;
  }
  index_info = info_it->second;
  return DB_SUCCESS;
}

dberr_t CatalogManager::GetTableIndexes(const std::string &table_name, std::vector<IndexInfo *> &indexes) const {
  auto name_it = index_names_.find(table_name);
  if (name_it == index_names_.end()) {
    return DB_SUCCESS;
  }
  for (auto &[idx_name, idx_id] : name_it->second) {
    auto info_it = indexes_.find(idx_id);
    if (info_it != indexes_.end()) {
      indexes.push_back(info_it->second);
    }
  }
  return DB_SUCCESS;
}

dberr_t CatalogManager::DropTable(const string &table_name) {
  auto it = table_names_.find(table_name);
  if (it == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  return DropTable(it->second);
}

dberr_t CatalogManager::DropIndex(const string &table_name, const string &index_name) {
  auto name_it = index_names_.find(table_name);
  if (name_it == index_names_.end()) {
    return DB_INDEX_NOT_FOUND;
  }
  auto idx_it = name_it->second.find(index_name);
  if (idx_it == name_it->second.end()) {
    return DB_INDEX_NOT_FOUND;
  }
  index_id_t index_id = idx_it->second;
  // Delete from indexes
  auto info_it = indexes_.find(index_id);
  if (info_it != indexes_.end()) {
    delete info_it->second;
    indexes_.erase(info_it);
  }
  // Delete from catalog meta
  catalog_meta_->DeleteIndexMetaPage(buffer_pool_manager_, index_id);
  // Remove from index names
  name_it->second.erase(idx_it);
  if (name_it->second.empty()) {
    index_names_.erase(name_it);
  }
  FlushCatalogMetaPage();
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::FlushCatalogMetaPage() const {
  auto page = buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
  if (page == nullptr) {
    return DB_FAILED;
  }
  catalog_meta_->SerializeTo(page->GetData());
  buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID, true);
  return DB_SUCCESS;
}

dberr_t CatalogManager::LoadTable(const table_id_t table_id, const page_id_t page_id) {
  auto page = buffer_pool_manager_->FetchPage(page_id);
  TableMetadata *table_meta = nullptr;
  TableMetadata::DeserializeFrom(page->GetData(), table_meta);
  buffer_pool_manager_->UnpinPage(page_id, false);
  auto table_heap = TableHeap::Create(buffer_pool_manager_, table_meta->GetFirstPageId(), table_meta->GetSchema(),
                                       log_manager_, lock_manager_);
  auto table_info = TableInfo::Create();
  table_info->Init(table_meta, table_heap);
  table_names_[table_meta->GetTableName()] = table_id;
  tables_[table_id] = table_info;
  return DB_SUCCESS;
}

dberr_t CatalogManager::LoadIndex(const index_id_t index_id, const page_id_t page_id) {
  auto page = buffer_pool_manager_->FetchPage(page_id);
  IndexMetadata *index_meta = nullptr;
  IndexMetadata::DeserializeFrom(page->GetData(), index_meta);
  buffer_pool_manager_->UnpinPage(page_id, false);
  table_id_t table_id = index_meta->GetTableId();
  TableInfo *table_info = nullptr;
  GetTable(table_id, table_info);
  if (table_info == nullptr) {
    return DB_FAILED;
  }
  auto index_info = IndexInfo::Create();
  index_info->Init(index_meta, table_info, buffer_pool_manager_);
  index_names_[table_info->GetTableName()][index_meta->GetIndexName()] = index_id;
  indexes_[index_id] = index_info;
  return DB_SUCCESS;
}

dberr_t CatalogManager::GetTable(const table_id_t table_id, TableInfo *&table_info) {
  auto it = tables_.find(table_id);
  if (it == tables_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  table_info = it->second;
  return DB_SUCCESS;
}

dberr_t CatalogManager::DropTable(table_id_t table_id) {
  auto it = tables_.find(table_id);
  if (it == tables_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  TableInfo *table_info = it->second;
  std::string table_name = table_info->GetTableName();
  // Delete all indexes on this table
  auto name_it = index_names_.find(table_name);
  if (name_it != index_names_.end()) {
    for (auto &[idx_name, idx_id] : name_it->second) {
      auto idx_it = indexes_.find(idx_id);
      if (idx_it != indexes_.end()) {
        delete idx_it->second;
        indexes_.erase(idx_it);
      }
      catalog_meta_->DeleteIndexMetaPage(buffer_pool_manager_, idx_id);
    }
    index_names_.erase(name_it);
  }
  // Delete table heap
  table_info->GetTableHeap()->DeleteTable();
  // Delete table metadata page
  catalog_meta_->DeleteTableMetaPage(buffer_pool_manager_, table_id);
  // Remove from maps
  table_names_.erase(table_name);
  tables_.erase(table_id);
  delete table_info;
  FlushCatalogMetaPage();
  return DB_SUCCESS;
}