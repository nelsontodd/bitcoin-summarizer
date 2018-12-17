#include <ccan/err/err.h>
#include <ccan/opt/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "iterate.h"
#include "calculations.h"
#include <gsl/gsl_rstat.h>

/* Iterator */
static struct block *last_block;

/* Days */
static s64 bdc_block = 0;
static s64 bdd_block = 0;
static s64 bdc_cum   = 0;
static s64 bdd_cum   = 0;

/* Age of last use */
static s64 btc_spendable_total;
#define NUM_AGE_BINS 11
#define NUM_AMOUNT_BINS 14
#define NUM_DENSITY_BINS 11
#define ONE_MINUTE       60
#define ONE_HOUR         3600
#define ONE_DAY          86400
#define ONE_WEEK         604800
#define ONE_MONTH        2592000
#define THREE_MONTHS     7776000
#define SIX_MONTHS       15552000
#define ONE_YEAR         31536000
#define TWELVE_MONTHS    31536000
#define EIGHTEEN_MONTHS  47088000
#define TWO_YEARS        63072000
#define THREE_YEARS      94608000
#define FIVE_YEARS       157680000
#define TEN_YEARS        315360000
/* AGE_LABELS       = ['<1d',   '1d-1w',  '1w-1m',   '1-3m',       '3-6m',     '6-12m',       '12-18m',        '18-24m',  '2-3y',      '3-5y',     '>5y'] */
u32 age_bins[]   = {ONE_DAY, ONE_WEEK, ONE_MONTH, THREE_MONTHS, SIX_MONTHS, TWELVE_MONTHS, EIGHTEEN_MONTHS, TWO_YEARS, THREE_YEARS, FIVE_YEARS, UINT32_MAX};
u64 age_counts[] = {0,       0,        0,         0,            0,          0,             0,               0,         0,           0,          0         };

/*Separates UTXO into bins by unspent amount (sats) */
u64 amount_bins[]   = {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000,100000000000000};
u64 amount_totals[] = {0,  0,   0,    0,     0,      0,       0,        0,         0,          0,           0,            0,             0,             0};

/*UTXO Densty bins*/
u64 density_bins[]   = {10, 25, 50, 75, 90, 100, 250, 500, 750, 1000, 10000000000000000};
u64 density_totals[] = {0,  0,  0,  0,  0,  0,   0,   0,   0,   0,    0};

/*UTXO Percentile Global Pointers*/
static int *workspace_ptr_0001;
static int *workspace_ptr_001;
static int *workspace_ptr_01;
static int *workspace_ptr_1;
static int *workspace_ptr_5;
static int *workspace_ptr_10;
static int *workspace_ptr_25;
static int *workspace_ptr_50;
static int *workspace_ptr_75;
static int *workspace_ptr_90;
static int *workspace_ptr_95;
static int *workspace_ptr_99;
static int *workspace_ptr_999;
static int *workspace_ptr_9999;
static int *workspace_ptr_99999;
/*UTXO Percentile Workspaces*/
gsl_rstat_quantile_workspace *work_0001;
gsl_rstat_quantile_workspace *work_001;
gsl_rstat_quantile_workspace *work_01;
gsl_rstat_quantile_workspace *work_1;
gsl_rstat_quantile_workspace *work_5;
gsl_rstat_quantile_workspace *work_10;
gsl_rstat_quantile_workspace *work_25;
gsl_rstat_quantile_workspace *work_50;
gsl_rstat_quantile_workspace *work_75;
gsl_rstat_quantile_workspace *work_90;
gsl_rstat_quantile_workspace *work_95;
gsl_rstat_quantile_workspace *work_99;
gsl_rstat_quantile_workspace *work_999;
gsl_rstat_quantile_workspace *work_9999;
gsl_rstat_quantile_workspace *work_99999;

/* Counting */
static u64    block_length_cum               = 0;
static u64    block_vlength_cum              = 0;
static size_t transaction_count_block        = 0;
static size_t transaction_count_day          = 0;
static size_t block_count_day                = 0;
static size_t segwit_transaction_count_block = 0;
static u64    transaction_count_cum          = 0;
static u64    segwit_transaction_count_cum   = 0;
static u64    transaction_length_block       = 0;
static u64    transaction_length_cum         = 0;
static u64    transaction_vlength_block      = 0;
static u64    transaction_vlength_cum        = 0;
static varint_t input_count_block            = 0;
static u64      input_count_cum              = 0;
static varint_t output_count_block           = 0;
static u64      output_count_cum             = 0;

