#include "concurrency/lock_manager.h"

#include <iostream>
#include <algorithm>
#include <functional>
#include <thread>

#include "common/rowid.h"
#include "concurrency/txn.h"
#include "concurrency/txn_manager.h"

void LockManager::SetTxnMgr(TxnManager *txn_mgr) { txn_mgr_ = txn_mgr; }

/**
 * TODO: Student Implement
 */
bool LockManager::LockShared(Txn *txn, const RowId &rid) {
    if (txn->GetIsolationLevel() == IsolationLevel::kReadUncommitted) {
        txn->SetState(TxnState::kAborted);
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kLockSharedOnReadUncommitted);
    }
    LockPrepare(txn, rid);
    std::unique_lock<std::mutex> lock(latch_);
    LockRequestQueue &queue = lock_table_[rid];

    // Check if txn already holds the lock
    if (txn->GetExclusiveLockSet().count(rid) || txn->GetSharedLockSet().count(rid)) {
        return true;
    }

    queue.EmplaceLockRequest(txn->GetTxnId(), LockMode::kShared);

    while (queue.is_writing_ || queue.is_upgrading_) {
        queue.cv_.wait(lock);
        CheckAbort(txn, queue);
    }

    auto iter = queue.GetLockRequestIter(txn->GetTxnId());
    iter->granted_ = LockMode::kShared;
    queue.sharing_cnt_++;
    txn->GetSharedLockSet().insert(rid);

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::LockExclusive(Txn *txn, const RowId &rid) {
    LockPrepare(txn, rid);
    std::unique_lock<std::mutex> lock(latch_);
    LockRequestQueue &queue = lock_table_[rid];

    if (txn->GetExclusiveLockSet().count(rid)) {
        return true;
    }

    queue.EmplaceLockRequest(txn->GetTxnId(), LockMode::kExclusive);

    while (queue.is_writing_ || queue.sharing_cnt_ > 0 || queue.is_upgrading_) {
        queue.cv_.wait(lock);
        CheckAbort(txn, queue);
    }

    auto iter = queue.GetLockRequestIter(txn->GetTxnId());
    iter->granted_ = LockMode::kExclusive;
    queue.is_writing_ = true;
    txn->GetExclusiveLockSet().insert(rid);

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::LockUpgrade(Txn *txn, const RowId &rid) {
    LockPrepare(txn, rid);
    std::unique_lock<std::mutex> lock(latch_);
    LockRequestQueue &queue = lock_table_[rid];

    if (queue.is_upgrading_) {
        txn->SetState(TxnState::kAborted);
        queue.cv_.notify_all(); // Notify others to check their state
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kUpgradeConflict);
    }

    auto iter_map_idx = queue.req_list_iter_map_.find(txn->GetTxnId());
    ASSERT(iter_map_idx != queue.req_list_iter_map_.end(), "Should have a lock request");
    auto iter = iter_map_idx->second;
    ASSERT(iter->granted_ == LockMode::kShared, "Should hold shared lock before upgrade");

    // Pre-upgrade: release shared lock count
    queue.sharing_cnt_--;
    // txn->GetSharedLockSet().erase(rid); // DON'T ERASE YET, Keep until fully upgraded or aborted
    iter->granted_ = LockMode::kNone;
    iter->lock_mode_ = LockMode::kExclusive;
    queue.is_upgrading_ = true;

    while (queue.sharing_cnt_ > 0 || queue.is_writing_) {
        queue.cv_.wait(lock);
        // Important: check if the txn was aborted while waiting
        if (txn->GetState() == TxnState::kAborted) {
            queue.is_upgrading_ = false; // Reset the flag!
            txn->GetSharedLockSet().erase(rid); // Clean up the lock set upon abort
            queue.EraseLockRequest(txn->GetTxnId());
            queue.cv_.notify_all();
            throw TxnAbortException(txn->GetTxnId(), AbortReason::kDeadlock);
        }
    }

    // Double check if txn is still alive after waking up
    if (txn->GetState() == TxnState::kAborted) {
        queue.is_upgrading_ = false;
        txn->GetSharedLockSet().erase(rid);
        queue.EraseLockRequest(txn->GetTxnId());
        queue.cv_.notify_all();
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kDeadlock);
    }

    iter->granted_ = LockMode::kExclusive;
    queue.is_upgrading_ = false;
    queue.is_writing_ = true;
    txn->GetSharedLockSet().erase(rid); // NOW erase shared
    txn->GetExclusiveLockSet().insert(rid);

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::Unlock(Txn *txn, const RowId &rid) {
    std::unique_lock<std::mutex> lock(latch_);
    auto table_iter = lock_table_.find(rid);
    if (table_iter == lock_table_.end()) {
      return true;
    }
    LockRequestQueue &queue = table_iter->second;

    // Check if txn holding the lock
    auto iter_map_idx = queue.req_list_iter_map_.find(txn->GetTxnId());
    if (iter_map_idx == queue.req_list_iter_map_.end()) {
      return true;
    }
    auto iter = iter_map_idx->second;
    
    // 2PL transition to SHRINKING
    if (txn->GetState() == TxnState::kGrowing) {
        if (!((txn->GetIsolationLevel() == IsolationLevel::kReadCommitted) &&
              (iter->granted_ == LockMode::kShared))) {
            txn->SetState(TxnState::kShrinking);
        }
    }

    if (iter->granted_ == LockMode::kShared) {
        queue.sharing_cnt_--;
        txn->GetSharedLockSet().erase(rid);
    } else if (iter->granted_ == LockMode::kExclusive) {
        queue.is_writing_ = false;
        txn->GetExclusiveLockSet().erase(rid);
    } else if (iter->lock_mode_ == LockMode::kExclusive && queue.is_upgrading_) {
        // Special case: txn is in the middle of Upgrading but Aborted/Unlocked
        queue.is_upgrading_ = false;
    }

    queue.EraseLockRequest(txn->GetTxnId());
    queue.cv_.notify_all();

    return true;
}

