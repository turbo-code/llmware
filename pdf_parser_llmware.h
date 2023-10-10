
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

//
//  pdf_parser_llmware.h
//  pdf_parser_llmware
//
//  Created by Darren Oberst
//

#ifndef pdf_parser_llmware_h
#define pdf_parser_llmware_h

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>
#include <time.h>
#include <err.h>

#include <bson/bson.h>
#include <zip.h>
#include <mongoc.h>
#include <zlib.h>

#include <tiffio.h>

#include <png.h>


//  currently the png image conversion takes place as a post-processing step
//  there may be alternative to implement png conversion at the time that the image is read
//  main reason not to do it:  can be time intensive, especially for high volume of small images
//#include <png.h>


// Global Adobe PDF character names

// Adobe glyph list is standard, published Adobe list that consists of thousands of specialized names
// the list below is a good starting point that covers most, but not all, of the common cases
// this list is used in the lookup tables for Differences
//
// Note:  there will be encoding problems if a glyph not on the list below is used, since the parser...
//  ...will not be able to pick it up and identify it

int adobe_glyph_count = 101;

char* glyph_names[101] = {"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","zero","one","two","three","four","five","six","seven","eight","nine","period","space","quoteright","quoteleft","semicolon", "comma","colon","numbersign", "quotedblleft","quotedblright", "hyphen","ampersand","parenleft","parenright", "slash", "quotesingle","endash","percent", "dollar", "underscore","bracketleft","bracketright","asterisk","less","greater","section","bullet", "uni00A0","fl","fi","ff","question","at","plus","f_f","f_f_i","ffi","f_i","f_l"};

int glyph_lookup[101] = {65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,48,49,50,51,52,53,54,55,56,57,46,32,39,39,59,44,58,35,34,34,45,38,40,41,47,39,45,37,36,95,91,93,42,60,62,167,42,32,64258,64257,64256,63,64,43,64256,64259,64259,64257,64258};

// Global PDF parsing constants

int form_seq[4] = {70,111,114,109}; // "F" "o" "r" "m"
int encrypt_seq[8] = {47,69,110,99,114,121,112,116}; // "/" "E" "n" "c" "r" "y" "p" "t"
int catalog_seq[7] = {67,97,116,97,108,111,103};   // "C" "a" "t" "a" "l" "o" "g"
int objstm_seq[6] = {79,98,106,83,116,109}; // "O" "b" "j" "S" "t" "m"
int pages_seq[6] = {47, 80,97,103,101,115}; // "/" "P" "a" "g" "e" "s"
int page_seq[5] = {47, 80,97,103,101}; // "/" "P" "a" "g" "e"
int font_seq[4] = {70,111,110,116}; // "F" "o" "n" "t"
int resources_seq[9] = {82,101,115,111,117,114,99,101,115}; // "R" "e" "s" "o" "u" "r" "c" "e" "s"
int contents_seq[8] = {67,111,110,116,101,110,116,115}; // "C" "o" "n" "t" "e" "n" "t" "s"
int count_seq[5] = {67,111,117,110,116}; // "C" "o" "u" "n" "t"
int kids_seq[4] = {75,105,100,115} ; // "K" "i" "d" "s"
int n_seq[1] = {78}; // "N"
int flatedecode_seq[11] = {70,108,97,116,101,68,101,99,111,100,101} ; // "F""l""a""t""e" "D" "e""c""o""d" "e"
int tounicode_seq[9] = {84,111,85,110,105,99,111,100,101} ; // "T" "o" "U" "n" "i" "c" "o" "d" "e"
int encoding_seq[8] = {69,110,99,111,100,105,110,103} ; // "E" "n" "c" "o" "d" "i" "n" "g"
int differences_seq[11] = {68,105,102,102,101,114,101,110,99,101,115}; // "D" "i" "f" "f" "e" "r" "e" "n" "c" "e" "s"
int beginbfrange_seq[12] = {98,101,103,105,110,98,102,114,97,110,103,101}; // "b" "e" "g" "i" "n" "b" "f" "r" "a" "n" "g" "e"
int endbfrange_seq[10] = {101,110,100,98,102,114,97,110,103,101}; // "e" "n" "d" "b" "f" "r" "a" "n" "g" "e"
int xobject_seq[7] = {88,79,98,106,101,99,116}; // "X" "O" "b" "j" "e" "c" "t"
int image_seq[5] = {73,109,97,103,101}; // "I" "m" "a" "g" "e"
int im_seq[3] = {47,73,109}; // "/Im"
int dctdecode_seq[9] = {68,67,84,68,101,99,111,100,101}; // "D" "C" "T" "D" "e" "c" "o" "d" "e"
int height_seq[6] = {72,101,105,103,104,116}; // "H" "e" "i" "g" "h" "t"
int width_seq[5] = {87,105,100,116,104}; // "W" "i" "d" "t" "h"
int metadata_seq[8] = {77,101,116,97,100,97,116,97}; // "M" "e" "t" "a" "d" "a" "t" "a"
int beginbfchar_seq[11] = {98,101,103,105,110,98,102,99,104,97,114}; // "b" "e" "g" "i" "n" "b" "f" "c" "h" "a" "r"
int endbfchar_seq[9] =  {101,110,100,98,102,99,104,97,114}; // "e" "n" "d" "b" "f" "c" "h" "a" "r"
int dccreator_seq[12] = {60,100,99,58,99,114,101,97,116,111,114,62}; // "d" "c" ":" "c" "r" "e" "a" "t" "o" "r", ">" - all lower case
int xmp_modify_date_seq[16] = {60,120,109,112,58,77,111,100,105,102,121,68,97,116,101,62}; // "<xmp:ModifyDate>"
int xmp_create_date_seq[16] = {60,120,109,112,58,67,114,101,97,116,101,68,97,116,101,62}; // "<xmp:CreateDate>"
int xmp_creator_tool_seq[17] = {60,120,109,112,58,67,114,101,97,116,111,114,84,111,111,108,62}; // "xmp:CreatorTool"
int pdf_producer_seq[12] = {112,100,102,58,80,114,111,100,117,99,101,114}; // "pdf:Producer"