/* Fees & BTC created */
static s64    coinbase_output_block              = 0;
static s64    btc_created_block                  = 0;
static s64    fees_block                         = 0;
static s64    fees_cum                           = 0;
static s64    btc_created_cum                    = 0;
static u64    num_bytes_per_utxo                 = 172; //Taken from https://eprint.iacr.org/2018/513.pdf
static double smallest_transaction_fee_per_byte  = -1;
static double smallest_transaction_fee_per_vbyte = -1;
static double largest_transaction_fee_per_byte   = -1;
static double largest_transaction_fee_per_vbyte  = -1;
static double fees_per_byte_block;
static double fees_per_transaction_block;
static double fees_per_vbyte_block;
static double transaction_fee_per_byte;
static double transaction_fee_per_vbyte; 

/* UTXOs */
static size_t utxo_count_block = 0;
static u32    utxo_unspent_output_count_block = 0;
static double percentile_0001;
static double percentile_001;
static double percentile_01;
static double percentile_1;
static double percentile_5;
static double percentile_10;
static double percentile_25;
static double percentile_50;
static double percentile_75;
static double percentile_90;
static double percentile_95;
static double percentile_99;
static double percentile_999;
static double percentile_9999;
static double percentile_99999;

/* Forks & Versions */
// To generate:
// 
//   $ bitcoin-iterate ... --block '%bv' | sort -n | uniq -c | sort -nr
// 
//     215047 1                core
//     140752 2                core
//      61208 536870912
//      29304 3                core
//      27212 4                core
//      15831 536870914
//       4976 536870913
//       2058 805306368
//       1190 536870930
//        304 536870928
//        212 536870916
//        114 805306369
//         41 536870919
//         39 134217732
//          4 805306375
//
#define MAX_CORE_VERSION 4
#define XT_VERSION       536870919
#define CLASSIC_VERSION  805306368
#define BITCOIN_CORE     "core"
#define BITCOIN_XT       "xt"
#define BITCOIN_CLASSIC  "classic"
#define BITCOIN_UNKNOWN  "unknown"

/* #define increase(height, label, thing, amount)  if (thing > (thing += amount)) { fprintf(stderr, "Possible overflow error at block %u when incrementing %s\n", height, label); } */

