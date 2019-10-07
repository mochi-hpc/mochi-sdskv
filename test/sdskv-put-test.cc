/*
 * (C) 2015 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <margo.h>
#include <string>
#include <vector>

#include "sdskv-client.h"

static std::string gen_random_string(size_t len);

int main(int argc, char *argv[])
{
    char cli_addr_prefix[64] = {0};
    char *sdskv_svr_addr_str;
    char *db_name;
    margo_instance_id mid;
    hg_addr_t svr_addr;
    uint8_t mplex_id;
    uint32_t num_keys;
    sdskv_client_t kvcl;
    sdskv_provider_handle_t kvph;
    hg_return_t hret;
    int ret;
    
    int use_poolset = 0;
    hg_size_t poolset_npools = 1;
    hg_size_t poolset_nbufs = 8;
    hg_size_t poolset_first_size = 1024;
    hg_size_t poolset_size_multiple = 2;

    char* var = getenv("SDSKV_USE_POOLSET");
    if(var != NULL) {
        use_poolset = 1;
        var = getenv("SDSKV_POOLSET_NPOOLS");
        if(var != NULL) poolset_npools = atol(var);
        var = getenv("SDSKV_POOLSET_NBUFS");
        if(var != NULL) poolset_nbufs = atol(var);
        var = getenv("SDSKV_POOLSET_FIRST_SIZE");
        if(var != NULL) poolset_first_size = atol(var);
        var = getenv("SDSKV_POOLSET_SIZE_MULTIPLE");
        if(var != NULL) poolset_size_multiple = atol(var);
    }

    if(argc != 5)
    {
        fprintf(stderr, "Usage: %s <sdskv_server_addr> <mplex_id> <db_name> <num_keys>\n", argv[0]);
        fprintf(stderr, "  Example: %s tcp://localhost:1234 1 foo 1000\n", argv[0]);
        return(-1);
    }
    sdskv_svr_addr_str = argv[1];
    mplex_id          = atoi(argv[2]);
    db_name           = argv[3];
    num_keys          = atoi(argv[4]);

    /* initialize Margo using the transport portion of the server
     * address (i.e., the part before the first : character if present)
     */
    for(unsigned i=0; (i<63 && sdskv_svr_addr_str[i] != '\0' && sdskv_svr_addr_str[i] != ':'); i++)
        cli_addr_prefix[i] = sdskv_svr_addr_str[i];

    /* start margo */
    mid = margo_init(cli_addr_prefix, MARGO_SERVER_MODE, 0, 0);
    if(mid == MARGO_INSTANCE_NULL)
    {
        fprintf(stderr, "Error: margo_init()\n");
        return(-1);
    }

    ret = sdskv_client_init(mid, &kvcl);
    if(ret != 0)
    {
        fprintf(stderr, "Error: sdskv_client_init()\n");
        margo_finalize(mid);
        return -1;
    }

    if(use_poolset) {
        ret = sdskv_client_configure_bulk_poolset(
                kvcl, poolset_npools, poolset_nbufs,
                poolset_first_size, poolset_size_multiple);
        if(ret != 0) {
            fprintf(stderr, "Error: sdskv_client_configure_bulk_poolset()\n");
            margo_finalize(mid);
            return -1;
        }
    }

    /* look up the SDSKV server address */
    hret = margo_addr_lookup(mid, sdskv_svr_addr_str, &svr_addr);
    if(hret != HG_SUCCESS)
    {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        sdskv_client_finalize(kvcl);
        margo_finalize(mid);
        return(-1);
    }

    /* create a SDSKV provider handle */
    ret = sdskv_provider_handle_create(kvcl, svr_addr, mplex_id, &kvph);
    if(ret != 0)
    {
        fprintf(stderr, "Error: sdskv_provider_handle_create()\n");
        margo_addr_free(mid, svr_addr);
        sdskv_client_finalize(kvcl);
        margo_finalize(mid);
        return(-1);
    }

    /* open the database */
    sdskv_database_id_t db_id;
    ret = sdskv_open(kvph, db_name, &db_id);
    if(ret == 0) {
        printf("Successfuly open database %s, id is %ld\n", db_name, db_id);
    } else {
        fprintf(stderr, "Error: could not open database %s\n", db_name);
        sdskv_provider_handle_release(kvph);
        margo_addr_free(mid, svr_addr);
        sdskv_client_finalize(kvcl);
        margo_finalize(mid);
        return(-1);
    }

    /* **** put keys ***** */
    std::vector<std::string> keys;
    std::vector<std::string> data;
    for(unsigned i=0; i < num_keys; i++) {
        auto k = gen_random_string(16);
        // half of the entries will be put using bulk
        auto v = gen_random_string(i*8000/num_keys);
        ret = sdskv_put(kvph, db_id,
                (const void *)k.data(), k.size(),
                (const void *)v.data(), v.size());
        if(ret != 0) {
            fprintf(stderr, "Error: sdskv_put() failed (iteration %d, ret = %d)\n", i, ret);
            sdskv_shutdown_service(kvcl, svr_addr);
            sdskv_provider_handle_release(kvph);
            margo_addr_free(mid, svr_addr);
            sdskv_client_finalize(kvcl);
            margo_finalize(mid);
            return -1;
        }
    }

    /* shutdown the server */
    ret = sdskv_shutdown_service(kvcl, svr_addr);

    /**** cleanup ****/
    sdskv_provider_handle_release(kvph);
    margo_addr_free(mid, svr_addr);
    sdskv_client_finalize(kvcl);
    margo_finalize(mid);
    return(ret);
}

static std::string gen_random_string(size_t len) {
    static const char alphanum[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
    std::string s(len, ' ');
    for (unsigned i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}