int ccittfaxdecode_seq[14] = {67,67,73,84,84,70,97,120,68,101,99,111,100,101}; // "CCITTFaxDecode"
int jbig2decode_seq[11] = {74,66,73,71,50,68,101,99,111,100,101}; // "JBIG2Decode"
int jpxdecode_seq[9] = {74,80,88,68,101,99,111,100,101}; // "JPXDecode"

int widths_seq[7] = {47,87,105,100,116,104,115}; // "/Widths"
int first_char_seq[9] = {70,105,114,115,116,67,104,97,114}; // "FirstChar"
int last_char_seq[8] = {76,97,115,116,67,104,97,114}; // "LastChar"

int font_descriptor_seq[14] = {70,111,110,116,68,101,115,99,114,105,112,116,111,114}; // "FontDescriptor"
int font_file2_seq[9] = {70,111,110,116,70,105,108,101,50}; // "FontFile2"
int font_file_seq[8] = {70,111,110,116,70,105,108,101}; // "FontFile"
int length1_seq[7] = {76,101,110,103,116,104,49}; // "Length1"

int mac_roman_encoding_seq[16] = {77,97,99,82,111,109,97,110,69,110,99,111,100,105,110,103}; // "MacRomanEncoding"
int pdf_doc_encoding_seq[14] = {80,68,70,68,111,99,69,110,99,111,100,105,110,103}; // "PDFDocEncoding"

int win_ansi_encoding_seq[15] = {87,105,110,65,110,115,105,69,110,99,111,100,105,110,103}; // "WinAnsiEncoding"

int times_roman_seq[11] = {84,105,109,101,115,45,82,111,109,97,110}; // "Times-Roman"
int times_new_roman_seq[13] = {84,105,109,101,115,78,101,119,82,111,109,97,110}; // "TimesNewRoman"

int arial_mt_seq[7] = {65,114,105,97,108,77,84}; // "ArialMT"

int symbol_seq[6] = {83,121,109,98,111,108}; // "Symbol"

int device_cmyk_seq[10] = {68,101,118,105,99,101,67,77,89,75}; // "DeviceCMYK"

int color_space_seq[10] = {67,111,108,111,114,83,112,97,99,101}; // "ColorSpace"