static void transition_to_new_block(const struct utxo_map *utxo_map, struct block *new_block)
{

  /* Counting */
  transaction_count_cum        += transaction_count_block;
  segwit_transaction_count_cum += segwit_transaction_count_block;
  transaction_length_cum       += transaction_length_block;
  transaction_vlength_cum      += transaction_vlength_block;
  input_count_cum              += input_count_block;
  output_count_cum             += output_count_block;
  
  /* Days */
  bdd_cum += bdd_block;
  bdc_cum += bdc_block;
  
  /* Age of last use */
  int i;
  for ( i = 0 ; i < NUM_AGE_BINS; i++ ) {
    btc_spendable_total += age_counts[i];
  }

  /* Fees & BTC created */
  fees_cum += fees_block;
  btc_created_block = (coinbase_output_block - fees_block);
  btc_created_cum += btc_created_block;
  if (transaction_length_block > 0) {
    fees_per_byte_block = (double) fees_block / (double) transaction_length_block;
  } else {
    fees_per_byte_block = -1.0;
  }
  if (transaction_vlength_block > 0) {
    fees_per_vbyte_block = (double) fees_block / (double) transaction_vlength_block;
  } else {
    fees_per_vbyte_block = -1.0;
  }
  if (transaction_count_block > 0) {
    fees_per_transaction_block = (double) fees_block / (double) transaction_count_block;
  } else {
    fees_per_transaction_block = -1.0;
  }
  
  if (new_block->height > 0 && last_block) {
    /* This can't be done above with the other accumulation because it
       relies on last_block which is only defined when new_block has
       height > 0, so we do it here... */
    block_length_cum  += transaction_length_block;
    block_vlength_cum += transaction_vlength_block;

    char *fork;
    if (last_block->bh.version > MAX_CORE_VERSION) {
      switch (last_block->bh.version) {
      case XT_VERSION:
	fork = BITCOIN_XT;
	break;
      case CLASSIC_VERSION:
	fork = BITCOIN_CLASSIC;
	break;
      default:
	fprintf(stderr, "ERROR: Cannot assign fork for block version %u\n", last_block->bh.version);
	fork = BITCOIN_UNKNOWN;
	break;
      }
    } else {
      fork = BITCOIN_CORE;
    }
	
    printf("block,fork=%s height=%ui,version=%ui %u\n",
	   fork,
	   last_block->height, last_block->bh.version,
	   last_block->bh.timestamp);

    printf("sizes,fork=%s,v=%u block.cum=%"PRIi64"i,block.virtual.cum=%"PRIi64"i,transaction=%"PRIi64"i,transaction.cum=%"PRIi64"i,transaction.virtual=%"PRIi64"i,transaction.virtual.cum=%"PRIi64"i %u\n",
	   fork, last_block->bh.version,
	   block_length_cum, block_vlength_cum,
	   transaction_length_block, transaction_length_cum,
	   transaction_vlength_block, transaction_vlength_cum,
	   last_block->bh.timestamp);

    printf("counts,fork=%s,v=%u transaction=%zui,transaction.cum=%"PRIi64"i,segwit.transaction=%zui,segwit.transaction.cum=%"PRIi64"i,input=%zui,input.cum=%"PRIi64"i,output=%zui,output.cum=%"PRIi64"i",
	   fork, last_block->bh.version,
	   transaction_count_block,        transaction_count_cum,
	   segwit_transaction_count_block, segwit_transaction_count_cum,
	   input_count_block,              input_count_cum,
	   output_count_block,             output_count_cum);
    if (utxo_count_block > 0) {
      printf(",utxoset=%zui,utxo=%ui", utxo_count_block, utxo_unspent_output_count_block);
    }   

    printf(" %u\n", last_block->bh.timestamp);

    printf("fees,fork=%s,v=%u block=%"PRIi64"i,cum=%"PRIi64"i",
	   fork, last_block->bh.version,
	   fees_block, fees_cum);

    if (fees_per_transaction_block >= 0.0) {
      printf(",per.transaction=%f", fees_per_transaction_block);
    }
    printf(",average.per.byte=%f", fees_per_byte_block);
    printf(",smallest.per.byte=%f", smallest_transaction_fee_per_byte);
    printf(",largest.per.byte=%f", largest_transaction_fee_per_byte);
    printf(",average.per.vbyte=%f", fees_per_vbyte_block);
    printf(",smallest.per.vbyte=%f", smallest_transaction_fee_per_vbyte);
    printf(",largest.per.vbyte=%f", largest_transaction_fee_per_vbyte);
    printf(" %u\n", last_block->bh.timestamp);

    if (block_count_day == 144){
      printf("transaction,fork=%s,v=%u transaction.count_per_day=%i %u\n",fork,last_block->bh.version,transaction_count_day, last_block->bh.timestamp);
    }

    printf("btc,fork=%s,v=%u created=%f,created.cum=%f",
	   fork, last_block->bh.version,
	   to_btc(btc_created_block), to_btc(btc_created_cum));
    if (btc_spendable_total > 0) {
      printf(",spendable.cum=%f", to_btc(btc_spendable_total));
    }
    printf(" %u\n", last_block->bh.timestamp);

    printf("days,fork=%s,v=%u destroyed=%f,destroyed.cum=%f",
	   fork, last_block->bh.version,
	   to_btc(bdd_block), to_btc(bdd_cum));
    if (bdc_block > 0) {
      printf(",created=%f,created.cum=%f", to_btc(bdc_block), to_btc(bdc_cum));
    }
    printf(" %u\n", last_block->bh.timestamp);
    if (btc_spendable_total > 0) {
      printf("utxo.ages,fork=%s,v=%u <1d=%f,1d-1w=%f,1w-1m=%f,1-3m=%f,3-6m=%f,6-12m=%f,12-18m=%f,18-24m=%f,2-3y=%f,3-5y=%f,>5y=%f %u\n",
       fork, last_block->bh.version,
       to_btc(age_counts[0]), to_btc(age_counts[1]), to_btc(age_counts[2]), to_btc(age_counts[3]), to_btc(age_counts[4]), to_btc(age_counts[5]), to_btc(age_counts[6]), to_btc(age_counts[7]), to_btc(age_counts[8]), to_btc(age_counts[9]), to_btc(age_counts[10]),
       last_block->bh.timestamp);
      printf("utxo.amounts,fork=%s,v=%u <10=%f,<100=%f,<1.000=%f,<10.000=%f,<100.000=%f,<1.000.000=%f,<10.000.000=%f,<100.000.000=%f,<1.000.000.000=%f,<10.000.000.000=%f,<100.000.000.000=%f,<1.000.000.000.000=%f,<10.000.000.000.000=%f,<100.000.000.000.000=%f %u\n",
       fork, last_block->bh.version,
       to_btc(amount_totals[0]), to_btc(amount_totals[1]), to_btc(amount_totals[2]), to_btc(amount_totals[3]), to_btc(amount_totals[4]), to_btc(amount_totals[5]), to_btc(amount_totals[6]), to_btc(amount_totals[7]), to_btc(amount_totals[8]), to_btc(amount_totals[9]),to_btc(amount_totals[10]),to_btc(amount_totals[11]),to_btc(amount_totals[12]),to_btc(amount_totals[13]),
       last_block->bh.timestamp);
      printf("utxo.densities,fork=%s,v=%u <10=%f,<25=%f,<50=%f,<75=%f,<90=%f,<100=%f,<250=%f,<500=%f,<750=%f,<1.000=%f,>1.000=%f %u\n",
       fork, last_block->bh.version,
       to_btc(density_totals[0]), to_btc(density_totals[1]), to_btc(density_totals[2]), to_btc(density_totals[3]),to_btc(density_totals[4]),to_btc(density_totals[5]),to_btc(density_totals[6]),to_btc(density_totals[7]),to_btc(density_totals[8]),to_btc(density_totals[9]),to_btc(density_totals[10]),
       last_block->bh.timestamp);

      /*UTXO Percentiles*/
      percentile_0001  = gsl_rstat_quantile_get(*workspace_ptr_0001);
      gsl_rstat_quantile_reset(*workspace_ptr_0001);
      percentile_001   = gsl_rstat_quantile_get(*workspace_ptr_001);
      gsl_rstat_quantile_reset(*workspace_ptr_001);
      percentile_01    = gsl_rstat_quantile_get(*workspace_ptr_01);
      gsl_rstat_quantile_reset(*workspace_ptr_01);
      percentile_1     = gsl_rstat_quantile_get(*workspace_ptr_1);
      gsl_rstat_quantile_reset(*workspace_ptr_1);
      percentile_5     = gsl_rstat_quantile_get(*workspace_ptr_5);
      gsl_rstat_quantile_reset(*workspace_ptr_5);
      percentile_10    = gsl_rstat_quantile_get(*workspace_ptr_10);
      gsl_rstat_quantile_reset(*workspace_ptr_10);
      percentile_25    = gsl_rstat_quantile_get(*workspace_ptr_25);
      gsl_rstat_quantile_reset(*workspace_ptr_25);
      percentile_50    = gsl_rstat_quantile_get(*workspace_ptr_50);
      gsl_rstat_quantile_reset(*workspace_ptr_50);
      percentile_75    = gsl_rstat_quantile_get(*workspace_ptr_75);
      gsl_rstat_quantile_reset(*workspace_ptr_75);
      percentile_90    = gsl_rstat_quantile_get(*workspace_ptr_90);
      gsl_rstat_quantile_reset(*workspace_ptr_90);
      percentile_95    = gsl_rstat_quantile_get(*workspace_ptr_95);
      gsl_rstat_quantile_reset(*workspace_ptr_95);
      percentile_99    = gsl_rstat_quantile_get(*workspace_ptr_99);
      gsl_rstat_quantile_reset(*workspace_ptr_99);
      percentile_999   = gsl_rstat_quantile_get(*workspace_ptr_999);
      gsl_rstat_quantile_reset(*workspace_ptr_999);
      percentile_9999  = gsl_rstat_quantile_get(*workspace_ptr_9999);
      gsl_rstat_quantile_reset(*workspace_ptr_9999);
      percentile_99999 = gsl_rstat_quantile_get(*workspace_ptr_99999);
      gsl_rstat_quantile_reset(*workspace_ptr_99999);
        
      printf("utxo.amount_percentiles,fork=%s,v=%u 0.001=%f,0.01=%f,0.1=%f,1=%f,5=%f,10=%f,25=%f,50=%f,75=%f,90=%f,95=%f,99=%f,99.9=%f,99.99=%f,99.999=%f %u\n",
          fork, last_block->bh.version, percentile_0001, percentile_001, percentile_01, percentile_1, percentile_5, percentile_10, percentile_25, percentile_50, percentile_75, percentile_90, percentile_95, percentile_99, percentile_999, percentile_9999, percentile_99999, last_block->bh.timestamp);
    }
  }

  /* Iterator */
  last_block = new_block;

  /* Days */
  bdd_block = 0;
  bdc_block = 0;

  /* Age of last use */
  btc_spendable_total = 0;
  for (i=0; i < NUM_AGE_BINS; i++) {
    age_counts[i] = 0;
  }
  
  /* UTXO bins reset*/
  for (i=0; i < NUM_AMOUNT_BINS; i++) {
    amount_totals[i] = 0;
  }
  for (i=0; i < NUM_DENSITY_BINS; i++) {
    density_totals[i] = 0;
  }


  /* Transactions, fees & BTC created */
  fees_block                     = 0;
  coinbase_output_block          = 0;
  transaction_length_block       = 0;
  transaction_vlength_block      = 0;
  segwit_transaction_count_block = 0;
  smallest_transaction_fee_per_byte  = -1;
  largest_transaction_fee_per_byte   = -1;
  smallest_transaction_fee_per_vbyte = -1;
  largest_transaction_fee_per_vbyte  = -1;

  /* UTXOs */
  utxo_count_block                = 0;
  utxo_unspent_output_count_block = 0;

  /* Input & Output Count */
  input_count_block  = 0;
  output_count_block = 0;

  /*Transaction Count*/
  transaction_count_block = 0;
  if (block_count_day == 144) {
    block_count_day       = 0;
    transaction_count_day = 0;
  }
  else{
    block_count_day      += 1;
  }
}

