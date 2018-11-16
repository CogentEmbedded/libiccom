MAJOR_VERSION = 1
MINOR_VERSION = 0

CFLAGS += -g -Wall -I./public

SRCDIR   = src
OBJDIR   = $(SRCDIR)
TESTDIR  = test
OUTDIR   = out
SRCS     = $(SRCDIR)/iccom_library.c
OBJS     = $(OBJDIR)/iccom_library.o
LIBNAME  = libiccom.so
SONAME   = $(LIBNAME).$(MAJOR_VERSION)
REALNAME = $(SONAME).$(MINOR_VERSION)
TARGET   = $(OUTDIR)/$(REALNAME)
TESTSRC  = $(TESTDIR)/test.c
TEST     = $(OUTDIR)/iccom-test
LOGLEVEL ?= LOGERR

ifeq ($(LOGLEVEL),LOGERR)
	CFLAGS += -DICCOM_API_ERROR
else ifeq ($(LOGLEVEL),LOGNRL)
	CFLAGS += -DICCOM_API_ERROR -DICCOM_API_NORMAL
else ifeq ($(LOGLEVEL),LOGDBG)
	CFLAGS += -DICCOM_API_ERROR -DICCOM_API_NORMAL -DICCOM_API_DEBUG
else #ifeq ($(LOGLEVEL),LOGNONE)
endif

all : $(TARGET) $(TEST)

$(TARGET) : $(OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -shared -Wl,-soname=$(SONAME) -o $@ $< -pthread

$(OBJS): $(SRCS)
	@[ -d $(OBJDIR) ]
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

$(TEST) : $(TESTSRC) $(TARGET)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TESTSRC) $(TARGET) -o $@

.PHONY: clean
clean :
	rm -f $(OBJS)
	rm -rf $(OUTDIR)
