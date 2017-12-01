/*
 * Copyright (c) 2017, Los Alamos National Security, LLC.
 * All rights reserved.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <mpi.h>

#include <margo.h>
#include <mercury.h>
#include <abt.h>

#include "sds-keyval-group.h"

#include <vector>
#include <stdlib.h>
#include <time.h>

#include <boost/functional/hash.hpp>

#define DIE_IF(cond_expr, err_fmt, ...) \
    do { \
        if (cond_expr) { \
            fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " \
                    err_fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            exit(1); \
        } \
    } while(0)

// test-mpi-group addr_str num_servers [vsize]
// vsize is size of std::vector<int32_t> with default of 1
int main(int argc, char *argv[])
{
    int rank;
    int nranks;

    assert(argc >= 3);
    char *addr_str = argv[1];
    int num_servers = atoi(argv[2]);
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);

    assert(nranks >= (num_servers+1)); // insist on at least 1 clients

    MPI_Comm clientComm, ssgComm;

    if (rank < num_servers) {
      hg_size_t addr_str_sz = 128;
      char server_addr_str[addr_str_sz];
      hg_addr_t server_addr;
      hg_return_t hret;
      const char *ssg_name = "test-server-group";
      
      MPI_Comm_split(MPI_COMM_WORLD, 0, rank, &ssgComm);
      MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, rank, &clientComm);
      
      // kv-server
      char *proto = kv_protocol(addr_str);
      kvgroup_context_t *context = kvgroup_server_register(kv_margo_init(proto, MARGO_SERVER_MODE),
							   ssg_name,
							   ssgComm);
      free(proto); // only needed by kv_margo_init

      hret = margo_addr_self(context->mid, &server_addr);
      DIE_IF(hret != HG_SUCCESS, "margo_addr_self");

      // get server address
      hret = margo_addr_to_string(context->mid, server_addr_str, &addr_str_sz, server_addr);
      DIE_IF(hret != HG_SUCCESS, "margo_addr_to_string");
      margo_addr_free(context->mid, server_addr);
      
      printf("server (rank %d): server addr_str: %s, group: %s\n", rank, server_addr_str, ssg_name);

      int server_rank;
      MPI_Comm_rank(ssgComm, &server_rank);

      // broadcast (send) SSG ID to all clients
      kvgroup_server_send_gid(context->gid, MPI_COMM_WORLD);
      if (server_rank == 0) {
	printf("server (rank %d): sent group\n", rank);
      }

      // process requests until finalized
      kvgroup_server_wait_for_shutdown(context);

      // now finish cleaning up
      kvgroup_server_deregister(context);
      printf("rank %d: server deregistered\n", rank);

      //kv_margo_finalize(mid); // already finalized in server's shutdown_handler
    }
    else {
      hg_size_t addr_str_sz = 128;
      char client_addr_str[addr_str_sz];
      hg_addr_t client_addr;
      hg_return_t hret;
      
      MPI_Comm_split(MPI_COMM_WORLD, MPI_UNDEFINED, rank, &ssgComm);
      MPI_Comm_split(MPI_COMM_WORLD, 1, rank, &clientComm);
      
      // broadcast (recv) SSG ID
      ssg_group_id_t gid;
      kvgroup_client_recv_gid(&gid, MPI_COMM_WORLD);
      printf("client (rank %d): received group\n", rank);

      // kv-client
      char *proto = kvgroup_protocol(gid);
      kvgroup_context_t *context = kvgroup_client_register(kv_margo_init(proto, MARGO_CLIENT_MODE), 
							   gid);
      free(proto); // only needed by kv_margo_init

      hret = margo_addr_self(context->mid, &client_addr);
      DIE_IF(hret != HG_SUCCESS, "margo_addr_self");

      // get client address
      hret = margo_addr_to_string(context->mid, client_addr_str, &addr_str_sz, client_addr);
      DIE_IF(hret != HG_SUCCESS, "margo_addr_to_string");
      margo_addr_free(context->mid, client_addr);
      printf("client (rank %d): client addr_str: %s\n", rank, client_addr_str);
      
      // open specified "DB" (pass in the server's address)
      const char *db = "db/minima_store";
      hret = kvgroup_open(context, (char*)db);
      DIE_IF(hret != HG_SUCCESS, "kvgroup_open");
      
      size_t vsize = 1;
      if (argc == 4) {
	vsize = atoi(argv[2]);
      }
      printf("client (rank %d): using vsize = %lu, dsize = %lu\n", rank, vsize, vsize*sizeof(int32_t));

      // put
      for (int i=1; i<1000; i++) {
	int32_t key = 1000*rank + i;
	std::vector<int32_t> put_data;
	put_data.resize(vsize, key);
	hg_size_t data_size = put_data.size()*sizeof(int32_t); // size in char (bytes)

	// create OID for key
	size_t oid = boost::hash<int32_t>()(key);
	hret = kvgroup_put(context, oid, (void*)&key, sizeof(key),
			   (void*)put_data.data(), data_size);
	printf("(rank %d: put) key %d, size=%lu\n", rank, key, data_size);
	DIE_IF(hret != HG_SUCCESS, "kv_put");
      }

      sleep(2);

      // get
      for (int i=1; i<1000; i++) {
	int32_t key = 1000*rank + i;
	std::vector<int32_t> expected_get_data;
	expected_get_data.resize(vsize, key);
	std::vector<int32_t> get_data;
	get_data.resize(vsize);
	hg_size_t data_size = get_data.size()*sizeof(int32_t); // size in char (bytes)
	printf("(rank %d: get) key %d, size=%lu\n", rank, key, data_size);

	// create OID for key
	size_t oid = boost::hash<int32_t>()(key);
	hret = kvgroup_get(context, oid, (void*)&key, sizeof(key),
			   (void*)get_data.data(), &data_size);
	DIE_IF(hret != HG_SUCCESS, "kv_get");

	get_data.resize(data_size/sizeof(int32_t)); // size in int32_t
	if (expected_get_data == get_data) {
	  printf("(rank %d: put/get succeeded) key %d, actual size=%lu\n", rank, key, data_size);
	}
	else {
	  printf("(rank %d: put/get failed) key %d, actual size=%lu\n", rank, key, data_size);
	}
      }

      // close
      hret = kvgroup_close(context);
      DIE_IF(hret != HG_SUCCESS, "kv_close");

      // once all clients are done with the close, one client can signal server
      int client_rank;
      MPI_Comm_rank(clientComm, &client_rank);
      MPI_Barrier(clientComm);
      if (client_rank==0) {
	printf("rank %d: sending server a shutdown request\n", rank);
	kvgroup_client_signal_shutdown(context);
      }

      // now finish cleaning up
      kvgroup_client_deregister(context);
      printf("rank %d: client deregistered\n", rank);

      //kv_margo_finalize(mid); // already finalized in kv_client_deregister
    }

    MPI_Finalize();

    printf("rank %d: finalized\n", rank);
    return 0;
}