static void accumulate_transaction(const struct utxo_map *utxo_map, struct block *b, struct transaction *t, size_t txnum) {
  /* Segwit */
  if (t->segwit) {
    segwit_transaction_count_block += 1;
  }

  /* Days destroyed */
  s64 bdd_transaction = calculate_bdd(utxo_map, t, txnum == 0, b->bh.timestamp);
  if (bdd_transaction >= 0) {
    bdd_block += bdd_transaction;
  }

  /* Input & Output count */
  input_count_block  += t->input_count;
  output_count_block += t->output_count;
  
  if (txnum == 0) {
    /* Fees * BTC Created */
    size_t i;
    for (i = 0; i < t->output_count ; i++) {
      coinbase_output_block += t->output[i].amount;
    }
  } else {
    /* Fees & BTC created */
    s64 fees_transaction = calculate_fees(utxo_map, t, false);
    if (fees_transaction >= 0) {
      fees_block += fees_transaction;
    }
    /* Transactions */
    transaction_count_block   += 1;
    transaction_count_day     += 1;
    transaction_length_block  += t->total_len;
    transaction_vlength_block += segwit_length(t);
    transaction_fee_per_byte   = (double)fees_transaction / (double) t->total_len;
    transaction_fee_per_vbyte  = (double)fees_transaction / (double) segwit_length(t);

    if (smallest_transaction_fee_per_byte < 0 || transaction_fee_per_byte < smallest_transaction_fee_per_byte) {
      smallest_transaction_fee_per_byte  = transaction_fee_per_byte;
    }
    if (largest_transaction_fee_per_byte < 0 || transaction_fee_per_byte > largest_transaction_fee_per_byte) {
      largest_transaction_fee_per_byte   = transaction_fee_per_byte;
    }

    if (smallest_transaction_fee_per_vbyte < 0 || transaction_fee_per_vbyte < smallest_transaction_fee_per_vbyte) {
      smallest_transaction_fee_per_vbyte = transaction_fee_per_vbyte;
    }
    if (largest_transaction_fee_per_vbyte < 0 || transaction_fee_per_vbyte > largest_transaction_fee_per_vbyte) {
      largest_transaction_fee_per_vbyte  = transaction_fee_per_vbyte;
    }
  }
}

