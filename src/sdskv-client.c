#include "sdskv-client.h"
#include "sdskv-rpc-types.h"

#define MAX_RPC_MESSAGE_SIZE 4000 // in bytes

struct sdskv_client {
    margo_instance_id mid;

    hg_id_t sdskv_put_id;
    hg_id_t sdskv_bulk_put_id;
    hg_id_t sdskv_get_id;
    hg_id_t sdskv_erase_id;
    hg_id_t sdskv_length_id;
    hg_id_t sdskv_bulk_get_id;
    hg_id_t sdskv_open_id;
    hg_id_t sdskv_list_keys_id;
    hg_id_t sdskv_list_keyvals_id;

    uint64_t num_provider_handles;
};

struct sdskv_provider_handle {
    sdskv_client_t client;
    hg_addr_t      addr;
    uint8_t        mplex_id;
    uint64_t       refcount;
};

static int sdskv_client_register(sdskv_client_t client, margo_instance_id mid)
{
    client->mid = mid;

    /* check if RPCs have already been registered */
    hg_bool_t flag;
    hg_id_t id;
    margo_registered_name(mid, "sdskv_put_rpc", &id, &flag);

    if(flag == HG_TRUE) { /* RPCs already registered */

        margo_registered_name(mid, "sdskv_put_rpc",      &client->sdskv_put_id,      &flag);
        margo_registered_name(mid, "sdskv_bulk_put_rpc", &client->sdskv_bulk_put_id, &flag);
        margo_registered_name(mid, "sdskv_get_rpc",      &client->sdskv_get_id,      &flag);
        margo_registered_name(mid, "sdskv_erase_rpc",    &client->sdskv_erase_id,    &flag);
        margo_registered_name(mid, "sdskv_length_rpc",   &client->sdskv_length_id,   &flag);
        margo_registered_name(mid, "sdskv_bulk_get_rpc", &client->sdskv_bulk_get_id, &flag);
        margo_registered_name(mid, "sdskv_open_rpc",     &client->sdskv_open_id,     &flag);
        margo_registered_name(mid, "sdskv_list_keys_rpc", &client->sdskv_list_keys_id, &flag);
        margo_registered_name(mid, "sdskv_list_keyvals_rpc", &client->sdskv_list_keyvals_id, &flag);

    } else {

        client->sdskv_put_id =
            MARGO_REGISTER(mid, "sdskv_put_rpc", put_in_t, put_out_t, NULL);
        client->sdskv_bulk_put_id =
            MARGO_REGISTER(mid, "sdskv_bulk_put_rpc", bulk_put_in_t, bulk_put_out_t, NULL);
        client->sdskv_get_id =
            MARGO_REGISTER(mid, "sdskv_get_rpc", get_in_t, get_out_t, NULL);
        client->sdskv_erase_id =
            MARGO_REGISTER(mid, "sdskv_erase_rpc", erase_in_t, erase_out_t, NULL);
        client->sdskv_length_id =
            MARGO_REGISTER(mid, "sdskv_length_rpc", length_in_t, length_out_t, NULL);
        client->sdskv_bulk_get_id =
            MARGO_REGISTER(mid, "sdskv_bulk_get_rpc", bulk_get_in_t, bulk_get_out_t, NULL);
        client->sdskv_open_id =
            MARGO_REGISTER(mid, "sdskv_open_rpc", open_in_t, open_out_t, NULL);
        client->sdskv_list_keys_id =
            MARGO_REGISTER(mid, "sdskv_list_keys_rpc", list_keys_in_t, list_keys_out_t, NULL);
        client->sdskv_list_keyvals_id =
            MARGO_REGISTER(mid, "sdskv_list_keyvals_rpc", list_keyvals_in_t, list_keyvals_out_t, NULL);
    }

    return 0;
}

int sdskv_client_init(margo_instance_id mid, sdskv_client_t* client)
{
    sdskv_client_t c = (sdskv_client_t)calloc(1, sizeof(*c));
    if(!c) return -1;

    c->num_provider_handles = 0;

    int ret = sdskv_client_register(c, mid);
    if(ret != 0) return ret;

    *client = c;
    return 0;
}

