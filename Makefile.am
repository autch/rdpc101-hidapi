
AM_CFLAGS = @hidapi_CFLAGS@

bin_PROGRAMS = rdpc101 rdpc-test

rdpc101_SOURCES = rdpc101.c librdpc101.c
rdpc101_LDADD = @hidapi_LIBS@

rdpc_test_SOURCES = rdpc-test.c librdpc101.c
rdpc_test_LDADD = @hidapi_LIBS@