int type3_seq[5] = {84,121,112,101,51}; // "Type3" font
int type0_seq[5] = {84,121,112,101,48}; // "Type0" font
int char_procs_seq[9] = {67,104,97,114,80,114,111,99,115}; // "CharProcs"

// Font embeddings - this is simplified loading table for the widths of several major fonts
//  * will need to continue to expand over time 
//  * some PDF files do not provide width information for these fonts, as it is assumed that they will be loaded by the PDF reader program directly *
//  * inaccurate font widths is one of the major sources of unintentional space gaps *
//  * most fonts have public .afm (adobe font metric) files available that specify the widths
//  * note: widths are for the major ASCII character codes -starting at 32 - 'z' (122)

// starting at asci 32 -> 122

int times_roman_widths[91] = {250,333,408,500,500,833,778,333,333,333,500,564,250,333,250,278,500,500,500,500,500,500,500,500,500,500,278,278,564,564,564,444,921,722,667,667,722,611,556,722,722,333,389,722,611,889,722,722,556,722,667,556,611,722,722,944,722,722,611,333,278,333,469,500,333,444,500,444,500,444,333,500,500,278,278,500,278,778,500,500,500,500,333,389,278,500,500,722,500,500,444};

int times_bold_widths[91] = {250,333,555,500,500,1000,833,333,333,333,500,570,250,333,250,278,500,500,500,500,500,500,500,500,500,500,333,333,570,570,570,500,930,722,667,722,722,667,611,778,778,389,500,778,667,944,722,778,611,778,722,556,667,722,722,1000,722,722,667,333,278,333,581,500,333,500,556,444,556,444,333,500,556,278,333,556,278,833,556,500,556,556,444,389,333,556,500,722,500,500,444};

int test_widths[91] = {250,0,0,0,500,833,0,0,0,0,0,0,250,333,250,278,500,500,500,0,500,500,500,500,0,0,278,278,0,0,0,0,0,722,667,667,722,611,556,722,722,333,0,0,611,889,722,722,556,0,667,556,722,0,944,0,722,0,0,0,0,0,0,0,444,500,444,500,444,333,500,500,278,278,500,278,778,500,500,500,500,333,389,278,500,500,722,500,500};

int arial_mt_widths[91] = {277,277,354,556,556,889,666,190,333,333,389,583,277,333,277,277,556,556,556,556,556,556,556,556,556,556,277,277,583,583,583,556,1015,666,666,722,722,666,610,777,722,277,500,666,556,833,722,777,666,777,722,666,610,722,666,943,666,666,610,277,277,277,469,556,333,556,556,500,556,556,277,556,556,222,222,500,222,833,556,556,556,556,333,500,277,556,500,722,500,500,500};

int symbol_widths[91] = {250,333,713,500,549,833,778,439,333,333,500,549,250,549,250,278,500,500,500,500,500,500,500,500,500,500,278,278,549,549,549,444,549,722,667,722,612,611,763,603,722,333,631,722,686,889,722,722,768,741,556,592,611,690,439,768,645,795,611,333,863,333,658,500,500,631,549,549,494,439,521,411,603,329,603,549,549,576,521,549,549,521,549,603,439,576,713,686,493,686,494};


// MAX CAPS

// experiment - higher global_max_obj - 100K stable
int GLOBAL_MAX_OBJ = 200000;

int GLOBAL_MAX_PAGES = 5000; // stable at 2000 -> try higher limit

int GLOBAL_MAX_BLOKS = 5000;
int GLOBAL_MAX_FONTS = 10000;

int GLOBAL_BLOK_SIZE;

int GLOBAL_DEFAULT_WIDTH_SIZE = 500;

int GLOBAL_PAGE_CMAP = -1;
int GLOBAL_FONT_SIZE = 10;
int GLOBAL_ACTUAL_TEXT_FLAG = 0;

// min for H, W & total volume
int IMG_MIN_HEIGHT;
int IMG_MIN_WIDTH;
int IMG_MIN_HxW;

int global_png_convert_on;

// Key PDF Objects
 
unsigned char *buffer;
int global_buffer_cursor;

//unsigned char *hidden_objstm_buffer;
//int hidden_buffer_cursor;

