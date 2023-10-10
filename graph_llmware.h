
// Copyright 2023 llmware

// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License.  You
// may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.  See the License for the specific language governing
// permissions and limitations under the License.

//  graph_llmware.h
//  graph_llmware
//
//
//  consolidation of Mac OS and Linux build versions -> harmonizing the source code
//  parameterizing all file paths as function inputs
//
//
//  High-volume processing utility functions associated with post-parsing
//
//      1. text_extract_main_handler -> reads database entries, excludes stop words, and
//              creates a bag-of-words txt file for entire library
//              -- adding parameterized file_paths
//              -- better stop_words list handling
//              -- adding ability to do incremental creation, e.g., add to existing bow.txt
// 
//      2. bow_context_table-main -> picks up the bow.txt and creates bg.txt 'bloks graph'
//              -- needs better controls for very large libraries
//
//      3. bulk_image_handler -> reviews image directory, converts .ras files to .png files
//              -- needs better error checks


#ifndef graph_llmware_h
#define graph_llmware_h

#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <regex.h>
#include <math.h>
#include <dirent.h>
#include <err.h>
#include <time.h>
#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <png.h>


//  major function - text_extract -> generates bow.txt files in initialization

// max size of token parsing
char my_tokens[10000][100];

// max size of stop words list
char stop_words[2000][100];


char *text_clean (char*tok);
int tokenizer (char*core_text);
int stop_words_checker (char*tok, int stop_words_len);


int text_extract_main_handler (char *input_account_name,
                               char *input_library_name,
                               int new_bow,
                               char *input_mongodb_path,
                               char *input_stop_words_fp,
                               char *input_bow_fp,
                               char *input_text_field,
                               int bow_index,
                               int bow_len);

// end - text_extract function

int text_extract_one_doc_handler (char *input_account_name,
                                  char *input_library_name,
                                  char *input_mongodb_path,
                                  char *input_stop_words_fp,
                                  char *input_bow_fp,
                                  char *input_text_field);

// deprecated
int text_extract_main_handler_old_copy_works (char *input_account_name,
                                              char *input_library_name,
                                              int doc_start,
                                              char *input_mongodb_path,
                                              char *input_stop_words_fp,
                                              char *input_bow_fp,
                                              char *input_text_field,
                                              int bow_index,
                                              int bow_len);

// right now this is the max size of the bow file = 10,000,000
// once 10M reached, new BOW is created and bow_index is incremented
// BOW creation can theoretically scale infinitely large

int BOW_MAX = 10000000;


typedef struct {
    int count;
    char context_word[50];
} bg;


// graph[][] = 'co-occurrence matrix'
//  --> creates max size of 6000 target words & 12000 context words
//
//  --this requires 72M of memory for 6000 X 12000 matrix
//  --assumes that even as BOW increases in size, the graph size is not much bigger
//  --...but graph will be 'deeper' with high volume of co-occurences within the network

int graph[6000][12000];




//  current implementation of graph builder
//  ...scales mostly linearly with size of the BOW
//  relies upon mcw -> unique vocabulary list, and effectively applies ...
//  ...a tokenization of the BOW

int graph_builder_old_works (char *input_account_name,
                   char *input_library_name,
                   char *input_bow_fp,
                   char *input_mcw_fp,
                   int bow_index,
                   int bow_len,
                   int mcw_target_len,
                   int mcw_context_len,
                   int vocab_len,
                   char *graph_fp,
                   int graph_index,
                   int graph_max_size,
                   int min_len);


int graph_builder (char *input_account_name,
                   char *input_library_name,
                   char *input_bow_fp,
                   char *input_mcw_fp,
                   int bow_index,
                   int bow_len,
                   int mcw_target_len,
                   int mcw_context_len,
                   int vocab_len,
                   char *graph_fp,
                   int graph_index,
                   int graph_max_size,
                   int min_len);

// note: several input variables not currently being used
// future direction:  need to scale to handle multiple BOW files and enable iterative graph building
// right now, graph is limited to a single BOW file -> 10M words
// processing of 10M word BOW takes approximately ~90 seconds

int cmpfunc( const void *a, const  void *b);
int cstring_cmp(const void *a, const void *b);
int myStrCmp (const void * a, const void * b);
int cmpstr(const void* a, const void* b);
int struct_cmp_by_count(const void *a, const void *b);

// alternative implementation of graph_builder function
// works on small libraries well, but has scaling challenges
// also, has hard-coded paths

int bow_context_table_main  (char *input_account_name, char *input_corpus_name, int bow_len, int target_len);

// variables below used in bow_context_table_main
// memory intensive + does not scale well
// 10M BOW takes ~25 minutes

//char *context_table[1000000];
char *context_table[10];
//char bow[10000000][50];
char bow[10][50];


// end - graph building function


// major function - bulk image handler -> converts raw binary formats into png for viewing

int bulk_image_handler(char *input_account_name,
                       char *input_corpus_name,
                       char *input_mongodb_path,
                       char *image_fp,
                       int min_height,
                       int min_width);

char *get_image_file_type (char *full_name);

void setRGB(png_byte *ptr, float val);


// end - bulk_image_handler

#endif /* graph_functions_h */