int sdskv_client_finalize(sdskv_client_t client)
{
    if(client->num_provider_handles != 0) {
        fprintf(stderr,
                "[SDSKV] Warning: %d provider handles not released before sdskv_client_finalize was called\n",
                client->num_provider_handles);
    }
    free(client);
    return 0;
}

int sdskv_provider_handle_create(
        sdskv_client_t client,
        hg_addr_t addr,
        uint8_t mplex_id,
        sdskv_provider_handle_t* handle)
{
    if(client == SDSKV_CLIENT_NULL) return -1;

    sdskv_provider_handle_t provider =
        (sdskv_provider_handle_t)calloc(1, sizeof(*provider));

    if(!provider) return -1;

    hg_return_t ret = margo_addr_dup(client->mid, addr, &(provider->addr));
    if(ret != HG_SUCCESS) {
        free(provider);
        return -1;
    }

    provider->client   = client;
    provider->mplex_id = mplex_id;
    provider->refcount = 1;

    client->num_provider_handles += 1;

    *handle = provider;
    return 0;
}

int sdskv_provider_handle_ref_incr(
        sdskv_provider_handle_t handle)
{
    if(handle == SDSKV_PROVIDER_HANDLE_NULL) return -1;
    handle->refcount += 1;
    return 0;
}

int sdskv_provider_handle_release(sdskv_provider_handle_t handle)
{
    if(handle == SDSKV_PROVIDER_HANDLE_NULL) return -1;
    handle->refcount -= 1;
    if(handle->refcount == 0) {
        margo_addr_free(handle->client->mid, handle->addr);
        handle->client->num_provider_handles -= 1;
        free(handle);
    }
    return 0;
}

int sdskv_open(
        sdskv_provider_handle_t provider,
        const char* db_name,
        sdskv_database_id_t* db_id)
{
    hg_return_t hret;
    int ret;
    open_in_t in;
    open_out_t out;
    hg_handle_t handle;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_open_id,
            &handle);
    if(hret != HG_SUCCESS) return -1;

    hret = margo_set_target_id(handle, provider->mplex_id);

    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    in.name = (char*)db_name;

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    ret = out.ret;
    *db_id = out.db_id;

    margo_free_output(handle, &out);
    margo_destroy(handle);

    return ret;
}

int sdskv_put(sdskv_provider_handle_t provider, 
        sdskv_database_id_t db_id,
        const void *key, hg_size_t ksize,
        const void *value, hg_size_t vsize)
{
    hg_return_t hret;
    int ret;
    hg_handle_t handle;

    hg_size_t msize = ksize + vsize + 2*sizeof(hg_size_t);

    if(msize <= MAX_RPC_MESSAGE_SIZE) {

        put_in_t in;
        put_out_t out;

        in.db_id = db_id;
        in.key   = (kv_data_t)key;
        in.ksize = ksize;
        in.value = (kv_data_t)value;
        in.vsize = vsize;

        /* create handle */
        hret = margo_create(
                provider->client->mid,
                provider->addr,
                provider->client->sdskv_put_id,
                &handle);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_create() failed in sdskv_put()\n");
            return -1;
        }

        hret = margo_set_target_id(handle, provider->mplex_id);

        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_set_target_id() failed in sdskv_put()\n");
            margo_destroy(handle);
            return -1;
        }

        hret = margo_forward(handle, &in);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_forward() failed in sdskv_put()\n");
            margo_destroy(handle);
            return -1;
        }

        hret = margo_get_output(handle, &out);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_get_output() failed in sdskv_put()\n");
            margo_destroy(handle);
            return -1;
        }

        ret = out.ret;

        margo_free_output(handle, &out);

    } else {

        bulk_put_in_t in;
        bulk_put_out_t out;

        in.bulk.db_id = db_id;
        in.bulk.key   = (kv_data_t)key;
        in.bulk.ksize = ksize;
        in.bulk.vsize = vsize;

        hret = margo_bulk_create(provider->client->mid, 1, (void**)(&value), &in.bulk.vsize,
                                HG_BULK_READ_ONLY, &in.bulk.handle);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_bulk_create() failed in sdskv_put()\n");
            return -1;
        }

        /* create handle */
        hret = margo_create(
                provider->client->mid,
                provider->addr,
                provider->client->sdskv_bulk_put_id,
                &handle);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_create() failed in sdskv_put()\n");
            margo_bulk_free(in.bulk.handle);
            return -1;
        }

        hret = margo_set_target_id(handle, provider->mplex_id);

        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_set_target_id() failed in sdskv_put()\n");
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        hret = margo_forward(handle, &in);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_forward() failed in sdskv_put()\n");
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        hret = margo_get_output(handle, &out);
        if(hret != HG_SUCCESS) {
            fprintf(stderr,"[SDSKV] margo_get_output() failed in sdskv_put()\n");
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        ret = out.ret;
        margo_free_output(handle, &out);
        margo_bulk_free(in.bulk.handle);
    }

    margo_destroy(handle);
    return ret;
}

