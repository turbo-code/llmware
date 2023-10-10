
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
//  graph_llmware.c
//  graph_llmware

#include "graph_llmware.h"



char *text_clean (char*tok) {
    
    //  high-volume utility function that parses tokens and removes low-information value elements
    //
    //  simple rules applied:
    //
    //      (1) removes any token that does not start with a letter
    //      (2) OK to have numbers in the word - as long as there is a letter at the start
    //      (3) removes one letter words, e.g., "a", "b", "d" -> essentially stop-words
    //      (4) excludes very long tokens, e.g., >15 letters -> may need to adjust for ...
    //              ... longer token support or compound names > 15 letters !!!
    
    //  key notes:
    //      --caps length of individual token @ 50000
    //      --if the input token is longer than len > 15, it is excluded
    //
    
    char new_token[50000];
    char *new_token_ptr = NULL;
    int i=0;
    int tmp;;
    char char_tmp[2];
    
    strcpy(new_token,"");
    strcpy(char_tmp,"");

    if (strlen(tok) < 15) {
        
    for (i=0; i < strlen(tok); i++) {
        
        if ((tok[i]>='a' && tok[i]<='z') || (tok[i]>='A' && tok[i] <= 'Z')) {
            
            // note: this is important -> changes every character to lower-case
            //      -- if less than 'a', then add 32, else, keep existing value
            
            if (tok[i] < 'a') {
                
                tmp = tok[i] + 32;

                sprintf(char_tmp,"%c",tmp);
                strncat(new_token, &char_tmp, 1);
            }
            else {
                strncat(new_token,&tok[i],1);
            }
        }
            
            if ((tok[i] >= 48 && tok[i] <=57)) {
                
                // if digit 0-9 and not first character then OK to save
                // useful for phrases with numbers, e.g., "v16"
                
                if (strlen(new_token) > 0) {
                    strncat(new_token,&tok[i],1);
                    }
                
                else {
                    // skip whole token if first digit is a number

                    strcpy(new_token,"");
                    break;
                }
            }
            
    }
    }
    
    if (strlen(new_token) < 2) {
        //
        // if token has len of 1, then skip
        // there are potentially a lot of stray one-letter characters in documents
        // ...but little information value
        
        strcpy(new_token,"");
    }
    
    new_token_ptr = &new_token;
    
    return new_token_ptr;
}


int tokenizer (char*core_text) {
    
    //
    // utility function - breaks up extract of text into tokens
    // three simple processing steps -
    //
    //  (1) starts with chunk of text extracted from database
    //  (2) tokenizes the text into words using strtok
    //  (3) cleans the text using text_clean -> removing unnecessary elements
    //  (4) saves tokens into global array = my_tokens for further downstream processing
    //
    //  NOTE:   my_tokens is GLOBAL ARRAY STRUCTURE
    
    char *tokens;
    char *tmp;
    int tok_counter = 0;
    
    tokens = strtok(core_text," ");
    
    while (tokens != NULL) {
        tmp = text_clean(tokens);

        if (strlen(tmp) > 0) {
        strcpy(my_tokens[tok_counter],tmp);
        tok_counter ++;
        }
        tokens = strtok(NULL, " ");
    }
    
    return tok_counter;
}


int stop_words_checker (char*tok, int stop_words_len) {
    
    //  simple but important component of downstream natural language processing
    //  compares token with stop_words list and identifies any matching elements
    //
    //  out = 0 -> not a stop_word
    //  out = 1 -> is a stop_word
    //
    
    int i=0;
    int out = 0;
    
    for (i=0; i < stop_words_len ; i++) {
        
        if (strlen(tok) == strlen(stop_words[i])){
            if (strncmp(stop_words[i],tok,strlen(tok))==0) {
                out = 1;
                break;
            }}
    }
    
    return out;
}


//
//                             TEXT_EXTRACT_MAIN_HANDLER
//
//          Major step in the initialization process for a library
//          Output:  creates bow (bag-of-words) files, used in downstream processing
//
//          Extracts text, removes stop words, removes numbers & punctuations ...
//          ... & puts all text in lower_case
//
//          Aggregates into bow{}.txt file
//
//          Incremental creation enabled - parameters - doc_start, bow_index, bow_len
//              --pulls doc_start from initialization history
//              --bow_index & bow_len - master tracker to cap bow at MAX LEN
//              --once MAX LEN reached, start new bow.txt file and increment bow_index
//
//          Caps/constraints:
//              --assumes stop_words_list < 1000 words -> can be easily adjusted
//              --excludes words with len > 15 -> can be easily adjusted

//          Designed to use little memory and have the potential to scale infinitely large
//              --extracts from database cursor (generator, only current entry in memory)
//              --writes extracted (tokenized, cleaned) sample directly to file
//
//
//          3 utility functions called by text_extract_main_handler:
//              --text_clean
//              --stop_words_checker
//              --tokenizer
//
//          2 global variables used:
//              --stop_words[]
//              --my_tokens[]
//


int text_extract_main_handler (char *input_account_name,
                               char *input_library_name,
                               int new_bow,
                               char *input_mongodb_path,
                               char *input_stop_words_fp,
                               char *input_bow_fp,
                               char *input_text_field,
                               int bow_index,
                               int bow_len)

