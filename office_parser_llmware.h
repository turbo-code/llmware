
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

//  office_parser_llmware.h
//  office parser llmware


#ifndef office_parser_llmware_h
#define office_parser_llmware_h

#include <stdio.h>
#include <libxml/xmlreader.h>
#include <libxml/parser.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <regex.h>
#include <math.h>
#include <dirent.h>
#include <bson/bson.h>
#include <zip.h>
#include <mongoc/mongoc.h>
#include <err.h>
#include <time.h>

//#include <ctype.h>
//#include <pthread.h>


typedef struct {
    char *r_id;
    char *external_file_name;
} rels;

typedef struct {
    //int text;
    char text[100];
    int x;
    int y;
} emf_text_coords;

typedef struct {
    int x;
    int y;
    int cx;
    int cy;
} coords;

typedef struct {
    int b;
    int u;
    int i;
    int sz;
    char color[20];
    int format_flag;
} format;


typedef struct {
    int slide_num;
    int shape_num;
    coords position;
    char content_type[20];
    char relationship[50];
    char formatted_text[50000];
    char ft_tmp[25000];
    char text_run[50000];
    char linked_text[50000];
    // table_text works stable at 100000
    // need to expand for very large sheets
    char table_text[100000];
    char file_type[20];
} blok;

typedef struct {
    int sz;
    char *b;
    char *u;
    char *i;
    char *fills;
    char *borders;
} xl_style;


typedef struct {
    char author[500];
    char file_name[500];
    char file_short_name[300];
    char last_modified[500];
    int slide_count;
    int xml_files;
    char account_name[200];
    char corpus_name[200];
    char thread_num;
    int image_count_start;
    int iter;
    char modified_date[200];
    char created_date[200];
    char creator_tool[200];
} document;

char global_headlines[10000];

// two main global parameters for extracting current info
// adapt [50] to variable linked to MAX INPUT at any one time
// runs up against memory constraints!!

// this is large array -> designed to allow for potential multi-threading- not implemented
// stable at first index 12 -> [12][1000]
blok Bloks[1][2000];
int GLOBAL_MAX_BLOKS = 2000;

// stable at first index 12 -> [12]
document my_doc[1];

char time_stamp[64];

int blok_counter_array[12];

char global_docx_running_text[50000];
char global_docx_formatted_text[50000];

/*
typedef struct {
    char fp[200];
    int th;
} thread_args_alt;
*/

//extern const char WORKING_FOLDER[];

// stable works @ NTHREADS = 10
// #define NTHREADS 1
// int *thread_function(document *my_args, char *ws_fp);
// pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int thread_counter = 0;

int master_blok_tracker;
int master_doc_tracker;
int master_image_tracker;

int global_docx_page_tracker;
int global_docx_para_on_page_tracker;

int debug_mode;

// utility functions
char* get_file_name (char*longer_string);
int special_formatted_text (char*b,char*i,char*u,char*sz,char*color);
char *get_file_type (char*full_name);
int order_string (const void* e1, const void* e2);
char *clean_token (char *tok);
int keep_value (char *v);
int keep_value_does_not_crash_old (char *v);

int compare_emf_text_coords_y (const void *a, const void *b);
int compare_emf_text_coords_x (const void *a, const void *b);

char *number_display (char *v);

char *global_workspace_fp;
char *global_image_fp;
char *global_mongo_db_path;

int global_total_pages_added;
int global_total_tables_added;

int GLOBAL_WRITE_TO_DB;
int GLOBAL_BLOK_SIZE=400;

char* global_write_to_filename;

char* global_special_filepath;

char* global_write_to_filename;

// overall main functions


// current working entry point into parser -> no change in declaration

int add_files_main  (char *account_name,
                     char *library_name,
                     char *fp,
                     char *workspace_fp,
                     char *account_master_fp,
                     char *input_mongodb_path,
                     char *image_fp,
                     int input_debug_mode);


// intended future main entry point into parser with wider set of configuration options

int add_files_main_customize  (char *account_name,
                               char *library_name,
                               char *fp,
                               char *workspace_fp,
                               char *account_master_fp,
                               char *input_mongodb_path,
                               char *image_fp,
                               int input_debug_mode,
                               int write_to_db_on,
                               char *write_to_filename,
                               char *file_source_path);


// intended future main entry point into parser with wider set of configuration options

int add_files_main_customize_new_blok_size_config  (char *account_name,
                                                    char *library_name,
                                                    char *fp,
                                                    char *workspace_fp,
                                                    char *account_master_fp,
                                                    char *input_mongodb_path,
                                                    char *image_fp,
                                                    int input_debug_mode,
                                                    int write_to_db_on,
                                                    char *write_to_filename,
                                                    char *file_source_path,
                                                    int input_target_blok_size);




//  new main entry point with multiple files passed in folder - 'parallel' version
//  * does not use master_tracker *
//  * starts with unique_doc_num passed by client application *

int add_files_main_customize_parallel (char *account_name,
                                       char *library_name,
                                       char *fp,
                                       char *workspace_fp,
                                       char *account_master_fp,
                                       char *input_mongodb_path,
                                       char *image_fp,
                                       int input_debug_mode,
                                       int write_to_db_on,
                                       char *write_to_filename,
                                       char *file_source_path,
                                       int unique_doc_num);


// main entry point with single file passed -> will parse and route depending upon file type