static void accumulate_input(const struct utxo_map *utxo_map, struct block *b, struct transaction *t, size_t txnum, struct input *i) {
}

static void accumulate_output(const struct utxo_map *utxo_map, struct block *b, struct transaction *t, size_t txnum, struct output *o) {
}

static void accumulate_utxo(const struct utxo_map *utxo_map, struct block *current_block, struct block *last_utxo_block, struct utxo *u)
{

  /* Days created */
	if (last_utxo_block) {
  	 bdc_block += calculate_bdc(u, current_block->bh.timestamp, last_utxo_block->bh.timestamp);
	}

  /* UTXO Age distribution*/
  /*NOTE: We are ignoring UTXOs with negative ages. Negatives ages occur because on certain small height blocks it appears that UTXO timestamps are from the future*/
  int i;

  u32 age = (current_block->bh.timestamp - u->timestamp);
  if ((int)age > 0) {
    for (i=0; i < NUM_AGE_BINS; i++) {
      if (age < age_bins[i]) {
        break;
      }
    }
    age_counts[i] += u->amount;
  }

  /*UTXO Size Distribution*/
  for (i=0; i < NUM_AMOUNT_BINS; i++) {
    if (u->amount < amount_bins[i]) {
      break;
    }
  }
  amount_totals[i] += u->amount;
  /*UTXO Density Distribution*/
  for (i=0; i < NUM_DENSITY_BINS; i++) {
    if ( ((double) u->amount /(double) num_bytes_per_utxo) < density_bins[i]) {
      break;
    }
  }
  density_totals[i] += u->amount;

  /*UTXO Percentiles*/
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_0001);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_001);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_01);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_1);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_5);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_10);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_25);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_50);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_75);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_90);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_99);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_999);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_9999);
  gsl_rstat_quantile_add((double) u->amount,*workspace_ptr_99999);


  /* UTXO Count */
  utxo_count_block += 1;
  utxo_unspent_output_count_block += u->amount;
}

