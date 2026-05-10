#ifndef MINISQL_RECOVERY_MANAGER_H
#define MINISQL_RECOVERY_MANAGER_H

#include <map>
#include <unordered_map>
#include <vector>

#include "recovery/log_rec.h"

using KvDatabase = std::unordered_map<KeyType, ValType>;
using ATT = std::unordered_map<txn_id_t, lsn_t>;

struct CheckPoint {
    lsn_t checkpoint_lsn_{INVALID_LSN};
    ATT active_txns_{};
    KvDatabase persist_data_{};

    inline void AddActiveTxn(txn_id_t txn_id, lsn_t last_lsn) { active_txns_[txn_id] = last_lsn; }

    inline void AddData(KeyType key, ValType val) { persist_data_.emplace(std::move(key), val); }
};

class RecoveryManager {
public:
    /**
    * TODO: Student Implement
    */
    void Init(CheckPoint &last_checkpoint) {
        persist_lsn_ = last_checkpoint.checkpoint_lsn_;
        active_txns_ = last_checkpoint.active_txns_;
        data_ = last_checkpoint.persist_data_;
    }

    /**
    * TODO: Student Implement
    */
    void RedoPhase() {
        for (const auto &entry : log_recs_) {
            const auto &log = entry.second;
            if (persist_lsn_ != INVALID_LSN && log->lsn_ < persist_lsn_) {
                continue;
            }
            switch (log->type_) {
                case LogRecType::kBegin:
                    active_txns_[log->txn_id_] = log->lsn_;
                    break;
                case LogRecType::kCommit:
                    active_txns_.erase(log->txn_id_);
                    break;
                case LogRecType::kAbort:
                    UndoFrom(log->prev_lsn_);
                    active_txns_.erase(log->txn_id_);
                    break;
                case LogRecType::kInsert:
                    data_[log->new_key_] = log->new_val_;
                    active_txns_[log->txn_id_] = log->lsn_;
                    break;
                case LogRecType::kDelete:
                    data_.erase(log->old_key_);
                    active_txns_[log->txn_id_] = log->lsn_;
                    break;
                case LogRecType::kUpdate:
                    if (log->old_key_ != log->new_key_) {
                        data_.erase(log->old_key_);
                        data_[log->new_key_] = log->new_val_;
                    } else {
                        data_[log->new_key_] = log->new_val_;
                    }
                    active_txns_[log->txn_id_] = log->lsn_;
                    break;
                default:
                    break;
            }
        }
    }

    /**
    * TODO: Student Implement
    */
    void UndoPhase() {
        std::vector<txn_id_t> to_undo;
        to_undo.reserve(active_txns_.size());
        for (const auto &entry : active_txns_) {
            to_undo.push_back(entry.first);
        }
        for (auto txn_id : to_undo) {
            auto it = active_txns_.find(txn_id);
            if (it == active_txns_.end()) {
                continue;
            }
            UndoFrom(it->second);
            active_txns_.erase(txn_id);
        }
    }

    // used for test only
    void AppendLogRec(LogRecPtr log_rec) { log_recs_.emplace(log_rec->lsn_, log_rec); }

    // used for test only
    inline KvDatabase &GetDatabase() { return data_; }

private:
    void UndoFrom(lsn_t lsn) {
        lsn_t cursor = lsn;
        while (cursor != INVALID_LSN) {
            auto it = log_recs_.find(cursor);
            if (it == log_recs_.end()) {
                break;
            }
            const auto &log = it->second;
            switch (log->type_) {
                case LogRecType::kInsert:
                    data_.erase(log->new_key_);
                    break;
                case LogRecType::kDelete:
                    data_[log->old_key_] = log->old_val_;
                    break;
                case LogRecType::kUpdate:
                    if (log->old_key_ != log->new_key_) {
                        data_.erase(log->new_key_);
                        data_[log->old_key_] = log->old_val_;
                    } else {
                        data_[log->old_key_] = log->old_val_;
                    }
                    break;
                default:
                    break;
            }
            cursor = log->prev_lsn_;
        }
    }

    std::map<lsn_t, LogRecPtr> log_recs_{};
    lsn_t persist_lsn_{INVALID_LSN};
    ATT active_txns_{};
    KvDatabase data_{};  // all data in database
};

#endif  // MINISQL_RECOVERY_MANAGER_H
