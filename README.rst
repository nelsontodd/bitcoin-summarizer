Installation
======================

Required Dependencies
--------------------------
It is assumed your local environment already has:

   1. `GNU Make <https://www.gnu.org/software/make/>`_ for building
   2. `git <https://git-scm.com/>`_ for version control
   3. `The GNU Scientific Library <https://www.gnu.org/software/gsl/>`_ for calculations
   4. At least 8 GB of RAM.

Instructions
--------------------------
     
In bash::

   git clone git@github.com:nelsontodd/summarizer
   cd summarizer
   git submodule update --init
   make bitcoin-iterate bitcoin-iterators

Using Bitcoin-iterate
======================
The bitcoin-iterate tool, originally written by Rusty Russell, is a  method of extracting large amounts of data from the Bitcoin blockchain quickly. `bitcoin-iterate` examines Bitcoin blockchain data files on disk and assembles them into an ordered chain of blocks, just like a bitcoind node does. `bitcoin-iterate` then iterates through each block printing out the data you ask for.
The following examples are a brief tour through the functionality of bitcoin-iterate. All the examples assume bitcoind's blocks are stored in the default location `(${HOME}/.bitcoin/blocks)`. 
Before we dig in, lets look out the output of `--help`::

   $ ./bitcoin-iterate --help | tail -n18
   ...
   -h|--help                 Display help message
   --block <arg>             Format to print for each block
   --tx|--transaction <arg>  Format to print for each transaction
   --input <arg>             Format to print for each transaction input
   --output <arg>            Format to print for each transaction output
   --utxo <arg>              Format to print for each UTXO
   --utxo-period <arg>       Loop over UTXOs every this many blocks
   --progress <arg>          Print . to stderr this many times
   --no-mmap                 Don't mmap the block files
   --quiet|-q                Don't output progress information
   --testnet|-t              Look for testnet3 blocks
   --blockdir <arg>          Block directory instead of ~/.bitcoin/[testnet3/]blocks
   --end-hash <arg>          Best blockhash to use instead of longest chain.
   --start-hash <arg>        Blockhash to start at instead of genesis.
   --start <arg>             Block number to start instead of genesis.
   --end <arg>               Block number to end at instead of longest chain.
   --cache <arg>             Cache for multiple runs.

From this output we can see that, conceptually, bitcoin-iterate breaks chain analysis into five separate categories, denoted by flags: block data, transaction data, input data, output data and unspent transaction output data.

Here's a quick example of usage. Using bitcoin-iterate, we can correctly determine the distribution of block versions currently represented in the bitcoin blockchain::

   $ ./bitcoin-iterate --block "%bv" | sort -n | uniq -c | sort -nr | awk '{print "Version "$2" has "$1" blocks"}'
   ...
   Version 1 has 215047 blocks
   Version 2 has 140752 blocks
   Version 536870912 has 90254 blocks
   Version 3 has 29304 blocks
   Version 4 has 27212 blocks
   Version 536870914 has 15832 blocks
   Version 536870913 has 4976 blocks
   Version 805306368 has 2058 blocks
   Version 536870930 has 1213 blocks
   Version 536870928 has 304 blocks
   Version 536870916 has 212 blocks
   Version 805306369 has 114 blocks
   Version 536870919 has 41 blocks
   Version 134217732 has 39 blocks
   Version 536928256 has 19 blocks
   Version 1073676288 has 16 blocks
   Version 1073733632 has 15 blocks
   Version 553644032 has 10 blocks
   Version 553647872 has 9 blocks
   Version 536874752 has 9 blocks
   Version 1073741823 has 8 blocks
   Version 805306375 has 4 blocks
   Version 536870927 has 3 blocks
   Version 1073741808 has 2 blocks

Using The Summarizer
---------------------------

The summarizer is nothing more than a set of functions that bitcoin-iterate uses to map across the bitcoin blockchain. Use it like this::

   ./bin/summarize > data/summarizer.tmp