unsigned char *objstm_flate_buffer;
unsigned char *flate_dst_tmp_buffer;

// FIRST MAJOR DATA STRUCT TYPE = object
// Main list of <obj><endobj> // Key elements of object

typedef struct {
    int obj_num;
    int gen_num;
    int start;
    int stop;
    int dict_start;
    int dict_stop;
    int stream_start;
    int hidden;
} object;

object obj[200000];  // main global reference for obj list
int global_obj_counter;
object global_tmp_obj;

object catalog;

typedef struct {
    int page_obj;
    int content_entries;
    int* content[500];
    int image_entries;
    int* images[500];
    char image_names[500][40];
    int* image_global_ref[500];
    float image_x_coord[500];
    float image_y_coord[500];
    int* image_cx_coord[500];
    int* image_cy_coord[500];
    int* image_found_on_page[500];
} page;

// page count > 2000 -> may need to expand and offer more options for iterative processing for longer docs
// e.g., 2005 is arbitrary and can be changed higher or lower ...

int global_total_pages_added;

char global_headlines[50000];

page Pages[5005];  // stable at 2005 -> try for higher page count ?

int global_page_count;

typedef struct {
    char font_name[50];
    int obj_ref1;
    int obj_ref2;
    int cmap_apply;
    int* cmap_in[10000];
    int* cmap_out[10000];
    int pages[2000];
    int page_apply_counter;
    int* widths[10000];
    int standard_font_identifier;
} encoding;

encoding Font_CMAP[10000];
int global_font_count;


int flate_count;

typedef struct {
    char name[1000];
    int num;
} nn;

typedef struct {
    int page_num;
    int obj_ref_num;
    char name[25];
} pc;


typedef struct {
    int start;
    int stop;
    int value_start;
    int value_stop;
    int brackets;
} slice;

nn nn_global_tmp[10000];

typedef struct {
    float scaler;
    float x;
    float y;
} ts;

float tm_x;
float tm_y;
float tml_x;

// new - table structure tracker

typedef struct {
    int col_locs[100];
    int col_count;
} table_col;

table_col pdf_table[1000];

typedef struct {
    int x;
    int y;
    char cell[1000];
} table_cells;


// end - table structure tracker


// Major PDF Functions

// master function - open_pdf
int pdf_builder (char *fp, char*account, char*corpus, char *time_stamp);
int open_pdf_old (char *fp, int t);

// step 1 - get all of the objects from pdf buffer
int build_obj_master_list (int filelen);

// step 2 - specialized handler for ObjStm
int objstm_handler (int start, int stop);
int objstm_handler_old (int stream_len);
int flate_handler_buffer_v2 (int stream_start, int stream_stop);

// step 3 - build page structure, font encodings + images
int catalog_to_page_handler (int start, int stop);
int catalog_to_page_handler_old_works (int start, int stop);

int page_resource_handler (int page_count);
int font_encoding_cmap_handler (int font_len);
int process_cmap (int stream_buffer_len, int font_number);
int process_cmap_buffer (int buffer_start, int buffer_stop, int font_number);
int process_cmap_buffer_alt (int buffer_start, int buffer_stop, int font_number);
int create_new_buffer (int buffer_start, int buffer_stop);

// step 4 - text + page processor
int text_processor_page (int page_buffer_size, int page_number);
int text_processor_page_old_works (unsigned char* page_buffer, int page_buffer_size, int page_number);

int image_handler (int height, int width,int img_type, int stream_start, int stream_stop, int global_ref_num);

char* hex_handler_new (char* hex_string, int hex_len, int my_cmap,int font_size, float scaler);

char* char_special_handler (int char_tmp, int escape_on);
char* char_special_handler_string (int char_tmp, int escape_on);

int metadata_handler (int dict_start, int dict_stop);

int cmap_get_char(int tmp_char,int my_cmap);
int differences_handler (char* differences, int font_number);

int fontfile_handler (char* fontfile, int font_number);


// TM operator to set text state
ts* get_text_state (char* tm_window, int tm_len);

// new function -> mirrors get_text_state for TD operator
ts* get_text_state_from_td (char* td_window, int td_len);

