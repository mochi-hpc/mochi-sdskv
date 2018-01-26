#ifndef datastore_factory_h
#define datastore_factory_h

#ifdef SDSKV
#include "sdskv-common.h"
#else
#include "sds-keyval.h"
#endif
#include "datastore.h"

#ifdef USE_BWTREE
#include "bwtree_datastore.h"
#endif

#ifdef USE_BDB
#include "berkeleydb_datastore.h"
#endif

#ifdef USE_LEVELDB
#include "leveldb_datastore.h"
#endif

class datastore_factory {

    static AbstractDataStore* create_bwtree_datastore() {
#ifdef USE_BWTREE
        return new BwTreeDataStore();
#else
        return nullptr;
#endif
    }

    static AbstractDataStore* create_berkeleydb_datastore() {
#ifdef USE_BDB
        return new BerkeleyDBDataStore();
#else
        return nullptr;
#endif
    }

    static AbstractDataStore* create_leveldb_datastore() {
#ifdef USE_LEVELDB
        return new LevelDBDataStore();
#else
        return nullptr;
#endif
    }

    public:

#ifdef SDSKV
    static AbstractDataStore* create_datastore(sdskv_db_type_t type)
#else
    static AbstractDataStore* create_datastore(kv_db_type_t type)
#endif
    {
        switch(type) {
            case KVDB_BWTREE:
                return create_bwtree_datastore();
            case KVDB_LEVELDB:
                return create_berkeleydb_datastore();
            case KVDB_BERKELEYDB:
                return create_leveldb_datastore();
        }
        return nullptr;
    };

};

#endif
