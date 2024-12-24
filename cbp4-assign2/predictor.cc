#include "predictor.h"

#define NUM_PT_ENTRIES          4096
#define NUM_BHT_ENTRIES         512
#define NUM_PHT_ENTRIES         64       // 2^6 = 64
#define NUM_PHT                 8
UINT32 pt[NUM_PT_ENTRIES];
UINT32 bht[NUM_BHT_ENTRIES];
UINT32 pht[NUM_PHT][NUM_PHT_ENTRIES];

#define NUM_PERCEPTRON_ENTRIES  256           // 16384/(32*2) = 256 (2^8) - cache size / (history length * size of int16_t)
#define THRESHOLD               100           // Deviated from ideal THRESHOLD = 1.93*HISTORY_LENGTH+14 (from research paper) - THRESHOLD is a parameter for the training algorithm to use to determine when enough training has been done
#define HISTORY_LENGTH          32            // Optimal length found through trial-and-error for 16 KB cache size
bool ghr[HISTORY_LENGTH];                     // global history register
int16_t perceptron_t[NUM_PERCEPTRON_ENTRIES][HISTORY_LENGTH]; // signed because perceptron should be either 1 or **-1** to denote TAKEN or NOT_TAKEN

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

void InitPredictor_2bitsat() {
  for (int i = 0; i < NUM_PT_ENTRIES; i++) {
    pt[i] = 0b01;  // weak not-taken is the initial state of saturating counters
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {

  UINT32 index = PC & 0x00000FFF;  // 12 bit index because 2^12 = 4096 which is the number of entries we have in the prediction table

  if ((pt[index] == 0b00) || (pt[index] == 0b01)) {
    return NOT_TAKEN;
  } else {
    return TAKEN;
  }
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

  UINT32 index = PC & 0x00000FFF;  // 12 bit index because 2^12 = 4096 which is the number of entries we have in the prediction table

  if ((resolveDir == NOT_TAKEN) && (predDir == NOT_TAKEN)) {
    pt[index] = 0b00;
  }
  if ((resolveDir == TAKEN) && (predDir == NOT_TAKEN)) {
    pt[index] += 0b1;
  }
  if ((resolveDir == NOT_TAKEN) && (predDir == TAKEN)) {
    pt[index] -= 0b1;
  }
  if ((resolveDir == TAKEN) && (predDir == TAKEN)) {
    pt[index] = 0b11;
  }
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

void InitPredictor_2level() {
  int i, j;
  for (i = 0; i < NUM_BHT_ENTRIES; i++) {
    bht[i] = 0b00;  // not-taken is the initial state of branch history table entries
  }

  for (i = 0; i < NUM_PHT; i++) {
    for (j = 0; j < NUM_PHT_ENTRIES; j++) {
      pht[i][j] = 0b01;  // not-taken is the initial state of pattern history table entries
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {

  UINT32 bht_index = (PC >> 3) & 0x000001FF;  // 9-bits after 3-bits for BHT index
  UINT32 pht_index = PC & 0x00000007;         // lower 3-bits for PHT index

  if ((pht[pht_index][bht[bht_index]] == 0b00) || (pht[pht_index][bht[bht_index]] == 0b01)) {
    return NOT_TAKEN;
  } else {
    return TAKEN;
  }
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

  UINT32 bht_index = (PC >> 3) & 0x000001FF;  // 9-bits after 3-bits for BHT index
  UINT32 pht_index = PC & 0x00000007;         // lower 3-bits for PHT index

  if ((resolveDir == NOT_TAKEN) && (predDir == NOT_TAKEN)) {
    pht[pht_index][bht[bht_index]] = 0b00;
  }
  if ((resolveDir == TAKEN) && (predDir == NOT_TAKEN)) {
    pht[pht_index][bht[bht_index]] += 0b1;
  }
  if ((resolveDir == NOT_TAKEN) && (predDir == TAKEN)) {
    pht[pht_index][bht[bht_index]] -= 0b1;
  }
  if ((resolveDir == TAKEN) && (predDir == TAKEN)) {
    pht[pht_index][bht[bht_index]] = 0b11;
  }

  // bht entry should contain 6 history bits
  bht[bht_index] = ((bht[bht_index] << 1) | resolveDir) & 0x3F;
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////
// Perceptron-based Branch Predictor
// https://www.youtube.com/watch?v=nGkwqS6RyDU&t=328s - YouTube link for algorithm overview
// https://www.cs.utexas.edu/~lin/papers/hpca01.pdf - Research paper link used to determine NUM_PERCEPTRON_ENTRIES, HISTORY_LENGTH, THRESHOLD values for given available hardware storage (128 Kbits / 16 KB)

void InitPredictor_openend() {
  int i, j;
  for (i = 0; i < HISTORY_LENGTH; i++) {
    ghr[i] = 0;                                   // not-taken is the initial state of global history register entries
  }

  for (i = 0; i < NUM_PERCEPTRON_ENTRIES; i++) {
    for (j = 0; j < HISTORY_LENGTH; j++) {
      perceptron_t[i][j] = 0;                     // not-taken is the initial state of perceptron table entries
    }
  }
}

bool GetPrediction_openend(UINT32 PC) {
  int i, pred = 0;
  UINT32 index = PC & (NUM_PERCEPTRON_ENTRIES - 1); // index so PC is mapped to valid entry in perceptron table

  // assigns prediction based on a weighted sum of past branch history as an input to the perceptron algorithm
  // if ghr entry holds NOT_TAKEN (0) then decrement prediction value by that perceptron table entry value for that specific history length (i) (denoting an increase likelihood of NOT_TAKEN / higher weight of NOT_TAKEN), 
  // otherwise if ghr entry holds TAKEN (1) then do opposite and increment prediction value with perceptron table entry value for that specific history length (i)
  for (i = 0; i < HISTORY_LENGTH; i++) {
    pred += (ghr[i] == 0) ? -perceptron_t[index][i] : perceptron_t[index][i];
  }

  if (pred < 0) {
    return NOT_TAKEN;   // if perceptron is -1 (negative) then branch is NOT_TAKEN
  } else {
    return TAKEN;       // if perceptron is 1 (positive) then branch is TAKEN
  }
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int i, pred = 0;
  UINT32 index = PC & (NUM_PERCEPTRON_ENTRIES - 1);

  for (i = 0; i < HISTORY_LENGTH; i++) {
    pred += (ghr[i] == 0) ? -perceptron_t[index][i] : perceptron_t[index][i];
  }

  // pred must be absolute value (positive)
  if (pred < 0) { 
    pred = pred*-1; 
  }

  // weights are dynamically calculated using formula: if predicted outcome using perceptrons != actual outcome taken or abs(prediction) <= THRESHOLD then re-calculate weights
  if ((predDir != resolveDir) || (pred <= THRESHOLD)) {
    for (i = 0; i < HISTORY_LENGTH; i++) {
      if (resolveDir == ghr[i]) {
        // if branch direction is equal to the prediction ghr holds for a particular history length then it made right prediction for that entry and should increase perceptron value weight
        perceptron_t[index][i] = (perceptron_t[index][i] < 32767) ? perceptron_t[index][i] + 1 : 32767;  // Clamp at 32767 ((2^16 / 2) - 1)
      } else {
        // if branch direction is NOT equal to the prediction ghr holds for a particular history length then it made wrong prediction for that entry and should decrement perceptron value weight
        perceptron_t[index][i] = (perceptron_t[index][i] > -32768) ? perceptron_t[index][i] - 1 : -32768;  // Clamp at -32768 (-2^16 / 2)
      }
    }
  }

  // updating branch history for increased prediction accuracy for future branches
  for (i = 1; i < HISTORY_LENGTH; i++) {
    ghr[i-1] = ghr[i];  // removes oldest branch history bit to make space for newest branch outcome

    if (i == HISTORY_LENGTH - 1) {
      ghr[i] = resolveDir;  // last entry in ghr is updated to hold most recent branch outcome 
    }
  }
}