int evaluate_text_distance (float x, float y, float x_last, float y_last, int font_size_state, float tm_scaler);


int image_handler_flate (int height,int width, int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int dct_flag, int no_text_flag, int png_convert_on, int cmyk_flag);

int image_handler_dct (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int cmyk_flag);

int image_handler_ccitt (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int height, int width);
 
int image_handler_jpx (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag);

int image_handler_jbig2 (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int height, int width);

//int post_page_handler (int start_blok, int stop_blok);

int post_page_table_builder (int start_blok, int stop_blok, int table_markers);

int global_table_count;

// key pdf utility parsing functions

int get_obj (int obj_num);

int is_new_font (char* font_name, int ref_num);

int get_hex_one_digit (int h);
int get_hex (int*b[], int len_b);

char* get_string_from_byte_array (int*b[], int len_b);
char* get_string_from_buffer (int buffer_start, int buffer_stop);

int get_int_from_byte_array (int*b[]);
int get_int_from_buffer (int start, int stop);
float get_float_from_byte_array (int*b[]);

int extract_obj_from_dict_value (int*b[], int len_b);
int extract_obj_from_buffer (int buffer_start, int buffer_stop);

int extract_obj_from_buffer_with_safety_check (int buffer_start, int buffer_stop);

int dict_search (int*d[],int d_len, int key[],int k_len);
int dict_search_buffer (int buffer_start, int buffer_stop, int key[], int k_len);
int dict_search_hidden_buffer (int buffer_start, int buffer_stop, int key[], int k_len);

slice* dict_find_key_value (int*d[],int d_len, int key[], int k_len);
slice* dict_find_key_value_buffer (int buffer_start, int buffer_stop, int key[], int k_len);

int nearby_text (int blok_start, int blok_stop, int x, int y, int blok_number);

// end key pdf parsing functions

int update_library_inc_totals(char*account_name, char*library_name, int inc_docs, int inc_bloks, int inc_img, int inc_pages, int inc_tables);

int pull_new_doc_id (char*account_name, char*library_name);

/* CONTENT BELOW THIS LINE IS GENERAL PURPOSE AI BLOKS STRUCTURES AND FUNCTIONS */


// General Bloks data structures

int master_blok_tracker;
int master_doc_tracker;
int master_image_tracker;
int master_new_images_added;
int master_new_bloks_added;

int master_page_blok_start;
int master_page_blok_stop;


typedef struct {
    int x;
    int y;
    int cx;
    int cy;
} coords;


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
    char table_text[100000];  // experiment - Oct 29 - otherwise, not used - can reset to small num
    char file_type[20];
} blok;


typedef struct {
    int b;
    int u;
    int i;
    int sz;
    char color[20];
    int format_flag;
} format;


typedef struct {
    char author[1000];
    char file_name[300];
    // add starts here
    char file_short_name[300];
    // add ends here
    char create_date[300];
    char modify_date[300];
    char creator_tool[300];
    char pdf_producer[300];
    //char last_modified[200];
    int slide_count;
    int xml_files;
    char account_name[100];
    char corpus_name[100];
    char thread_num;
    int image_count_start;
    int iter;
} document;


typedef struct {
    char font_name[50];
    int obj_ref1;
    int obj_ref2;
    int obj_ref3;
    int encode_obj;
    char *encoding;
} font;

// blok count - 5000 stable - trying more
blok Bloks[5000];
int global_blok_counter;

int global_text_found;

int global_blok_save_off;

document my_doc;

int blok_counter_array[12];

char *global_workspace_fp;
char *global_image_fp;
char *global_master_fp;
char *global_mongo_db_path;

int global_image_save_on;
int global_ccitt_image_save_on;

int debug_mode;

int global_unhandled_img_counter;

// utility functions
char* get_file_name (char*longer_string);
int special_formatted_text (char*b,char*i,char*u,char*sz,char*color);
char *get_file_type (char*full_name);
int order_string (const void* e1, const void* e2);
char *clean_token (char *tok);
int keep_value (char *v);


// new entry-point into the parser for single pdf file

int add_one_pdf (char *account_name,
                 char *library_name,
                 char *input_fp,
                 char *input_filename,
                 char *input_images_fp,
                 char *write_to_filename,
                 int user_blok_size);