int sdskv_get(sdskv_provider_handle_t provider,
        sdskv_database_id_t db_id, 
        const void *key, hg_size_t ksize,
        void *value, hg_size_t* vsize)
{
    hg_return_t hret;
    hg_size_t size;
    hg_size_t msize;
    int ret;
    hg_handle_t handle;

    size = *(hg_size_t*)vsize;
    msize = size + sizeof(hg_size_t) + sizeof(hg_return_t);

    if (msize <= MAX_RPC_MESSAGE_SIZE) {

        get_in_t in;
        get_out_t out;

        in.db_id = db_id;
        in.key = (kv_data_t)key;
        in.ksize = ksize;
        in.vsize = size;

        /* create handle */
        hret = margo_create(
                provider->client->mid,
                provider->addr,
                provider->client->sdskv_get_id,
                &handle);
        if(hret != HG_SUCCESS) return -1;

        hret = margo_set_target_id(handle, provider->mplex_id);

        if(hret != HG_SUCCESS) {
            margo_destroy(handle);
            return -1;
        }

        hret = margo_forward(handle, &in);
        if(hret != HG_SUCCESS) {
            margo_destroy(handle);
            return -1;
        }

        hret = margo_get_output(handle, &out);
        if(hret != HG_SUCCESS) {
            margo_destroy(handle);
            return -1;
        }

        ret = out.ret;
        *vsize = (hg_size_t)out.vsize;

        if (out.vsize > 0) {
            memcpy(value, out.value, out.vsize);
        }

        margo_free_output(handle, &out);

    } else {

        bulk_get_in_t in;
        bulk_get_out_t out;

        in.bulk.db_id = db_id;
        in.bulk.key = (kv_data_t)key;
        in.bulk.ksize = ksize;
        in.bulk.vsize = size;

        hret = margo_bulk_create(provider->client->mid, 1, &value, &in.bulk.vsize,
                                HG_BULK_WRITE_ONLY, &in.bulk.handle);
        if(hret != HG_SUCCESS) return -1;

        /* create handle */
        hret = margo_create(
                provider->client->mid,
                provider->addr,
                provider->client->sdskv_bulk_get_id,
                &handle);
        if(hret != HG_SUCCESS) {
            margo_bulk_free(in.bulk.handle);
            return -1;
        }

        hret = margo_set_target_id(handle, provider->mplex_id);
        if(hret != HG_SUCCESS) {
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        hret = margo_forward(handle, &in);
        if(hret != HG_SUCCESS) {
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        hret = margo_get_output(handle, &out);
        if(hret != HG_SUCCESS) {
            margo_bulk_free(in.bulk.handle);
            margo_destroy(handle);
            return -1;
        }

        ret = out.ret;
        *vsize = (hg_size_t)out.size;

        margo_free_output(handle, &out);
        margo_bulk_free(in.bulk.handle);
    }

    margo_destroy(handle);
    return ret;
}

int sdskv_length(sdskv_provider_handle_t provider, 
        sdskv_database_id_t db_id, const void *key, 
        hg_size_t ksize, hg_size_t* vsize)
{
    hg_return_t hret;
    int ret;
    hg_handle_t handle;

    length_in_t in;
    length_out_t out;

    in.db_id = db_id;
    in.key   = (kv_data_t)key;
    in.ksize = ksize;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_length_id,
            &handle);
    if(hret != HG_SUCCESS) return -1;

    hret = margo_set_target_id(handle, provider->mplex_id);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    ret = out.ret;
    if(ret == 0) *vsize = out.size;

    margo_free_output(handle, &out);
    margo_destroy(handle);
    return ret;
}

int sdskv_erase(sdskv_provider_handle_t provider,
        sdskv_database_id_t db_id, const void *key,
        hg_size_t ksize)
{
    hg_return_t hret;
    int ret;
    hg_handle_t handle;

    erase_in_t in;
    erase_out_t out;

    in.db_id = db_id;
    in.key   = (kv_data_t)key;
    in.ksize = ksize;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_erase_id,
            &handle);
    if(hret != HG_SUCCESS) return -1;

    hret = margo_set_target_id(handle, provider->mplex_id);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    ret = out.ret;

    margo_free_output(handle, &out);
    margo_destroy(handle);
    return ret;
}

