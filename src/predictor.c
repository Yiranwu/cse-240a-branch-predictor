//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Gandhar Deshpande";
const char *studentID = "A59005457";
const char *email = "gdeshpande@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

//define number of bits required for indexing the BHT here.
int ghistoryBits = 14; // Number of bits used for Global History
int lhistoryBits = 10;
int bpType;       // Branch Prediction Type
int verbose;


#include <stdlib.h>
#include <execinfo.h>
void print_trace(void) {
    char **strings;
    size_t i, size;
    enum Constexpr { MAX_SIZE = 1024 };
    void *array[MAX_SIZE];
    size = backtrace(array, MAX_SIZE);
    strings = backtrace_symbols(array, size);
    for (i = 0; i < size; i++)
        printf("%s\n", strings[i]);
    puts("");
    free(strings);
}
//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//gshare
uint8_t *bht_gshare;
uint64_t ghistory;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

//gshare functions
void init_gshare() {
    int bht_entries = 1 << ghistoryBits;
    bht_gshare = (uint8_t *) malloc(bht_entries * sizeof(uint8_t));
    for (int i = 0; i < bht_entries; i++) {
        bht_gshare[i] = WN;
    }
    ghistory = 0;
}

uint8_t saturate_counter_predict(uint8_t bht_entry) {
    switch (bht_entry) {
        case WN:
            return NOTTAKEN;
        case SN:
            return NOTTAKEN;
        case WT:
            return TAKEN;
        case ST:
            return TAKEN;
        default:
            printf("Warning: Undefined state of entry in GSHARE BHT! %d\n", bht_entry);
            print_trace();
            return NOTTAKEN;
    }
}

uint32_t get_lower_bits(uint32_t addr, int nbits) {
    uint32_t mask = (1 << nbits)-1;
    uint32_t lower_bits = addr & mask;
    return lower_bits;
}

uint32_t get_gshare_bht_index(uint32_t pc) {

    uint32_t index = get_lower_bits(pc, ghistoryBits) ^ get_lower_bits(ghistory, ghistoryBits);
    return index;
}

uint8_t gshare_predict(uint32_t pc) {
    uint32_t index = get_gshare_bht_index(pc);
    return saturate_counter_predict(bht_gshare[index]);
}

void saturate_counter_update(uint8_t *bht_entry, uint8_t outcome) {
    switch (*bht_entry) {
        case WN:
            *bht_entry = (outcome == TAKEN) ? WT : SN;
            break;
        case SN:
            *bht_entry = (outcome == TAKEN) ? WN : SN;
            break;
        case WT:
            *bht_entry = (outcome == TAKEN) ? ST : WN;
            break;
        case ST:
            *bht_entry = (outcome == TAKEN) ? ST : WT;
            break;
        default:
            printf("Warning: Undefined state of entry in GSHARE BHT! : %d\n", *bht_entry);
    }
}

void train_gshare(uint32_t pc, uint8_t outcome) {
    uint32_t index = get_gshare_bht_index(pc);

    //Update state of entry in bht based on outcome
    saturate_counter_update(&bht_gshare[index], outcome);

    //Update history register
    ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare() {
    free(bht_gshare);
}


uint8_t *gbht_tournament, *lbht_tournament, *choicebht_tournament;
uint64_t ghistory, *lhistory;

//gshare functions
void init_tournament() {
    int gbht_entries = 1 << ghistoryBits;
    gbht_tournament = (uint8_t *) malloc(gbht_entries * sizeof(uint8_t));
    for (int i = 0; i < gbht_entries; i++) {
        gbht_tournament[i] = WN;
    }
    int lbht_entries = 1 << lhistoryBits;
    lbht_tournament = (uint8_t *) malloc(lbht_entries * sizeof(uint8_t));
    for (int i = 0; i < lbht_entries; i++) {
        lbht_tournament[i] = WN;
    }
    int choicebht_entries = 1 << ghistoryBits;
    choicebht_tournament = (uint8_t *) malloc(choicebht_entries * sizeof(uint8_t));
    for (int i = 0; i < choicebht_entries; i++) {
        choicebht_tournament[i] = 0;
    }
    int lhistory_entries = 1 << lhistoryBits;
    lhistory = (uint64_t *) malloc(lhistory_entries * sizeof(uint64_t));
    for (int i = 0; i < lhistory_entries; i++) {
        lhistory[i] = 0;
    }
    ghistory = 0;
}

uint8_t tournament_predict(uint32_t pc) {
    uint32_t gindex = get_gshare_bht_index(pc);
    uint32_t pcindex = get_lower_bits(pc, lhistoryBits);
    uint32_t lindex = get_lower_bits(lhistory[pcindex], lhistoryBits);

    uint8_t lpred = saturate_counter_predict(lbht_tournament[lindex]);
    uint8_t gpred = saturate_counter_predict(gbht_tournament[gindex]);
    uint8_t choice = saturate_counter_predict(choicebht_tournament[pcindex]);

    // choice true -> local pred
    return choice?lpred:gpred;
    //return gpred;
}

void train_tournament(uint32_t pc, uint8_t outcome) {

    uint32_t gindex = get_gshare_bht_index(pc);
    uint32_t pcindex = get_lower_bits(pc, lhistoryBits);
    uint32_t lindex = get_lower_bits(lhistory[pcindex], lhistoryBits);

    uint8_t *lentry = &lbht_tournament[lindex];
    uint8_t *gentry = &gbht_tournament[gindex];
    uint8_t *choiceEntry = &choicebht_tournament[pcindex];

    uint8_t lpred = saturate_counter_predict(*lentry), gpred = saturate_counter_predict(*gentry), choice = saturate_counter_predict(*choiceEntry);
    uint8_t pred = choice ? lpred : gpred;

    saturate_counter_update(lentry, outcome);
    saturate_counter_update(gentry, outcome);
    if (lpred != gpred) saturate_counter_update(choiceEntry, outcome == lpred);

    //Update history register
    ghistory = ((ghistory << 1) | outcome);
    lhistory[lindex] = ((lhistory[lindex] << 1) | outcome);
}

void cleanup_tournament() {
    free(gbht_tournament);
    free(lbht_tournament);
    free(choicebht_tournament);
    free(lhistory);
}


void init_predictor() {
    switch (bpType) {
        case STATIC:
        case GSHARE:
            init_gshare();
            break;
        case TOURNAMENT:
            init_tournament();
            break;
        case CUSTOM:
        default:
            break;
    }

}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc) {

    // Make a prediction based on the bpType
    switch (bpType) {
        case STATIC:
            return TAKEN;
        case GSHARE:
            return gshare_predict(pc);
        case TOURNAMENT:
            return tournament_predict(pc);
        case CUSTOM:
        default:
            break;
    }

    // If there is not a compatable bpType then return NOTTAKEN
    return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint8_t outcome) {

    switch (bpType) {
        case STATIC:
        case GSHARE:
            return train_gshare(pc, outcome);
        case TOURNAMENT:
            return train_tournament(pc, outcome);
        case CUSTOM:
        default:
            break;
    }


}