int add_one_pdf_main  (char *account_name,
                       char *library_name,
                       char *input_fp,
                       char *input_filename,
                       char *input_mongodb_path,
                       char *input_master_fp,
                       char *input_images_fp,
                       int input_debug_mode,
                       int input_image_save_mode,
                       int write_to_db_on,
                       char *write_to_filename,
                       int user_blok_size,
                       int user_img_height,
                       int user_img_width,
                       int user_img_hxw,
                       int ccitt_img_on,
                       char *file_source_path,
                       int png_convert_on);


int add_one_pdf_main_parallel  (char *account_name,
                                char *library_name,
                                char *input_fp,
                                char *input_filename,
                                char *input_mongodb_path,
                                char *input_master_fp,
                                char *input_images_fp,
                                int input_debug_mode,
                                int input_image_save_mode,
                                int write_to_db_on,
                                char *write_to_filename,
                                int user_blok_size,
                                int user_img_height,
                                int user_img_width,
                                int user_img_hxw,
                                int ccitt_img_on,
                                char *file_source_path,
                                int png_convert_on,
                                int unique_doc_num);


// traditional entry-point into the parser to iterate thru file folder

int add_pdf_main  (char *account_name,
                   char *library_name,
                   char *fp,
                   char *input_mongodb_path,
                   char *input_master_fp,
                   char *input_images_fp,
                   int input_debug_mode,
                   int input_image_save_mode);


int add_pdf_main_customize  (char *account_name,
                             char *library_name,
                             char *fp,
                             char *input_mongodb_path,
                             char *input_master_fp,
                             char *input_images_fp,
                             int input_debug_mode,
                             int input_image_save_mode,
                             int write_to_db_on,
                             char *write_to_filename,
                             int user_blok_size,
                             int user_img_height,
                             int user_img_width,
                             int user_img_hxw,
                             int ccitt_img_on,
                             char *file_source_path,
                             int png_convert_on);


// entry point added on Oct 26, 2022
// same design as add_pdf_main_customize with one notable change to handling of unique_doc_num
// designed for 'parallel' operation in which 'client app' responsible for allocating unique_doc_num
// will roll through all files in the *fp folder - and increment the unique_doc_num

int add_pdf_main_customize_parallel  (char *account_name,
                                      char *library_name,
                                      char *fp,
                                      char *input_mongodb_path,
                                      char *input_master_fp,
                                      char *input_images_fp,
                                      int input_debug_mode,
                                      int input_image_save_mode,
                                      int write_to_db_on,
                                      char *write_to_filename,
                                      int user_blok_size,
                                      int user_img_height,
                                      int user_img_width,
                                      int user_img_hxw,
                                      int ccitt_img_on,
                                      char *file_source_path,
                                      int png_convert_on,
                                      int unique_doc_num);


int add_pdf_main_llmware  (char *input_account_name,
                           char *input_library_name,
                           char *input_fp,
                           char *input_mongodb_path,
                           char *input_images_fp,
                           int input_debug_mode,
                           int input_image_save_mode,
                           int write_to_db_on,
                           char *write_to_filename,
                           int user_blok_size,
                           int unique_doc_num,
                           char *db_user_name,
                           char *db_pw);




int handle_zip (char *fp, int t);

int GLOBAL_WRITE_TO_DB;

char* global_write_to_filename;

// char* global_special_filepath;

int write_to_db (int start_blok, int stop_blok, char *account, char *library, char *file_source, int doc_number, int blok_number, char *time_stamp);

int write_to_file (int start_blok, int stop_blok, char *account, char *library, char *file_source, int doc_number, int blok_number, char *time_stamp, char *filename);

int update_counters (char *account, char *corpus, int blok_count, int doc_count, int image_count);
int save_images (int start_blok, int stop_blok, int image_count, char *account, char *corpus, int wf);

int builder(char *fp,int thread_number,int slide_count);
 
// new function - potential to expand significantly - add "account event" register to parser
// e.g., pick up a key event like an error or skipped document

int register_ae_add_pdf_event(char *ae_type, char *event, char *account,char *corpus,char *fp, char *time_stamp);



#endif /* pdf_parser_llmware_h */
