#
# == Paths ==
#

ROOT_DIR                   := $(shell pwd)
BITCOIN_ITERATE_DIR 	   := $(ROOT_DIR)/vendor/bitcoin-iterate
BIN_DIR                    := bin
BUILD_DIR                  := build
DATA_DIR                   := data
ITERATOR_DIR               := lib/

#
# == Commands ==
#

CP      := cp
FIND    := find

#
#
# == bitcoin-iterate ==
#

ifeq ($(UNAME),Darwin)
	BITCOIN_ITERATE_INCLUDES ?= -I /usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib
else
	BITCOIN_ITERATE_INCLUDES ?=
endif
BITCOIN_ITERATE_CFLAGS ?= -O3 -flto -ggdb -I $(BITCOIN_ITERATE_DIR) -I $(BITCOIN_ITERATE_DIR)/ccan $(BITCOIN_ITERATE_INCLUDES) -Wall
ITERATOR_INCLUDES      ?= -I/usr/local/include/

BITCOIN_ITERATE_LDFLAGS ?= -O3 -flto
BITCOIN_ITERATE_LDLIBS  ?= -lcrypto
ITERATOR_LDLIBS          = -L/usr/local/lib
ITERATOR_LDFLAGS         = -lgsl -lgslcblas -lm

# Do NOT include cli.o in this list because it has its own main() function
BITCOIN_ITERATE_OBJ_NAMES := utils.o io.o blockfiles.o format.o parse.o calculations.o utxo.o block.o cache.o iterate.o
BITCOIN_ITERATE_OBJS = $(foreach obj,$(BITCOIN_ITERATE_OBJ_NAMES),$(BITCOIN_ITERATE_DIR)/$(obj))
CCAN_OBJ_NAMES := ccan-tal.o ccan-tal-path.o ccan-tal-str.o ccan-take.o ccan-list.o ccan-str.o ccan-opt-helpers.o ccan-opt.o ccan-opt-parse.o ccan-opt-usage.o ccan-htable.o ccan-rbuf.o ccan-hex.o ccan-tal-grab-file.o ccan-noerr.o
CCAN_OBJS = $(foreach obj,$(CCAN_OBJ_NAMES),$(BITCOIN_ITERATE_DIR)/$(obj))

ITERATORS := summarize
ITERATOR_OBJS = $(foreach iterator,$(ITERATORS),$(BUILD_DIR)/$(iterator).o)
ITERATOR_BINS = $(foreach iterator,$(ITERATORS),$(BIN_DIR)/$(iterator))

#
# == Top-Level Targets ==
#

default: bitcoin-iterate bitcoin-iterators

clean:
	$(MAKE) -C $(BITCOIN_ITERATE_DIR) distclean
	$(RM) $(BUILD_DIR)/*.o $(ITERATOR_BINS)

purge:  purge-data

bitcoin-iterators: $(ITERATOR_OBJS) $(ITERATOR_BINS)

bitcoin-iterate:
	$(MAKE) -C $(BITCOIN_ITERATE_DIR) -e CFLAGS="$(BITCOIN_ITERATE_CFLAGS)"

$(BUILD_DIR)/%.o: $(ITERATOR_DIR)/%.c $(BITCOIN_ITERATE_OBJS) $(CCAN_OBJS)
	$(CC) $(BITCOIN_ITERATE_CFLAGS) $(ITERATOR_INCLUDES) $(BITCOIN_ITERATE_LDFLAGS) -c -o $@ $<

$(BIN_DIR)/%: $(BUILD_DIR)/%.o
	$(CC) $(BITCOIN_ITERATE_CFLAGS) $(BITCOIN_ITERATE_LDFLAGS) $(ITERATOR_LDLIBS) -o $@ $< $(ITERATOR_LDFLAGS) $(BITCOIN_ITERATE_OBJS) $(CCAN_OBJS) $(BITCOIN_ITERATE_LDLIBS)

#
# == Cleaning ==
#

purge-data:
	sudo $(RM) -r $(DATA_DIR)/*