static void print_header(char *database)
{
  printf("# DDL\n");
  printf("CREATE DATABASE %s\n", database);
  printf("\n");
  printf("# DML\n");
  printf("# CONTEXT-DATABASE: %s\n", database);
  printf("\n");
}

#define DESCRIPTION "\n"\
  "\n"\
  "Outputs summary statistics for each block in the following format, suitable for\n"\
  "bulk-loading by InfluxDB:\n"\
  "\n"\
  "  block,fork=STRING height=INTEGER,version=INTEGER TIMESTAMP\n"\
  "  sizes,fork=STRING,v=STRING block=INTEGER,block.cum=INTEGER,transaction=INTEGER,transaction.cum=INTEGER TIMESTAMP\n"\
  "  counts,fork=STRING,v=STRING transaction=INTEGER,transaction.cum=INTEGER,segwit.transaction=INTEGER,segwit.transaction.cum=INTEGER,input=INTEGER,input.cum=INTEGER,output=INTEGER,output.cum=INTEGER,utxoset=INTEGER,utxo=INTEGER TIMESTAMP\n"\
  "  fees,fork=STRING,v=STRING block=INTEGER,cum=INTEGER,per.transaction=FLOAT,per.byte=FLOAT,smallest per.byte=FLOAT,largest per.byte=FLOAT,per.vbyte=FLOAT,smallest per.vbyte=FLOAT,largest per.vbyte=FLOAT TIMESTAMP\n"\
  "  btc,fork=STRING,v=STRING created=FLOAT,created.cum=FLOAT,spendable.cum=FLOAT TIMESTAMP\n"\
  "  days,fork=STRING,v=STRING destroyed=FLOAT,destroyed.cum=FLOAT,created=FLOAT,created.cum=FLOAT TIMESTAMP\n"\
  "  utxo.age,fork=STRING,v=STRING <1d=FLOAT,1d-1w=FLOAT,1w-1m=FLOAT,1-3m=FLOAT,3-6m=FLOAT,6-12m=FLOAT,12-18m=FLOAT,18-24m=FLOAT,2-3y=FLOAT,3-5y=FLOAT,>5y=FLOAT TIMESTAMP\n"\
  "\n"\
  "  utxo.amount,fork=STRING,v=STRING <10=FLOAT, <100=FLOAT, <1,000=FLOAT, <10,000=FLOAT, <100,000=FLOAT, <1,000,000=FLOAT,10,000,000=FLOAT, <100,000,000=FLOAT, <1,000,000,000=FLOAT, <10,000,000,000=FLOAT, <100,000,000,000=FLOAT, <1,000,000,000,000=FLOAT, <10,000,000,000,000=FLOAT, <100,000,000,000,000=FLOAT, <1,000,000,000,000,000=FLOAT TIMESTAMP\n"\
  "\n"\
  "Each line is represents a single InfluxDB data point in line protocol format:\n"\
  "\n"\
  "  SERIES,TAGSET -- the InfluxDB series name ('block', 'sizes', &c.) and \n"\
  "                   tagset.  Tag values are always strings. Tag names include:\n"\
  "\n"\
  "                     fork -- the name of the software which mined the block\n"\
  "                     v    -- the block version of the block\n"\
  "\n"\
  "  FIELDSET      -- the InfluxDB fieldset.  Field values are typed as:\n"\
  "\n"\
  "                     INTEGER -- formatted with a trailing 'i'\n"\
  "                     FLOAT   -- formatted normally\n"\
  "\n"\
  "  TIMESTAMP     -- the InfluxDB timestamp in seconds (UNIX timestamp)\n"\
  "\n"\
  "The series schema are as follows:\n"\
  "\n"\
  "  block -- Information about the block itself.\n"\
  "\n"\
  "    height  -- (INTEGER)\n"\
  "    version -- (INTEGER)\n"\
  "\n"\
  "  sizes -- Data sizes in the block (in bytes).\n"\
  "\n"\
  "    block           -- block itself (INTEGER)\n"\
  "    block.cum       -- cum. size of all blocks so far (INTEGER)\n"\
  "    transaction     -- cum. size of all transactions in the block (INTEGER)\n"\
  "    transaction.cum -- cum. size of all transactions so far (INTEGER)\n"\
  "\n"\
  "  counts -- Counts in the block.\n"\
  "\n"\
  "    transaction            -- transactions in the block itself (INTEGER)\n"\
  "    transaction.cum        -- transactions so far (INTEGER)\n"\
  "    segwit.transaction     -- segwit transactions in the block itself (INTEGER)\n"\
  "    segwit.transaction.cum -- segwit transactions so far (INTEGER)\n"\
  "    input                  -- inputs in the block itself (INTEGER)\n"\
  "    input.cum              -- inputs so far (INTEGER)\n"\
  "    output                 -- outputs in the block itself (INTEGER)\n"\
  "    output.cum             -- outputs so far (INTEGER)\n"\
  "    utxoset                -- number of UTXOs at this block (INTEGER)\n"\
  "    utxo                   -- number of UTXO outputs at this block (INTEGER)\n"\
  "\n"\
  "  fees -- Fees in the block (in Satoshis).\n"\
  "\n"\
  "    block           -- total fees for this block (INTEGER)\n"\
  "    cum             -- total fees so far (INTEGER)\n"\
  "    per.transaction -- average fee per transaction in this block (FLOAT)\n"\
  "    per.byte        -- average fee per byte in this block (FLOAT)\n"\
  "\n"\
  "  btc -- Money supply as of this block (in BTC).\n"\
  "\n"\
  "    created       -- BTC created in this block (FLOAT)\n"\
  "    created.cum   -- BTC created so far (FLOAT)\n"\
  "    spendable.cum -- BTC created so far which remains spendable (FLOAT)\n"\
  "\n"\
  "  days -- Days created/destroyed in this block (in Satoshi-days).\n"\
  "\n"\
  "    destroyed     -- days destroyed in this block (FLOAT)\n"\
  "    destroyed.cum -- days destroyed so far (FLOAT)\n"\
  "    created       -- days created in this block (FLOAT)\n"\
  "    created.cum   -- days created so far (FLOAT)\n"\
    "\n"\
  "  age -- Counts by age of the UTXO set of this block (in BTC) (FLOAT).\n"\
  "\n"\
  "The following options are available:\n"