int sdskv_list_keys(sdskv_provider_handle_t provider,
        sdskv_database_id_t db_id,  // db instance
        const void *start_key,  // we want keys strictly after this start_key
        hg_size_t start_ksize,  // size of the start_key
        void **keys,            // pointer to an array of void* pointers,
        //     this array has size *max_keys
        hg_size_t* ksizes,      // pointer to an array of hg_size_t sizes
        //    representing sizes allocated in
        //     keys for each key
        hg_size_t* max_keys)   // maximum number of keys requested
{
    list_keys_in_t  in;
    list_keys_out_t out;
    in.keys_bulk_handle   = HG_BULK_NULL;
    in.ksizes_bulk_handle = HG_BULK_NULL;
    hg_return_t hret      = HG_SUCCESS;
    hg_handle_t handle    = HG_HANDLE_NULL;
    int ret = 0;
    int i;

    in.db_id = db_id;
    in.start_key = (kv_data_t) start_key;
    in.start_ksize = start_ksize;
    in.max_keys = *max_keys;

    /* create bulk handle to expose the segments with key sizes */
    hg_size_t ksize_bulk_size = (*max_keys)*sizeof(*ksizes);
    void* buf_ptr[1] = { ksizes };
    hret = margo_bulk_create(provider->client->mid,
                             1, buf_ptr, &ksize_bulk_size,
                             HG_BULK_READWRITE,
                             &in.ksizes_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* create bulk handle to expose where the keys should be placed */
    hret = margo_bulk_create(provider->client->mid,
                             *max_keys, keys, ksizes,
                             HG_BULK_WRITE_ONLY,
                             &in.keys_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }


    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_list_keys_id,
            &handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* set target id */
    hret = margo_set_target_id(handle, provider->mplex_id);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* forward to provider */
    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* get the output from provider */
    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* set return values */
    *max_keys = out.nkeys;
    ret = out.ret;

finish:
    /* free everything we created */
    margo_bulk_free(in.ksizes_bulk_handle);
    margo_bulk_free(in.keys_bulk_handle);
    margo_free_output(handle, &out);
    margo_destroy(handle);

    return ret;
}

int sdskv_list_keyvals(sdskv_provider_handle_t provider,
        sdskv_database_id_t db_id,  // db instance
        const void *start_key,  // we want keys strictly after this start_key
        hg_size_t start_ksize,  // size of the start_key
        void **keys,            // pointer to an array of void* pointers,
                                //     this array has size *max_keys
        hg_size_t* ksizes,      // pointer to an array of hg_size_t sizes
                                //    representing sizes allocated in
                                //     keys for each key
        void **values,          // pointer to an array of void* pointers,
                                //     this array has size *max_keys
        hg_size_t* vsizes,      // pointer to an array of hg_size_t sizes
                                //    representing sizes allocated in
                                //    values for each value
        hg_size_t* max_keys)    // maximum number of keys requested
{
    list_keyvals_in_t  in;
    list_keyvals_out_t out;
    in.keys_bulk_handle   = HG_BULK_NULL;
    in.ksizes_bulk_handle = HG_BULK_NULL;
    in.vals_bulk_handle   = HG_BULK_NULL;
    in.vsizes_bulk_handle = HG_BULK_NULL;
    hg_return_t hret      = HG_SUCCESS;
    hg_handle_t handle    = HG_HANDLE_NULL;
    int ret = 0;
    int i;

    in.db_id = db_id;
    in.start_key = (kv_data_t) start_key;
    in.start_ksize = start_ksize;
    in.max_keys = *max_keys;

    /* create bulk handle to expose the segments with key sizes */
    hg_size_t ksize_bulk_size = (*max_keys)*sizeof(*ksizes);
    void* ksizes_buf_ptr[1] = { ksizes };
    hret = margo_bulk_create(provider->client->mid,
                             1, ksizes_buf_ptr, &ksize_bulk_size,
                             HG_BULK_READWRITE,
                             &in.ksizes_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* create bulk handle to expose the segments with value sizes */
    hg_size_t vsize_bulk_size = (*max_keys)*sizeof(*vsizes);
    void* vsizes_buf_ptr[1] = { vsizes };
    hret = margo_bulk_create(provider->client->mid,
                             1, vsizes_buf_ptr, &ksize_bulk_size,
                             HG_BULK_READWRITE,
                             &in.vsizes_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* create bulk handle to expose where the keys should be placed */
    hret = margo_bulk_create(provider->client->mid,
                             *max_keys, keys, ksizes,
                             HG_BULK_WRITE_ONLY,
                             &in.keys_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* create bulk handle to expose where the keys should be placed */
    hret = margo_bulk_create(provider->client->mid,
                             *max_keys, values, vsizes,
                             HG_BULK_WRITE_ONLY,
                             &in.vals_bulk_handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_list_keyvals_id,
            &handle);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* set target id */
    hret = margo_set_target_id(handle, provider->mplex_id);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* forward to provider */
    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* get the output from provider */
    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        ret = -1;
        goto finish;
    }

    /* set return values */
    *max_keys = out.nkeys;
    ret = out.ret;

finish:
    /* free everything we created */
    margo_bulk_free(in.ksizes_bulk_handle);
    margo_bulk_free(in.keys_bulk_handle);
    margo_bulk_free(in.vsizes_bulk_handle);
    margo_bulk_free(in.vals_bulk_handle);
    margo_free_output(handle, &out);
    margo_destroy(handle);

    return ret;
}
#if 0
int sdskv_list_keyvals(
        sdskv_provider_handle_t provider,
        sdskv_database_id_t db_id, 
        const void *start_key,
        hg_size_t start_ksize,
        void **keys,
        hg_size_t* ksizes,
        void** values,
        hg_size_t* vsizes,
        hg_size_t* max_keys)
{
    list_in_t  in;
    list_out_t out;
    hg_return_t hret = HG_SUCCESS;
    hg_handle_t handle;
    int ret;
    int i;

    in.db_id = db_id;
    in.start_key = (kv_data_t) start_key;
    in.start_ksize = start_ksize;
    in.max_keys = *max_keys;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->sdskv_list_keyvals_id,
            &handle);
    if(hret != HG_SUCCESS) return -1;

    hret = margo_set_target_id(handle, provider->mplex_id);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return -1;
    }

    *max_keys = out.nkeys;
    ret = out.ret;

    if(ret == 0) {

        hg_size_t s;
        for (i=0; i < out.nkeys; i++) {
            s = ksizes[i] > out.ksizes[i] ? out.ksizes[i] : ksizes[i];
            ksizes[i] = s;
            memcpy(keys[i], out.keys[i], s);
            if(s < out.ksizes[i]) ret = -1; // truncated key
        }

        for(i=0; i< out.nvalues; i++) {
            s = vsizes[i] > out.vsizes[i] ? out.vsizes[i] : vsizes[i];
            vsizes[i] = s;
            memcpy(values[i], out.values[i], s);
            if(s < out.vsizes[i]) return -1; // truncated value
        }
    }

    margo_free_output(handle, &out);
    margo_destroy(handle);

    return ret;
}
#endif
int sdskv_shutdown_service(sdskv_client_t client, hg_addr_t addr)
{
    return margo_shutdown_remote_instance(client->mid, addr);
}