/**
 * TODO: Student Implement
 */
void LockManager::LockPrepare(Txn *txn, const RowId &rid) {
    if (txn->GetState() == TxnState::kShrinking) {
        txn->SetState(TxnState::kAborted);
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kLockOnShrinking);
    }
    
    if (txn->GetState() == TxnState::kAborted) {
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kDeadlock); // Or other reason
    }
}

/**
 * TODO: Student Implement
 */
void LockManager::CheckAbort(Txn *txn, LockManager::LockRequestQueue &req_queue) {
    if (txn->GetState() == TxnState::kAborted) {
        auto iter = req_queue.GetLockRequestIter(txn->GetTxnId());
        if (iter->lock_mode_ == LockMode::kExclusive && req_queue.is_upgrading_) {
          // If this was an upgrade request, we MUST reset is_upgrading_
          // However, we need to be careful: only reset if WE were the upgrader.
          // Since only one upgrade can happen at a time, this is safe if we are exclusive and queue.is_upgrading_ is true.
          req_queue.is_upgrading_ = false;
        }
        req_queue.EraseLockRequest(txn->GetTxnId());
        req_queue.cv_.notify_all();
        throw TxnAbortException(txn->GetTxnId(), AbortReason::kDeadlock);
    }
}

/**
 * TODO: Student Implement
 */
void LockManager::AddEdge(txn_id_t t1, txn_id_t t2) {
    waits_for_[t1].insert(t2);
}

/**
 * TODO: Student Implement
 */
void LockManager::RemoveEdge(txn_id_t t1, txn_id_t t2) {
    if (waits_for_.count(t1)) {
        waits_for_[t1].erase(t2);
    }
}

/**
 * TODO: Student Implement
 */
