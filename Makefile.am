ACLOCAL_AMFLAGS="-Im4"

SERVER_LIBS=@SERVER_LIBS@
CLIENT_LiBS=@CLIENT_LIBS@

AM_CPPFLAGS = -I${srcdir}/src -I${srcdir}/include

bin_PROGRAMS = bin/sdskv-server-daemon 	    \
	       bin/sdskv-shutdown

check_PROGRAMS = test/sdskv-open-test              \
		 test/sdskv-put-test               \
		 test/sdskv-length-test            \
		 test/sdskv-get-test               \
		 test/sdskv-erase-test             \
		 test/sdskv-list-keys-test         \
		 test/sdskv-list-keyvals-test      \
		 test/sdskv-list-keys-prefix-test  \
		 test/sdskv-custom-cmp-test        \
		 test/sdskv-custom-server-daemon

bin_sdskv_server_daemon_SOURCES = src/sdskv-server-daemon.c
bin_sdskv_server_daemon_DEPENDENCIES = lib/libsdskv-server.la
bin_sdskv_server_daemon_LDFLAGS = -Llib -lsdskv-server
bin_sdskv_server_daemon_LDADD = ${LIBS} -lsdskv-server ${SERVER_LIBS}


bin_sdskv_shutdown_SOURCES = src/sdskv-shutdown.c
bin_sdskv_shutdown_DEPENDENCIES = lib/libsdskv-client.la
bin_sdskv_shutdown_LDFLAGS = -Llib -lsdskv-client
bin_sdskv_shutdown_LDADD = ${LIBS} -lsdskv-client

#lib_LTLIBRARIES = lib/libkvclient.la \
#                  lib/libkvserver.la \
#                  lib/libkvgroupclient.la \
#                  lib/libkvgroupserver.la \
#		   lib/libsdskv-client.la \
#		   lib/libsdskv-server.la
lib_LTLIBRARIES = lib/libsdskv-client.la \
		  lib/libsdskv-server.la

lib_libsdskv_client_la_SOURCES = src/sdskv-client.c

#lib_libkvclient_la_SOURCES = src/kv-client.c

#lib_libkvgroupclient_la_SOURCES = src/kvgroup-client.cc

#lib_libkvserver_la_SOURCES = src/kv-server.cc \
#			     src/datastore/datastore.cc

lib_libsdskv_server_la_SOURCES = src/sdskv-server.cc \
				 src/datastore/datastore.cc

if BUILD_BWTREE
#lib_libkvserver_la_SOURCES += src/BwTree/src/bwtree.cpp \
#			      src/datastore/bwtree_datastore.cc

lib_libsdskv_server_la_SOURCES += src/BwTree/src/bwtree.cpp \
				  src/datastore/bwtree_datastore.cc
endif

if BUILD_BDB
#lib_libkvserver_la_SOURCES += src/datastore/berkeleydb_datastore.cc
#
lib_libsdskv_server_la_SOURCES += src/datastore/berkeleydb_datastore.cc
endif

if BUILD_LEVELDB
#lib_libkvserver_la_SOURCES += src/datastore/leveldb_datastore.cc

lib_libsdskv_server_la_SOURCES += src/datastore/leveldb_datastore.cc
endif

lib_libsdskv_server_la_LIBADD = @SERVER_LIBS_PKG@

include_HEADERS = include/sdskv-client.h \
		  include/sdskv-server.h \
		  include/sdskv-common.h

noinst_HEADERS = src/bulk.h \
		 src/sds-keyval.h \
		 src/sds-keyval-group.h \
		 src/sdskv-rpc-types.h \
		 src/datastore/datastore.h \
		 src/datastore/map_datastore.h \
		 src/datastore/bwtree_datastore.h \
		 src/datastore/leveldb_datastore.h \
		 src/datastore/berkeleydb_datastore.h \
		 src/datastore/datastore_factory.h \
		 src/keyval-internal.h \
		 src/BwTree/src/bwtree.h \
		 src/BwTree/src/atomic_stack.h\
		 src/BwTree/src/bloom_filter.h \
		 src/BwTree/src/sorted_small_set.h

#lib_libkvgroupserver_la_SOURCES = src/kvgroup-server.cc


TESTS = test/basic.sh 		\
	test/open-test.sh 	\
       	test/put-test.sh 	\
	test/length-test.sh 	\
	test/get-test.sh 	\
	test/erase-test.sh      \
	test/list-keys-test.sh  \
	test/list-keyvals-test.sh  \
	test/list-keys-prefix-test.sh \
	test/custom-cmp-test.sh

TESTS_ENVIRONMENT = TIMEOUT="$(TIMEOUT)" \
		    MKTEMP="$(MKTEMP)"

test_sdskv_open_test_SOURCES = test/sdskv-open-test.cc
test_sdskv_open_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_open_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_put_test_SOURCES = test/sdskv-put-test.cc
test_sdskv_put_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_put_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_length_test_SOURCES = test/sdskv-length-test.cc
test_sdskv_length_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_length_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_get_test_SOURCES = test/sdskv-get-test.cc
test_sdskv_get_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_get_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_list_keys_test_SOURCES = test/sdskv-list-keys-test.cc
test_sdskv_list_keys_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_list_keys_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_list_keys_prefix_test_SOURCES = test/sdskv-list-keys-prefix-test.cc
test_sdskv_list_keys_prefix_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_list_keys_prefix_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_list_keyvals_test_SOURCES = test/sdskv-list-kv-test.cc
test_sdskv_list_keyvals_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_list_keyvals_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_erase_test_SOURCES = test/sdskv-erase-test.cc
test_sdskv_erase_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_erase_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_custom_cmp_test_SOURCES = test/sdskv-custom-cmp-test.cc
test_sdskv_custom_cmp_test_DEPENDENCIES = lib/libsdskv-client.la
test_sdskv_custom_cmp_test_LDFLAGS = -Llib -lsdskv-client

test_sdskv_custom_server_daemon_SOURCES = test/sdskv-custom-server-daemon.c
test_sdskv_custom_server_daemon_DEPENDENCIES = lib/libsdskv-server.la
test_sdskv_custom_server_daemon_LDFLAGS = -Llib -lsdskv-server
test_sdskv_custom_server_daemon_LDADD = ${LIBS} -lsdskv-server ${SERVER_LIBS}

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = maint/sdskv-server.pc \
		 maint/sdskv-client.pc