int main(int argc, char *argv[])
{
  char *blockdir = NULL, *cachedir = NULL;
  bool use_testnet = false;
  unsigned long block_start = 0, block_end = -1UL;
  u8 tip[SHA256_DIGEST_LENGTH] = { 0 }, start_hash[SHA256_DIGEST_LENGTH] = { 0 };
  bool needs_utxo = true;
  unsigned int utxo_period = 144;
  bool use_mmap = true;
  unsigned progress_marks = 0;
  bool quiet = false;
  char *database = NULL;
  
  err_set_progname(argv[0]);
  opt_register_noarg("-h|--help", opt_usage_and_exit, DESCRIPTION, "Print this help");
  opt_register_arg("--utxo-period", opt_set_uintval, NULL,
		   &utxo_period, "Loop over UTXOs every this many blocks");
  opt_register_arg("--progress", opt_set_uintval, NULL,
		   &progress_marks, "Print . to stderr this many times");
  opt_register_noarg("--no-mmap", opt_set_invbool, &use_mmap,
		     "Don't mmap the block files");
  opt_register_noarg("--quiet|-q", opt_set_bool, &quiet,
		     "Don't output progress information");
  opt_register_noarg("--testnet|-t", opt_set_bool, &use_testnet,
		     "Look for testnet3 blocks");
  opt_register_arg("--blockdir", opt_set_charp, NULL, &blockdir,
		   "Block directory instead of ~/.bitcoin/[testnet3/]blocks");
  opt_register_arg("--end-hash", opt_set_hash, NULL, tip,
		   "Best blockhash to use instead of longest chain.");
  opt_register_arg("--start-hash", opt_set_hash, NULL, start_hash,
		   "Blockhash to start at instead of genesis.");
  opt_register_arg("--start", opt_set_ulongval, NULL, &block_start,
		   "Block number to start instead of genesis.");
  opt_register_arg("--end", opt_set_ulongval, NULL, &block_end,
		   "Block number to end at instead of longest chain.");
  opt_register_arg("--cache", opt_set_charp, NULL, &cachedir,
		   "Cache for multiple runs.");
  opt_register_arg("--database", opt_set_charp, NULL, &database,
		   "Include DDL/DML header for given InfluxDB database name.");
  
  opt_parse(&argc, argv, opt_log_stderr_exit);

  if (argc != 1)
    opt_usage_and_exit(NULL);

  if (database) {
    print_header(database);
  }
  /*Initialize UTXO Quantile workspaces*/
  gsl_rstat_quantile_workspace *work_0001 = gsl_rstat_quantile_alloc(0.00001);
  workspace_ptr_0001 = &work_0001;
  gsl_rstat_quantile_workspace *work_001  = gsl_rstat_quantile_alloc(0.0001);
  workspace_ptr_001 = &work_001;
  gsl_rstat_quantile_workspace *work_01   = gsl_rstat_quantile_alloc(0.001);
  workspace_ptr_01 = &work_01;
  gsl_rstat_quantile_workspace *work_1    = gsl_rstat_quantile_alloc(0.01);
  workspace_ptr_1 = &work_1;
  gsl_rstat_quantile_workspace *work_5    = gsl_rstat_quantile_alloc(0.05);
  workspace_ptr_5 = &work_5;
  gsl_rstat_quantile_workspace *work_10   = gsl_rstat_quantile_alloc(0.1);
  workspace_ptr_10 = &work_10;
  gsl_rstat_quantile_workspace *work_25   = gsl_rstat_quantile_alloc(0.25);
  workspace_ptr_25 = &work_25;
  gsl_rstat_quantile_workspace *work_50   = gsl_rstat_quantile_alloc(0.50);
  workspace_ptr_50 = &work_50;
  gsl_rstat_quantile_workspace *work_75   = gsl_rstat_quantile_alloc(0.75);
  workspace_ptr_75 = &work_75;
  gsl_rstat_quantile_workspace *work_90   = gsl_rstat_quantile_alloc(0.90);
  workspace_ptr_90 = &work_90;
  gsl_rstat_quantile_workspace *work_95   = gsl_rstat_quantile_alloc(0.95);
  workspace_ptr_95 = &work_95;
  gsl_rstat_quantile_workspace *work_99   = gsl_rstat_quantile_alloc(0.99);
  workspace_ptr_99 = &work_99;
  gsl_rstat_quantile_workspace *work_999  = gsl_rstat_quantile_alloc(0.999);
  workspace_ptr_999 = &work_999;
  gsl_rstat_quantile_workspace *work_9999 = gsl_rstat_quantile_alloc(0.9999);
  workspace_ptr_9999 = &work_9999;
  gsl_rstat_quantile_workspace *work_99999= gsl_rstat_quantile_alloc(0.99999);
  workspace_ptr_99999 = &work_99999;
  
  iterate(blockdir, cachedir,
	  use_testnet,
	  block_start, block_end, start_hash, tip,
	  needs_utxo, utxo_period,
	  use_mmap,
	  progress_marks, quiet,
	  transition_to_new_block, accumulate_transaction, accumulate_input, accumulate_output, accumulate_utxo);
  return 0;
}
