// Copyright (c) 2017, Los Alamos National Security, LLC.
// All rights reserved.
#ifndef map_datastore_h
#define map_datastore_h

#include <map>
#include <cstring>
#include "kv-config.h"
#include "bulk.h"
#include "datastore/datastore.h"

class MapDataStore : public AbstractDataStore {

    private:

        struct keycmp {
            MapDataStore* _store;
            keycmp(MapDataStore* store)
                : _store(store) {}
            bool operator()(const ds_bulk_t& a, const ds_bulk_t& b) const {
                if(_store->_less)
                    return _store->_less((const void*)a.data(),
                            a.size(), (const void*)b.data(), b.size()) < 0;
                else
                    return std::less<ds_bulk_t>()(a,b);
            }
        };

    public:

        MapDataStore()
            : AbstractDataStore(), _less(nullptr), _map(keycmp(this)) {
            ABT_rwlock_create(&_map_lock);
        }

        MapDataStore(Duplicates duplicates, bool eraseOnGet, bool debug)
            : AbstractDataStore(duplicates, eraseOnGet, debug), _less(nullptr), _map(keycmp(this)){
            ABT_rwlock_create(&_map_lock);
        }

        ~MapDataStore() {
            ABT_rwlock_free(&_map_lock);
        }

        virtual bool openDatabase(const std::string& db_name, const std::string& path) {
            _name = db_name;
            _path = path;
            ABT_rwlock_wrlock(_map_lock);
            _map.clear();
            ABT_rwlock_unlock(_map_lock);
            return true;
        }

        virtual void sync() {}

        virtual bool put(const ds_bulk_t &key, const ds_bulk_t &data) {
            ABT_rwlock_wrlock(_map_lock);
            auto x = _map.count(key);
            if(_no_overwrite && (x != 0)) {
                ABT_rwlock_unlock(_map_lock);
                return false;
            }
            if(_duplicates == Duplicates::IGNORE && (x != 0)) {
                ABT_rwlock_unlock(_map_lock);
                return false;
            }
            _map.insert(std::make_pair(key,data));
            ABT_rwlock_unlock(_map_lock);
            return true;
        }

        virtual bool get(const ds_bulk_t &key, ds_bulk_t &data) {
            ABT_rwlock_rdlock(_map_lock);
            auto it = _map.find(key);
            if(it == _map.end()) {
                ABT_rwlock_unlock(_map_lock);
                return false;
            }
            data = it->second;
            ABT_rwlock_unlock(_map_lock);
            return true;
        }

        virtual bool get(const ds_bulk_t &key, std::vector<ds_bulk_t>& values) {
            values.clear();
            values.resize(1);
            return get(key, values[0]);
        }

        virtual bool exists(const ds_bulk_t& key) {
            ABT_rwlock_rdlock(_map_lock);
            bool e = _map.count(key) > 0;
            ABT_rwlock_unlock(_map_lock);
            return e;
        }

        virtual bool erase(const ds_bulk_t &key) {
            ABT_rwlock_wrlock(_map_lock);
            bool b = _map.find(key) != _map.end();
            _map.erase(key);
            ABT_rwlock_unlock(_map_lock);
            return b;
        }

        virtual void set_in_memory(bool enable) {
            _in_memory = enable;
        }

        virtual void set_comparison_function(const std::string& name, comparator_fn less) {
           _comp_fun_name = name;
           _less = less; 
        }

        virtual void set_no_overwrite() {
            _no_overwrite = true;
        }

#ifdef USE_REMI
        virtual remi_fileset_t create_and_populate_fileset() const {
            return REMI_FILESET_NULL;
        }
#endif

    protected:

        virtual std::vector<ds_bulk_t> vlist_keys(
                const ds_bulk_t &start_key, size_t count, const ds_bulk_t &prefix) const {
            ABT_rwlock_rdlock(_map_lock);
            std::vector<ds_bulk_t> result;
            decltype(_map.begin()) it;
            if(start_key.size() > 0) {
                it = _map.upper_bound(start_key);
            } else {
                it = _map.begin();
            }
            while(result.size() < count && it != _map.end()) {
                const auto& p = *it;
                if(prefix.size() > p.first.size()) continue;
                int c = std::memcmp(prefix.data(), p.first.data(), prefix.size());
                if(c == 0) {
                    result.push_back(p.first);
                } else if(c < 0) {
                    break; // we have exceeded prefix
                }
                it++;
            }
            ABT_rwlock_unlock(_map_lock);
            return result;
        }

        virtual std::vector<std::pair<ds_bulk_t,ds_bulk_t>> vlist_keyvals(
                const ds_bulk_t &start_key, size_t count, const ds_bulk_t &prefix) const {
            ABT_rwlock_rdlock(_map_lock);
            std::vector<std::pair<ds_bulk_t,ds_bulk_t>> result;
            decltype(_map.begin()) it;
            if(start_key.size() > 0) {
                it = _map.upper_bound(start_key);
            } else {
                it = _map.begin();
            }
            while(result.size() < count && it != _map.end()) {
                const auto& p = *it;
                if(prefix.size() > p.first.size()) continue;
                int c = std::memcmp(prefix.data(), p.first.data(), prefix.size());
                if(c == 0) {
                    result.push_back(p);
                } else if(c < 0) {
                    break; // we have exceeded prefix
                }
                it++;
            }
            ABT_rwlock_unlock(_map_lock);
            return result;
        }

        virtual std::vector<ds_bulk_t> vlist_key_range(
                const ds_bulk_t &lower_bound, const ds_bulk_t &upper_bound, size_t max_keys) const {
            ABT_rwlock_rdlock(_map_lock);
            std::vector<ds_bulk_t> result;
            decltype(_map.begin()) it, ub;
            // get the first element that goes immediately after lower_bound
            it = _map.upper_bound(lower_bound);
            if(it == _map.end()) {
                return result;
            }
            // get the element that goes immediately before upper bound
            ub = _map.lower_bound(upper_bound);
            if(ub->first != upper_bound) ub++;

            while(it != ub) {
                result.push_back(it->second);
                it++;
                if(max_keys != 0 && result.size() == max_keys)
                    break;
            }
            ABT_rwlock_unlock(_map_lock);
            return result;
        }

        virtual std::vector<std::pair<ds_bulk_t,ds_bulk_t>> vlist_keyval_range(
                const ds_bulk_t &lower_bound, const ds_bulk_t& upper_bound, size_t max_keys) const {
            ABT_rwlock_rdlock(_map_lock);
            std::vector<std::pair<ds_bulk_t,ds_bulk_t>> result;
            decltype(_map.begin()) it, ub;
            // get the first element that goes immediately after lower_bound
            it = _map.upper_bound(lower_bound);
            if(it == _map.end()) {
                return result;
            }
            // get the element that goes immediately before upper bound
            ub = _map.lower_bound(upper_bound);
            if(ub->first != upper_bound) ub++;

            while(it != ub) {
                result.emplace_back(it->first,it->second);
                it++;
                if(max_keys != 0 && result.size() == max_keys)
                    break;
            }
            ABT_rwlock_unlock(_map_lock);
            return result;
        }

    private:
        AbstractDataStore::comparator_fn _less;
        std::map<ds_bulk_t, ds_bulk_t, keycmp> _map;
        ABT_rwlock _map_lock;
};

#endif
