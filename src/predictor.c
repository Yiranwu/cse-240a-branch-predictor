//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Yiran Wu";
const char *studentID = "A59004775";
const char *email = "yiw073@ucsd.edu";

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

// gshare/bimodal tournament bp
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
}

void train_tournament(uint32_t pc, uint8_t outcome) {

    uint32_t gindex = get_gshare_bht_index(pc);
    uint32_t pcindex = get_lower_bits(pc, lhistoryBits);
    uint32_t lindex = get_lower_bits(lhistory[pcindex], lhistoryBits);

    uint8_t *lentry = &lbht_tournament[lindex];
    uint8_t *gentry = &gbht_tournament[gindex];
    uint8_t *choiceEntry = &choicebht_tournament[pcindex];

    uint8_t lpred = saturate_counter_predict(*lentry), gpred = saturate_counter_predict(*gentry), choice = saturate_counter_predict(*choiceEntry);

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

// YAGS bp

int custom_history_bits = 13;
int custom_cache_bits = 10, custom_cache_tag_bits = 6;
const int nway = 2;
uint8_t *choicebht_custom, *t_cache_tags[2], *t_cache_2bcs[2], *nt_cache_tags[2], *nt_cache_2bcs[2], *t_cache_LRU, *nt_cache_LRU;
uint64_t ghistory;

void init_custom() {
    int choicebht_entries = 1 << custom_history_bits;
    choicebht_custom = (uint8_t *) malloc(choicebht_entries * sizeof(uint8_t));
    for (int i = 0; i < choicebht_entries; i++) {
        choicebht_custom[i] = WT;
    }
    int cache_entries = 1 << custom_cache_bits;
    t_cache_tags[0] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    t_cache_tags[1] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    t_cache_2bcs[0] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    t_cache_2bcs[1] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    nt_cache_tags[0] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    nt_cache_tags[1] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    nt_cache_2bcs[0] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    nt_cache_2bcs[1] = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    t_cache_LRU = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    nt_cache_LRU = (uint8_t *) malloc(cache_entries * sizeof(uint8_t));
    for (int i = 0; i < cache_entries; i++) {
        t_cache_tags[0][i] = 0;
        t_cache_tags[1][i] = 0;
        t_cache_2bcs[0][i] = WN;
        t_cache_2bcs[1][i] = WN;
        nt_cache_tags[0][i] = 0;
        nt_cache_tags[1][i] = 0;
        nt_cache_2bcs[0][i] = WN;
        nt_cache_2bcs[1][i] = WN;
        t_cache_LRU[i] = 0;
        nt_cache_LRU[i] = 0;
    }
    ghistory = 0;
}

int cache_lookup(uint8_t **target_cache_tags, uint32_t cache_index, uint32_t pc_tag) {

    for(int i=0;i<2;++i) {
        uint8_t cache_tag = target_cache_tags[i][cache_index];
        if(cache_tag == pc_tag) return i;
    }
    return -1;
}

int cache_hit_cnt=0, cache_miss_cnt=0, verbose_cnt=0;

void cache_update(uint8_t **target_cache_tags, uint8_t **target_cache_2bcs, uint8_t *target_LRU,
                  uint32_t cache_index, uint32_t pc_tag, uint8_t outcome) {
    int hit_way = cache_lookup(target_cache_tags, cache_index, pc_tag);
    if(hit_way!=-1) {
        ++cache_hit_cnt;
        saturate_counter_update(&target_cache_2bcs[hit_way][cache_index], outcome);
        target_LRU[cache_index] = hit_way ^ 1;
    }
    else {
        ++cache_miss_cnt;
        target_cache_tags[target_LRU[cache_index]][cache_index] = pc_tag;
        target_cache_2bcs[target_LRU[cache_index]][cache_index] = outcome ? WT : WN;
        target_LRU[cache_index] ^= 1;
    }
}

int pred_verbose_cnt=0;


uint8_t custom_predict(uint32_t pc) {
    uint32_t choice_index = get_lower_bits(pc, custom_history_bits);
    uint8_t choice = saturate_counter_predict(choicebht_custom[choice_index]);
    uint32_t cache_key = get_lower_bits(pc, custom_history_bits) ^ get_lower_bits(ghistory, custom_history_bits);
    uint32_t cache_index = get_lower_bits(cache_key, custom_cache_bits), pc_tag = get_lower_bits(pc, custom_cache_tag_bits);
    uint8_t **target_cache_tags = choice?nt_cache_tags:t_cache_tags, *target_LRU = choice?nt_cache_LRU:t_cache_LRU;
    uint8_t **target_cache_2bcs = choice?nt_cache_2bcs:t_cache_2bcs;
    int hit_way = cache_lookup(target_cache_tags, cache_index, pc_tag);

    if(hit_way!=-1) {
        //if(verbose_cnt<200)
        //if(verbose_cnt>=1000000 && verbose_cnt<1000200)
        //    printf("pred: hit, index=%d, answer=%d\n", cache_index, saturate_counter_predict(target_cache_2bcs[hit_way][cache_index]));
        return saturate_counter_predict(target_cache_2bcs[hit_way][cache_index]);
    }
    else {
        //if(verbose_cnt<200)

        //if(verbose_cnt>=1000000 && verbose_cnt<1000200)
        //    printf("pred: mis, index=%d, answer=%d\n", cache_index, choice);

        return choice;
    }
}

int choice_correct_cnt=0;

void train_custom(uint32_t pc, uint8_t outcome) {
    verbose_cnt++;
    //if(verbose_cnt<200)

    //if(verbose_cnt>=1000000 && verbose_cnt<1000200)
    //    printf("pc=%d, outcome=%d\n", pc, outcome);

    uint32_t choice_index = get_lower_bits(pc, custom_history_bits);
    uint8_t choice = saturate_counter_predict(choicebht_custom[choice_index]);
    uint32_t cache_key = get_lower_bits(pc, custom_history_bits) ^ get_lower_bits(ghistory, custom_history_bits);
    uint32_t cache_index = get_lower_bits(cache_key, custom_cache_bits), pc_tag = get_lower_bits(pc, custom_cache_tag_bits);
    uint8_t **target_cache_tags = choice?nt_cache_tags:t_cache_tags, *target_LRU = choice?nt_cache_LRU:t_cache_LRU;
    uint8_t **target_cache_2bcs = choice?nt_cache_2bcs:t_cache_2bcs;

    if(choice != outcome || cache_lookup(target_cache_tags, cache_index, pc_tag)!=-1) {
        cache_update(target_cache_tags, target_cache_2bcs, target_LRU, cache_index, pc_tag, outcome);
    }
    else {
        ++choice_correct_cnt;
        //if(verbose_cnt<200)
        //    printf("choice is correct\n");
    }

    saturate_counter_update(&choicebht_custom[choice_index], outcome);

    //Update history register
    ghistory = ((ghistory << 1) | outcome);

    if(verbose_cnt%100000==0){
        //printf("hit rate: %.3f\n", ((float)cache_hit_cnt)/verbose_cnt);
        //printf("choice correct rate: %.3f\n", ((float)choice_correct_cnt)/verbose_cnt);
    }
}

void cleanup_custom() {
    free(choicebht_custom);
    free(t_cache_tags[0]);
    free(t_cache_tags[1]);
    free(t_cache_2bcs[0]);
    free(t_cache_2bcs[1]);
    free(nt_cache_tags[0]);
    free(nt_cache_tags[1]);
    free(nt_cache_2bcs[0]);
    free(nt_cache_2bcs[1]);
    free(t_cache_LRU);
    free(nt_cache_LRU);
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
            init_custom();
            break;
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
            return custom_predict(pc);
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
            return train_custom(pc, outcome);
        default:
            break;
    }


}
