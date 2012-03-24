AUTOMAKE_OPTIONS=foreign
bin_PROGRAMS=spider
spider_SOURCES=spider.c $(common_hdr) $(common_src)
noinst_HEADERS   = $(common_hdr)
#INCLUDES= $(common_hdr)
#lib_LIBRARIES=spider-tool.a
#spider-tool_a_SOURCES = 
#spider-tool_a_LDADD =
#spider-tool_a_LDFLAGS=



common_hdr=spider.h  options.h lock.h const.h linkedlist.h threadprivdata.h  utils.h  strings.h \
     logger.h config.h thread.h socket.h times.h poll.h tcp_engine.h str.h obj.h io.h \
	taskqueue.h cli.h internal_db.h  test_engine.h

common_src=threadprivdata.c  utils.c strings.c  logger.c  config.h config.c  thread.c \
     socket.c    poll.c tcp_engine.c  str.c   obj.c io.c taskqueue.c cli.c internal_db.c test_engine.c

LIBS += -lpthread -lsqlite3 