int add_one_office_main  (char *account_name,
                          char *library_name,
                          char *fn,
                          char *fp,
                          char *workspace_fp,
                          char *account_master_fp,
                          char *input_mongodb_path,
                          char *image_fp,
                          int input_debug_mode,
                          int write_to_db_on,
                          char *write_to_filename,
                          char *file_source_path);

// new simplified entry point to create one office file to disk

int add_one_office (char *input_account_name,
                    char *input_library_name,
                    char *input_fp,
                    char *input_fn,
                    char *workspace_fp,
                    char *image_fp,
                    char *write_to_filename);

// end - new simplified entry point

// new simplified entry point for llmware - bulk office parsing

int add_files_main_llmware (char *input_account_name,
                            char *input_library_name,
                            char *input_fp,
                            char *workspace_fp,
                            char *input_mongodb_path,
                            char *image_fp,
                            int input_debug_mode,
                            int write_to_db_on,
                            char *write_to_filename,
                            int unique_doc_num,
                            char *db_user_name,
                            char *db_pw);


// end - new simplified entry point for llmware







// new main entry point with single file passed - parallel -> will parse and route depending upon file type

int add_one_office_main_parallel  (char *account_name,
                                   char *library_name,
                                   char *fn,
                                   char *fp,
                                   char *workspace_fp,
                                   char *account_master_fp,
                                   char *input_mongodb_path,
                                   char *image_fp,
                                   int input_debug_mode,
                                   int write_to_db_on,
                                   char *write_to_filename,
                                   char *file_source_path,
                                   int unique_doc_num);



// specific to one pptx file -> provides future option to separate / configure ppptx

int add_one_pptx_main  (char *account_name,
                        char *library_name,
                        char *fn,
                        char *fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path);

int add_one_docx_main  (char *account_name,
                        char *library_name,
                        char *fn,
                        char *fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path);

int add_one_xlsx_main  (char *account_name,
                        char *library_name,
                        char *fn,
                        char *fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path);



void do_nothing_function (void);

int handle_zip (char *fp, int t, char *ws_fp);

int write_to_db (int start_blok, int stop_blok, char *account, char *library, int doc_number, int blok_number, int wf, char* time_stamp);

int write_to_file (int start_blok, int stop_blok, char *account, char *library, int doc_number, int blok_number, char *time_stamp, char *filename, int wf);

int register_ae_add_office_event(char *ae_type, char *event, char *account,char *library,char *fp, char *time_stamp, char *current_file_name);

int update_counters (char *account, char *corpus, int blok_count, int doc_count, int image_count, char *account_fp);

int update_library_inc_totals(char*account_name, char*library_name, int inc_docs, int inc_bloks, int inc_img, int inc_pages, int inc_tables);

int pull_new_doc_id (char*account_name, char*library_name);

// builder -> no multi-threading - works with new 'add_files_main'
int builder(char *fp,int thread_number,int slide_count, char *ws_fp);

int builder_old_works (char *fp,int thread_number,int slide_count, char *ws_fp);


// docx functions
int doc_build_index (int working_folder, int local_slide_count, char *ws_fp);
int doc_para_handler (xmlNode *element_node, int global_blok_counter, int slide_num, int shape_num, int wf);
int doc_tbl_handler (xmlNode *element_node,int global_blok_counter, int slide_num, int shape_num, int wf);
int doc_post_doc_handler (int start, int stop,int global_blok_counter, int wf);

// xl functions
int xl_build_index (int working_folder, int local_sheet_count, char *ws_fp);
int xl_build_index_old_works (int working_folder, int local_sheet_count, char *ws_fp);
int xl_shared_strings_handler (int wf, char *ws_fp);
int xl_style_handler (int wf);

char shared_strings[400000][150];

xl_style style_lookup[10000];

//xmlDoc *doc;
//xmlDoc *doc_rels;

char doc_rels_fp[500];

//char *ws_fp;

// pptx functions

int pptx_build_index (int working_folder, int local_slide_count, char *ws_fp);

int pptx_build_index_old_works (int working_folder, int local_slide_count, char *ws_fp);


int sp_handler_new (xmlNode *element, int global_blok_counter, int short_last_text_blok, int slide_num, int shape_num, int wf, char *ws_fp);
int pics_handler_new (xmlNode *element, int global_blok_counter, int slide_num, int shape_num, int wf);
int gf_handler (xmlNode *element,int global_blok_counter, int slide_num,int shape_num, int wf);
int post_slide_handler (int start, int stop, int wf);
coords check_rels_coords(char *fp_rels, char *nv_id, int wf, char *ws_fp);

int save_images (int start_blok, int stop_blok, int image_count, char *account, char *corpus, int wf, char *ws_fp);

int save_images_alt (int start_blok, int stop_blok, int image_count, char *account, char *corpus, int wf, char *ws_fp);

char* rels_handler_pic_new (char *fp, char *embed, int wf, int global_blok_counter, char *ws_fp);
char* doc_rels_handler_pic_new (char *fp, char *embed, int wf, int global_blok_counter, char *ws_fp);


int emf_handler (char *fp, int global_blok_counter, int wf);

// is drawing_handler only called by doc?
int drawing_handler (xmlNode *element, int global_blok_counter, int slide_num, int shape_num, int wf);


//int grp_shp_handler (xmlNode *element,int global_blok_counter, int slide_num, int shape_num);

int pptx_meta_handler (int working_folder, char *ws_fp);

int doc_rels_handler (char *fp);





#endif /* office_parser_llmware_h */