{
    //
    //
    // input function declaration variables:
    //
    //      input_account_name = name of the account
    //      input_library_name = name of the library
    //
    //      int doc_start = number pulled from initialization_history
    //              -> key to enabling incremental initialization
    //              -> if doc_start > 0 -> write to existing bow.txt file
    //
    //      input_mongodb_path = avoid hard-coding, keeps options if mongo path changes
    //
    //      input_stop_words_fp = directory path to find the stop_words list
    //              -> enables future options to try different stop_words lists
    //              -> should be "plug and play" for another alpha based language, e.g., Italian
    //              -> provides option for user to apply their own stop_words list, or ...
    //                      adjust defaults
    //
    //
    //      input_bow_fp = directory path to find the bow file
    //      input_text_field = default set to "text1_core"
    //              -> allows other options in the future for other text extract fields
    //
    //      bow_index = currently active BOW file - capped at 10M tokens
    //      bow_len = len of the currently active BOW file
    //
    //      to scale to very large datasets, each bow.txt file can be added incrementally
    //      bow_index + bow_len are tracked in the initialization_history
    //      ... and updated after every initialization
    //
    
    
    //printf("update - c bow_builder:  starting text_extract-%d-%d-%d \n", new_bow,bow_index,bow_len);
    
    // const char *uri_string = "mongodb://localhost:27017";
    const char *uri_string = input_mongodb_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    //mongoc_collection_t *coll_docs;
    bson_error_t error;
    mongoc_read_prefs_t *read_prefs;
    
    int ERROR_CODE = -1;
    int i=0;
    int j=0;
    uint32_t *len = NULL;
    int tok_num = 0;
    int bow_counter = 0;
    int bow_index_local = 0;
    char tmp[100];
    char bow_name[100];
    
    int BOW_MAX_LEN = 10000000;
    
    bson_t *filter;
    bson_t *opts;
    mongoc_cursor_t *cursor;
    bson_iter_t iter;
    const bson_t *doc;
    char *core_text_out;
    int doc_id = 0;
    int last_doc_id = 0;
    int blok_id = 0;
    int first_doc_flag = 1;
    
    const char *bow_fp_local_copy = input_bow_fp;
    
    const char *text1_core = input_text_field;
    
    FILE* bow_out;
    FILE* stop_words_in;
    int stop_words_counter=0;
    char *sw_fp;
    char fp[500];
    
    // new approach - track file_byte_counter and register in the database
    int file_byte_counter = 0;
    FILE* file_ptr_counter;
    
    //char library_docs_collection[200];
    //strcpy(library_docs_collection,"");
    //strcat(library_docs_collection, input_library_name);
    //strcat(library_docs_collection, "_docs");
    
    //
    //              Step 1 - load master stop_words list from file
    //
    //
    //      note: the stop_words list = exclusion list
    //          --enables the filtering / exclusion of any words
    //          --easy to swap out the stop_words list or make changes
    //          --each library saves its own copy of the stop words list
    //          --gives option for other language support, e.g., Italian, French, Spanish, etc
    //
    //      general objective of the stop words list is to filter low-information-value words
    //          -- ... but ... could be applied in almost any case as general purpose exclusion list
    //
    //      assumed to be less than 2000 words
    //
    //      could be performance impact if list is too long
    //


    // change - fully parameterize path to finding the stop_words list
    sw_fp = input_stop_words_fp;
    
    // reads entire stop words list into memory - should always be less than 2000 words
    
    stop_words_in = fopen(sw_fp,"r");
    while (fscanf(stop_words_in,"%[^,],",tmp) != EOF) {
        //printf("update: iterating - %s \n", tmp);
        strcpy(stop_words[stop_words_counter],tmp);
        stop_words_counter ++;
    }
    fclose(stop_words_in);

    //printf("update: loaded stop words counter-%d \n",stop_words_counter);
    
    //  end - step 1 - loaded stop word list from file into memory
    
    
    //
    //                          step 2 - open up the bow.txt file
    //
    //      two options:
    //
    //          -- if doc_start == 0, then start to write new .txt file
    //
    //          -- if doc_start >0, then look up existing .txt file and append (open in "a" mode)
    //
    
    
    // start bow_counter @ bow_len provided as input
    bow_counter = bow_len;
    bow_index_local = bow_index;
    
    // prepare full name of bow
    // input parameters assumed to include the 'base name', e.g., 'bow'
    // but not include any index number - which is appended here
    
    strcpy(bow_name,"");
    sprintf(bow_name, "%d.txt",bow_index);
    
    strcpy(fp,"");
    strcat(fp, bow_fp_local_copy);
    strcat(fp, bow_name);
    
    //printf("update: building bow (c library) - bow_name - fp: %s - %s \n", bow_name, fp);
    
    // if doc_start == 0 -> then start with writing bow from scratch
    // if doc_start > 0  -> then open up existing file in 'a' mode (append)
    
    if (new_bow == 0) {
        bow_out = fopen(fp,"w");
        //printf("update: building bow (c library) - new_bow == 0 -> opening NEW file to write bow \n");
    }
    
    else {
        // get file_byte_counter
        file_ptr_counter = fopen(fp, "rb");
        fseek(file_ptr_counter, 0, SEEK_END);          // Jump to the end of the file
        file_byte_counter = ftell(file_ptr_counter);   // Get the current byte offset in the file
        //rewind(file_ptr_counter);                      // Jump back to the beginning of the file
        fclose(file_ptr_counter);
        
        //printf("update: file_byte_counter - %d \n", file_byte_counter);
        
        bow_out = fopen(fp,"a");
        
        //printf("update: build bow - (c library) - opening existing bow in 'a' mode and will append to bow \n");
        
    }
    
    // end - step 2 - initiated bow file
    
    
    //
    //                              step 3 - connect to mongodb
    //
    
    // mongo script
    
     mongoc_init ();
     uri = mongoc_uri_new_with_error (uri_string, &error);
     
    if (!uri) {
        printf("error: graph bow_builder - problem with URI connection to database. \n");
        
        return ERROR_CODE;
    }
    
     if (!uri) {
         fprintf (stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
         
         return ERROR_CODE;
     }

    client = mongoc_client_new_from_uri (uri);
    database = mongoc_client_get_database (client, input_account_name);
    coll = mongoc_client_get_collection (client, input_account_name, input_library_name);
    read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

    //  connected successfully to mongodb
    
    
    //                 Step 4 - Main Loop - Run DB query + iterate thru cursor
    //
    //
    
    // query_filter definition
    // connected to account/library collection - will access all of the bloks in the library collection
    // add sub-filters
    
    // Example of filter construction:
    //          --e.g., filter = BCON_NEW("content_type","text");
    //
    // query filter: {"$and": [{"content_type": "text"}, {"doc_ID": {"$gt": doc_start}} ] }
    //
    //      -- retrieves all items with content_type == "text"
    //
    //      -- AND ... only those with doc_ID > doc_start -> enables incremental bow.txt creation
    //
    
    // "old test" -> would filter based on doc_number with "doc_start"
    
    /*
    filter = BCON_NEW("$and",
                        "[",
                        "{", "content_type", "text", "}",
                        "{", "doc_ID",
                      "{","$gt", BCON_INT64(doc_start),"}","}",
                      "]"
                      );
    */
    
    // "new test" -> gets all bloks with "false" initialization_status
    
    filter = BCON_NEW("$and",
                        "[",
                        "{", "graph_status", "false", "}",
                        "{", "content_type", "text", "}",
                        "]"
                      );
    
    // filter = BCON_NEW("initialization_status", "false");
    
    // printf("update: created filter successfully \n");
    
    // need to confirm/test that this sort will not create any problems
    
    opts = BCON_NEW("sort", "{","doc_ID",BCON_INT32(1),"}",
                    "allowDiskUse",BCON_BOOL(true)
                    );
    
   //  printf("update: created opts successfully \n");
    
    // *** CURRENTLY TAKES ALL TEXT BLOKS - and READS TEXT1_CORE OUTPUT ***
    // note: does not pick up table data
    
    // generate mongodb cursor
    //cursor = mongoc_collection_find_with_opts (coll, filter, NULL, NULL);
    cursor = mongoc_collection_find_with_opts (coll, filter, opts, NULL);
    
    //printf("update: generated database cursor! \n");
    
    core_text_out = "";
    
    // iterate through cursor -> main processing loop
    
    while (mongoc_cursor_next (cursor, &doc)) {

        if (bson_iter_init_find (&iter, doc, text1_core)) {

            core_text_out = bson_iter_utf8(&iter,len);
            
        }
        
        if (bson_iter_init_find (&iter, doc, "doc_ID")) {
            doc_id = bson_iter_int64(&iter);
            
            if (first_doc_flag == 1) {
                // this is the first entry - add doc_id
                fprintf(bow_out,"<doc_id=%d>,", doc_id);
                first_doc_flag = 0;
                last_doc_id = doc_id;
            }
            
            // assumes that sorted by doc - but docs need not be sequential
            if (doc_id != last_doc_id) {
                fprintf(bow_out,"<doc_id=%d>,",doc_id);
                last_doc_id = doc_id;
                
                /*
                printf("update: iterating bow - starting new doc - %d \n", doc_id);
                last_doc_id = doc_id;

                doc_insert = BCON_NEW (
                                       "doc_id", BCON_INT64(doc_id),
                                       "local_doc_id", BCON_INT64(local_doc_counter),
                                       "bow_index_local", BCON_INT64(bow_index_local),
                                       "test_field", BCON_UTF8("registered_document_complete"),
                                       "start_byte", BCON_INT64(file_byte_counter)
                                   );
                
                local_doc_counter ++;
                
                if (!mongoc_collection_insert_one (coll_docs, doc_insert, NULL, NULL, &error)) {
                  fprintf (stderr, "%s\n", error.message);
                }
                 */
                
            }
        }
        
        if (bson_iter_init_find (&iter, doc, "block_ID")) {
            blok_id = bson_iter_int64(&iter);
            fprintf(bow_out, "[block_id=%d],",blok_id);
        }
        
        j++;
        
        if (strlen(core_text_out) > 0 ){
            
            tok_num = tokenizer(core_text_out);
            
            for (i=0; i < tok_num ; i++) {
                
                if (stop_words_checker(my_tokens[i],stop_words_counter)==0) {
                    fprintf(bow_out,"%s,",my_tokens[i]);
                    bow_counter ++;
                    
                    if (bow_counter > BOW_MAX_LEN) {
                        fclose(bow_out);
                        
                        // need to update new bow file
                        // increment index
                        // reset counter
                        
                        bow_counter = 0;
                        
                        bow_index_local ++;
                        
                        strcpy(bow_name,"");
                        sprintf(bow_name, "%d.txt",bow_index_local);
                        
                        strcpy(fp,"");
                        strcat(fp,bow_fp_local_copy);
                        strcat(fp,bow_name);
                        
                        //fp = strcat(input_bow_fp,bow_name);
                        
                        bow_out = fopen(fp,"w");
                        //fprintf(bow_out,"%s,","test");
                        //printf("update: test write to new bow_out- %s- OK \n", bow_name);
                    }
            }
        }
        
        }
    }

    // printf("update: cursor completed \n");
    
    // end - update initialization_status
    
    if (mongoc_cursor_error (cursor, &error)) {
        printf("error: graph builder - database retrieval problem. \n");
        
        fprintf (stderr, "error: database error occurred: %s\n", error.message);
    }
    
    fclose(bow_out);
    
    mongoc_cursor_destroy (cursor);
    bson_destroy (filter);
    
    bson_destroy (opts);
    bson_destroy (doc);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    // memory overflow leak check - confirm that stop words list still the same
    
    //for (k=0; k < stop_words_counter; k++) {
        //printf("stop words list at end - %d - %s \n", k, stop_words[k]);
    //}
    
    //printf("update: bow_builder c library- completed bow creation text_extract successfully \n");
    
    return bow_counter;

}


int text_extract_main_handler_old_copy_works (char *input_account_name,
                                              char *input_library_name,
                                              int doc_start,
                                              char *input_mongodb_path,
                                              char *input_stop_words_fp,
                                              char *input_bow_fp,
                                              char *input_text_field,
                                              int bow_index,
                                              int bow_len)

{
    //
    //
    // input function declaration variables:
    //
    //      input_account_name = name of the account
    //      input_library_name = name of the library
    //
    //      int doc_start = number pulled from initialization_history
    //              -> key to enabling incremental initialization
    //              -> if doc_start > 0 -> write to existing bow.txt file
    //
    //      input_mongodb_path = avoid hard-coding, keeps options if mongo path changes
    //
    //      input_stop_words_fp = directory path to find the stop_words list
    //              -> enables future options to try different stop_words lists
    //              -> should be "plug and play" for another alpha based language, e.g., Italian
    //              -> provides option for user to apply their own stop_words list, or ...
    //                      adjust defaults
    //
    //
    //      input_bow_fp = directory path to find the bow file
    //      input_text_field = default set to "text1_core"
    //              -> allows other options in the future for other text extract fields
    //
    //      bow_index = currently active BOW file - capped at 10M tokens
    //      bow_len = len of the currently active BOW file
    //
    //      to scale to very large datasets, each bow.txt file can be added incrementally
    //      bow_index + bow_len are tracked in the initialization_history
    //      ... and updated after every initialization
    //
    
    
    // printf("update - c:  starting text_extract-%d-%d-%d \n", doc_start,bow_index,bow_len);
    
    //printf("update - c:  key inputs - %s-%s-%s-%s \n", input_text_field, input_mongodb_path,input_stop_words_fp,input_bow_fp);

    // const char *uri_string = "mongodb://localhost:27017";
    const char *uri_string = input_mongodb_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_error_t error;
    mongoc_read_prefs_t *read_prefs;
    
    int ERROR_CODE = -1;
    int i=0;
    int j=0;
    uint32_t *len = NULL;
    int tok_num = 0;
    int bow_counter = 0;
    int bow_index_local = 0;
    char tmp[100];
    char bow_name[100];
    
    int BOW_MAX_LEN = 10000000;
    
    bson_t *filter;
    bson_t *opts;
    mongoc_cursor_t *cursor;
    bson_iter_t iter;
    const bson_t *doc;
    char *core_text_out;
    int doc_id = 0;
    int last_doc_id = 0;
    int blok_id = 0;
    
    const char *bow_fp_local_copy = input_bow_fp;
    
    // const char *text1_core = "text1_core";
    
    const char *text1_core = input_text_field;
    
    FILE* bow_out;
    FILE* stop_words_in;
    int stop_words_counter=0;
    char *sw_fp;
    char fp[500];

    char library_docs[200];
    
    strcpy(library_docs,"");
    strcat(library_docs, input_library_name);
    strcat(library_docs, "_docs");
    
    //
    //              Step 1 - load master stop_words list from file
    //
    //
    //      note: the stop_words list = exclusion list
    //          --enables the filtering / exclusion of any words
    //          --easy to swap out the stop_words list or make changes
    //          --each library saves its own copy of the stop words list
    //          --gives option for other language support, e.g., Italian, French, Spanish, etc
    //
    //      general objective of the stop words list is to filter low-information-value words
    //          -- ... but ... could be applied in almost any case as general purpose exclusion list
    //
    //      assumed to be less than 1000 words
    //
    //      could be performance impact if list is too long
    //


    
    // change - fully parameterize path to finding the stop_words list
    sw_fp = input_stop_words_fp;
    
    // reads entire stop words list into memory - should always be less than 1000 words
    
    stop_words_in = fopen(sw_fp,"r");
    while (fscanf(stop_words_in,"%[^,],",tmp) != EOF) {
        strcpy(stop_words[stop_words_counter],tmp);
        stop_words_counter ++;
    }
    fclose(stop_words_in);

    // printf("update: loaded stop words counter-%d \n",stop_words_counter);
    
    //  end - step 1 - loaded stop word list from file into memory
    
    
    //
    //                          step 2 - open up the bow.txt file
    //
    //      two options:
    //
    //          -- if doc_start == 0, then start to write new .txt file
    //
    //          -- if doc_start >0, then look up existing .txt file and append (open in "a" mode)
    //
    
    
    // start bow_counter @ bow_len provided as input
    bow_counter = bow_len;
    bow_index_local = bow_index;
    
    // prepare full name of bow
    // input parameters assumed to include the 'base name', e.g., 'bow'
    // but not include any index number - which is appended here
    
    strcpy(bow_name,"");
    sprintf(bow_name, "%d.txt",bow_index);
    
    strcpy(fp,"");
    strcat(fp, bow_fp_local_copy);
    strcat(fp, bow_name);
    
    // printf("update: building bow (c library) - bow_name - fp: %s - %s \n", bow_name, fp);
    
    // if doc_start == 0 -> then start with writing bow from scratch
    // if doc_start > 0  -> then open up existing file in 'a' mode (append)
    
    if (doc_start == 0) {
        
        bow_out = fopen(fp,"w");
        
        // printf("update: building bow (c library) - doc_start == 0 -> opening NEW file to write bow \n");
    
        //fprintf(bow_out,"%s,","test");

        //printf("update: test write to bow_out OK \n");
    }
    
    else {
        
        //printf("update: doc_start > 0 - %d \n", doc_start);
        
        bow_out = fopen(fp,"a");
        
        //printf("update: build bow - (c library) - opening existing bow in 'a' mode and will append to bow \n");
    }
    
    // end - step 2 - initiated bow file
    
    
    //
    //                              step 3 - connect to mongodb
    //
    
    // mongo script
    
     mongoc_init ();
     uri = mongoc_uri_new_with_error (uri_string, &error);
     
    if (!uri) {
        printf("error: graph builder - problem with URI connecting to database. \n");
        
        return ERROR_CODE;
    }
    
     if (!uri) {
         fprintf (stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
         
         return ERROR_CODE;
     }

    client = mongoc_client_new_from_uri (uri);
     
    database = mongoc_client_get_database (client, input_account_name);
    coll = mongoc_client_get_collection (client, input_account_name, input_library_name);
     
    //printf("update: initiating database read-%s-%s \n", input_account_name,input_library_name);

    read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

    //  end - connected successfully to mongodb
    
    
    //                 Step 4 - Main Loop - Run DB query + iterate thru cursor
    //
    //
    
    // query_filter definition
    // connected to account/library collection - will access all of the bloks in the library collection
    // add sub-filters
    
    // Example of filter construction:
    //          --e.g., filter = BCON_NEW("content_type","text");
    //
    // query filter: {"$and": [{"content_type": "text"}, {"doc_ID": {"$gt": doc_start}} ] }
    //
    //      -- retrieves all items with content_type == "text"
    //
    //      -- AND ... only those with doc_ID > doc_start -> enables incremental bow.txt creation
    //
    
    
    if (doc_start == 0) {
        doc_start = -1;
    }
    else {
        doc_start += -1;
    }
    
    /*
    filter = BCON_NEW("$and",
                        "[",
                        "{", "content_type", "text", "}",
                        "{", "doc_ID",
                      "{","$gt", BCON_INT64(doc_start),"}","}",
                      "]"
                      );
    */
    
    filter = BCON_NEW("{", "initialization_status", "false", "}");
    
    opts = BCON_NEW("sort","{","doc_ID",BCON_INT32(1),"}");
    
    // *** CURRENTLY TAKES ALL TEXT BLOKS - and READS TEXT1_CORE OUTPUT ***
    // note: does not pick up table data
    
    // generate mongodb cursor
    //cursor = mongoc_collection_find_with_opts (coll, filter, NULL, NULL);
    cursor = mongoc_collection_find_with_opts (coll, filter, opts, NULL);
    
    //printf("update: generated database cursor! \n");
    
    fprintf(bow_out,"<doc_id=%d>,",doc_start+1);
    
    // for incremental
    last_doc_id = doc_start +1;
    
    core_text_out = "";
    
    // iterate through cursor -> main processing loop
    
    while (mongoc_cursor_next (cursor, &doc)) {

        if (bson_iter_init_find (&iter, doc, text1_core)) {

            core_text_out = bson_iter_utf8(&iter,len);
            
        }
        
        if (bson_iter_init_find (&iter, doc, "doc_ID")) {
            doc_id = bson_iter_int64(&iter);
            if (doc_id > last_doc_id) {
                fprintf(bow_out,"<doc_id=%d>,",doc_id);
                // printf("update: iterating bow - starting new doc - %d \n", doc_id);
                last_doc_id = doc_id;
            }
        }
        
        if (bson_iter_init_find (&iter, doc, "blok_ID")) {
            blok_id = bson_iter_int64(&iter);
            fprintf(bow_out, "[blok_id=%d],",blok_id);
        }
        
        j++;
        
        if (strlen(core_text_out) > 0 ){
            
            tok_num = tokenizer(core_text_out);
            
            for (i=0; i < tok_num ; i++) {
                
                if (stop_words_checker(my_tokens[i],stop_words_counter)==0) {
                    fprintf(bow_out,"%s,",my_tokens[i]);
                    bow_counter ++;
                    
                    if (bow_counter > BOW_MAX_LEN) {
                        fclose(bow_out);
                        
                        // need to update new bow file
                        // increment index
                        // reset counter
                        
                        bow_counter = 0;
                        
                        bow_index_local ++;
                        
                        strcpy(bow_name,"");
                        sprintf(bow_name, "%d.txt",bow_index_local);
                        
                        strcpy(fp,"");
                        strcat(fp,bow_fp_local_copy);
                        strcat(fp,bow_name);
                        
                        //fp = strcat(input_bow_fp,bow_name);
                        
                        bow_out = fopen(fp,"w");
                        //fprintf(bow_out,"%s,","test");
                        //printf("update: test write to new bow_out- %s- OK \n", bow_name);
                    }
            }
        }
        
        }
    }

    //printf("update: cursor completed \n");
    
    if (mongoc_cursor_error (cursor, &error)) {
        printf("error: graph builder - error with database collection cursor! \n");
        
        fprintf (stderr, "error: graph builder - database error occurred: %s\n", error.message);
    }
    
    fclose(bow_out);
    
    mongoc_cursor_destroy (cursor);
    bson_destroy (filter);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    // memory overflow leak check - confirm that stop words list still the same
    
    //for (k=0; k < stop_words_counter; k++) {
        //printf("stop words list at end - %d - %s \n", k, stop_words[k]);
    //}
    
    // printf("update: bow_builder c library- completed bow creation text_extract successfully \n");
    
    return bow_counter;

}






//          end - text_extract / bow_creation functions
//

// experiment - creating one_doc version to create BOW for single file

int text_extract_one_doc_handler (char *input_account_name,
                                  char *input_library_name,
                                  char *input_mongodb_path,
                                  char *input_stop_words_fp,
                                  char *input_bow_fp,
                                  char *input_text_field)

{
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_error_t error;
    mongoc_read_prefs_t *read_prefs;
    const char *uri_string = input_mongodb_path;
        
    int ERROR_CODE = -1;
    int i=0;
    int j=0;
    uint32_t *len = NULL;
    int tok_num = 0;
    int bow_counter = 0;
    int bow_index_local = 0;
    char tmp[100];
    char bow_name[100];
    
    int BOW_MAX_LEN = 10000000;
    
    bson_t *filter;
    mongoc_cursor_t *cursor;
    bson_iter_t iter;
    const bson_t *doc;
    char *core_text_out;
    
    const char *bow_fp_local_copy = input_bow_fp;
    
    // const char *text1_core = "text1_core";
    
    const char *text1_core = input_text_field;
    
    FILE* bow_out;
    FILE* stop_words_in;
    int stop_words_counter=0;
    char *sw_fp;
    char fp[500];

    //
    //              Step 1 - load master stop_words list from file
    //
    //
    //      note: the stop_words list = exclusion list
    //          --enables the filtering / exclusion of any words
    //          --easy to swap out the stop_words list or make changes
    //          --each library saves its own copy of the stop words list
    //          --gives option for other language support, e.g., Italian, French, Spanish, etc
    //
    //      general objective of the stop words list is to filter low-information-value words
    //          -- ... but ... could be applied in almost any case as general purpose exclusion list
    //
    //      assumed to be less than 1000 words
    //
    //      could be performance impact if list is too long
    //


    
    // change - fully parameterize path to finding the stop_words list
    sw_fp = input_stop_words_fp;
    
    // reads entire stop words list into memory - should always be less than 1000 words
    
    stop_words_in = fopen(sw_fp,"r");
    while (fscanf(stop_words_in,"%[^,],",tmp) != EOF) {
        strcpy(stop_words[stop_words_counter],tmp);
        stop_words_counter ++;
    }
    fclose(stop_words_in);

    // printf("update: loaded stop words counter-%d \n",stop_words_counter);
    
    //  end - step 1 - loaded stop word list from file into memory
    
    
    //
    //                          step 2 - open up the bow.txt file
    //
    //      two options:
    //
    //          -- if doc_start == 0, then start to write new .txt file
    //
    //          -- if doc_start >0, then look up existing .txt file and append (open in "a" mode)
    //
    
    
    // prepare full name of bow
    // input parameters assumed to include the 'base name', e.g., 'bow'
    // but not include any index number - which is appended here
    
    strcpy(bow_name,"");
    strcat(bow_name, "bow.txt");
    
    strcpy(fp,"");
    strcat(fp, bow_fp_local_copy);
    strcat(fp, bow_name);
    
    // printf("update: bow_name - fp: %s - %s \n", bow_name, fp);
    
    // if doc_start == 0 -> then start with writing bow from scratch
    // if doc_start > 0  -> then open up existing file in 'a' mode (append)
    
    bow_out = fopen(fp,"w");
    
    // printf("update: doc_start == 0 -> opening NEW file to write bow \n");
    
    fprintf(bow_out,"%s,","test");

    // printf("update: test write to bow_out OK \n");
    
    // end - step 2 - initiated bow file
    
    
    //
    //                              step 3 - connect to mongodb
    //
    
    // mongo script
    
     mongoc_init ();
     uri = mongoc_uri_new_with_error (uri_string, &error);
     
    if (!uri) {
        printf("error: graph builder - problem with URI database connection. \n");
        
        return ERROR_CODE;
    }
    
     if (!uri) {
         fprintf (stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
         
         return ERROR_CODE;
     }

    client = mongoc_client_new_from_uri (uri);
     
    database = mongoc_client_get_database (client, input_account_name);
    coll = mongoc_client_get_collection (client, input_account_name, input_library_name);
     
    // printf("update: initiating database read-%s-%s \n", input_account_name,input_library_name);

    read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

    //  end - connected successfully to mongodb
    
    
    //                 Step 4 - Main Loop - Run DB query + iterate thru cursor
    //
    //
    
    // query_filter definition
    // connected to account/library collection - will access all of the bloks in the library collection
    // add sub-filters
    
    // Example of filter construction:
    //          --e.g., filter = BCON_NEW("content_type","text");
    //
    // query filter: {"$and": [{"content_type": "text"}, {"doc_ID": {"$gt": doc_start}} ] }
    //
    //      -- retrieves all items with content_type == "text"
    //
    //      -- AND ... only those with doc_ID > doc_start -> enables incremental bow.txt creation
    //
    
    
    // change filter -> look for document name
    
    filter = BCON_NEW("$and",
                        "[",
                        "{", "content_type", "text", "}",
                        "{", "doc_ID",
                      "{","$gt", BCON_INT64(0),"}","}",
                      "]"
                      );
    
    
    // *** CURRENTLY TAKES ALL TEXT BLOKS - and READS TEXT1_CORE OUTPUT ***
    // note: does not pick up table data
    
    // generate mongodb cursor
    cursor = mongoc_collection_find_with_opts (coll, filter, NULL, NULL);

    //printf("update: generated database cursor! \n");
    
    core_text_out = "";
    
    // iterate through cursor -> main processing loop
    
    while (mongoc_cursor_next (cursor, &doc)) {

        if (bson_iter_init_find (&iter, doc, text1_core)) {

            core_text_out = bson_iter_utf8(&iter,len);

        }
        
        j++;
        
        if (strlen(core_text_out) > 0 ){
            
            tok_num = tokenizer(core_text_out);
            
            for (i=0; i < tok_num ; i++) {
                
                if (stop_words_checker(my_tokens[i],stop_words_counter)==0) {
                    fprintf(bow_out,"%s,",my_tokens[i]);
                    bow_counter ++;
                    
                    if (bow_counter > BOW_MAX_LEN) {
                        fclose(bow_out);
                        
                        // need to update new bow file
                        // increment index
                        // reset counter
                        
                        bow_counter = 0;
                        
                        bow_index_local ++;
                        
                        strcpy(bow_name,"");
                        sprintf(bow_name, "%d.txt",bow_index_local);
                        
                        strcpy(fp,"");
                        strcat(fp,bow_fp_local_copy);
                        strcat(fp,bow_name);
                        
                        //fp = strcat(input_bow_fp,bow_name);
                        
                        bow_out = fopen(fp,"w");
                        fprintf(bow_out,"%s,","test");
                        
                        // printf("update: test write to new bow_out- %s- OK \n", bow_name);
                    }
            }
        }
        
        }
    }

    // printf("update: cursor completed \n");
    
    if (mongoc_cursor_error (cursor, &error)) {
        printf("error: graph builder - database cursor problem. \n");
        
        fprintf (stderr, "error: database cursor error occurred: %s\n", error.message);
    }
    
    fclose(bow_out);
    
    mongoc_cursor_destroy (cursor);
    bson_destroy (filter);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    // memory overflow leak check - confirm that stop words list still the same
    
    //for (k=0; k < stop_words_counter; k++) {
        //printf("stop words list at end - %d - %s \n", k, stop_words[k]);
    //}
    
    //printf("update: completed text_extract successfully \n");
    
    return bow_counter;
    
    return 0;
    
}





//
//                          GRAPH BUILDER FUNCTIONS
//
//      Last major step in initializing the library
//      Needs two major steps completed first:
//          --creation of the updated bow{}.txt
//          --creation of the updated most_common_words list
//          --most_common_words list also serves as a unique vocabulary word list
//
//      Two different implementations
//          (1) graph_builder is the most recent - designed to scale
//                  -- no real limitations on theoretical max BOW size ... BUT
//                  -- right now, only 'one loop' through 'one BOW' file
//                  -- will need to enable multiple BOW files to be processed
//
//          (2) context_builder is the original version - caps on size
//                  -- works very well on smaller datasets
//                  -- does a lot of in-memory processing and sorting
//                  -- capped at 10M tokens in bow file


int graph_builder (char *input_account_name, char *input_library_name, char *input_bow_fp, char *input_mcw_fp, int bow_index, int bow_len, int mcw_target_len, int mcw_context_len, int vocab_len, char *graph_fp, int graph_index, int graph_max_size, int min_len) {

    char bow_fp[300];
    char bow_fn[50];
    char mcw_fp[300];

    //printf("update: initiating graph builder - NEW - in c function -%d - %d \n", mcw_target_len, mcw_context_len);
    
    char graph_fp_local[300];
    char bow_tmp[100];
    int i=0;
    int m=0;
    int n=0;
    int p=0;
    int q=0;
    int x=0;
    
    int bow_iter_loop = 0;
    
    int graph_count=0;

    int cw[10];
    
    char most_common_words[vocab_len][25];
    
    int absolute_min_count = 4;
    
    bg Bloks_Graph[50000];
    int new_entry_counter = 0;
    
    //printf("update: in graph builder - %d - %d \n", mcw_target_len,mcw_context_len);
    
    int bow_token_index = -1;
    
    //printf("update: initiating graph_builder-%s-%s \n",input_account_name,input_library_name);
    //printf("update: input_mcw_fp-%s \n", input_mcw_fp);
    //printf("update: parameters - %d - %d \n", mcw_target_len,mcw_context_len);
    
    // get most common words from file
    strcpy(mcw_fp, "");
    strcat(mcw_fp, input_mcw_fp);
    
    FILE *mc = fopen(mcw_fp, "r");
    
    //printf("update: opened mcw file- %s \n", mcw_fp);
    
    // load vocab list into memory in most_common_words array
    // may need to cap at max vocab len > 100K
    
    for (i=0; i < vocab_len ; i++) {
        fscanf(mc,"%[^,],",most_common_words[i]);
    }
    
    // copy graph_fp into local var
    strcpy(graph_fp_local,"");
    strcat(graph_fp_local, graph_fp);
    
    // construct starting graph path
    // not used: ... but potential for multiple graph output files
    
    //sprintf(graph_name,"%d.txt",graph_index);
    //strcat(graph_fp_local,graph_name);
    
    FILE *fout = fopen(graph_fp_local,"w");
    
    // pull bow file and load into memory
    
    //printf("update: graph-c- bow_index count- %d \n", bow_index);
    
    for (x=0; x < bow_index; x++) {
    
        strcpy(bow_fp, "");
        strcat(bow_fp, input_bow_fp);
        sprintf(bow_fn,"%d.txt",x);
        strcat(bow_fp,bow_fn);
    
        FILE *bow_test = fopen(bow_fp,"r");
    
        //printf("update: opened bow_fp file - %s \n", bow_fp);
    
        // main loop - iterate thru bow file - token-by-token
    
        cw[0] = -1;
        cw[1] = -1;
        cw[2] = -1;
        cw[3] = -1;
        cw[4] = -1;
        cw[5] = -1;
        cw[6] = -1;
    
        // using graph as global variable -> automatically set to 0 as default
        // in future, may choose to make local variable, and would require memset / free
    
        //memset(graph, 0, mcw_target_len * mcw_context_len);
        //memset(graph, 0, 6000 * 12000);
    
        for (i=3; i < 7; i++) {
        
            fscanf(bow_test,"%[^,],",bow_tmp);
            for (p=0; p < mcw_context_len; p++) {
                if (strcmp(most_common_words[p],bow_tmp)==0) {
                    cw[i] = p;
                    //printf("update: matched - %d - %d \n", i, p);
                    break;
                }
            }
        }
    

        // safety check: in theory this loop could be infinitely large, BUT ...
        // ... the BOW file has a max of 10M tokens
        
        if (x == bow_index-1) {
            bow_iter_loop = bow_len;
        }
        else {
            bow_iter_loop = 10000000;
        }
        
        if (bow_iter_loop > 10000000) {
            bow_iter_loop = 10000000;
        }
    
        //printf("update: initiating main loop - bow_len-%d \n", bow_iter_loop);
    
        for (i=7; i < bow_iter_loop +3 ; i++) {
        
            if (cw[3] > -1 && cw[3] < mcw_target_len) {
                
                if (cw[0] > -1 && cw[0] != cw[3] && cw[0] < mcw_context_len) {
                    graph[cw[3]][cw[0]] += 1;
                }
                if (cw[1] > -1 && cw[1] != cw[3] && cw[1] < mcw_context_len) {
                    graph[cw[3]][cw[1]] += 1;
                }
                if (cw[2] > -1 && cw[2] != cw[3] && cw[2] < mcw_context_len) {
                    graph[cw[3]][cw[2]] += 1;
                }
                if (cw[4] > -1 && cw[4] != cw[3] && cw[4] < mcw_context_len) {
                    graph[cw[3]][cw[4]] += 1;
                }
                if (cw[5] > -1 && cw[5] != cw[3] && cw[5] < mcw_context_len) {
                    graph[cw[3]][cw[5]] += 1;
                }
                if (cw[6] > -1 && cw[6] != cw[3] && cw[6] < mcw_context_len) {
                    graph[cw[3]][cw[6]] += 1;
                }
                
            }
        
            // turn the crank - iterate through BOW file
        
            bow_token_index = -1;
        
            if (i < bow_iter_loop) {
                fscanf(bow_test,"%[^,],",bow_tmp);
            
                for (p=0; p < mcw_context_len; p++) {
                    if (strcmp(most_common_words[p],bow_tmp)==0) {
                        bow_token_index = p;
                        break;
                    }
                }
            }

        
            // update the context_window
            // note:  it would be better to shift the pointers, rather than new assignment
        
            cw[0] = cw[1];
            cw[1] = cw[2];
            cw[2] = cw[3];
            cw[3] = cw[4];
            cw[4] = cw[5];
            cw[5] = cw[6];
            cw[6] = bow_token_index;
                
            }
            fclose(mc);
    }
    
    //printf("update: completed bow iteration - writing to graph\n");
    

    for (m=0; m < mcw_target_len-1; m++) {
        
        new_entry_counter = 0;
        fprintf(fout,"%s,<START>,",most_common_words[m]);
                
        for (n=0; n < mcw_context_len-1; n++) {
            
            if (graph[m][n] > absolute_min_count) {
                
                // caps each graph 'target' at 200 max 'context' entries
                // need to experiment on the best length / pruning strategy
                // can adapt
                
                if (new_entry_counter < 200) {
                    Bloks_Graph[new_entry_counter].count = graph[m][n];
                    strcpy(Bloks_Graph[new_entry_counter].context_word, most_common_words[n]);
                    new_entry_counter ++;
                    
                }
                
                else {
                    
                    // if more than 200 entries added, then stop here
                    
                    break;
                }
                
            }
        }
        
        if (new_entry_counter > 0) {
            qsort(Bloks_Graph,new_entry_counter, sizeof(bg),struct_cmp_by_count);
        
            for (q=0; q < new_entry_counter ; q++) {
                fprintf(fout,"%s,",Bloks_Graph[q].context_word);
                fprintf(fout,"%d,",Bloks_Graph[q].count);
                }
        }
        
        fprintf(fout,"%s\n", "<END>");
        
        
    }
    
    fclose(fout);
    
    return graph_count;
}




int graph_builder_old_works (char *input_account_name, char *input_library_name, char *input_bow_fp, char *input_mcw_fp, int bow_index, int bow_len, int mcw_target_len, int mcw_context_len, int vocab_len, char *graph_fp, int graph_index, int graph_max_size, int min_len) {

    char bow_fp[300];
    char mcw_fp[300];

    //printf("update: initiating graph builder in c function -%d - %d \n", mcw_target_len, mcw_context_len);
    
    char graph_fp_local[300];
    char bow_tmp[100];
    int i=0;

    int m=0;
    int n=0;
    int p=0;
    int q=0;
    
    int graph_count=0;

    // char graph_name[100];

    int cw[10];
    
    char most_common_words[vocab_len][25];
    
    int absolute_min_count = 4;
    
    bg Bloks_Graph[50000];
    int new_entry_counter = 0;
    
    //int graph[mcw_target_len][mcw_context_len];
    
    // printf("update: in graph builder - %d - %d \n", mcw_target_len,mcw_context_len);
    
    int bow_token_index = -1;
    
    // printf("update: initiating graph_builder-%s-%s \n",input_account_name,input_library_name);
    // printf("update: input_mcw_fp-%s \n", input_mcw_fp);
    // printf("update: parameters - %d - %d \n", mcw_target_len,mcw_context_len);
    
    // get most common words from file
    strcpy(mcw_fp, "");
    strcat(mcw_fp, input_mcw_fp);
    
    FILE *mc = fopen(mcw_fp, "r");
    
    // printf("update: opened mcw file- %s \n", mcw_fp);
    
    // load vocab list into memory in most_common_words array
    // may need to cap at max vocab len > 100K
    
    for (i=0; i < vocab_len ; i++) {
        fscanf(mc,"%[^,],",most_common_words[i]);
    }
    
    // pull bow file and load into memory
    
    strcpy(bow_fp, "");
    strcat(bow_fp, input_bow_fp);
    FILE *bow_test = fopen(bow_fp,"r");
    
    // printf("update: opened bow_fp file - %s \n", bow_fp);
    
    // copy graph_fp into local var
    strcpy(graph_fp_local,"");
    strcat(graph_fp_local, graph_fp);
    
    // construct starting graph path
    // not used: ... but potential for multiple graph output files
    
    //sprintf(graph_name,"%d.txt",graph_index);
    //strcat(graph_fp_local,graph_name);
    
    FILE *fout = fopen(graph_fp_local,"w");
    
    // main loop - iterate thru bow file - token-by-token
    
    cw[0] = -1;
    cw[1] = -1;
    cw[2] = -1;
    cw[3] = -1;
    cw[4] = -1;
    cw[5] = -1;
    cw[6] = -1;
    
    // using graph as global variable -> automatically set to 0 as default
    // in future, may choose to make local variable, and would require memset / free
    
    //memset(graph, 0, mcw_target_len * mcw_context_len);
    //memset(graph, 0, 6000 * 12000);
    
    for (i=3; i < 7; i++) {
        
        fscanf(bow_test,"%[^,],",bow_tmp);
        for (p=0; p < mcw_context_len; p++) {
            if (strcmp(most_common_words[p],bow_tmp)==0) {
                cw[i] = p;
                
                // printf("update: matched - %d - %d \n", i, p);
                
                break;
            }
        }
    }
    

    // safety check: in theory this loop could be infinitely large, BUT ...
    // ... the BOW file has a max of 10M tokens
    
    if (bow_len > 10000000) {
        bow_len = 10000000;
    }
    
    // printf("update: initiating main loop - bow_len-%d \n", bow_len);
    
    for (i=7; i < bow_len +3 ; i++) {
        
        //printf("iterating - %d -%d - ", i,cw[3]);
        
        if (cw[3] > -1 && cw[3] < mcw_target_len) {
                
                if (cw[0] > -1 && cw[0] != cw[3] && cw[0] < mcw_context_len) {
                    graph[cw[3]][cw[0]] += 1;
                }
                if (cw[1] > -1 && cw[1] != cw[3] && cw[1] < mcw_context_len) {
                    graph[cw[3]][cw[1]] += 1;
                }
                if (cw[2] > -1 && cw[2] != cw[3] && cw[2] < mcw_context_len) {
                    graph[cw[3]][cw[2]] += 1;
                }
                if (cw[4] > -1 && cw[4] != cw[3] && cw[4] < mcw_context_len) {
                    graph[cw[3]][cw[4]] += 1;
                }
                if (cw[5] > -1 && cw[5] != cw[3] && cw[5] < mcw_context_len) {
                    graph[cw[3]][cw[5]] += 1;
                }
                if (cw[6] > -1 && cw[6] != cw[3] && cw[6] < mcw_context_len) {
                    graph[cw[3]][cw[6]] += 1;
                }
                
            }
        
        // turn the crank - iterate through BOW file
        
        bow_token_index = -1;
        
        if (i < bow_len) {
            fscanf(bow_test,"%[^,],",bow_tmp);
            
            for (p=0; p < mcw_context_len; p++) {
                if (strcmp(most_common_words[p],bow_tmp)==0) {
                    bow_token_index = p;
                    break;
                }
            }
        }

        
        // update the context_window
        // note:  it would be better to shift the pointers, rather than new assignment
        
        cw[0] = cw[1];
        cw[1] = cw[2];
        cw[2] = cw[3];
        cw[3] = cw[4];
        cw[4] = cw[5];
        cw[5] = cw[6];
        cw[6] = bow_token_index;
                
        }
    
        fclose(mc);

        //fclose(fout);
    
    // printf("update: completed bow iteration - writing to graph\n");
    

    for (m=0; m < mcw_target_len-1; m++) {
        
        new_entry_counter = 0;
        fprintf(fout,"%s,<START>,",most_common_words[m]);
                
        for (n=0; n < mcw_context_len-1; n++) {
            
            if (graph[m][n] > absolute_min_count) {
                
                // caps each graph 'target' at 200 max 'context' entries
                // need to experiment on the best length / pruning strategy
                // can adapt
                
                if (new_entry_counter < 200) {
                    Bloks_Graph[new_entry_counter].count = graph[m][n];
                    strcpy(Bloks_Graph[new_entry_counter].context_word, most_common_words[n]);
                    new_entry_counter ++;
                    
                }
                
                else {
                    
                    // if more than 200 entries added, then stop here
                    
                    break;
                }
                
            }
        }
        
        if (new_entry_counter > 0) {
            qsort(Bloks_Graph,new_entry_counter, sizeof(bg),struct_cmp_by_count);
        
            for (q=0; q < new_entry_counter ; q++) {
                fprintf(fout,"%s,",Bloks_Graph[q].context_word);
                fprintf(fout,"%d,",Bloks_Graph[q].count);
                }
        }
        
        fprintf(fout,"%s\n", "<END>");
        
        
    }
    
    //  ***   alternative implementation - no sort  ***
    //
    //  potential to save as output a triple of integers, and do lookup later
    //
    //  ***   + potential to write triples directly to graph DB  ***
    //
    //  writing triples to graph DB would enable much larger scalability
    //  integrate with Neo4J - which has C SDK !
    //
    
    /*
    for (m=0; m < mcw_target_len-1; m++) {
        
        fprintf(fout,"%s,<START>,",most_common_words[m]);
        
        for (n=0; n < mcw_context_len-1; n++) {
            
            // three  steps
            //  --first, write to Bloks_Graph struct
            //  --second, sort the Bloks_Graph struct
            //  --third, write to file
            
            if (graph[m][n] > absolute_min_count) {
                
                if (m==0) {
                    printf("update: sample - %d-%d-%d \n", m,n,graph[m][n]);
                }
                
                // options: can do reverse lookup back to most_common_words[ ]
                // m & n will correspond directly
                // package differently - group by m index - and then return break
                
                fprintf(fout,"%s,%d,",most_common_words[n],graph[m][n]);
                
                // alternative: output as triple of integers - (target,context,count)
                //fprintf(fout,"(%d,%d,%d)\n",m,n,graph[m][n]);
            }
        }
        fprintf(fout,"<END>\n");
    }
     */
    
    fclose(fout);
    
    return graph_count;
}


//
//                              ALTERNATIVE GRAPH BUILDER
//                              bow_context_table_main

//  alternative implementation of graph_builder = works and performs well on smaller libraries
//  ... BUT ... memory-intensive & does not scale to very large BOW
//
//  e.g., for BOW with 10M words -> it can take 20+ minutes to process
//
//  it is very stable - does not crash
//
//  file_paths are hard-coded - not parameterized


int bow_context_table_main  (char *input_account_name, char *input_corpus_name, int bow_len, int target_len) {
    
    
    char fp_test[200];
    char mc_test[200];
    char bg_test[200];
    int i=0;
    int j=0;
    int k=0;
    int q=0;
    int new_entry_counter = 0;
    int counter=0;
    int tmp_counter = 0;
    int CONTEXT_MIN_COUNT = 5;

    int CONTEXT_ENTRIES = 500;  // max number of context entries - won't save beyond this number
    char target_word[100];

    bg Bloks_Graph[50000];
    
    // printf("landed in bow context table main! \n");
    // printf("accounts-%s-corpus-%s-bow_len-%d \n", input_account_name,input_corpus_name,bow_len);
    
    // get most common words from file
    
    // NOTE: need to replace hard-coded file path
    
    sprintf(mc_test,"/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/datasets/ds1_core/most_common_words.txt", input_account_name,input_corpus_name);
    
    FILE *mc = fopen(mc_test, "r");
    
    // pull bow file and load into memory
    
    // NOTE: need to replace hard-coded file path
    
    sprintf(fp_test, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/datasets/ds1_core/bow0.txt",input_account_name,input_corpus_name);
    

    FILE *bow_test = fopen(fp_test,"r");
    
    // reads bow file into memory - capped at size of bow[]
    // need to handle very big bow - prune in some way
    
    if (bow_len > BOW_MAX) {
        bow_len = BOW_MAX;
    }
    
    for (i=0; i < bow_len ; i++) {
        
        fscanf(bow_test,"%[^,],",bow[i]);
    }
    
    fclose(bow_test);
    
    // NOTE: need to replace hard-coded file path
    
    sprintf(bg_test,"/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/datasets/ds1_core/bg.txt",input_account_name,input_corpus_name);
    

    FILE *fout = fopen(bg_test,"w");
    
    for (k=0; k < target_len; k++) {
        fscanf(mc,"%[^,],",target_word);
        
        counter = 0;
    
        for (j=0; j < bow_len-1 ; j++) {
        
        if (strcmp (bow[j], target_word) == 0) {
            if (j >= 3) {
                context_table[counter] = bow[j-3];
                counter++;
            }
            
            if (j >= 2) {
                context_table[counter] = bow[j-2];
                counter++;
            }
            
            if (j >= 1) {
                context_table[counter] = bow[j-1];
                counter++;
            }
            
            if (j+3 < bow_len) {
                context_table[counter] = bow[j+3];
                counter++;
            }
            
            if (j+2 < bow_len) {
                context_table[counter] = bow[j+2];
                counter++;
            }
            
            if (j+1 < bow_len) {
                context_table[counter] = bow[j+1];
                counter++;
                
            }}
            
            if (counter > 999990) {
                // printf("found max context table - stop here - %d \n", j);
                break;
            }
            
        }

        
        qsort(context_table,counter,sizeof(char*),cstring_cmp);

        
        fprintf(fout,"%s,",target_word);
        fprintf(fout,"%s,","<START>");
        
        new_entry_counter = 0;
        tmp_counter = 0;
        
        for (q=0; q < counter-1; q++) {
            
            if (strcmp(context_table[q],context_table[q+1]) == 0) {
                    tmp_counter ++;
            }
            
            else
            {
                if (tmp_counter >= CONTEXT_MIN_COUNT && new_entry_counter < CONTEXT_ENTRIES) {
                    // add to list and write to file

                    Bloks_Graph[new_entry_counter].count = tmp_counter;
                    strcpy(Bloks_Graph[new_entry_counter].context_word,context_table[q-1]);
                    new_entry_counter ++;
                }
                tmp_counter = 0;
            }
            }
        
        qsort(Bloks_Graph,new_entry_counter, sizeof(bg),struct_cmp_by_count);
        
        for (q=0; q < new_entry_counter ; q++) {
            if (strcmp(Bloks_Graph[q].context_word,target_word) !=0) {
                fprintf(fout,"%s,",Bloks_Graph[q].context_word);
                fprintf(fout,"%d,",Bloks_Graph[q].count);
            }
        }
        fprintf(fout,"%s\n", "<END>");

        counter = 0;
    }
    
    fclose(mc);
    fclose(fout);
    
    return bow_len;
    
}


//
//                              Comparison functions
//
//
//      --used in graph functions -> qsort
//

int cmpfunc( const void *a, const void *b) {
  return *(char*)a - *(char*)b;
}


int cstring_cmp(const void *a, const void *b) {
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}

int myStrCmp (const void * a, const void * b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

int cmpstr(const void* a, const void* b)
{
    const char* aa = *(const char**)a;
    const char* bb = *(const char**)b;
    return strcmp(aa,bb);
}

int struct_cmp_by_count (const void *a, const void *b)
{
    bg *ia = (bg *)a;
    bg *ib = (bg *)b;
    return (int)(ib->count - ia->count);
    /* float comparison: returns negative if b > a
    and positive if a > b. We multiplied result by 100.0
    to preserve decimal fraction */
}



//
//
//                          IMAGE PROCESSING UTILITY
//
//      --functions for post-processing of PDF images
//      --PDF encodes images in multiple ways, including "raw binary"
//
//      --the PDF parser takes a "raw binary" image and saves as ".ras"
//      --re-packaging the raw binary image as ".png" is time-consuming
//
//      --may need further pruning/filtering of images - there can be a lot of images in PDF
//


char *get_image_file_type (char*full_name) {
    
    //  extracts image file type -> returns part of the filename after "."
    //  can be used for multiple file checks - used here to identify image type
    
    char *file_type = NULL;
    char local_copy[500];
    char *tokens;
    
    strcpy(local_copy,full_name);
    tokens = strtok(local_copy,".");
    while (tokens !=NULL) {
        file_type = tokens;
        tokens = strtok(NULL,".");
    }

    return file_type;
}


void setRGB(png_byte *ptr, float val)
{
    int v = (int)(val * 767);
    if (v < 0) v = 0;
    if (v > 767) v = 767;
    int offset = v % 256;

    if (v<256) {
        ptr[0] = 0; ptr[1] = 0; ptr[2] = offset;
    }
    else if (v<512) {
        ptr[0] = 0; ptr[1] = offset; ptr[2] = 255-offset;
    }
    else {
        ptr[0] = offset; ptr[1] = 255-offset; ptr[2] = 0;
    }
}



int bulk_image_handler(char *input_account_name, char *input_corpus_name, char *input_mongodb_path, char *image_fp, int min_height, int min_width) {
    
    //printf("update: inititing bulk_image_handler \n");
    
    // const char *uri_string = "mongodb://localhost:27017";
    
    const char *uri_string = input_mongodb_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_t *update = NULL;
    bson_error_t error;
    mongoc_read_prefs_t *read_prefs;
    
    int j=0;
    
    uint32_t *len = 0;
    
    unsigned char *buffer;
    int filelen;
    
    bson_t *filter;
    mongoc_cursor_t *cursor;
    bson_iter_t iter;
    bson_iter_t iter2;
    bson_iter_t iter3;
    
    const bson_t *doc;
    
    char *image_name;
    uint64_t height;
    uint64_t width;
    
    FILE* image_in;
    
    int rgb = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    char image_dir_fp[300];
    char new_fp[300];
    char image_base [300];
    char tmp_c[10];
    char image_fp_local_copy[300];
    
    // Parameters for pnglib

    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;
    
    // png_bytep *row_pointers = NULL;
    
    // End - Parameters for pnglib

    strcpy(image_fp_local_copy,"");
    strcat(image_fp_local_copy,image_fp);
    
    // mongo script
   
     mongoc_init ();
     uri = mongoc_uri_new_with_error (uri_string, &error);
     
     if (!uri) {
         fprintf (stderr,
                "failed to parse URI: %s\n"
                "error message:       %s\n",
                uri_string,
                error.message);
         return EXIT_FAILURE;
     }

    client = mongoc_client_new_from_uri (uri);
     
    database = mongoc_client_get_database (client, input_account_name);
    coll = mongoc_client_get_collection (client, input_account_name, input_corpus_name);
     
    //printf("update: connecting to db in image handler-%s-%s \n", input_account_name,input_corpus_name);

    read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

    filter = BCON_NEW("content_type","image");
     
    cursor = mongoc_collection_find_with_opts (coll, filter, NULL, NULL);

    //printf("update: created cursor in database in image handler \n");
    
    j=0;
    
    while (mongoc_cursor_next (cursor, &doc)) {
        
        height = 0;
        width = 0;
        
        if (bson_iter_init_find (&iter, doc, "external_files")) {
            
            image_name = bson_iter_utf8(&iter,len);

            if (strcmp(get_image_file_type(image_name),"ras")==0) {
                
                //printf("RAS image_name-%s \n", image_name);
                if (bson_iter_init_find (&iter2, doc, "coords_cx")) {
                    height = bson_iter_int64(&iter2);
                    //printf("height - %d \n", height);
                }
                
                if (bson_iter_init_find (&iter3, doc, "coords_cy")) {
                    width = bson_iter_int64(&iter3);
                    //printf("width - %d \n", width);
                }
                
                // min_height & min_width passed as input parameters
                // usually 0 OK, but can explore other alternatives
                // pdf can generate a lot of very small images
                
                if (height > min_height && width > min_width) {
                    
                    strcpy(image_base,"");
                    
                    for (z=0; z < strlen(image_name); z++) {
                        if (image_name[z] == 46) {
                            break;
                        }
                        sprintf(tmp_c,"%c",image_name[z]);
                        strcat(image_base,tmp_c);
                    }
                    
                    strcat(image_base,".png");
                    
                    // found .ras image with height and width
                    // need to open the file from directory
                    // save as png file
                    
                    // Step 1 - get the file & load into local buffer
                    
                    
                    strcpy(image_dir_fp,"");
                    strcat(image_dir_fp,image_fp_local_copy);
                    strcat(image_dir_fp,image_name);
                    
                    /*
                    sprintf(image_dir_fp,"/bloks/accounts/main_accounts/%s/%s/images/%s",input_account_name,input_corpus_name,image_name);
                    */
                    
                    // insert safety check - if image not found in directory!
                    
                    if (image_in = fopen(image_dir_fp,"rb")) {
                    
                        fseek(image_in, 0, SEEK_END);    // jump to end-of-file
                        filelen = ftell(image_in);       // get current byte offset
                        rewind(image_in);                // jump back to beginning

                        buffer = (unsigned char *)malloc(filelen * sizeof(char)); // Enough memory for the file

                        fread(buffer, filelen, 1, image_in); // Read in the entire file
                    
                        fclose(image_in);

                        if (filelen == (3 * height * width)) {
                            
                            // effective, but not perfect test
                            // indicates RGB image (3 pixels per height/width)
                            //
                            
                            rgb = 1;
                        }
                        
                        if (filelen == (height * width)) {
                        
                            // if exact match, then grayscale image (1 pixel per h/w)

                            rgb = 2;
                        }
                        
                        if (rgb == 0) {
                            
                            // if neither condition met with height/width match ...
                            // ... need to handle these cases better
                            
                            //printf("error: no match in height/width- skipping - %d - %d - %d - %s \n", filelen, height, width, image_dir_fp);

                        }
                        
                        strcpy(new_fp,"");
                        strcat(new_fp,image_fp_local_copy);
                        strcat(new_fp,image_base);
                    
                        /*
                        sprintf(new_fp, "/bloks/accounts/main_accounts/%s/%s/images/%s", input_account_name,input_corpus_name,image_base);
                         */
                        
                    
                        j++;
                    
                        fp = fopen(new_fp, "wb");
                        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
                        info_ptr = png_create_info_struct(png_ptr);
                    
                        png_init_io(png_ptr, fp);
                    
                        if (rgb == 1) {
                            png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
                            row=malloc(3 * width * sizeof(png_byte));
                        }
                        
                        else {
                            png_set_IHDR(png_ptr,info_ptr, width,height,8, PNG_COLOR_TYPE_GRAY,
                                     PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_BASE,PNG_FILTER_TYPE_BASE);
                            row=malloc(width * sizeof(png_byte));
                        }
                    
                        png_write_info(png_ptr,info_ptr);
                    
                    //
                    //
                    // alternative attempt to unpack the binary image into PNG
                        
                    // Allocate memory for one row (3 bytes per pixel - RGB)
                    
                    //row = malloc(3 * width * sizeof(png_byte));
                    
                    /*
                    row_pointers = (png_bytep*)malloc(sizeof(png_bytep*)*height);
                    for (y=0; y < height ; y++) {
                        row_pointers[y] = (png_bytep*)malloc(png_get_rowbytes(png_ptr,info_ptr));
                    }
                    
                    png_write_image(png_ptr,row_pointers);
                    
                    for (y=0; y < height ; y++) {
                        free(row_pointers[y]);
                    }
                    */
                    
                    // Write image data
                     
                    //png_set_rows(png_ptr,info_ptr,row_pointers);
                    //png_image_write_to_file(png_ptr,fp,0,*buffer,0,NULL);
                    //printf("survived to writer! \n");
                    
                    //printf("next step is row pointers write \n");
                    //png_set_rows(png_ptr,info_ptr,row_pointers);
                    //printf("next step is write_png \n");
                    //png_write_image(png_ptr, row_pointers);
                    //png_write_end(png_ptr, info_ptr);
                    
                    for (y=0 ; y< height ; y++) {
                        /*
                        for (x=0 ; x < width ; x++) {
                            //setRGB (&(row[x*3]), buffer[y*width*3 + x]);
                            row[x*3] = buffer[y*width*3+x];
                            row[1+(x*3)] = buffer[y*width*3+x+1];
                            row[2+(x*3)] = buffer[y*width*3+x+2];
                           }
                         */
                        if (rgb==1) {
                            for (x=0 ; x < 3*width; x++) {
                                row[x] = buffer[y*width*3 +x];
                            }
                        }
                        else {
                            for (x=0; x < width; x++) {
                                row[x] = buffer[y*width + x];
                            }
                        }
                        
                        png_write_row(png_ptr, row);
                        
                        }
                    
                    // End write
                    png_write_end(png_ptr, NULL);

                    fclose(fp);
                    
                    png_destroy_write_struct(&png_ptr,&info_ptr);
                    
                    // update Mongo record with new image name (e.g., .png)
                    
                    update = BCON_NEW ("$set", "{",
                                           "external_files", BCON_UTF8 (image_base),
                                           "updated", BCON_BOOL (true),
                                       "}");

                    if (!mongoc_collection_update (coll, MONGOC_UPDATE_NONE, doc, update, NULL, &error)) {
                        printf ("error:  database generated error - %s\n", error.message);
                    }
                    
                }
                }
                    
                }
            }
        
        
    }

    
    if (mongoc_cursor_error (cursor, &error)) {
       fprintf (stderr, "error:  database generated - an error occurred: %s\n", error.message);
    }
    
    
    mongoc_cursor_destroy (cursor);
    bson_destroy (filter);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    //printf("update: finished bulk_image_handler file cleanup successfully \n");
    
    return 0;
}