bool LockManager::HasCycle(txn_id_t &newest_tid_in_cycle) {
    visited_set_.clear();
    std::vector<txn_id_t> txn_ids;
    for (auto const& [txn_id, _] : waits_for_) {
        txn_ids.push_back(txn_id);
    }
    std::sort(txn_ids.begin(), txn_ids.end());

    for (auto txn_id : txn_ids) {
        std::unordered_set<txn_id_t> on_stack;
        std::vector<txn_id_t> path;
        
        std::function<bool(txn_id_t)> dfs = [&](txn_id_t u) -> bool {
            visited_set_.insert(u);
            on_stack.insert(u);
            path.push_back(u);

            std::vector<txn_id_t> neighbors(waits_for_[u].begin(), waits_for_[u].end());
            std::sort(neighbors.begin(), neighbors.end());

            for (auto v : neighbors) {
                if (on_stack.count(v)) {
                    // Cycle detected!
                    newest_tid_in_cycle = v;
                    auto it = std::find(path.begin(), path.end(), v);
                    for (; it != path.end(); ++it) {
                        if (*it > newest_tid_in_cycle) {
                            newest_tid_in_cycle = *it;
                        }
                    }
                    return true;
                }
                if (visited_set_.count(v) == 0) {
                    if (dfs(v)) return true;
                }
            }
            on_stack.erase(u);
            path.pop_back();
            return false;
        };

        if (visited_set_.count(txn_id) == 0) {
            if (dfs(txn_id)) return true;
        }
    }
    return false;
}

/**
 * TODO: Student Implement
 */
void LockManager::RunCycleDetection() {
    while (enable_cycle_detection_) {
        std::this_thread::sleep_for(cycle_detection_interval_);
        {
            std::unique_lock<std::mutex> lock(latch_);
            // Build waits-for graph
            waits_for_.clear();
            for (auto const& [rid, queue] : lock_table_) {
                std::vector<txn_id_t> holders;
                for (auto const& req : queue.req_list_) {
                    if (req.granted_ != LockMode::kNone) {
                        holders.push_back(req.txn_id_);
                    }
                }
                for (auto const& req : queue.req_list_) {
                    if (req.granted_ == LockMode::kNone) {
                        for (auto holder : holders) {
                            AddEdge(req.txn_id_, holder);
                        }
                    }
                }
            }

            txn_id_t victim;
            while (HasCycle(victim)) {
                Txn *txn = txn_mgr_->GetTransaction(victim);
                txn->SetState(TxnState::kAborted);
                // Breakdown the node to break cycles
                DeleteNode(victim);
                // Also need to notify the waiting transaction
                for (auto const& [rid, queue] : lock_table_) {
                    for (auto const& req : queue.req_list_) {
                        if (req.txn_id_ == victim) {
                            // The waiting txn will call CheckAbort when notified
                            const_cast<LockRequestQueue&>(queue).cv_.notify_all();
                        }
                    }
                }
            }
        }
    }
}

/**
 * TODO: Student Implement
 */
std::vector<std::pair<txn_id_t, txn_id_t>> LockManager::GetEdgeList() {
    std::vector<std::pair<txn_id_t, txn_id_t>> result;
    for (auto const& [u, neighbors] : waits_for_) {
        for (auto v : neighbors) {
            result.emplace_back(u, v);
        }
    }
    return result;
}

void LockManager::DeleteNode(txn_id_t txn_id) {
    waits_for_.erase(txn_id);

    auto *txn = txn_mgr_->GetTransaction(txn_id);

    for (const auto &row_id: txn->GetSharedLockSet()) {
        for (const auto &lock_req: lock_table_[row_id].req_list_) {
            if (lock_req.granted_ == LockMode::kNone) {
                RemoveEdge(lock_req.txn_id_, txn_id);
            }
        }
    }

    for (const auto &row_id: txn->GetExclusiveLockSet()) {
        for (const auto &lock_req: lock_table_[row_id].req_list_) {
            if (lock_req.granted_ == LockMode::kNone) {
                RemoveEdge(lock_req.txn_id_, txn_id);
            }
        }
    }
}
