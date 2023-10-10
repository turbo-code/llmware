
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

//  office_parser_llmware.c
//  office_parser_llmware

//  XML-based Parser for Microsoft Office Documents
//  Handles three major document types:
//
//      -- Powerpoint, e.g., pptx -> does not handle older ppt format
//      -- Word, e.g., docx -> does not handle older doc format
//      -- XL, e.g., xlsx -> does not handle older xl format
//
//  Also, includes specialized binary parser for .EMF embedded file types
//
//  All three document types start as a ZIP compressed folder
//  ... within each ZIP folder, there are dozens of .XML based files specific to each doc format
//  ... all of the XML files follow the Office Open XML protocols
//
//  Reference:  ISO 29500 - Office Open XML - first enacted in 2012, and updated in 2016




#include "office_parser_llmware.h"


char* get_file_name (char*longer_string) {
    
    // utility function to extract file_name from longer_string
    // e.g., longer_string input, e.g., /ppt/media/image32.png
    
    char *fn = NULL;
    char *tokens;
    
    tokens = strtok(longer_string,"/");
    while (tokens != NULL) {
        fn = tokens;
        tokens = strtok(NULL,"/");
    }
    
    return fn;
}

char *get_file_type (char*full_name) {
    
    // utility function to extract the file type
    // e.g., image32.png -> .png
    
    char *file_type = NULL;
    char local_copy[5000];
    char *tokens;
    
    strcpy(local_copy,full_name);
    tokens = strtok(local_copy,".");
    while (tokens !=NULL) {
        file_type = tokens;
        tokens = strtok(NULL,".");
    }

    return file_type;
}


char* number_display(char*v) {
    
    char* display_value = NULL;
    int i;
    int found_decimal = -1;
    int decimal_count = 0;
    
    for (i=0; i < strlen(v); i++) {
        if (found_decimal == -1) {
            if (v[i] > 47 && v[i] < 58) {
                strcat(display_value,v[i]);
            }
            if (v[i] == 46) {
                strcat(display_value,v[i]);
                found_decimal = 1;
            }
        }
        else {
            if (v[i] > 47 && v[i] < 58) {
                strcat(display_value,v[i]);
                decimal_count ++;
                if (decimal_count == 2) {
                    break;
                }
            }
        }
    }
    
    return display_value;
}


int keep_value (char*v) {
    
    // utility function used only by xl
    // spreadsheets can potentially contain extremely large datasets
    // current design decision:  don't keep numeric fields
    // over time, there could be a lot of interesting applications to 'correlate' spreadsheets
    //
    // right now, for spreadsheets -> all alpha data is extracted, including column/row headers
    //
    // keep_value function looks at the start of a string- if not alpha
    // if a non-numeric chr is found, then keep_value is changed to save
    
    int i;
    int keep_value = -1;
    
    for (i=0; i < strlen(v); i++) {
        
        if ((v[i] > 47 && v[i] < 58) || v[i] == 45 || v[i] == 46 || ((i > 0) && (v[i]==69 || v[i] == 101))) {
            
            // this is a valid float - keep going
        }
        else {
            // not a float -> must be string
            keep_value = 1;
            break;
        }
        
    }

    return keep_value;
}


int keep_value_does_not_crash_old (char*v) {
    
    // utility function used only by xl
    // spreadsheets can potentially contain extremely large datasets
    // current design decision:  don't keep numeric fields
    // over time, there could be a lot of interesting applications to 'correlate' spreadsheets
    //
    // right now, for spreadsheets -> all alpha data is extracted, including column/row headers
    //
    // keep_value function looks at the start of a string- if not alpha
    // if a non-numeric chr is found, then keep_value is changed to save
    
    int i;
    int keep_value = -1;
    
    for (i=0; i < strlen(v); i++) {
        
        if (isalpha(v[i]) != 0) {
            // if !=0, then alpha
            // secondary check, not "E" - which can be float!
            if (isalpha(v[i]) != 69) {
                keep_value = 1;
                break;
            }
        }
        
    }

    return keep_value;
}

int special_formatted_text (char*b, char*i, char*u, char*sz, char*color) {
    
    // will apply rules on formatting in rPr to assess whether to flag as special_text
    // *** returns 0 if normal // returns 1 if special ***
    
    // will need to add color handler too
    
    int bold=0;
    int italics=0;
    int size=0;
    int special = 0;
    
    if (b != NULL) {
        if (strcmp(b,"") !=0) {
            bold = atoi(b);
        }
    }
    
    if (i != NULL) {
        if (strcmp(i,"") !=0) {
            italics = atoi(i);
        }
    }
    
    if (sz != NULL) {
        if (strcmp(sz,"") !=0) {
            size = atoi(sz);
        }
    }
    
    if (size >= 1800) {special = 1;}
    if (bold == 1) {special = 1;}
    if (italics == 1) {special = 1;}
    
    if (u != NULL) {
        if (strcmp(u,"") !=0) {
            special = 1;
        }
    }
 
    return special;
}




coords check_rels_coords (char*fp, char*nv_id, int wf, char *ws_fp) {
    
    coords rel_position;
    
    xmlDoc *doc = NULL;
    xmlDoc *slideLayout_doc = NULL;
    
    xmlNode *root_element = NULL;
    xmlNode *iter_element = NULL;
    
    xmlNode *layout_root_element = NULL;
    xmlNode *layout_sptree_element = NULL;
    xmlNode *cur_node = NULL;
    xmlNode *sp_node = NULL;
    xmlNode *sp_iter_node = NULL;
    xmlNode *coords_node = NULL;
    
    char layout_fp[200];
    char layout_fp_base[200];
    char *prop_id = NULL;
    char nv_compare[10];
    char *x = NULL;
    char *y = NULL;
    char *cx = NULL;
    char *cy = NULL;
    int my_x,my_y,my_cx,my_cy;
    char tmp_str[100];
    
    strcpy(layout_fp_base,ws_fp);
    sprintf(tmp_str,"%d/",wf);
    strcat(layout_fp_base,tmp_str);

    doc = xmlReadFile(fp,NULL,0);
    root_element = xmlDocGetRootElement(doc)->children;
    
    for (iter_element=root_element; iter_element; iter_element = iter_element->next) {
        
        prop_id = xmlGetProp(iter_element,"Target");
        
        if (strstr(prop_id,"slideLayouts/slideLayout") != NULL) {
            strcpy(layout_fp,get_file_name(prop_id));
            strcat(layout_fp_base,layout_fp);
            
            slideLayout_doc = xmlReadFile(layout_fp_base,NULL,0);
            
            if (slideLayout_doc != NULL) {
                layout_root_element = xmlDocGetRootElement(slideLayout_doc)->children;
                layout_sptree_element = layout_root_element->children->children;
                
                for (cur_node = layout_sptree_element; cur_node; cur_node=cur_node->next) {
                    
                    if (strcmp(cur_node->name,"sp") == 0) {
                        sp_node = cur_node->children;
                        while (sp_node != NULL) {
                            
                            //note useful for debugging:   printf ("iterating thru sp: %s \n",sp_node->name);
                            
                            if (strcmp(sp_node->name, "nvSpPr")==0) {
                                strcpy(nv_compare,(xmlGetProp(sp_node->children,"id")));
                                
                                //note useful for debugging:  printf ("found nv_compare-%s \n", nv_compare);
                                
                                if (strcmp(nv_compare,nv_id)==0) {
                                    
                                    sp_iter_node = sp_node->next;
                                    while (sp_iter_node !=NULL) {
                                        if (strcmp(sp_iter_node->name,"spPr")==0) {
                                            coords_node = sp_iter_node->children;
                                            while (coords_node != NULL) {
                                                if (strcmp(coords_node->name,"xfrm")==0) {
                                                    
                                                    x= xmlGetProp(coords_node->children,"x");
                                                    y= xmlGetProp(coords_node->children,"y");
                                                    cx = xmlGetProp(coords_node->children->next,"cx");
                                                    cy = xmlGetProp(coords_node->children->next,"cy");}
                                                
                                                coords_node = coords_node->next;}}
                                        sp_iter_node = sp_iter_node->next;}}}
                            sp_node = sp_node->next;}}}
            }}
    }
    
    if (x != NULL)
    { my_x = atoi(x);}
    else
    { my_x = 0;}
    
    if (y != NULL)
    { my_y = atoi(y);}
    else
    { my_y = 0;}
    
    if (cx != NULL)
    { my_cx = atoi(cx);}
    else
    { my_cx = 0;}
    
    if (cy != NULL)
    { my_cy = atoi(cy);}
    else
    { my_cy = 0;}
    
    rel_position.x = my_x;
    rel_position.y = my_y;
    rel_position.cx = my_cx;
    rel_position.cy = my_cy;

    xmlMemFree(sp_node);
    xmlMemFree(sp_iter_node);
    xmlMemFree(coords_node);
    xmlMemFree(layout_sptree_element);
    xmlMemFree(root_element);
    xmlMemFree(layout_root_element);
    xmlMemFree(iter_element);
    xmlMemFree(cur_node);
    xmlFreeDoc(doc);
    xmlFreeDoc(slideLayout_doc);
    
    return rel_position;
}

int compare_emf_text_coords_y (const void *a, const void *b) {
    
    emf_text_coords *cordA = (emf_text_coords *)a;
    emf_text_coords *cordB = (emf_text_coords *)b;
    
    return (cordA->y - cordB->y);
}

int compare_emf_text_coords_x (const void *a, const void *b) {
    
    emf_text_coords *cordA = (emf_text_coords *)a;
    emf_text_coords *cordB = (emf_text_coords *)b;
    
    return (cordA->x - cordB->x);
}

int emf_handler (char *fp, int global_blok_counter, int wf) {
    
    int z=0;
    int t=0;
    int t_char;
    int counter = 0;
    FILE *fileptr;
    unsigned char *buffer;
    int filelen;
    int text_len = 0;
    int text_start = 0;
    int x_out = 0;
    int y_out = 0;
    char tmp[1000];
    
    emf_text_coords text_coords[100000];
    emf_text_coords text_row[1000];
     
    int coords_count = 0;
    char running_text[50000];
    char table_text_out[100000];
    char cell_tmp[1000];
    int row_count = 0;
    int last_y = 0;
    int last_x = 0;
    int col = 0;
    int col_tracker = 0;
    int x = 0;
    int y = 0;
    int first_entry = 0;
    int max_x = 0;
    int max_y = 0;
    int space_gap_th = -1;
    int success_code = 0;

    int added_nonempty_cell_to_row = 0;
    
    strcpy(running_text,"");
    strcpy(table_text_out,"");
    strcpy(cell_tmp,"");
    
    if (debug_mode == 1) {
        printf("update: office_parser - processing embedded emf - in emf handler - file path-%s \n", fp);
    }
    
    //
    // note: EMF is enhanced metafile - it is a windows/microsoft specific format
    //      -- many pptx, in particular, will embed copied xlsx tables as .emf files
    //      -- EMF is a binary format and text/table can usually be extracted
    //      -- this function works pretty well, but could be enhanced
    //      -- essentially, it converts .emf image file -> text + table
    //
    
    fileptr = fopen(fp, "rb");
    fseek(fileptr, 0, SEEK_END);         // Jump to the end of the file
    filelen = ftell(fileptr);            // Get the current byte offset in the file
    rewind(fileptr);                     // Jump back to the beginning of the file

    buffer = (unsigned char *)malloc(filelen * sizeof(char)); // Enough memory for the file
    fread(buffer, filelen, 1, fileptr); // Read in the entire file

    strcpy(text_coords[0].text,"");
    
    if (debug_mode == 1) {
        // printf("update: in emf- len - %d \n", filelen);
    }
    
    for (z=0; z < filelen; z++) {
        
        if (z > counter && (buffer[z] == 83 || buffer[z] == 84)) {
            if (buffer[z+1] == 0 && buffer[z+2] == 0 && buffer[z+3] == 0) {
                text_len = buffer[z+44];
                text_start = buffer[z+48];

                if (text_start == 76 && buffer[z+52] == 0 && buffer[z+53] == 0 && buffer[z+54] == 0 && buffer[z+55] == 0) {
                    
                    for (t=0; t<(text_len*2); t++) {
                        
                        x_out = buffer[z+37]*256 + buffer[z+36];
                        y_out = buffer[z+41]*256 + buffer[z+40];
                        
                        if (x_out > max_x) {
                            max_x = x_out;
                        }
                        
                        if (y_out > max_y) {
                            max_y = y_out;
                        }
                        
                        if (first_entry == 0) {
                            last_x = x_out;
                            last_y = y_out;
                            text_coords[0].x = x_out;
                            text_coords[0].y = y_out;
                            first_entry = 1;
                        }
                    
                        t_char = buffer[z+76+t];
                        
                        if (((t_char >= 32) && (t_char <128)) || (t_char == 160)) {
                            
                            // if same coords, then add to string in .text
                            
                            if (x_out == last_x && y_out == last_y) {
                                
                                if (t_char == 160) {
                                    
                                    if (strlen(text_coords[coords_count].text) < 99) {
                                        strcat(text_coords[coords_count].text," ");
                                    }
                                }
                                else {

                                    sprintf(tmp,"%c",t_char);
                                    
                                    if (strlen(text_coords[coords_count].text) < 99) {
                                        strcat(text_coords[coords_count].text,tmp);
                                    }
                                    
                                }
                            }
                            
                            else {
                                
                                if (coords_count < 99999) {
                                    coords_count ++;
                                }
                                else {
                                    
                                    if (debug_mode == 1) {
                                        printf("update: office_parser - emf_handler- max cap surpassed - %d \n", coords_count);
                                    }
                                }
                                
                                text_coords[coords_count].x=x_out;
                                text_coords[coords_count].y=y_out;
                                strcpy(text_coords[coords_count].text,"");
                                last_x = x_out;
                                last_y = y_out;
                                
                                if (t_char == 160) {
                                    
                                    if (strlen(text_coords[coords_count].text) < 99) {
                                        strcat(text_coords[coords_count].text, " ");
                                    }
                                }
                                
                                else {
                                    sprintf(tmp,"%c",t_char);
                                    
                                    if (strlen(text_coords[coords_count].text) < 99) {
                                        strcat(text_coords[coords_count].text, tmp);
                                    }
                                }
                                
                            }
                            
                        }

                }
                    counter = z + text_len + text_start + 10;
                }
                
                
            }}}
    

    fclose(fileptr);
    free(buffer);
    
    if (strlen(text_coords[coords_count].text) >0) {
        coords_count ++;
    }
    
    if (debug_mode == 1) {
        //printf("update: emf_handler - total coords - %d - max-x %d - max-y %d \n", coords_count, max_x,max_y);
    }
    
    // two resolutions of emf images - one is "high-res" with coordinates in the range of 1000-10K
    // the lower resolution is typically <1000
    
    if (max_x > 2000 || max_y > 2000) {
        space_gap_th = 300;
    }
    else {
        space_gap_th = 20;
    }
    
    if (debug_mode == 1) {
        //printf("update: emf_handler - based on max_x & max_y analysis - space_gap_th - %d \n", space_gap_th);
    }
    
    if (coords_count > 0) {
        qsort(text_coords,coords_count,sizeof(emf_text_coords),compare_emf_text_coords_y);
        col = 0;
        last_y = 0;
        
        for (t=0; t < coords_count; t++) {
            
            if (text_coords[t].y == last_y || t == 0) {
                text_row[col].x = text_coords[t].x;
                text_row[col].y = text_coords[t].y;
                strcpy(text_row[col].text,text_coords[t].text);
                
                if (col < 999) {
                    col ++;
                }
                else {
                    
                    if (debug_mode == 1) {
                        printf("update:  office_parser - emf_handler - max col surpassed- not saving more content in table - %d \n", col);
                    }
                }
                
                last_y = text_coords[t].y;
            }
            
            else {
                // end of row
                qsort(text_row,col,sizeof(emf_text_coords),compare_emf_text_coords_x);
                row_count ++;
                last_y = text_coords[t].y;
                
                
                last_x = 0;
                
                if (strlen(table_text_out) > 99000 || strlen(running_text) > 49000) {
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - emf_handler - surpassed total table size - table - %d - text - %d \n", strlen(table_text_out), strlen(running_text));
                    }
                    
                    break;
                }
                
                strcat(table_text_out," <tr> ");
                
                /*
                strcat(table_text_out," <tr> <th> <A");
                strcat(running_text, " <A");
                
                sprintf(tmp,"%d", row_count);
                strcat(table_text_out,tmp);
                strcat(table_text_out,"> ");
                
                strcat(running_text,tmp);
                strcat(running_text,"> ");
                */
                
                strcpy(cell_tmp,"");
                strcpy(tmp,"");
                col_tracker = 0;
                
                last_x = text_row[0].x;
                               
                for (x=0; x < col; x++) {
                    
                    if (text_row[x].x > last_x + space_gap_th) {
                        
                        // new col - save current cell_tmp
                        
                        if (strlen(cell_tmp) > 0) {
                            // only save if cell_tmp is not empty
                            
                            if ( ((strlen(table_text_out)+strlen(cell_tmp)) < 99900) && ((strlen(running_text)+strlen(cell_tmp)) < 49900) ) {
                                
                                
                                if (debug_mode == 1) {
                                    //printf("update: emf_hander - building col - %d - %d - %s \n", text_row[x].x, last_x, cell_tmp);
                                }
                                
                                // good to go to add cell to table + running_text
                                
                                sprintf(tmp,"%c", 65 + col_tracker);
                                strcat(table_text_out," <th> <");
                                strcat(table_text_out,tmp);
                                strcat(running_text," <");
                                strcat(running_text,tmp);
                                
                                sprintf(tmp,"%d",row_count);
                                strcat(table_text_out,tmp);
                                strcat(table_text_out,"> ");
                                
                                strcat(running_text,tmp);
                                strcat(running_text,"> ");
                                
                                strcat(table_text_out, cell_tmp);
                                strcat(running_text, cell_tmp);
                                strcat(running_text, " ");
                                
                                strcat(table_text_out, " </th>");
                                
                                //strcat(table_text_out, " </th> <th> <");
                                //strcat(running_text, " <");
                        
                                //sprintf(tmp,"%c", 65 + col_tracker);
                                //strcat(table_text_out, tmp);
                                //strcat(running_text, tmp);
                        
                                //sprintf(tmp,"%d", row_count);
                                //strcat(table_text_out,tmp);
                                //strcat(table_text_out, "> ");
                                //strcat(running_text,tmp);
                                //strcat(running_text, "> ");
                        
                                //strcat(table_text_out, cell_tmp);
                        
                                //strcat(running_text, cell_tmp);
                                //strcat(running_text, " ");
                        
                                strcpy(cell_tmp, "");
                                sprintf(tmp,"%s", text_row[x].text);
                                strcat(cell_tmp, tmp);
                                col_tracker ++;
                                

                            }
                        }
                    }
                    
                    else {
                        sprintf(tmp,"%s",text_row[x].text);
                        strcat(cell_tmp, tmp);
                        
                        //printf("building cell_tmp - %d-%d- %s \n", text_row[x].x,last_x, cell_tmp);
                    
                    }
                    
                    if (debug_mode == 1) {
                        //printf("%s", text_row[x].text);
                    }
                    
                    last_x = text_row[x].x;
                }
                
                // clean up last bit of text and close up the row
                
                if (strlen(cell_tmp) > 0) {
                    
                    if ( ((strlen(table_text_out)+strlen(cell_tmp)) < 99900) && ((strlen(running_text)+strlen(cell_tmp)) < 49900) ) {
                        
                        if (debug_mode == 1) {
                            //printf("update: in emf_handler - last col on row - %s \n", cell_tmp);
                        }
                        
                        sprintf(tmp,"%c", 65 + col_tracker);
                        strcat(table_text_out," <th> <");
                        strcat(table_text_out,tmp);
                        strcat(running_text," <");
                        strcat(running_text,tmp);
                        
                        sprintf(tmp,"%d",row_count);
                        strcat(table_text_out,tmp);
                        strcat(table_text_out,"> ");
                        
                        strcat(running_text,tmp);
                        strcat(running_text,"> ");
                        
                        strcat(table_text_out, cell_tmp);
                        strcat(table_text_out, " </th> </tr>");
                        
                        strcat(running_text, cell_tmp);
                        strcat(running_text, " ");
                    }
                }
                
                // save entry and start new row
                col = 0;
                text_row[col].x = text_coords[t].x;
                text_row[col].y = text_coords[t].y;
                strcpy(text_row[col].text,text_coords[t].text);
                col ++;
                
            }
        
            //last_y = text_coords[t].y;
            
        }
        
        if (debug_mode == 1) {
            printf("update: office_parser - emf_handler - emf total row count - %d \n", row_count);
        }
    }
    
    if (strlen(running_text) > 0 && strlen(table_text_out) > 0) {
        
        success_code = 99;
        
        if (strlen(running_text) < 49999) {
            strcpy(Bloks[wf][global_blok_counter].text_run,running_text);
            strcpy(Bloks[wf][global_blok_counter].content_type,"text");
        }
        else {
            success_code = -1;
        }

        if (strlen(table_text_out) < 99999) {
            strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
            strcpy(Bloks[wf][global_blok_counter].content_type, "table");
            Bloks[wf][global_blok_counter].position.cx = row_count;
            Bloks[wf][global_blok_counter].position.cy = 0;
            Bloks[wf][global_blok_counter].position.y = 0;
            Bloks[wf][global_blok_counter].position.x = 0;
            
            global_total_tables_added ++;
            
        }
        else {
            success_code = -2;
        }
    }

    if (debug_mode == 1) {
        printf("update: office_parser - emf_handler - text_table_out - %s \n", table_text_out);
        printf("update: office_parser - emf_handler - running_text - %s \n", running_text);
    }
    
    return success_code;
}



int sp_handler_new (xmlNode *element, int global_blok_counter, int short_last_text_blok, int slide_num, int shape_num, int wf,
                    char *ws_fp)
{
    
    const char *t = "t";
    const char *txbody = "txBody";
    const char *p = "p";
    const char *r = "r";
    
    xmlNode *iter_node = NULL;
    xmlNode *text_node = NULL;
    xmlNode *iter2_node = NULL;
    xmlNode *iter3_node = NULL;
    xmlNode *text = NULL;
    xmlNode *props = NULL;
    xmlNode *coords_node = NULL;
    
    char *key=NULL;
    char *doc=NULL;
    char sz[10];
    char bold[30];
    char underline[30];
    char italics[30];
    char *dummy = NULL;
    
    char *x=NULL;
    char *y=NULL;
    char *cx=NULL;
    char *cy=NULL;
    
    char* core_text_out;
    char* special_format_text_out;
    
    int format_flag = 0;
    int success_factor = 1;
    coords my_sp_position;
    int my_x = 0;
    int my_y = 0;
    int my_cx = 0;
    int my_cy = 0;
    char nv_id[10];
    char fp_rels[200];
    char fp_rels_tmp_str[100];
    //coords rels_coords;
    int coords_set = 0;
    
    int found_non_space = 0;
    int i=0;
    
    if (debug_mode == 1) {
        //printf("update:  in sp handler starting-%d \n",wf);
    }
    
    core_text_out = (char *) malloc(50000);
    special_format_text_out = (char *) malloc(50000);

    strcpy(core_text_out,"");
    strcpy(special_format_text_out,"");
    
    my_sp_position.x = my_x;
    my_sp_position.y = my_y;
    my_sp_position.cx = my_cx;
    my_sp_position.cy = my_cy;
    
    // first iteration inside <sp> to find txBody - which contains text
    //iter_node = element->next;
    
    
    iter_node = element;
    
    while (iter_node != NULL) {
        
        
        if (strcmp(iter_node->name, "nvSpPr")==0) {
            
            strcpy(nv_id,(xmlGetProp(iter_node->children,"id")));
        }
        
        if (strcmp(iter_node->name,"spPr")==0) {
            
            x= "0";
            y= "0";
            cx="0";
            cy="0";
            
            coords_node = iter_node->children;
            while (coords_node != NULL) {
                if (strcmp(coords_node->name,"xfrm")==0) {
                    
                    x= xmlGetProp(coords_node->children,"x");
                    y= xmlGetProp(coords_node->children,"y");
                    cx = xmlGetProp(coords_node->children->next,"cx");
                    cy = xmlGetProp(coords_node->children->next,"cy");
                    }
                
                coords_node = coords_node->next;
                
            }
            
            if ((strcmp(x,"0") && strcmp(y,"0") && strcmp(cx,"0") && strcmp(cy,"0"))==0) {
                
                strcpy(fp_rels,ws_fp);
                
                sprintf(fp_rels_tmp_str,"%d/slide%d.xml.rels",wf,slide_num);
                strcat(fp_rels,fp_rels_tmp_str);
                
                my_sp_position = check_rels_coords(fp_rels,nv_id,wf, ws_fp);
                
                coords_set = 1;
                
            }
        }
            
        
        if (strcmp(iter_node->name,txbody)==0) {
            
            text_node = iter_node->children;
            while (text_node != NULL) {
                if (strcmp(text_node->name,p)==0) {
                    
                    iter2_node = text_node->children;
                    while (iter2_node != NULL) {
                        
                        if (strcmp(iter2_node->name,"br")==0) {
                            if (strlen(core_text_out) > 0) {
                                strcat(core_text_out, " ");
                            }
                            if (strlen(special_format_text_out) > 0) {
                                strcat(special_format_text_out, " ");
                            }
                            if (strlen(global_headlines) > 0) {
                                strcat(global_headlines, " ");
                            }
                        }
                        
                        if (strcmp(iter2_node->name,r)==0) {
                            
                            format_flag = 0;
                            iter3_node = iter2_node->children->next;
                            props = iter2_node->children;
                            
                            if (xmlGetProp(props,"sz") != NULL) {
                                strcpy(sz,xmlGetProp(props,"sz"));
                            }
                            
                            if (xmlGetProp(props,"b") != NULL) {
                                strcpy(bold,xmlGetProp(props,"b"));
                            }
                            
                            if (xmlGetProp(props,"u") != NULL) {
                                strcpy(underline,xmlGetProp(props,"u"));
                            }
                            
                            if (xmlGetProp(props,"i") != NULL) {
                                strcpy(italics,xmlGetProp(props,"i"));
                                
                            }
                            
                            // need to find <solidFill> attribute 'val'
                            // this is where the text color is held
                            // using dummy value for now
                            
                            dummy="color_tbd";

                            format_flag = special_formatted_text(bold,italics,underline,sz,dummy);
                            
                            strcpy(bold,"");
                            strcpy(italics,"");
                            strcpy(sz,"");
                            strcpy(underline,"");
                    
                            while (iter3_node != NULL) {
                                
                                if (strcmp(iter3_node->name,t)==0) {

                                    text = iter3_node->children;
                                    key = xmlNodeListGetString(doc,text,1);

                                    if (key != NULL) {
                                        
                                        if (key[0]==32) {
                                            // no action required
                                            // note: helpful tracker:  printf("key == space \n");
                                            strcat(core_text_out, key);
                                            
                                            if (format_flag == 1) {
                                                if (strlen(special_format_text_out) < 50000) {
                                                    strcat(special_format_text_out, key);
                                                }
                                                
                                                if (strlen(global_headlines) < 1000) {
                                                    strcat(global_headlines, key);
                                                }
                                            }
                                            
                                        }
                                        
                                        else {
                                            
                                        strcat(core_text_out,key);
                                        //strcat(core_text_out," ");
                                        
                                        // ...useful for debugging
                                        // ...will show the incremental buildout of the output text
                                        
                                            if (debug_mode == 1) {
                                                //printf("update:  core text out- %d-%s \n",strlen(core_text_out),core_text_out);
                                            }
                                            
                                        if (format_flag == 1) {
                                            if (strlen(special_format_text_out) < 50000) {
                                                strcat(special_format_text_out,key);
                                                //strcat(special_format_text_out," ");
                                                
                                            }
                                            
                                            if (strlen(global_headlines) < 1000) {
                                                strcat(global_headlines, key);
                                                //strcat(global_headlines, " ");
                                                if (debug_mode == 1) {
                                                    //printf("updating global headlines - %s \n", key);
                                                }
                                            }
                                        }
                                        }}
                                }
                    
                                iter3_node = iter3_node->next;

                            }
                        }

                        iter2_node = iter2_node->next;
                        
                        }
                    }
                
                // will concatenate several <p> potentially into a single text blok
                
                if (strlen(core_text_out) > 0) {
                    strcat(core_text_out, " ");
                }
                
                if (strlen(special_format_text_out) > 0) {
                    strcat(special_format_text_out, " ");
                }
                
                if (strlen(global_headlines) > 0) {
                    strcat(global_headlines, " ");
                }
                
                text_node = text_node->next;
            }
        }

        iter_node = iter_node->next;
    }
    
    // reached the end of the <sp>
    
    if (strlen(core_text_out) > 0) {
        
        // secondary check - ignore if core_text_out is only spaces
        
        found_non_space = -1;
        for (i=0; i < strlen(core_text_out); i++) {
            if (core_text_out[i] != 32) {
                found_non_space = 1;
                break;
            }
        }
        
        if (found_non_space == 1) {
            
            if (short_last_text_blok > -1) {
                // the last text blok was short -> add to this blok
                
                if ((strlen(core_text_out) + strlen(Bloks[wf][short_last_text_blok].text_run)) < 49900) {
                    strcat(Bloks[wf][short_last_text_blok].text_run, " ");
                    strcat(Bloks[wf][short_last_text_blok].text_run, core_text_out);
                    
                    strcat(Bloks[wf][short_last_text_blok].formatted_text, " ");
                    strcat(Bloks[wf][short_last_text_blok].formatted_text, special_format_text_out);
                    
                    //printf("update: sp_handler- short_blok - %d - %s \n",short_last_text_blok, Bloks[wf][short_last_text_blok].formatted_text);
                    
                    if (debug_mode == 1) {
                        //printf("update: office_parser- sp_handler_new - consolidating block - %s \n", Bloks[wf][short_last_text_blok].text_run);
                    }
                    
                    if (strlen(Bloks[wf][short_last_text_blok].text_run) > (0.50 * GLOBAL_BLOK_SIZE)) {
                        success_factor = 97;
                        // wrote to old blok - but OK
                    }
                    else {
                        success_factor = 96;
                        // wrote to old blok - and still short
                    }
                }
                
            }
            else {
                success_factor = 99;
                // wrote to new blok - and OK
                
                Bloks[wf][global_blok_counter].slide_num = slide_num;
                Bloks[wf][global_blok_counter].shape_num = shape_num;
                strcpy(Bloks[wf][global_blok_counter].content_type,"text");
                strcpy(Bloks[wf][global_blok_counter].relationship,"");
                strcpy(Bloks[wf][global_blok_counter].linked_text,"");
                strcpy(Bloks[wf][global_blok_counter].table_text,"");
                strcpy(Bloks[wf][global_blok_counter].file_type,"ppt");
                
                if (debug_mode == 1) {
                    printf("update: office_parser - sp_handler_new - core text out-copied-Blocks array-counter-%d-len-%d - %s \n", global_blok_counter, strlen(core_text_out), core_text_out);
                }
                
                strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
                strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
                
                //printf("update: sp_handler - normal blok - %d - %s \n", global_blok_counter, Bloks[wf][global_blok_counter].formatted_text);
                
                if (strlen(Bloks[wf][global_blok_counter].text_run) < (0.50 *GLOBAL_BLOK_SIZE)) {
                    success_factor = 98;
                    // wrote to new blok - but still short
                    if (debug_mode == 1) {
                        //printf("update: office_parser - wrote to new block - but still very short! \n");
                    }
                }
                
                if (coords_set == 0) {
                    
                    if (x != NULL)
                    { my_x = atoi(x);}
                    else
                    { my_x = 0;}
                    
                    if (y != NULL)
                    { my_y = atoi(y);}
                    else
                    { my_y = 0;}
                    
                    if (cx != NULL)
                    { my_cx = atoi(cx);}
                    else
                    { my_cx = 0;}
                    
                    if (cy != NULL)
                    { my_cy = atoi(cy);}
                    else
                    { my_cy = 0;}
                    
                    my_sp_position.x = my_x;
                    my_sp_position.y = my_y;
                    my_sp_position.cx = my_cx;
                    my_sp_position.cy = my_cy;
                }
                
                Bloks[wf][global_blok_counter].position = my_sp_position;
            }
        }
}

    free(core_text_out);
    free(special_format_text_out);
    xmlMemFree(coords_node);
    xmlMemFree(iter2_node);
    xmlMemFree(iter3_node);
    xmlMemFree(text_node);
    xmlMemFree(text);
    xmlMemFree(props);
    xmlMemFree(iter_node);
    
    return success_factor;
    
}



int pics_handler_new (xmlNode *element,int global_blok_counter, int slide_num, int shape_num, int wf)
{
    
    
    const char *blipfill = "blipFill";
    const char *sp_pr = "spPr";
    xmlNode *iter_element = NULL;
    xmlNode *tmp1 = NULL;
    xmlNode *tmp2 = NULL;
    char *embed = NULL;
    char *x = NULL;
    char *y = NULL;
    char *cx = NULL;
    char *cy = NULL;

    char core_text_out[50000];
    char special_format_text_out[50000];
    int success_factor = 1;
    coords my_sp_position;
    int my_x = 0;
    int my_y = 0;
    int my_cx = 0;
    int my_cy = 0;
    
    strcpy(core_text_out,"");
    strcpy(special_format_text_out,"");
    
    iter_element = element->next;
    
    while (iter_element != NULL) {
        
        if (strcmp(iter_element->name,blipfill) == 0) {
            tmp1 = iter_element->children;
            embed = xmlGetProp(tmp1,"embed");

            if (embed != NULL) {
                success_factor = 99;
            }
        }
         
        if (strcmp(iter_element->name,sp_pr) == 0) {
            
            tmp2 = iter_element->children;
            
            while (tmp2 != NULL) {
                
                if (strcmp(tmp2->name,"xfrm")==0) {
                    x = xmlGetProp(tmp2->children,"x");
                    y = xmlGetProp(tmp2->children,"y");
            
                    cx = xmlGetProp(tmp2->children->next,"cx");
                    cy = xmlGetProp(tmp2->children->next,"cy");
            }
                tmp2 = tmp2->next;
            }
        
        }
        
        iter_element= iter_element->next;
        }
    
    if (success_factor == 99) {
        
        strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
        strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
        strcpy(Bloks[wf][global_blok_counter].relationship,embed);
        
        //printf("update: in pics_handler - %d - %s \n", global_blok_counter, Bloks[wf][global_blok_counter].formatted_text);
        
    if (x != NULL)
    { my_x = atoi(x);}
    else
    { my_x = 0;}
    
    if (y != NULL)
    { my_y = atoi(y);}
    else
    { my_y = 0;}
    
    if (cx != NULL)
    { my_cx = atoi(cx);}
    else
    { my_cx = 0;}
    
    if (cy != NULL)
    { my_cy = atoi(cy);}
    else
    { my_cy = 0;}
    
    my_sp_position.x = my_x;
    my_sp_position.y = my_y;
    my_sp_position.cx = my_cx;
    my_sp_position.cy = my_cy;
    
    Bloks[wf][global_blok_counter].position = my_sp_position;
    Bloks[wf][global_blok_counter].slide_num = slide_num;
    Bloks[wf][global_blok_counter].shape_num = shape_num;
    strcpy(Bloks[wf][global_blok_counter].content_type,"image");
    strcpy(Bloks[wf][global_blok_counter].linked_text,"");
    strcpy(Bloks[wf][global_blok_counter].table_text,"");
    strcpy(Bloks[wf][global_blok_counter].file_type,"ppt");
    }
    
    xmlMemFree(iter_element);
    xmlMemFree(tmp1);
    xmlMemFree(tmp2);
    
    return success_factor;
}


int gf_handler (xmlNode *element,int global_blok_counter, int slide_num, int shape_num, int wf)

{

    const char *xfrm = "xfrm";
    const char *tbl = "tbl";
    const char *txbody = "txBody";
    const char *p = "p";
    const char *r = "r";
    const char *grid = "gridSpan";
    
    xmlNode *iter_element = NULL;
    xmlNode *tmp1 = NULL;
    xmlNode *tmp2 = NULL;
    xmlNode *tmp3 = NULL;
    xmlNode *tmp4 = NULL;
    xmlNode *tmp5  = NULL;
    xmlNode *tmp6 = NULL;
    xmlNode *tmp7 = NULL;
    xmlNode *props = NULL;
    xmlNode *text = NULL;
    
    coords my_sp_position;
    
    char *key = NULL;
    char *doc;
    char *sz;
    char *bold;
    char *underline;
    char *italics;
    char *dummy;
    
    char *x = NULL;
    char *y = NULL;
    char *cx = NULL;
    char *cy = NULL;

    char *gridspan = NULL;
    
    int coords_set = 0;

    char core_text_out[50000];
    char table_text_out[100000];
    char special_format_text_out[50000];
    char table_cell[10000];
    
    int format_flag = 0;
    int success_factor = 1;
    int my_x = 0;
    int my_y = 0;
    int my_cx = 0;
    int my_cy = 0;
    
    int added_nonempty_cell_to_row = 0;
    
    int row = 0;
    int col = 0;
    char rc_tmp[10];
    
    strcpy(core_text_out,"");
    strcpy(table_text_out,"");
    strcpy(special_format_text_out,"");
    strcpy(rc_tmp,"");
    strcpy(table_cell,"");
    
    iter_element = element->next;
    
    //printf("update: in gf_handler ! \n");
    
    while (iter_element != NULL) {
        
        if (strcmp(iter_element->name,xfrm)==0) {
            
            tmp2 = iter_element->children;
            x = xmlGetProp(tmp2,"x");
            y = xmlGetProp(tmp2,"y");
            tmp2 = tmp2->next;
            cx = xmlGetProp(tmp2,"cx");
            cy = xmlGetProp(tmp2,"cy");
            
            
        }
        
        if (strcmp(iter_element->name,"graphic")==0) {
            tmp1 = iter_element->children->children;
            while (tmp1 != NULL) {
                
                if (strcmp(tmp1->name, tbl) ==0) {
                    tmp2 = tmp1->children;
                    while (tmp2 != NULL) {
                        
                        if (strcmp(tmp2->name,"tr")==0) {
                            tmp3 = tmp2->children;
                            
                            // found new row to process
                            added_nonempty_cell_to_row = 0; // reset & start new row
                            col = -1;
                            
                            //strcat(table_text_out," <tr> ");
                            // row ++;
                            
                            while (tmp3 !=NULL) {
                                if (strcmp(tmp3->name,"tc")==0) {
                                    
                                    col ++;
                                    
                                    //printf("update: incrementing col - %d \n", col);
                                    
                                    gridspan = xmlGetProp(tmp3,"gridSpan");
                                    
                                    if (gridspan != NULL) {
                                        //printf("update: FOUND GRIDSPAN - %s \n", gridspan);
                                    }
                                    
                                    strcpy(table_cell,"");
                                    
                                    tmp7 = tmp3->children;
                                    while (tmp7 !=NULL) {
                                        if (strcmp(tmp7->name,txbody)==0) {
                                            tmp4 = tmp7->children;
                                            
                                            // iterate through content in text cell
                                            // potentially many <p> text elements
                                            
                                            while (tmp4 != NULL) {
                                                if (strcmp(tmp4->name,p)==0) {
                                                    
                                                    if (strlen(table_cell) > 0) {
                                                        strcat(table_cell, " ");
                                                    }
                                                    
                                                    /*
                                                    if (strlen(core_text_out) > 0) {
                                                        strcat(core_text_out, " ");
                                                    }
                                                     */
                                                    
                                                    tmp5 = tmp4->children;
                                                    while (tmp5 != NULL) {
                                                        if (strcmp(tmp5->name,r)==0) {
                                                            
                                                            tmp6 = tmp5->children->next;
                                                            props = tmp5->children;
                                                            sz = xmlGetProp(props,"sz");
                                                            
                                                            bold = xmlGetProp(props,"b");
                                                            underline = xmlGetProp(props,"u");
                                                            italics = xmlGetProp(props,"i");
                                                            dummy="color_tbd";
                                                            format_flag = special_formatted_text(bold,italics,underline,sz,dummy);
                              
                                                            
                                                            while (tmp6 != NULL) {
                                                                
                                                                // check for <br> - word space break
                                                                if (strcmp(tmp6->name, "br")==0) {
                                                                    
                                                                    if (strlen(table_cell) > 0) {
                                                                        strcat(table_cell, " ");
                                                                    }
                                                                    
                                                                    /*
                                                                    if (strlen(core_text_out) > 0) {
                                                                        strcat(core_text_out," ");
                                                                    }
                                                                     */
                                                                    
                                                                }
                                                                
                                                                if (strcmp(tmp6->name,"t")==0) {
                                                                    text = tmp6->children;
                                                                    key = xmlNodeListGetString(doc,text,1);
                                                            
                                                                    if (key != NULL) {
                                                                        
                                                                        /*
                                                                        if (strlen(core_text_out) < 50000) {
                                                                            strcat(core_text_out,key);
                                                                            //strcat(core_text_out, " ");
                                                                        }
                                                                        */
                                                                        
                                                                        if (strlen(table_cell) < 10000) {
                                                                            strcat(table_cell,key);
                                                                            //strcat(table_cell," ");
                                                                        }
                                                                        
                                                                    }}
                                                                
                                                                
                                                                tmp6= tmp6->next;}}
                                                        tmp5 = tmp5->next;}}

                                                tmp4 = tmp4->next;}}
                                        
                                        // finished inner iterations inside txBody
                                        
                                        if (strlen(table_cell) > 0) {
                                            
                                            if ( (strlen(table_text_out)+strlen(table_cell) < 99900) && (strlen(core_text_out)+strlen(table_cell) < 49900) ) {
                                                
                                                if (added_nonempty_cell_to_row == 0) {
                                                    strcat(table_text_out," <tr> ");
                                                    added_nonempty_cell_to_row = 1;
                                                    row ++;
                                                }
                                                
                                                strcat(table_text_out," <th> <");
                                                strcat(core_text_out," <");
                                                
                                                if (col < 26) {
                                                    sprintf(rc_tmp,"%c",65+col);
                                                    strcat(table_text_out,rc_tmp);
                                                    strcat(core_text_out,rc_tmp);
                                                }
                                                
                                                if (col >= 26 && col < 52) {
                                                    strcat(table_text_out,"A");
                                                    strcat(core_text_out,"A");
                                                    sprintf(rc_tmp,"%c", 65+col-26);
                                                    strcat(table_text_out, rc_tmp);
                                                    strcat(core_text_out, rc_tmp);
                                                }
                                                
                                                if (col >= 52) {
                                                    strcat(table_text_out,"ZZ");
                                                    strcat(core_text_out, "ZZ");
                                                }
                                                
                                                sprintf(rc_tmp,"%d",row);
                                                strcat(table_text_out,rc_tmp);
                                                strcat(table_text_out,"> ");
                                                strcat(core_text_out,rc_tmp);
                                                strcat(core_text_out,"> ");
                                
                                                strcat(table_text_out,table_cell);
                                                strcat(table_text_out," </th>");
                                                
                                                strcat(core_text_out,table_cell);
                                                
                                                success_factor = 99;
                                                
                                                added_nonempty_cell_to_row = 1;
                                                
                                                //printf("update: writing cell - %s \n", table_cell);
                                            }
                                            
                                            strcpy(table_cell,"");
                                            
                                            //col ++;
                                            
                                            if (gridspan != NULL) {
                                                
                                                if (strcmp(gridspan,"2") == 0) {
                                                    //col ++;
                                                    //printf("update: adding to col for gridspan - %s \n", gridspan);
                                                }
                                                if (strcmp(gridspan,"3") == 0) {
                                                    //col ++;
                                                    //col ++;
                                                }
                                            }
                                            
                                        }
                                        
                                        tmp7 = tmp7->next;}}
                                    tmp3 = tmp3->next;}
                            
                            // finished all cells in the row
                            
                            if (added_nonempty_cell_to_row == 1) {
                                strcat(table_text_out," </tr> ");
                            }
                            
                        }
                        
                        tmp2 = tmp2->next;
                    }
                }
                tmp1 = tmp1->next;
            }
        }
        iter_element = iter_element->next;
    }
    
    if (strlen(core_text_out) > 0) {
    
        success_factor = 99;
        
        strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
        strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
        
        //printf("update: in gf_handler - %d - %s \n", global_blok_counter, Bloks[wf][global_blok_counter].formatted_text);
        
        
        if (coords_set == 0) {
            
            if (x != NULL)
                { my_x = atoi(x);}
            else
                { my_x = 0;}
    
            if (y != NULL)
                { my_y = atoi(y);}
            else
                { my_y = 0;}
    
            if (cx != NULL)
                { my_cx = atoi(cx);}
            else
                { my_cx = 0;}
    
            if (cy != NULL)
                { my_cy = atoi(cy);}
            else
                { my_cy = 0;}
    
            my_sp_position.x = my_x;
            // my_sp_position.y = my_y;
            my_sp_position.y = 0;
            my_sp_position.cx = my_cx;
            my_sp_position.cy = my_cy;
        }
    
        my_sp_position.cx = row;
        //my_sp_position.cy = 0;
        
        Bloks[wf][global_blok_counter].position = my_sp_position;
        Bloks[wf][global_blok_counter].slide_num = slide_num;
        Bloks[wf][global_blok_counter].shape_num = shape_num;
        strcpy(Bloks[wf][global_blok_counter].content_type,"table");
        strcpy(Bloks[wf][global_blok_counter].linked_text,"");
        strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
        strcpy(Bloks[wf][global_blok_counter].relationship,"");
        strcpy(Bloks[wf][global_blok_counter].file_type,"ppt");
        
        global_total_tables_added ++;
        
        if (strlen(table_text_out) >0 ) {
            if (debug_mode == 1) {
                printf("update: office_parser - table_text_out - %d - %s \n", strlen(table_text_out), table_text_out);
            }
        }
    }
    
    xmlMemFree(tmp1);
    xmlMemFree(tmp2);
    xmlMemFree(tmp3);
    xmlMemFree(tmp4);
    xmlMemFree(tmp5);
    xmlMemFree(tmp6);
    xmlMemFree(tmp7);
    xmlMemFree(text);
    xmlMemFree(props);
    xmlMemFree(iter_element);
    xmlMemFree(text);
    
    return success_factor;
}



int post_slide_handler (int start, int stop, int wf)
{
    
    // need to accomplish two objectives
    // 1- insert formatted_text on all slides
    // 2- run nearest_neighbor loop
    
    int i;
    int j;
    float x_center;
    float y_center;
    float x_tmp;
    float y_tmp;
    float x_distance;
    float y_distance;
    float min_distance = 400;
    float max_distance = 700;
    float total_distance;
    char all_ft [50000];
    char local_copy [100000];
    char text_near_me [500000];
    
    strcpy(all_ft,"");
    
    for (i=start; i<stop; i++) {
        if (strlen(Bloks[wf][i].formatted_text)>0) {

            strcat(all_ft,Bloks[wf][i].formatted_text);
            strcat(all_ft," ");
            
        }
    }
 
    for (i=start; i<stop; i++) {
        
        // add formatted text to all bloks on the page
        strcpy(Bloks[wf][i].formatted_text,all_ft);

        // if blok is an image, then look for nearby text to append
        
        if (strcmp(Bloks[wf][i].content_type, "image")==0) {
            
            x_center = (Bloks[wf][i].position.x + 0.5 * Bloks[wf][i].position.cx);
            y_center = (Bloks[wf][i].position.y + 0.5 * Bloks[wf][i].position.cy);
            
            strcpy(text_near_me,"");
            
            for (j=start; j<stop; j++) {
                if (i != j) {
                    x_tmp = (Bloks[wf][j].position.x + 0.5 * Bloks[wf][j].position.cx);
                    y_tmp = (Bloks[wf][j].position.y + 0.5 * Bloks[wf][j].position.cy);
                    
                    x_distance = fabs((x_center - x_tmp)) / 6096;
                    y_distance = fabs((y_center - y_tmp)) / 6096;
                    total_distance = sqrtf((x_distance*x_distance + y_distance*y_distance));
                    
                    if (total_distance < min_distance) {
                        
                        if (strcmp(Bloks[wf][j].text_run,"tbd") != 0) {
                            strcpy(local_copy,Bloks[wf][j].text_run);
                            strcat(text_near_me,local_copy);
                            strcat(text_near_me, " ");
                        }
                    }
                    else {
                        if (total_distance < max_distance) {
                            if (x_distance <50 || y_distance < 50) {
                                
                                if (Bloks[wf][j].text_run != NULL) {
                                    strcpy(local_copy,Bloks[wf][j].text_run);
                                    strcat(text_near_me,local_copy);
                                    strcat(text_near_me, " ");
                                }}}}
                }}
            // at end of comparing distances
            if (strlen(text_near_me) >0) {
                strcat(Bloks[wf][i].formatted_text,text_near_me);
            }
            else {
                strcat(Bloks[wf][i].formatted_text, "");
            }
        }
    }
    
    return 0;
}


char* rels_handler_pic_new (char *fp, char *embed, int wf, int global_blok_counter, char *ws_fp)
{
    xmlDoc *doc;
    xmlNode *root_element;
    xmlNode *iter_element;
    char *prop_id = NULL;
    char file_name[200];
    char const file_name_tmp[200];
    char *full_path_name = NULL;
    char *file_name_tmp_ptr = NULL;
    char *tokens = NULL;
    char file_type[10];
    char image_fp [200];
    int  emf_dummy = 0;
    char image_base[200];
    
    
    doc = xmlReadFile(fp,NULL,0);
    root_element = xmlDocGetRootElement(doc)->children;
    
    for (iter_element=root_element; iter_element; iter_element = iter_element->next) {
        
        prop_id = xmlGetProp(iter_element,"Id");

        if (strcmp(prop_id,embed)==0) {
            
            full_path_name = xmlGetProp(iter_element,"Target");
            strcpy(file_name,get_file_name(full_path_name));
            strcpy(file_name_tmp,file_name);
            tokens = strtok(file_name,".");
            while (tokens != NULL) {
                strcpy(file_type,tokens);
                tokens = strtok(NULL,"/");
            }
            
            if (strcmp(file_type,"emf") == 0) {
                
                // special handling for .emf files
                 
                strcpy(image_base,ws_fp);
                sprintf(image_fp,"%d/%s",wf,file_name_tmp);
                strcat(image_base,image_fp);
                
                emf_dummy = emf_handler(image_base, global_blok_counter, wf);
                
            }
            
            if (strcmp(file_type,"png") == 0) {
                
                // no special action taken
                // printf("found .png image %s \n", file_name_tmp);
                
            }}
    }
    
    
    xmlMemFree(iter_element);
    xmlMemFree(root_element);
    xmlFreeDoc(doc);
    
    file_name_tmp_ptr = &file_name_tmp;
    
    return file_name_tmp_ptr;
    
}


char* doc_rels_handler_pic_new (char *fp, char *embed, int wf, int global_blok_counter, char *ws_fp)
{
    xmlDoc *doc;
    xmlNode *root_element;
    xmlNode *iter_element;
    char *prop_id = NULL;
    char file_name[200];
    char const file_name_tmp[200];
    char *full_path_name = NULL;
    char *file_name_tmp_ptr = NULL;
    char *tokens = NULL;
    char file_type[10];
    char image_fp [200];
    int  emf_flag = 0;
    char image_base[200];
    
    strcpy(image_fp, global_image_fp);
    
    doc = xmlReadFile(fp,NULL,0);
    root_element = xmlDocGetRootElement(doc)->children;
    
    if (debug_mode == 1) {
        //printf("update: in doc_rels_handler_pic_new:  embed- %s \n", embed);
    }
    
    for (iter_element=root_element; iter_element; iter_element = iter_element->next) {
        
        prop_id = xmlGetProp(iter_element,"Id");
        
        if (debug_mode == 1) {
            //printf("iter- prop_id - %s \n", prop_id);
        }
        
        if (prop_id != NULL) {
            
            
            if (strcmp(prop_id,embed)==0) {
                
                full_path_name = xmlGetProp(iter_element,"Target");
                
                strcpy(file_name,get_file_name(full_path_name));
                strcpy(file_name_tmp,file_name);
                tokens = strtok(file_name,".");
                while (tokens != NULL) {
                    strcpy(file_type,tokens);
                    tokens = strtok(NULL,"/");
                }
                
                if (strcmp(file_type,"emf") == 0) {
                    
                    // special handling for .emf files
                    
                    strcpy(image_base,ws_fp);

                    sprintf(image_fp,"%d/",wf);
                    strcat(image_base,image_fp);
                    strcat(image_base,file_name_tmp);
                
                    
                    emf_flag = emf_handler(image_base, global_blok_counter, wf);
                    
                    if (emf_flag == 99) {
                        
                        strcpy(Bloks[wf][global_blok_counter].relationship,file_name_tmp);
                        strcpy(file_name_tmp,"EMF_FLAG");
                        
                    }
                }
                
                if (strcmp(file_type,"png") == 0) {
                    
                    // no special action taken
                    // printf("found .png image %s \n", file_name_tmp);
                    
                }}
        }}
    
    
    xmlMemFree(iter_element);
    xmlMemFree(root_element);
    xmlFreeDoc(doc);
    
    file_name_tmp_ptr = &file_name_tmp;
    
    return file_name_tmp_ptr;
    
}



int drawing_handler (xmlNode *element, int global_blok_counter, int slide_num, int shape_num, int wf) {
    
    if (debug_mode == 1) {
        printf("update: office_parser - in drawing_handler - processing image. \n");
    }
    
    // WIP - needs to be completed & updated
    
    xmlNode *outer_node = NULL;
    xmlNode *inner_node = NULL;
    xmlNode *pic_node = NULL;
    xmlNode *blip_node = NULL;

    xmlNode *start_node = NULL;

    char *embed;
    char *image_id = NULL;
    
    int success_factor = -1;
    
    coords my_sp_position;
    
    start_node = element->children;
    
    //printf("update: start_node - %s \n", start_node->name);
    
    outer_node = start_node->children;   // THIS WORKS
    
    //outer_node = start_node;
    
    while (outer_node != NULL) {
        
        if (debug_mode == 1) {
            //printf("update: drawing element - outer_node - %s \n", outer_node->name);
        }
        
        if (strcmp(outer_node->name,"graphic")==0) {
            
            //printf("update: found graphic ! \n");
            
            pic_node = outer_node->children->children;
            
            inner_node = pic_node->children;
            
            while (inner_node != NULL) {
                
                //printf("update: inner_node - %s \n", inner_node->name);

                if (strcmp(inner_node->name,"blipFill")==0) {
                    
                    //printf("update: found blipFill ! \n");
                    
                    blip_node = inner_node->children;
                    embed = xmlGetProp(blip_node,"embed");
                    
                    //printf("embed found - %s \n", embed);
                    
                    image_id = doc_rels_handler_pic_new(doc_rels_fp,embed,wf,global_blok_counter,global_workspace_fp);
                    
                    if (debug_mode == 1) {
                        //printf("update:  in drawing_handler- matched image_id = %s \n", image_id);
                    }
                }
                
                
                inner_node = inner_node->next;
                
            }}
            outer_node = outer_node->next;
        }
    
    if (image_id != NULL) {
        
        // save image blok here
        
        if (strcmp(image_id,"EMF_FLAG")==0) {
            
            if (debug_mode == 1) {
                printf("update: office_parser - drawing_handler - received emf_flag. \n");
            }
            
            // already processed and set 'table' content type, core_text, table_text & relationship
            
            //strcpy(Bloks[wf][global_blok_counter].text_run, "");
            //strcpy(Bloks[wf][global_blok_counter].table_text,"");
            //strcpy(Bloks[wf][global_blok_counter].content_type,"image");
            
            // already set relationship to emf image
            //strcpy(Bloks[wf][global_blok_counter].relationship,image_id);
            
            strcpy(Bloks[wf][global_blok_counter].formatted_text, "");
            
            // already set cx = # of rows
            Bloks[wf][global_blok_counter].position.x = 0;
            Bloks[wf][global_blok_counter].position.y = 0;
            Bloks[wf][global_blok_counter].position.cy = 0;
            
            Bloks[wf][global_blok_counter].slide_num = slide_num;
            Bloks[wf][global_blok_counter].shape_num = shape_num;
            
            strcpy(Bloks[wf][global_blok_counter].linked_text,"");
            
            strcpy(Bloks[wf][global_blok_counter].file_type,"doc");
            
            success_factor = 99;

        }
        
        else {
            
            // general case - not EMF - save as image
            
            strcpy(Bloks[wf][global_blok_counter].text_run, "");
            strcpy(Bloks[wf][global_blok_counter].formatted_text, "");
            strcpy(Bloks[wf][global_blok_counter].relationship,image_id);
            
            my_sp_position.x = 0;
            my_sp_position.y = 0;
            my_sp_position.cx = 0;
            my_sp_position.cy = 0;
            
            Bloks[wf][global_blok_counter].position = my_sp_position;
            Bloks[wf][global_blok_counter].slide_num = slide_num;
            Bloks[wf][global_blok_counter].shape_num = shape_num;
            strcpy(Bloks[wf][global_blok_counter].content_type,"image");
            strcpy(Bloks[wf][global_blok_counter].linked_text,"");
            strcpy(Bloks[wf][global_blok_counter].table_text,"");
            strcpy(Bloks[wf][global_blok_counter].file_type,"doc");
            
            success_factor = 99;
            
        }
    }
    
    xmlMemFree(outer_node);
    xmlMemFree(inner_node);
    xmlMemFree(pic_node);
    xmlMemFree(blip_node);
    xmlMemFree(start_node);
    
    
    return success_factor;
}



int xl_shared_strings_handler (int wf, char *ws_fp)
{
    
    xmlNode *root_element = NULL;
    xmlNode *iter_element = NULL;
    xmlNode *text_node = NULL;
    xmlDoc *doc = NULL;
    int MAX_SS_SIZE = 400000;
    int SS_FIELD_SIZE = 150;
    int x = 0;
    int y = 0;
    char *key = NULL;
    char ss_fp[500];
    char ss_tmp[200];
    char ss_max[SS_FIELD_SIZE];
    char tmp[10];
    
    int max_size_reached = -1;
    
    strcpy(ss_fp,ws_fp);
    sprintf(ss_tmp,"%d/sharedStrings.xml",wf);
    strcat(ss_fp,ss_tmp);
    
    doc = xmlReadFile(ss_fp,NULL,0);
    
    if (doc == NULL) {
        
        if (debug_mode == 1) {
            
            printf ("warning: xl parsing - unusual - no shared strings file found - will try to recover, but may not be able to parse xl file.");
        }
        
        x = -99;
    }
    
    else {
    root_element = xmlDocGetRootElement(doc);
    iter_element = root_element->children;
    
    while (iter_element != NULL) {
        
        if (max_size_reached > 0) {
            
            if (debug_mode == 1) {
                printf("update: office_parser - xl parsing - max shared string size reached - stopping processing. \n");
            }
            
            break;
        }
        
        // note: helpful for debugging: printf ("iter ss element-%d-%s \n",x,iter_element->name);
        
        if (strcmp(iter_element->name,"si")==0) {
            
            // get text + put in shared_strings[]
            
            text_node = iter_element->children;
            
            while (text_node != NULL) {
                if (strcmp(text_node->name,"t")==0) {
                    
                    //inside <t> - this is where the text can be found
                    
                    key = xmlNodeListGetString(doc,text_node->children,1);
                    
                    //useful for debugging:  printf("found ss key-%d-%s \n",x,key);
                    
                    if (key == NULL) {
                        //useful for debugging:  printf("found null key!");
                        
                        strcpy(shared_strings[x],"");}
                    else
                    {
                        // safety check if key found is greater than MAX FIELD SIZE
                        
                        if (strlen(key) < SS_FIELD_SIZE) {
                            strcpy(shared_strings[x],key);
                        }
                        else {
                            strcpy(ss_max,"");
                            for (y=0; y < SS_FIELD_SIZE; y++) {
                                sprintf(tmp,"%c",key[y]);
                                strcat(ss_max,tmp);
                            }
                            key = ss_max;
                        }
                    }
                    
                    x++;
                    if (x > MAX_SS_SIZE) {
                        
                        if (debug_mode == 1) {
                            printf("update:  office_parser - xl parsing - passed size limit in shared string handler = %d ! \n", MAX_SS_SIZE);
                        }
                        
                        max_size_reached = 1;
                        break;
                    }
                    }
                text_node = text_node->next;
            }
        }
        iter_element = iter_element->next;
    }
    }
    
    // returns the length of the shared string array
    
    xmlMemFree(iter_element);
    xmlMemFree(root_element);
    xmlMemFree(text_node);

    xmlFreeDoc(doc);
    
    return x;
    
}


int xl_style_handler (int wf) {
  
    xmlNode *root_element = NULL;
    xmlNode *iter_element = NULL;
    //xmlNode *text_node = NULL;
    xmlDoc *doc = NULL;
    xmlNode *font_node = NULL;
    xmlNode *font2 = NULL;
    
    xmlNode *filler_node = NULL;
    xmlNode *filler2 = NULL;
    
    xmlNode *border_node = NULL;
    xmlNode *border2 = NULL;
    
    xmlNode *xfs_node = NULL;

    char *left_tmp = NULL;
    char *right_tmp = NULL;
    char *top_tmp = NULL;
    char *bottom_tmp = NULL;
    char *diagonal_tmp = NULL;
    char *font_id = NULL;
    char *fill_id = NULL;
    char *border_id = NULL;
    
    char style_fp[200];
    char tmp_fp[200];
    
    char fonts[100][100];
    char fills[100][100];
    char borders[500][500];
    int font_counter =0;
    int fills_counter = 0;
    int borders_counter = 0;
    char tmp[500];
    int f1,f2,b1 = 0;
    
    strcpy(style_fp,global_workspace_fp);
    sprintf(tmp_fp,"%d/styles.xml",wf);
    strcat(style_fp,tmp_fp);
    
    
    doc = xmlReadFile(style_fp,NULL,0);
    root_element = xmlDocGetRootElement(doc);
    iter_element = root_element->children;
    
    while (iter_element != NULL) {
        
        if (debug_mode == 1) {
            //printf ("update:  xl styles - iter ss element-%s \n",iter_element->name);
        }
        
        if (strcmp(iter_element->name,"fonts")==0) {
            font_node = iter_element->children;
            while (font_node != NULL) {
                
                font2 = font_node->children;
                while (font2 != NULL) {
                    
                    if (strcmp(font2->name,"sz")==0) {
                        
                        if (xmlGetProp(font_node->children,"val") != NULL) {
                            strcpy(fonts[font_counter],xmlGetProp(font_node->children,"val"));
                        }
                        else { strcpy(fonts[font_counter], "11"); }
                        
                        //useful for debugging:  printf("sz-%s-%d \n", fonts[font_counter],font_counter);
                        
                        font_counter ++; }
                    
                    font2 = font2->next;}
                font_node = font_node->next;}
        }
        
        if (strcmp(iter_element->name,"fills")==0) {
            
            filler_node = iter_element->children;
            
            while (filler_node != NULL) {
                
                filler2 = filler_node->children;
                
                while (filler2 != NULL) {
                    if (strcmp(filler2->name,"patternFill")==0) {
                        
                        strcpy(fills[fills_counter],xmlGetProp(filler2,"patternType"));
                        
                        //printf("pattern-%s-%d \n",xmlGetProp(filler2,"patternType"),fills_counter);
                        
                        fills_counter ++;}
                    filler2 = filler2->next;}
                
                filler_node = filler_node->next;}
        }
        
        if (strcmp(iter_element->name, "borders")==0) {
            
            border_node = iter_element->children;
            
            while (border_node != NULL) {
                
                strcpy(tmp,"");
                
                border2 = border_node->children;
                while (border2 != NULL) {
                    if (strcmp(border2->name,"left")==0) {
                        
                        //found left
                        left_tmp = xmlGetProp(border2,"style");
                        if (left_tmp != NULL) {
                            //printf ("found left tmp-%s \n",left_tmp);
                            strcat(tmp,"left-");
                        }
                    }
                    
                    if (strcmp(border2->name,"right")==0) {
                        
                        //found right
                        right_tmp = xmlGetProp(border2,"style");
                        if (right_tmp != NULL) {
                            //printf ("found right tmp-%s \n", right_tmp);
                            strcat(tmp,"right-");
                        }
                    }
                    
                    if (strcmp(border2->name,"top")==0) {
                        
                        //found top
                        top_tmp = xmlGetProp(border2, "style");
                        if (top_tmp != NULL) {
                            //printf ("found top tmp-%s \n", top_tmp);
                            strcat(tmp,"top-");
                        }
                    }
                    
                    if (strcmp(border2->name,"bottom")==0) {
                        
                        //found bottom
                        bottom_tmp = xmlGetProp (border2, "style");
                        if (bottom_tmp != NULL) {
                            //printf ("found bottom tmp-%s \n", bottom_tmp);
                            strcat(tmp,"bottom-");
                        }
                    }
                    if (strcmp(border2->name,"diagonal")==0) {
                        
                        //found diagonal
                        diagonal_tmp = xmlGetProp (border2, "style");
                        if (diagonal_tmp != NULL) {
                            //printf ("found diagonal tmp-%s \n", diagonal_tmp);
                            strcat(tmp,"diagonal-");
                        }
                    }
                    if (tmp != NULL) {
                        strcpy(borders[borders_counter],tmp);}
                    else {
                        strcpy(borders[borders_counter],"");}
                    
                    //usefulf for debugging:   printf("borders-out-%s-%d \n", borders[borders_counter],borders_counter);
                    
                    borders_counter ++;
                    border2 = border2->next;
                }
                border_node = border_node->next;
            }
        }
        
        if (strcmp(iter_element->name,"cellXfs")==0) {
            
            xfs_node = iter_element->children;
            
            while (xfs_node != NULL) {
                
                //useful for debugging:  printf ("cell Xfs iter-%s \n", xfs_node->name);
                
                font_id = xmlGetProp(xfs_node,"fontId");
                fill_id = xmlGetProp(xfs_node,"fillId");
                border_id = xmlGetProp(xfs_node,"borderId");
                
                if (debug_mode == 1) {
                    //printf ("update:  cell Xfs font-%s-fill-%s-border-%s \n",font_id,fill_id,border_id);
                }
                
                f1 = atoi(font_id);
                f2 = atoi(fill_id);
                b1 = atoi(border_id);
                
                if (debug_mode == 1) {
                    //printf ("update: styles lookup - %s-%s-%s \n",fonts[f1],fills[f2],borders[b1]);
                }
                
                xfs_node = xfs_node->next;
        }
        }
        iter_element = iter_element->next;
    }
    
    xmlMemFree(iter_element);
    xmlMemFree(root_element);
    xmlFreeDoc(doc);
    
    return 0;
    
}



// new xl_build_index -> branched on Oct 11, 2022
// expanding support for preserving table structure and larger sheets

int xl_build_index (int working_folder, int local_sheet_count, char *ws_fp) {

    int i = 0;
    char sheet_fp[500];
    char sheet_fp_tmp[500];
    
    xmlNode *cur_node = NULL;
    int wf;
    
    int sheet_start;
    int sheet_stop;
    int vv=0;
    int k=0;
    int ss=0;
    int row_counter = 0;
    int row_tmp_counter = 0;
    
    int batch_counter = 0;
    char *r_value;
    char *s_value;
    char *t_value;
    char *v = NULL;
    
    coords my_sp_position;
    char core_text_out[50000];
    int CORE_MAX_SIZE = 50000;
    char special_format_text_out[50000];
    
    // works stable at 100000
    char table_text_out[100000];
    int CORE_TABLE_SIZE = 100000;
    
    xmlNode *root_element = NULL;
    xmlNode *sptree_node = NULL;
    xmlNode *iter_node = NULL;
    xmlNode *iter2 = NULL;
    xmlNode *iter3 = NULL;
    xmlNode *iter4 = NULL;
    xmlDoc *doc = NULL;

    int bloks_created = 0;
    int global_blok_counter = 0;
    int column_counter = 0;
    int MAX_COLS = 50;
    
    int MAX_ROWS_SAVE_NUMBERS = 500;
    int ROWS_IN_BATCH = 100;
    int stop_flag_reached = 0;
    
    char tmp_str[49000];
    char tmp_col[10];
    int shared_string_error_flag = 1;
    int dummy_styles = 0;
    int dummy = 0;
    
    float dummy_float = 0;
    
    wf = working_folder;
    
    // Step 0 - get core metadata
    
    dummy = pptx_meta_handler(working_folder, ws_fp);
        
    // end - Step 0
    
    // Step 1 - get sharedStrings
    
    if (debug_mode == 1) {
        //printf("update:  going into shared strings handler \n");
    }
    
    ss = xl_shared_strings_handler(working_folder, ws_fp);
    
    if (debug_mode == 1) {
        //printf("update: completed shared strings-%d \n", ss);
    }
    
    shared_string_error_flag = ss;
    
    // End - get shared strings
    
    
    // Prepare XL Style Sheet
    // note:  not activated right now
    
    if (debug_mode == 1) {
        //printf("update: calling on xl_style_handler \n");
    }
    
    //dummy_styles = xl_style_handler(wf);
    
    if (debug_mode == 1) {
        //printf("update: survived xl_style_handler \n");
    }
    
    // End - XL Style Sheet
    
    // Main Loop - Build Index - Cycles through all of the Slides
    

    for (i=1; i <local_sheet_count+1; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - starting new sheet - %d - %d \n", i, local_sheet_count);
        }
        
        strcpy(sheet_fp,ws_fp);
        sprintf(sheet_fp_tmp,"%d/sheet%d.xml",working_folder, i);
        strcat(sheet_fp,sheet_fp_tmp);
        
        // Inner Loop - parses every sheet
        // Every worksheet (root) -> sheetData -> row
        // All of the content is inside <row> -> <c>
        // ***for worksheets, sheetData is one layer down from <worksheet> root***
        
        doc = xmlReadFile(sheet_fp,NULL,0);
        root_element = xmlDocGetRootElement(doc);
        sptree_node = root_element->children;
        
        // start tracking first blok @ start of slide
        sheet_start = bloks_created;
        
        if (debug_mode == 1) {
            printf ("update: office_parser - processing XL sheet-%d \n", i);
        }
        
        strcpy(core_text_out,"");
        strcpy(table_text_out,"");
        strcpy(special_format_text_out,"");
        strcpy(tmp_str,"");
        
        row_counter = 0;
        row_tmp_counter = 0;
        
        if (stop_flag_reached > 0) {
            break;
        }
        
        for (cur_node = sptree_node; cur_node; cur_node=cur_node->next) {
            
            if (stop_flag_reached > 0) {
                break;
            }
            
            if (strcmp(cur_node->name,"sheetData") == 0) {
                iter_node = cur_node->children;
                
                while (iter_node != NULL) {
                    if (strcmp(iter_node->name,"row") == 0) {
                        iter2 = iter_node->children;
                        
                        row_counter ++;
                        row_tmp_counter ++;
                        
                        if (debug_mode == 1) {
                            //printf("iter loop - %s \n", iter_node->name);
                        }
                        
                        if (strlen(table_text_out) < (CORE_TABLE_SIZE - 50)) {
                            strcat(table_text_out," <tr> ");
                        }
                        column_counter = 0;
                        
                        while(iter2 != NULL) {
                            if (strcmp(iter2->name, "c")==0) {
                                
                                // found column
                                
                                column_counter ++;
                                
                                r_value = xmlGetProp(iter2,"r");
                                s_value = xmlGetProp(iter2,"s");
                                t_value = xmlGetProp(iter2,"t");
                                
                                if (r_value != NULL) {
                                    strcpy(tmp_col,r_value);
                                    
                                }
                                else {
                                    
                                    // unusual case - no cell value provided
                                    
                                    strcpy(tmp_col, "ZZZ999");
                                    strcpy(r_value, "ZZZ999");
                                }
                                
                                //useful for debugging:  printf("r-%s-s-%s-t-%s \n", r_value,s_value,t_value);
                                
                                iter3 = iter2->children;
                                vv = 0;
                                
                                
                                while (iter3 != NULL) {
                                    
                                    // unusual case -> check if "inline string" = "is"
                                    
                                    if (strcmp(iter3->name,"is")==0){
                                        
                                        iter4 = iter3->children;
                                        
                                        while (iter4 != NULL) {
                                            
                                            if (strcmp(iter4->name,"t")==0) {
      
                                                strcpy(tmp_str,xmlNodeListGetString(doc,iter4->children,1));
                                                       
                                                if ((strlen(core_text_out)+strlen(tmp_str)) < (CORE_MAX_SIZE - 20)) {
                                                    
                                                    strcat(core_text_out,tmp_str);
                                                    strcat(core_text_out," ");
                                                }
                                                
                                                if ((strlen(table_text_out) + strlen(tmp_str) + 100) < CORE_TABLE_SIZE) {
                                                    
                                                    // useful for debugging:  printf("adding more - %d-%d \n",strlen(table_text_out),strlen(tmp_str));
                                                    
                                                    if (column_counter < MAX_COLS) {
                                                        
                                                        //printf("r_value - %s \n", r_value);
                                                        
                                                        strcat(table_text_out, " <th> <");
                                                        strcat(table_text_out, r_value);
                                                        strcat(table_text_out, "> ");
                                                        strcat(table_text_out, tmp_str);
                                                        strcat(table_text_out, " </th> ");
                                                    }
                                                }
                                                strcpy(tmp_str,"");

                                                
                                            }
                                            
                                            iter4 = iter4->next;
                                        }

                                    }
                                    
                                    // main case - found "value" = "v"
                                    
                                    if (strcmp(iter3->name, "v")==0){
                                        v = xmlNodeListGetString(doc,iter3->children,1);
                                        
                                        if (v == NULL) {
                                            //no special action
                                            //useful for debugging:  printf("v is NULL! \n");
                                        }
                                        else {
                                            vv = atoi(v);
                                        }
                                        
                                        if (t_value != NULL) {
                                            if (strcmp(t_value,"s") == 0) {
                                                
                                                //useful for debugging: printf ("ss lookup-%s-%s-%s-%s \n", r_value,s_value,t_value,v);
                                                
                                                if (shared_string_error_flag > 0) {
                                                    
                                                    // shared_string_error_flag == -99 if not found
                                                    
                                                    if (vv <= shared_string_error_flag) {
                                                        
                                                        // confirm that lookup entry in scope
                                                        
                                                        if (debug_mode == 1) {
                                                            
                                                            //printf ("update:  ss lookup-%s-%s \n",v,shared_strings[vv]);
                                                        }
                                                        
                                                        strcpy(tmp_str,shared_strings[vv]);
                                                    }
                                                }
                                            }
                                            

                                        }
                                        
                                        else {
                                            
                                            if (debug_mode == 1) {
                                                //printf ("update:  value direct-%s-%s \n", r_value,v);
                                            }
                                            
                                            strcpy(tmp_str,v);
                                        }
                                        
                                        // have value of cell
                                        k = keep_value(tmp_str);
                                        
                                        if (debug_mode == 1) {
                                            //printf("update:  keep value-%d-%s \n", k,tmp_str);
                                        }
                                        
                                        // if keep_value is negative, then do not save in core_text
                                        
                                        if (k > 0) {
                                            
                                            if ((strlen(core_text_out)+strlen(tmp_str)) < (CORE_MAX_SIZE - 20)) {
                                                
                                                if (debug_mode == 1) {
                                                    //printf("update:  core text out -%d \n",strlen(core_text_out));
                                                }
                                                
                                                // experiment - insert <G6> coords into search index
                                                strcat(core_text_out, " <");
                                                strcat(core_text_out, r_value);
                                                strcat(core_text_out, "> ");
                                                // end - experiment
                                                
                                                strcat(core_text_out,tmp_str);
                                                strcat(core_text_out," ");
                                            }
                                            
                                            if ((strlen(table_text_out) + strlen(tmp_str) + 100) < CORE_TABLE_SIZE) {
                                                
                                                // useful for debugging:  printf("adding more - %d-%d \n",strlen(table_text_out),strlen(tmp_str));
                                                
                                                if (column_counter < MAX_COLS) {
                                                    
                                                    
                                                    strcat(table_text_out, " <th> <");
                                                    strcat(table_text_out, r_value);
                                                    strcat(table_text_out, "> ");
                                                    strcat(table_text_out, tmp_str);
                                                    strcat(table_text_out, " </th> ");
                                                }
                                            }
                                            else {
                                                if (debug_mode == 1) {
                                                    printf("update: office_parser - big table - %d-%d \n", strlen(table_text_out), row_counter);
                                                }
                                            }
                                            
                                            strcpy(tmp_str,"");
                                            
                                        }
                                        else {
                                            
                                            // if keep_value is negative, still save in table out
                                            
                                            // experimenting with cut-off/rounding of long floats
                                            
                                            dummy_float = atof(tmp_str);
                                            sprintf(tmp_str,"%.2f",dummy_float);
                                            
                                            // end - experiment
                                            
                                            
                                            if ((row_counter < MAX_ROWS_SAVE_NUMBERS) && (strlen(table_text_out)+strlen(tmp_str) + 100) < (CORE_TABLE_SIZE)) {
                                                
                                                if (column_counter < MAX_COLS) {
                                                    
                                                    strcat(table_text_out, " <th> <");
                                                    strcat(table_text_out, r_value);
                                                    strcat(table_text_out, "> ");
                                                    strcat(table_text_out, tmp_str);
                                                    strcat(table_text_out, " </th> ");
                                                }
                                            }
                                            
                                            strcpy(tmp_str,"");
                                        }
                                        
                                    }
                                    
                                    iter3 = iter3->next;
                                    
                                }
                                
                                iter2= iter2->next;}}
                        
                        iter_node = iter_node->next;
                        
                        if (strlen(table_text_out) < (CORE_TABLE_SIZE - 25)) {
                            
                            strcat(table_text_out, " </tr> ");
                        }
                        
                        // start new batch
                        
                        if (row_tmp_counter > ROWS_IN_BATCH) {
                            
                            if (debug_mode == 1) {
                                printf("update: office_parser - rows > ROWS_IN_BATCH - starting new batch - %d  \n", batch_counter);
                            }
                            
                            if (debug_mode == 1) {
                                
                                printf ("update: office_parser - finished batch -%d-%d \n", i,row_counter);
                                //printf ("update: core text out-%d \n", strlen(core_text_out));
                                //printf ("update: table text out-%d \n", strlen(table_text_out));
                            }
                            
                            
                            if (global_blok_counter < GLOBAL_MAX_BLOKS) {
                                
                                
                                strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
                                
                                
                                strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
                                my_sp_position.x = i;
                                my_sp_position.y = batch_counter;
                                my_sp_position.cx = row_counter;
                                my_sp_position.cy = 0;
                                
                                Bloks[wf][global_blok_counter].position = my_sp_position;
                                Bloks[wf][global_blok_counter].slide_num = 0;
                                Bloks[wf][global_blok_counter].shape_num = batch_counter;
                                strcpy(Bloks[wf][global_blok_counter].content_type,"table");
                                strcpy(Bloks[wf][global_blok_counter].linked_text,"");
                                strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
                                strcpy(Bloks[wf][global_blok_counter].relationship,"");
                                strcpy(Bloks[wf][global_blok_counter].file_type,"xl");
                                global_blok_counter ++;
                                bloks_created ++;
                                
                                global_total_tables_added ++;
                                
                                batch_counter ++;
                                
                                // increment global page counter with XL linked to Batch = 100 ROWS
                                global_total_pages_added ++;
                                
                                strcpy(core_text_out,"");
                                strcpy(table_text_out,"");
                                strcpy(special_format_text_out,"");
                                row_tmp_counter = 0;
                                //row_counter = 0;
                                
                            }
                            
                            else {
                                
                                printf("warning: office_parser - XL sheet parsing - MAX BLOKS REACHED - breaking here - some additional content in the sheet will be not saved ! \n");
                                
                                stop_flag_reached = 1;
                                break;
                            }
                            
                        }
                        
                        //printf("update: finished row - %d \n", row_counter);
                        
                    }
                    
                }
                // close the sheetData inner loop
            }
            // close the loop for sheet
        }
        
        // finished processing sheet <i> here
        // slide_num == sheet_num & shape_num == batch_counter
        
        if (debug_mode == 1) {
            printf ("update: office_parser - finished sheet-%d-%d \n", i,row_counter);
            printf ("update: office_parser - core text out-%d \n", strlen(core_text_out));
            printf ("update: office_parser - table text out-%d \n", strlen(table_text_out));
        }
        
        if (global_blok_counter < GLOBAL_MAX_BLOKS) {
            
            if (strlen(core_text_out) > 0 && strlen(table_text_out) > 0) {
                
                strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
            
                strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
                my_sp_position.x = i;
                my_sp_position.y = batch_counter;
                my_sp_position.cx = row_counter;
                my_sp_position.cy = 0;
            
                Bloks[wf][global_blok_counter].position = my_sp_position;
                Bloks[wf][global_blok_counter].slide_num = 0;
                Bloks[wf][global_blok_counter].shape_num = batch_counter;
                strcpy(Bloks[wf][global_blok_counter].content_type,"table");
                strcpy(Bloks[wf][global_blok_counter].linked_text,"");
                strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
                strcpy(Bloks[wf][global_blok_counter].relationship,"");
                strcpy(Bloks[wf][global_blok_counter].file_type,"xl");
                global_blok_counter ++;
                bloks_created ++;
                
                global_total_tables_added ++;
                
                global_total_pages_added ++;
            
                if (debug_mode == 1) {
                    printf("update: office_parser - completed update of blocks successfully-%d \n", global_blok_counter);
                }
            
            }
            
            
            // finished saving sheet here
            
            xmlMemFree(iter_node);
            xmlMemFree(root_element);
            xmlMemFree(sptree_node);
            xmlMemFree(iter2);
            xmlMemFree(iter3);
            
            xmlMemFree(iter4);
            xmlMemFree(cur_node);
            
            xmlFreeDoc(doc);
            
            // experiment
            xmlCleanupMemory();
            
            
        }
        
        else {
            if (debug_mode == 1) {
                printf("update: office_parser - reached XL Sheet end - Max blocks reached - stopping here. \n");
            }
            
            xmlMemFree(iter_node);
            xmlMemFree(root_element);
            xmlMemFree(sptree_node);
            xmlMemFree(iter2);
            xmlMemFree(iter3);
            
            xmlMemFree(iter4);
            xmlMemFree(cur_node);
            
            xmlFreeDoc(doc);
            
            // experiment
            xmlCleanupMemory();
            
            stop_flag_reached = 1;
            break;
        }
    }
    
    if (debug_mode == 1) {
        printf("update: office_parser - finished all sheets- %d \n", batch_counter);
    }
   
        sheet_stop = bloks_created-1;
        
        // note:  does not apply post_slide handling currently!
        //entry_dummy = post_slide_handler(sheet_start, sheet_stop);
        
    if (debug_mode == 1) {

        printf ("update: office_parser- new blocks created & counter: %d \n", bloks_created);
    }
    
        // need to return >0 or won't save to DB
    
    return global_blok_counter;
}



int xl_build_index_old_works (int working_folder, int local_sheet_count, char *ws_fp) {

    int i = 0;
    char sheet_fp[500];
    char sheet_fp_tmp[500];
    char sheet_rels_fp[500];
    char sheet_rels_fp_tmp[500];
    
    xmlNode *cur_node = NULL;
    int wf;
    
    //const char *gf = "graphicFrame";
    //const char *grpsp = "grpSp";
    //int shape_counter = 0;
    //int new_bloks_created = 0;
    //int entry_dummy = 0;
    
    
    int sheet_start;
    int sheet_stop;
    int vv=0;
    int k=0;
    int ss=0;
    int row_counter = 0;
    //int MAX_ROWS = 100;
    int batch_counter = 0;
    char *r_value;
    char *s_value;
    char *t_value;
    char *v = NULL;
    
    coords my_sp_position;
    char core_text_out[50000];
    int CORE_MAX_SIZE = 50000;
    char special_format_text_out[50000];
    char table_text_out[100000];
    int CORE_TABLE_SIZE = 100000;
    
    xmlNode *root_element = NULL;
    xmlNode *sptree_node = NULL;
    //xmlNode *element_node = NULL;
    xmlNode *iter_node = NULL;
    xmlNode *iter2 = NULL;
    xmlNode *iter3 = NULL;
    
    xmlDoc *doc = NULL;

    int bloks_created = 0;
    int global_blok_counter = 0;
    int column_counter = 0;
    int MAX_COLS = 50;
    char tmp_str[49000];
    char tmp_col[10];
    int shared_string_error_flag = 1;
    
    wf = working_folder;
    
    // Step 1 - get sharedStrings
    
    if (debug_mode == 1) {
        printf("update:  office_parser - going into xl shared strings handler \n");
    }
    
    ss = xl_shared_strings_handler(working_folder, ws_fp);
    
    if (debug_mode == 1) {
        printf("update: office_parser - completed xl shared strings-%d \n", ss);
    }
    
    shared_string_error_flag = ss;
    
    // Main Loop - Build Index - Cycles through all of the Slides
    

    for (i=1; i <local_sheet_count+1; i++) {
        
        strcpy(sheet_fp,ws_fp);
        sprintf(sheet_fp_tmp,"%d/sheet%d.xml",working_folder, i);
        strcat(sheet_fp,sheet_fp_tmp);
        
        strcpy(sheet_rels_fp,ws_fp);
        sprintf(sheet_rels_fp_tmp,"%d/sheet%d.xml.rels",working_folder, i);
        strcat (sheet_rels_fp,sheet_rels_fp_tmp);
        
        // Inner Loop - parses every sheet
        // Every worksheet (root) -> sheetData -> row
        // All of the content is inside <row> -> <c>
        // ***for worksheets, sheetData is one layer down from <worksheet> root***

        doc = xmlReadFile(sheet_fp,NULL,0);
        root_element = xmlDocGetRootElement(doc);
        sptree_node = root_element->children;
        
        // start tracking first blok @ start of slide
        sheet_start = bloks_created;
        
        if (debug_mode == 1) {
            printf ("update:  office_parser - processing sheet-%d \n", i);
        }
        
        strcpy(core_text_out,"");
        strcpy(table_text_out,"");
        strcpy(special_format_text_out,"");
        row_counter = 0;
        
        for (cur_node = sptree_node; cur_node; cur_node=cur_node->next) {
            
            if (strcmp(cur_node->name,"sheetData") == 0) {
                iter_node = cur_node->children;
                
                while (iter_node != NULL) {
                    if (strcmp(iter_node->name,"row") == 0) {
                        iter2 = iter_node->children;
                        row_counter ++;
                        
                        //useful for debugging:  printf("found row-%d \n", row_counter);
                        
                        if (strlen(table_text_out) < (CORE_TABLE_SIZE - 50)) {
                            strcat(table_text_out," <tr> ");
                        }
                        column_counter = 0;
                        
                        while(iter2 != NULL) {
                            if (strcmp(iter2->name, "c")==0) {
                                
                                // found column
                                // useful for debugging:  printf("found c! \n");
                                
                                column_counter ++;
                                if (column_counter > MAX_COLS) {
                                    
                                    //potential error - printf("max cols reached on this row-%d-%d //\n",row_counter,column_counter);
                                    
                                    break;
                                }
                                
                                r_value = xmlGetProp(iter2,"r");
                                s_value = xmlGetProp(iter2,"s");
                                t_value = xmlGetProp(iter2,"t");
                                if (r_value != NULL) {
                                    strcpy(tmp_col,r_value);
                                    
                                }
                                else {
                                    strcpy(tmp_col, "Z9");
                                    strcpy(r_value, "Z9");
                                }
                                
                                //useful for debugging:  printf("r-%s-s-%s-t-%s \n", r_value,s_value,t_value);
                                
                                iter3 = iter2->children;
                                vv = 0;
                                
                                while (iter3 != NULL) {
                                    
                                    if (strcmp(iter3->name, "v")==0){
                                        v = xmlNodeListGetString(doc,iter3->children,1);
                                        if (v == NULL) {
                                            //no special action
                                            //useful for debugging:  printf("v is NULL! \n");
                                        }
                                        else {
                                            vv = atoi(v);
                                        }
                                    
                                        
                                        if (t_value != NULL) {
                                            if (strcmp(t_value,"s") == 0) {
                                                
                                                //useful for debugging: printf ("ss lookup-%s-%s-%s-%s \n", r_value,s_value,t_value,v);
                                                
                                                if (shared_string_error_flag > 0) {
                                                    
                                                    // shared_string_error_flag == -99 if not found
                                                    
                                                    if (vv <= shared_string_error_flag) {
                                                        
                                                        // confirm that lookup entry in scope
                                                        
                                                        if (debug_mode == 1) {
                                                        
                                                            printf ("update: office_parser- shared string lookup-%s-%s \n",v,shared_strings[vv]);
                                                        }
                                                        
                                                        strcpy(tmp_str,shared_strings[vv]);
                                                        }
                                                }
                                            }}
                                    
                                        else {
                                            
                                            if (debug_mode == 1) {
                                                //printf ("update:  value direct-%s-%s \n", r_value,v);
                                            }
                                            
                                            strcpy(tmp_str,v);
                                        }
                                        
                                        // have value of cell
                                        k = keep_value(tmp_str);
                                        
                                        if (debug_mode == 1) {
                                            //printf("update:  keep value-%d-%s \n", k,tmp_str);
                                        }
                                        
                                        if (k > 0) {
                                            
                                            if ((strlen(core_text_out)+strlen(tmp_str)) < (CORE_MAX_SIZE - 20)) {
                                                
                                                if (debug_mode == 1) {
                                                    //printf("update:  core text out -%d \n",strlen(core_text_out));
                                                }
                                                
                                                strcat(core_text_out,tmp_str);
                                                strcat(core_text_out," ");
                                            }
                                            
                                            if ((strlen(table_text_out) + strlen(tmp_str) + 30) < CORE_TABLE_SIZE) {
                                                
                                                // useful for debugging:  printf("adding more - %d-%d \n",strlen(table_text_out),strlen(tmp_str));
                                                
                                                strcat(table_text_out, " <th> <");
                                                strcat(table_text_out, r_value);
                                                strcat(table_text_out, "> ");
                                                strcat(table_text_out, tmp_str);
                                                strcat(table_text_out, " </th> ");
                                            }
                                        }
                                        else {
                                            // useful for debugging:   printf("down -1 path!");
                                            
                                            if ((row_counter < 10) && (strlen(table_text_out)+strlen(tmp_str)) < (CORE_TABLE_SIZE + 30)) {
                                                strcat(table_text_out, " <th> ");
                                                strcat(table_text_out, tmp_str);
                                                strcat(table_text_out, " </th> ");
                                            }
                                        }
                                        
                                        }
                                    iter3 = iter3->next;}
                                
                                iter2= iter2->next;}}
                        iter_node = iter_node->next;
                        if (strlen(table_text_out) < (CORE_TABLE_SIZE - 25)) {
                            strcat(table_text_out, " </tr> ");
                        }
                        
                        // important:   spreadsheets can be extremely large
                        //      --stops after 50 rows
                        //      --could be adjusted to much larger over time
                        
                        if (row_counter > 50) {
                            
                            // NOTE:  DEPRECATED - this has been changed in new function!
                            
                            if (debug_mode == 1) {
                                printf("update: office_parser - rows > 50 - stopping here - not saving all data from large spreadsheets.  \n");
                            }
                            
                            
                            break;
                        }
                    }}
        }
        }
        // finished processing sheet <i> here
        // slide_num == sheet_num & shape_num == batch_counter
        
        if (debug_mode == 1) {
            printf ("update: office_parser - finished sheet-%d-%d \n", i,row_counter);
            printf ("update: office_parser - core text out-%s \n", core_text_out);
            printf ("update: office_parser - table text out-%s \n", table_text_out);
        }
        
        strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
        

        strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
        my_sp_position.x = i;
        my_sp_position.y = batch_counter;
        my_sp_position.cx = row_counter;
        my_sp_position.cy = 0;
        
        Bloks[wf][global_blok_counter].position = my_sp_position;
        Bloks[wf][global_blok_counter].slide_num = 0;
        Bloks[wf][global_blok_counter].shape_num = batch_counter;
        strcpy(Bloks[wf][global_blok_counter].content_type,"table");
        strcpy(Bloks[wf][global_blok_counter].linked_text,"");
        strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
        strcpy(Bloks[wf][global_blok_counter].relationship,"");
        strcpy(Bloks[wf][global_blok_counter].file_type,"xl");
        global_blok_counter ++;
        bloks_created ++;
        
        if (debug_mode == 1) {
            printf("update: office_parser - completed update of blocks successfully-%d \n", global_blok_counter);
        }
        
    }
        xmlMemFree(iter_node);
        xmlMemFree(root_element);
        xmlMemFree(sptree_node);
        xmlMemFree(iter2);
        xmlMemFree(iter3);
        xmlFree(doc);
        //xmlCleanupParser();
        
        sheet_stop = bloks_created-1;
        
        // note:  does not apply post_slide handling currently!
        //entry_dummy = post_slide_handler(sheet_start, sheet_stop);
        
    if (debug_mode == 1) {
        printf ("update: office_parser - new blocks created & counter: %d \n", bloks_created);
    }
    
        // need to return >0 or won't save to DB
    
    return global_blok_counter;
}





int doc_post_doc_handler (int start, int stop, int global_blok_counter, int wf)
{
    
    // need to accomplish two objectives
    // 1-  nearest_neighbor - simple model - grab next entry - deprecated / remove
    
    // 2 - formatted text - attach to span of +5- insert formatted_text on all slides
    
    int i;

    char all_ft [50000];
    //char text_near_me [500000];
    
    strcpy(all_ft, "");
    
    for (i=start; i <= stop; i++) {
        
        // nearest text easy- take new line
        // all_ft[0] = '\0';
        
        strcpy(Bloks[wf][i].linked_text, "");
        
        if (strcmp(Bloks[wf][i].content_type,"image")==0) {
            // if content is image - then look for nearby text to append
            
            if (i > start) {
                // pick up the blok BEFORE ... provided it is not an image
                if (strcmp(Bloks[wf][i-1].content_type,"image") !=0) {
                    if (strlen(Bloks[wf][i-1].text_run) > 0) {
                        if (strlen(all_ft) < 5000) {
                            strcat(all_ft, Bloks[wf][i-1].text_run);
                            strcat(all_ft, " ");
                        }
                    }
                }
            }
            
            if (i+1 <= stop) {
                if (strcmp(Bloks[wf][i+1].content_type,"image") !=0) {
                    // pick up the blok AFTER ... provided it is not an image
                    if (strlen(Bloks[wf][i+1].text_run) > 0) {
                        if (strlen(all_ft) < 5000) {
                            strcat(all_ft, Bloks[wf][i+1].text_run);
                            strcat(all_ft, " ");
                        }
                    }
                }
            }
        }
            
        
        // work on formatted text
        
        if ((i > start + 2) && (i < stop -2)) {
            //main case - not near an edge
            
            if (strlen(all_ft) < 5000) {
                strcat(all_ft,Bloks[wf][i-2].ft_tmp);
                strcat(all_ft," ");
                strcat(all_ft,Bloks[wf][i-1].ft_tmp);
                strcat(all_ft," ");
                strcat(all_ft,Bloks[wf][i].ft_tmp);
                strcat(all_ft," ");
                strcat(all_ft,Bloks[wf][i+1].ft_tmp);
                strcat(all_ft," ");
                strcat(all_ft,Bloks[wf][i+2].ft_tmp);
            }
            
        }
        
        else {
            
            // edge cases - need to handle appropriately
            if (i > start+2) {
                // worry about the backend
                //printf("landed in back-end worry edge case-%d \n", i);
                
                if (strlen(all_ft) < 5000) {
                    
                    if ((i-4) >= 0) {
                        strcat(all_ft,Bloks[wf][i-4].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i-3) >= 0) {
                        strcat(all_ft,Bloks[wf][i-3].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i-2) >= 0) {
                        strcat(all_ft,Bloks[wf][i-2].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i-1) >=0) {
                        strcat(all_ft,Bloks[wf][i-1].ft_tmp);
                        strcat(all_ft," ");
                    }
                    strcat(all_ft,Bloks[wf][i].ft_tmp);
                    strcat(all_ft," ");
                }
                }
            else {
                
                // edge case- worry about the front-end
                //printf("landed in front-end worry edge case-%d \n", i);
                
                if (strlen(all_ft) < 5000) {
                    strcat(all_ft,Bloks[wf][i].ft_tmp);
                    strcat(all_ft," ");
                    if ((i+1) < stop) {
                        strcat(all_ft,Bloks[wf][i+1].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i+2) < stop) {
                        strcat(all_ft,Bloks[wf][i+2].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i+3) < stop) {
                        strcat(all_ft,Bloks[wf][i+3].ft_tmp);
                        strcat(all_ft," ");
                    }
                    if ((i+4) < stop) {
                        strcat(all_ft,Bloks[wf][i+4].ft_tmp);
                        strcat(all_ft," ");
                    }
                }
            }}
        
        if (strlen(all_ft) < 100) {
            strcat(all_ft, global_headlines);
        }
        
        if (strlen(all_ft) < 4900) {
            strcat(Bloks[wf][i].formatted_text, all_ft);
        }
        else {
            strcat(Bloks[wf][i].formatted_text, global_headlines);
        }
    }
        
    return 0;
}


int doc_para_handler(xmlNode *element_node, int global_blok_counter, int slide_num, int shape_num, int wf)
{
    
    char core_text_out[50000];
    char special_format_text_out[50000];
    
    //const char *t = "t";
    //const char *rpr = "rPr";
    //const char *txbody = "txBody";
    //const char *p = "p";
    //const char *r = "r";
    
    xmlNode *iter_node = NULL;
    xmlNode *text_node = NULL;
    //xmlNode *iter2_node = NULL;
    //xmlNode *iter3_node = NULL;
    xmlNode *text = NULL;
    xmlNode *props = NULL;
    //xmlNode *coords_node = NULL;
    //xmlNode *coords_ext_node = NULL;
    char *key = NULL;
    char *doc;
    char sz[50];
    char bold[10];
    //char underline[10];
    char italics[10];
    char shading[10];
    char color[10];
    //char *dummy=NULL;
    
    int new_bloks_created = 0;
    int new_images_created = 0;
    
    int format_flag = 0;
    
    int success_factor = 1;
    int found_text_success = 0;
    int found_nonspace = 0;
    
    coords my_sp_position;
    
    int dummy = 0;
    
    int drawing_on = -1;
    
    //int my_x = 0;
    //int my_y = 0;
    //int my_cx = 0;
    //int my_cy = 0;
    //char nv_id[10];
    //char fp_rels[200];
    //coords rels_coords;
    //int coords_set = 0;
    
    int i=0;
    int j=0;
    
    strcpy(core_text_out,"");
    strcpy(special_format_text_out,"");
    
    my_sp_position.x = global_blok_counter;
    my_sp_position.y = 0;
    my_sp_position.cx = 0;
    my_sp_position.cy = 0;
    
    // first iteration inside <p> to find <r> elements
    // inside <r> elements are <t> elements which have the text
    
    iter_node = element_node;
    //iter_node = element_node->children;
    
    while (iter_node != NULL) {
        
        // printf ("iterating thru para node: %s \n",iter_node->name);
        
        format_flag = 0;
        
        if ((strcmp(iter_node->name, "r")==0) || (strcmp(iter_node->name, "ins")==0) ||
            (strcmp(iter_node->name, "hyperlink")==0)) {
            
            text_node = iter_node->children;
            
            while (text_node != NULL) {
                
                if (debug_mode == 1) {
                    //printf("iter text node - %s \n", text_node->name);
                }
                
                if (strcmp(text_node->name, "tab")==0) {
                    
                    if (debug_mode == 1) {
                        //printf("update: doc_para_handler - inside <r> - FOUND TAB \n");
                    }
                    
                    if (strlen(global_docx_running_text) > 0) {
                        if (global_docx_running_text[strlen(global_docx_running_text)-1] !=32) {
                            strcat(global_docx_running_text, " ");
                        }
                    }
                }
                
                if (strcmp(text_node->name, "br")==0) {
                    
                    if (debug_mode == 1) {
                        //print("update: doc_para_handler - insert <r> - FOUND BR \n");
                    }
                    
                    if (strlen(global_docx_running_text) > 0) {
                        if (global_docx_running_text[strlen(global_docx_running_text)-1] !=32) {
                            strcat(global_docx_running_text, " ");
                        }
                    }
                }
                
                if (strcmp(text_node->name, "rPr")==0) {
                    
                    // note: useful for debugging:   printf("found rPr \n");
                    
                    props = text_node->children;
                    while (props != NULL) {
                        
                        //printf("rPr props: %s \n", props->name);
                        

                        
                        if (strcmp(props->name,"b")==0) {
                            strcpy(bold,"1");
                            
                            //printf("found b-%s \n", bold);
                        }
                        
                        if (strcmp(props->name,"i")==0) {
                            strcpy(italics,"1");
                            
                            //printf("found i-%s \n", italics);
                        }
                        
                        if (strcmp(props->name,"shd")==0) {
                            strcpy(shading, xmlGetProp(props,"val"));
                            
                            //printf("found shd-%s \n", shading);
                        }
                        
                        if (strcmp(props->name,"color")==0) {
                            strcpy(color,"1");
                            
                            //printf("found color-%s \n", color);
                        }
                        
                        if (strcmp(props->name,"szCs")==0) {
                            strcpy(sz,xmlGetProp(props,"val"));
                            
                            //printf("found sz-%s \n", sz);
                        }
                        
                        if (strcmp(props->name, "lastRenderedPageBreak")==0) {
                            global_docx_page_tracker ++;
                            
                            // found new page - increment global page tracker
                            
                            global_total_pages_added ++;
                            
                            // printf("PAGE BREAK - %d \n", global_total_pages_added);
                        }
                        
                        props=props->next;}
                    
                    if (debug_mode == 1) {
                        //printf("update: format params-%s-%s-%s-%s-%s \n", bold,italics,shading,sz,color);
                    }
                    
                    format_flag = special_formatted_text(bold,italics,shading,sz,color);
                    
                    //useful for debugging:  printf("format flag value-%d \n",format_flag);
                    
                    strcpy(bold,"");
                    strcpy(italics,"");
                    strcpy(shading,"");
                    strcpy(sz,"");
                    strcpy(color,"");
                    
                    //useful for debugging:  printf("exiting rpr with format flag-%d \n",format_flag);
                }
                
                if (strcmp(text_node->name, "lastRenderedPageBreak")==0) {
                    global_docx_page_tracker ++;
                    
                    // found page break - add to global page tracker
                    
                    global_total_pages_added ++;
                    
                    //printf("PAGE BREAK - %d \n", global_total_pages_added);
                    
                }
                
                if (strcmp(text_node->name, "t")==0) {
                    text = text_node->children;
                    key = xmlNodeListGetString(doc,text,1);
                    
                    
                    if (key != NULL) {
                        
                        if ((strlen(global_docx_running_text) + strlen(key)) < 49000) {
                            strcat(global_docx_running_text, key);
                        }
                        
                        //strcat(core_text_out,key);
                        
                        //experiment whether to add space or not
                        //strcat(core_text_out, " ");
                        
                        //success_factor = 99;
                        found_text_success = 99;
                        
                        if (format_flag == 1) {
                            
                            if ((strlen(global_docx_formatted_text) + strlen(key)) < 49000) {
                                strcat(global_docx_formatted_text, key);
                                strcat(global_docx_formatted_text, " ");
                            }
                            
                            if ((strlen(global_headlines) + strlen(key) < 1000)) {
                                strcat(global_headlines, key);
                                strcat(global_headlines, " ");
                            }
                            
                            //strcat(special_format_text_out,key);
                            //strcat(special_format_text_out," ");
                            
                        }
                    }
                    // reset format after each <t>
                    format_flag = 0;
                    
                }
                
                if ( (strcmp(text_node->name,"drawing")==0) || (strcmp(text_node->name,"object")==0) || (strcmp(text_node->name,"pict")==0) ) {
                    // found picture/drawing inside of <r>
                    
                    if (debug_mode == 1) {
                        //printf("update: office_parser - in doc para handler - found drawing - going to drawing_handler. \n");
                    }
                    
                    dummy = drawing_handler(text_node,global_blok_counter,slide_num,shape_num,wf);
                    
                    if (dummy > 0) {
                        new_images_created ++;
                        new_bloks_created ++;
                        global_blok_counter ++;
                    }
                    
                }
                
                
                text_node = text_node->next;
            }
        }
        iter_node = iter_node->next;
        
    }
    
    if (debug_mode == 1) {
        //printf("update: para handler - core_text_out - %s \n",core_text_out);
        //printf("update: para handler - special_formatted text out- %s \n", special_format_text_out);
    }
    
    if (found_text_success == 99) {
        found_nonspace = -1;
        if (strlen(global_docx_running_text) > 0) {
            for (i=0; i < strlen(global_docx_running_text); i++) {
                if (global_docx_running_text[i] != 32) {
                    found_nonspace = 1;
                    break;
                }
            }
        }
        
        if (found_nonspace == 1) {
            
            if (strlen(global_docx_running_text) > (0.50 * GLOBAL_BLOK_SIZE)) {
                Bloks[wf][global_blok_counter].slide_num = global_docx_page_tracker;
                Bloks[wf][global_blok_counter].shape_num = shape_num;
                strcpy(Bloks[wf][global_blok_counter].content_type,"text");
                strcpy(Bloks[wf][global_blok_counter].relationship,"");
                strcpy(Bloks[wf][global_blok_counter].linked_text,"");
                strcpy(Bloks[wf][global_blok_counter].table_text,"");
                strcpy(Bloks[wf][global_blok_counter].file_type,"doc");
                
                //strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
                //strcpy(Bloks[wf][global_blok_counter].ft_tmp, special_format_text_out);
                
                strcpy(Bloks[wf][global_blok_counter].text_run, global_docx_running_text);
                strcpy(Bloks[wf][global_blok_counter].formatted_text,global_docx_formatted_text);
                
                if (debug_mode == 1) {
                    printf("update: office_parser - writing text block to save - %s \n", global_docx_running_text);
                }
                
                strcpy(global_docx_running_text,"");
                strcpy(global_docx_formatted_text,"");
                
                Bloks[wf][global_blok_counter].position = my_sp_position;
                
                new_bloks_created ++;
            }
            
            if (strlen(global_docx_running_text) > 0) {
                strcat(global_docx_running_text, " ");
            }
            if (strlen(global_docx_formatted_text) > 0) {
                strcat(global_docx_formatted_text," ");
            }
        }
    }
        
    
    xmlMemFree(text);
    xmlMemFree(props);
    xmlMemFree(iter_node);
    xmlMemFree(text_node);
    
    return new_bloks_created;
}



int doc_tbl_handler(xmlNode *element_node,int global_blok_counter, int slide_num, int shape_num, int wf) {
    

    //const char *rel_id = "relIds";
    //const char *xfrm = "xfrm";
    //const char *tbl = "tbl";
    //const char *xfrm_ext = "ext";
    //const char *txbody = "txBody";
    //const char *p = "p";
    //const char *r = "r";
        
    xmlNode *iter_element = NULL;
    xmlNode *tmp1 = NULL;
    xmlNode *tmp2 = NULL;
    xmlNode *tmp3 = NULL;
    xmlNode *tmp4 = NULL;
    //xmlNode *tmp5  = NULL;
    //xmlNode *tmp6 = NULL;
    //xmlNode *tmp7 = NULL;
    //xmlNode *tmp8 = NULL;
    //xmlNode *props = NULL;
    xmlNode *text = NULL;
        
    coords my_sp_position;
        
    char *key = NULL;
    char *doc;
    //char *sz;
    //char *bold;
    //char *underline;
    //char *italics;
    //char *dummy;
        
    //char *embed = NULL;
    //char *x = NULL;
    //char *y = NULL;
    //char *cx = NULL;
    //char *cy = NULL;

    //int coords_set = 0;
    //char *match = NULL;

    char core_text_out[50000];
    char special_format_text_out[50000];
    char table_text_out[100000];
    char table_cell[10000];
    
    //int format_flag = 0;
    int success_factor = 1;
    //int my_x = 0;
    //int my_y = 0;
    //int my_cx = 0;
    //int my_cy = 0;
    
    int added_nonempty_cell_to_row = 0;
    
    int row = 0;
    int col = 0;
    char rc_tmp[10];
    
    strcpy(core_text_out,"");
    strcpy(special_format_text_out,"");
    strcpy(table_text_out,"");
    strcpy(rc_tmp,"");
    strcpy(table_cell,"");
    
    my_sp_position.x = global_blok_counter;
    my_sp_position.y = 0;
    my_sp_position.cx = 0;
    my_sp_position.cy = 0;
    
    iter_element = element_node->next;
        
    while (iter_element != NULL) {
        
        if (strcmp(iter_element->name,"tr")==0) {
            
            //found table row
            //useful for debugging:  printf("found table row!!! \n");
            
            // strcat(table_text_out," <tr> ");
            // row ++;
            // reset
            col = -1;
            added_nonempty_cell_to_row = 0;
            
            tmp1 = iter_element->children;
            
            while (tmp1 != NULL) {
                if (strcmp(tmp1->name, "tc")==0) {
                    
                    // increment col count with every tc in row
                    col ++;
                    
                    strcpy(table_cell,"");
                    
                    //found tc element
                    
                    tmp2 = tmp1->children;
                    while (tmp2 != NULL) {
                        
                        //iterating thru tc
                        //useful for debugging:  printf("iterating thru tc-%s \n",tmp2->name);
                        
                        if ((strcmp(tmp2->name,"drawing")==0) || (strcmp(tmp2->name,"pict")==0)) {
                            
                            if (debug_mode == 1) {
                                //printf("update: found drawing/pict inside <tc> %s \n", tmp2->name);
                            }
                            //dummy = drawing_handler(tmp2,global_blok_counter,0, shape_num);
                            
                        }
                        
                        
                        if (strcmp(tmp2->name, "p")==0) {
                            
                            // insert space between multiple <p> elements inside a single cell
                            
                            if (strlen(table_cell) > 0) {
                                strcat(table_cell, " ");
                            }
                            
                            /*
                            if (strlen(core_text_out) > 0) {
                                strcat(core_text_out, " ");
                            }
                             */
                            
                            //found <p> inside tc
                            
                            tmp3= tmp2->children;
                            while (tmp3 !=NULL) {
                                
                                //inside p - looking for <r>
                                //inside r - will find rPr & t
                                
                                if (strcmp(tmp3->name,"r")==0) {
                                    
                                    tmp4 = tmp3->children;
                                    while (tmp4 != NULL) {
                                        
                                        // check for <br> - word space break
                                        if (strcmp(tmp4->name, "br")==0) {
                                            if (strlen(table_cell) > 0) {
                                                strcat(table_cell, " ");
                                            }
                                            
                                            /*
                                            if (strlen(core_text_out) > 0) {
                                                strcat(core_text_out," ");
                                            }
                                            */
                                            
                                        }
                                        
                                        if (strcmp(tmp4->name, "t") == 0) {
                                            
                                            text = tmp4->children;
                                            key = xmlNodeListGetString(doc,text,1);
                                            
                                            // printf("found text-%s \n",key);
                                            
                                            if (key != NULL) {
                                                
                                                //success_factor = 99;
                                                
                                                if ((strlen(table_cell) + strlen(key)) < 10000) {
                                                    strcat(table_cell, key);
                                                    //strcat(table_cell, " ");
                                                }
                                                
                                                /*
                                                if ((strlen(core_text_out) + strlen(key)) < 50000) {
                                                    strcat(core_text_out,key);
                                                    //strcat(core_text_out," ");
                                                }
                                                 */
                                                
                                                //col ++;
                                            }
                                            
                                        }
                                        tmp4 = tmp4->next;
                                    }
                                }
                                tmp3 = tmp3->next;}
                        }
                        tmp2 = tmp2->next;}
                    
                }
                
                if (col >= 0 && (strlen(table_cell) > 0) ) {
                    // write new cell
                    
                    if ( ((strlen(table_text_out)+strlen(table_cell)) < 99900) && ((strlen(core_text_out)+strlen(table_cell)) < 49900) ) {
                        
                        if (added_nonempty_cell_to_row == 0) {
                            strcat(table_text_out," <tr> ");
                            row ++;
                            added_nonempty_cell_to_row = 1;
                        }
                        
                        strcat(table_text_out," <th> <");
                        strcat(core_text_out, " <");
                        
                        if (col < 26) {
                            sprintf(rc_tmp,"%c",65+col);
                            strcat(table_text_out,rc_tmp);
                            strcat(core_text_out,rc_tmp);
                        }
                        
                        if (col >= 26 && col < 52) {
                            strcat(table_text_out,"A");
                            strcat(core_text_out,"A");
                            sprintf(rc_tmp,"%c",65+col-26);
                            strcat(table_text_out,rc_tmp);
                            strcat(core_text_out,rc_tmp);
                        }
                        
                        if (col >= 52) {
                            strcat(table_text_out,"ZZ");
                            strcat(core_text_out,"ZZ");
                        }
                        
                        sprintf(rc_tmp,"%d",row);
                        strcat(table_text_out,rc_tmp);
                        strcat(table_text_out,"> ");
                        strcat(core_text_out,rc_tmp);
                        strcat(core_text_out,"> ");
                        
                        strcat(table_text_out,table_cell);
                        strcat(table_text_out," </th>");
                        
                        strcat(core_text_out, table_cell);
                        
                        strcpy(table_cell,"");
                        
                        success_factor = 99;
                        
                        added_nonempty_cell_to_row = 1;
                        
                        // end - write new cell
                    }
                }
                
                tmp1 = tmp1->next;
                
            }
            // finished processing all cells
            
            if (added_nonempty_cell_to_row == 1) {
                strcat(table_text_out," </tr> ");
            }
            
        }
        
            iter_element = iter_element->next;
        
    }
           
    //  success_factor set to 99 if non-empty table cell element saved in table_text & core_text
    //      --need to save table content!!!!
    
    if (success_factor == 99) {
        
        my_sp_position.cx = row;
        my_sp_position.cy = 0;
        
        Bloks[wf][global_blok_counter].slide_num = global_docx_page_tracker;
        Bloks[wf][global_blok_counter].shape_num = shape_num;
        strcpy(Bloks[wf][global_blok_counter].content_type,"table");
        strcpy(Bloks[wf][global_blok_counter].linked_text,"");
        strcpy(Bloks[wf][global_blok_counter].file_type,"doc");
        
        strcpy(Bloks[wf][global_blok_counter].text_run, core_text_out);
        strcpy(Bloks[wf][global_blok_counter].formatted_text, special_format_text_out);
        Bloks[wf][global_blok_counter].position = my_sp_position;
        strcpy(Bloks[wf][global_blok_counter].table_text,table_text_out);
        
        global_total_tables_added ++;
        
        if (debug_mode == 1) {
            //printf("update: tbl core_text_out-%s \n", core_text_out);
            printf("update: office_parser - doc tbl table_text_out-%s \n",table_text_out);
        }
    }
    
    xmlMemFree(iter_element);
    xmlMemFree(tmp1);
    xmlMemFree(tmp2);
    xmlMemFree(tmp3);
    xmlMemFree(tmp4);
    xmlMemFree(text);
    
    
    return success_factor;
    }



int doc_build_index (int working_folder, int local_slide_count, char *ws_fp) {

    int i = 0;
    char doc_fp[500];
    char doc_fp_tmp[500];
    
    //char doc_rels_fp[500];
    char doc_rels_fp_tmp[500];
    
    xmlNode *cur_node = NULL;

    int shape_counter = 0;
    int new_bloks_created = 0;
    int entry_dummy = 0;
    int slide_start;
    int slide_stop;
    
    xmlNode *root_element = NULL;
    xmlNode *sptree_node = NULL;
    xmlNode *element_node = NULL;
    xmlNode *body_node = NULL;
    
    xmlDoc *doc = NULL;
    
    coords my_sp_position;
    
    int bloks_created = 0;
    
    int dummy = 0;
    
    my_sp_position.x = 0;
    my_sp_position.y = 0;
    my_sp_position.cx = 0;
    my_sp_position.cy = 0;
    
    //int global_blok_counter = 0;
    
    strcpy(global_docx_running_text,"");
    strcpy(global_docx_formatted_text,"");
    strcpy(global_headlines,"");
    
    // reset at start of reading document xml with global page tracker = 1
    // start with Page # 1
    global_docx_page_tracker = 1;
    
    global_docx_para_on_page_tracker = 0;
    
    // Main Loop - Build Index - Cycles through all of the Slides
    
    dummy = pptx_meta_handler(working_folder, ws_fp);
    
    strcpy(doc_fp,ws_fp);
    sprintf(doc_fp_tmp,"%d/document.xml",working_folder);
    strcat(doc_fp,doc_fp_tmp);
    
    strcpy(doc_rels_fp,ws_fp);
    sprintf(doc_rels_fp_tmp, "%d/document.xml.rels",working_folder);
    strcat(doc_rels_fp,doc_rels_fp_tmp);
    
    if (debug_mode == 1) {
        printf ("update: office_parser - Starting Build Index Main Loop: %s - %d \n", doc_fp,local_slide_count);
    }
    
    doc = xmlReadFile(doc_fp,NULL,0);
    
    if (doc == NULL) {
        printf ("warning:  office_parser - word docx parsing - problem loading document not found - skipping.");
    }
    
    else
        
    {
        // main loop - only if doc != NULL
        // root == document
        // child == body - all elements hang off body as children
        
        slide_start = bloks_created;
        
        root_element = xmlDocGetRootElement(doc);
        
        if (debug_mode == 1) {
            //printf("update: root element found-%s \n", root_element->name);
        }
        
        body_node = root_element->children;
        
        while (body_node != NULL) {
            
            if (bloks_created > GLOBAL_MAX_BLOKS) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - doc_build_index - reached MAX BLOCKS - stopping processing here. \n");
                }
                
                break;
            }
            
            if (strcmp(body_node->name, "body")==0) {
                
                sptree_node = body_node->children;
        
                for (cur_node = sptree_node; cur_node; cur_node=cur_node->next) {
                    
                    // main loop - iterating through shape tree
                    // printf("iter cur_node-%s \n",cur_node->name);
                    
                    shape_counter ++;
            
                    if (strcmp(cur_node->name,"p") == 0) {
                        
                        // found "sp" header - need to jump to sp_handler
                        element_node = cur_node->children;
                        
                        // in para handler in shape tree
                        // printf("in para_handler %d-%d \n", shape_counter,bloks_created);
                
                        entry_dummy = doc_para_handler (element_node, bloks_created,0, 0, working_folder);
                        
                        if (entry_dummy > 0) {
                    
                            new_bloks_created = new_bloks_created + entry_dummy;
                            bloks_created = bloks_created + entry_dummy;
                            
                            if (debug_mode == 1) {
                                //printf("update:  completed <p> bloks counter: %d \n",entry_dummy);
                            }
                        }
                    }
        
                    if (strcmp(cur_node->name,"tbl") == 0) {
                        
                        element_node = cur_node->children;
                        
                        if (debug_mode == 1) {
                            //printf("update:  in tbl_handler-blok counter-%d \n",bloks_created);
                        }
                        
                        entry_dummy = doc_tbl_handler (element_node, bloks_created, 0, 0, working_folder);
                            
                        //entry_dummy = 0;
                        
                        if (entry_dummy == 99) {
                    
                            // call rels handler and look up rId == embed
                    
                            //useful for debugging:  printf ("doc rels fp- %s-%d \n",doc_rels_fp,bloks_created);
                            
                            //strcpy(tmp_str,Bloks[working_folder][bloks_created].relationship);
                    
                            //printf ("rel Id: %s \n",tmp_str);
                    
                            //image_name = //rels_handler_pic_new(doc_rels_fp,tmp_str,working_folder,bloks_created);
                    
                            //strcpy(image_name, "TBD");
                            //printf("found matching image name %s-%s \n",tmp_str, image_name);
                            
                            strcpy(Bloks[working_folder][bloks_created].relationship,"TBD");
                            new_bloks_created ++;
                            bloks_created ++;
                            
                            }
                
                    }
            
                }
            }
            body_node = body_node->next;
        }
        
        //slide_stop = bloks_created-1;
    
        //if (bloks_created > 0) {
            //global_total_pages_added ++;
        //}
        
        // finished processing all of document
        
        // check for one last running text blok
        
        if (strlen(global_docx_running_text) > 0) {
            Bloks[working_folder][bloks_created].slide_num = global_docx_page_tracker;
            Bloks[working_folder][bloks_created].shape_num = 0;
            strcpy(Bloks[working_folder][bloks_created].content_type,"text");
            strcpy(Bloks[working_folder][bloks_created].relationship,"");
            strcpy(Bloks[working_folder][bloks_created].linked_text,"");
            strcpy(Bloks[working_folder][bloks_created].table_text,"");
            strcpy(Bloks[working_folder][bloks_created].file_type,"doc");
            
            strcpy(Bloks[working_folder][bloks_created].text_run, global_docx_running_text);
            
            strcpy(Bloks[working_folder][bloks_created].formatted_text,global_docx_formatted_text);
            
            strcpy(global_docx_running_text,"");
            strcpy(global_docx_formatted_text,"");
            
            my_sp_position.x = bloks_created;
            
            Bloks[working_folder][bloks_created].position = my_sp_position;
            
            new_bloks_created ++;
        }
        
        slide_stop = bloks_created - 1;
        
        if (bloks_created > 0) {
            global_total_pages_added ++;
        }
        
        entry_dummy = doc_post_doc_handler(slide_start, slide_stop,bloks_created,working_folder);
        
    }
    
    if (debug_mode == 1) {
        printf ("update: office_parser - new blocks created & counter: %d \n", bloks_created);
    }
    
    
    xmlMemFree(cur_node);
    xmlMemFree(sptree_node);
    xmlMemFree(element_node);
    xmlMemFree(body_node);
    xmlMemFree(root_element);
    
    xmlFreeDoc(doc);
    xmlCleanupParser();
    
    return bloks_created;
}



int pptx_meta_handler (int working_folder, char *ws_fp)
{
    //xmlDoc *doc_app;
    //xmlNode *root_element_app;
    
    xmlDoc *doc_core = NULL;
    xmlNode *root_element_core = NULL;
    xmlNode *iter_node = NULL;
    xmlNode *text = NULL;
    
    char *key= NULL;
    char fp_core_use[200];
    char fp_tmp[100];
    
    strcpy(fp_core_use, ws_fp);
    
    sprintf(fp_tmp, "%d/core.xml",working_folder);
    strcat(fp_core_use,fp_tmp);
    
    // set defaults
    
    strcpy(my_doc[working_folder].author,"");
    strcpy(my_doc[working_folder].last_modified,"");
    strcpy(my_doc[working_folder].creator_tool,"");
    strcpy(my_doc[working_folder].created_date,"");
    strcpy(my_doc[working_folder].modified_date,"");
    
    // end - set defaults
    
    doc_core = xmlReadFile(fp_core_use,NULL,0);
    
    if (doc_core == NULL) {
        
        if (debug_mode==1) {
            
            printf("warning: office_parser - office docx parsing - no core.xml found - will be missing metadata.");
        }
    }
    else {
        
    root_element_core = xmlDocGetRootElement(doc_core);

    iter_node = root_element_core->children;
    
    while (iter_node != NULL) {
    
        if (strcmp(iter_node->name,"lastModifiedBy")==0) {
            text = iter_node->children;
            key = xmlNodeListGetString(doc_core,text,1);
            
            if (debug_mode == 1) {
                //printf ("update: found lastModifiedBy-%d-%s \n", working_folder, key);
            }
            
            if (key == NULL) {
                strcpy(my_doc[working_folder].author, "");
            }
            else
            {
                strcpy(my_doc[working_folder].author,key);
                
            }
        }
        
        if (strcmp(iter_node->name,"modified")==0) {
            text = iter_node->children;
            key = xmlNodeListGetString(doc_core,text,1);
            
            if (debug_mode == 1) {
                //printf ("update: office metadata_handler- found 'modified_date'-%s \n", key);
            }
            
            if (key == NULL) {
                strcpy(my_doc[working_folder].modified_date, "");
            }
            else {
                strcpy(my_doc[working_folder].modified_date,key);
            }
        }
        
        if (strcmp(iter_node->name, "created")==0) {
            text = iter_node->children;
            key = xmlNodeListGetString(doc_core,text,1);
            
            if (debug_mode == 1) {
                //printf("update: office metadata_handler- found 'created_date' - %s \n", key);
            }
            if (key == NULL) {
                strcpy(my_doc[working_folder].created_date, "");
            }
            else {
                strcpy(my_doc[working_folder].created_date, key);
            }
            
        }
        
        iter_node = iter_node->next;
    }
        xmlMemFree(text);
        xmlMemFree(iter_node);
        xmlMemFree(root_element_core);
        xmlFreeDoc(doc_core);
    }

    return 0;
}



int pptx_build_index (int working_folder, int local_slide_count, char *ws_fp) {

    int i = 0;
    char slide_fp[200];
    char slide_rels_fp[200];
    
    const char *sp = "sp";
    const char *pic = "pic";
    const char *gf = "graphicFrame";
    const char *grpsp = "grpSp";
    const char *sptree = "spTree";
    
    int shape_counter = 0;
    int new_bloks_created = 0;
    int entry_dummy = 0;
    int slide_start = 0;
    int slide_stop = 0;
    int dummy;
    int current_text_blok = 0;
    int short_text_blok = -1;
    
    xmlNode *cur_node = NULL;
    xmlNode *root_element = NULL;
    xmlNode *sptree_node = NULL;
    xmlNode *element_node = NULL;
    xmlDoc *doc = NULL;

    xmlNode *sld_node = NULL;
    xmlNode *outer_node = NULL;
    
    int bloks_created = 0;
    char *image_name = NULL;
    char tmp_str[100];
    char fp_main2[200];
    char fp_str_tmp[100];
    char fp2_str_tmp[100];
    char tmp_str2[100];

    dummy = pptx_meta_handler(working_folder,ws_fp);
    
    strcpy(global_headlines,"");
    
    strcpy(slide_fp,"");
    strcpy(slide_rels_fp,"");
    
    strcpy(fp_main2, ws_fp);
    
    sprintf(tmp_str2,"%d/",working_folder);
    strcat(fp_main2,tmp_str2);
    
    // Main Loop - Build Index - Cycles through all of the Slides
    
    if (debug_mode == 1) {
        printf ("update: office_parser - Starting Build Index Main Loop: %d - %s - %d \n", working_folder, fp_main2,local_slide_count);
    }
    
    for (i=1; i <local_slide_count+1; i++) {
        
        if (bloks_created > GLOBAL_MAX_BLOKS) {
            
            printf("warning: office_parser - in pptx_build_index - MAX BLOCKS reached - stopping processing here - will be missing content. \n");
            
            break;
        }
        
        strcpy(slide_fp,fp_main2);
        sprintf(fp_str_tmp,"slide%d.xml",i);
        strcat(slide_fp,fp_str_tmp);
        
        strcpy(slide_rels_fp,fp_main2);
        sprintf(fp2_str_tmp,"slide%d.xml.rels",i);
        strcat(slide_rels_fp,fp2_str_tmp);
        
        if (debug_mode == 1) {
            //printf("update: on slide-%d-%d-%s \n", working_folder,i,slide_rels_fp);
        }
        
        // Inner Loop - parses every slide
        // Finds every shape & then passes to handler for further processing
        // Each handler writes to the global variable Slide_Bloks
        // passed to int entry counter
        // returns int success factor
        
        
        // ***for pptx slides, sptree_node is always two layers down from root***
        // sptree_node has all of the shapes on the slide as direct children
        
        doc = xmlReadFile(slide_fp,NULL,0);
        
        if (doc == NULL) {
            printf ("warning: office_parser - could not read xml file - skipping-%s \n",slide_fp);
        }
        
        else {
            // this is the main process - only initiate if doc != NULL
            // start tracking first blok @ start of slide
            
            root_element = xmlDocGetRootElement(doc);
            slide_start = bloks_created;
            short_text_blok = -1;
            //sptree_node = root_element->children->children->children;
        
            sld_node = root_element->children;
            
            if (debug_mode == 1) {
                //printf("update: found sld_node - %s \n", sld_node->name);
                //printf("update: sld_node->children - %s \n", sld_node->children->name);
            }
            
            for (outer_node = sld_node->children; outer_node; outer_node=outer_node->next) {
                
                if (strcmp(outer_node->name,sptree) == 0) {
                    
                    if (debug_mode == 1) {
                        //printf("update: found spTree node - %s \n", outer_node->name);
                    }
                    
                    for (cur_node = outer_node->children; cur_node; cur_node=cur_node->next) {
                        
                        shape_counter ++;
                        
                        if (debug_mode == 1) {
                            //printf("update:  iterating thru slide-%s \n", cur_node->name);
                        }
                        
                        entry_dummy = 0;
                        
                        if (strcmp(cur_node->name,sp) == 0) {
                            
                            element_node = cur_node->children;
                            entry_dummy = sp_handler_new (element_node, bloks_created,short_text_blok, i,shape_counter, working_folder, ws_fp);
                            
                            if (entry_dummy == 99) {
                                // all good - reset short_text_blok to -1 default
                                short_text_blok = -1;
                                new_bloks_created ++;
                                bloks_created ++;
                            }
                            else {
                                if (entry_dummy == 98) {
                                    // wrote to new blok - but it is short text blok
                                    short_text_blok = bloks_created;
                                    new_bloks_created ++;
                                    bloks_created ++;
                                }
                                else {
                                    if (entry_dummy == 97) {
                                        // no new blok created - filled in short blok
                                        // OK blok length - so reset short text blok
                                        short_text_blok = -1;
                                    }
                                    else {
                                        if (entry_dummy = 96) {
                                            // no new blok created and still short
                                            // no action - keep short_text_blok for next sp
                                        }
                                    }
                                }
                            }
                            
                        }
                        
                        if (strcmp(cur_node->name,pic) == 0) {
                            element_node = cur_node->children;
                            
                            entry_dummy = pics_handler_new (element_node,bloks_created,i,shape_counter, working_folder);
                            
                            
                            //useful for debugging:  printf("survived pics handler -%d - %d \n", working_folder, entry_dummy);
                            
                            if (entry_dummy == 99) {
                                strcpy(tmp_str,Bloks[working_folder][bloks_created].relationship);
                                
                                //useful for debugging:  printf("pics handler- going to rels-%s-%s \n",slide_rels_fp,tmp_str);
                                
                                image_name = rels_handler_pic_new(slide_rels_fp,tmp_str,working_folder,bloks_created,ws_fp);
                                strcpy(Bloks[working_folder][bloks_created].relationship,image_name);
                                
                                new_bloks_created ++;
                                bloks_created ++;
                            }
                            
                        }
                        
                        if (strcmp(cur_node->name,gf) == 0) {
                            element_node = cur_node->children;
                            
                            entry_dummy = gf_handler(element_node,bloks_created,i,shape_counter, working_folder);
                            
                            if (entry_dummy == 99) {
                                new_bloks_created ++;
                                bloks_created ++;
                            }
                        }
                        
                        if (strcmp(cur_node->name,grpsp) == 0) {
                            element_node = cur_node->children;
                            
                            // in group shape handler
                            // useful for debugging:  printf("in grp_shp_handler-%d \n",bloks_created);
                            
                            while (element_node != NULL) {
                                
                                if (strcmp(element_node->name,"sp")==0) {
                                    
                                    // will iterately call sp handler each time found inside grp_shp
                                    
                                    // if short text found in group shape - go with it
                                    
                                    entry_dummy = sp_handler_new(element_node,bloks_created,-1, i,shape_counter, working_folder, ws_fp);
                                    
                                    if (entry_dummy == 99 || entry_dummy == 98) {
                                        new_bloks_created ++;
                                        bloks_created ++;
                                    }
                                }
                                
                                if (strcmp(element_node->name,"pic")==0) {
                                    
                                    // found a pic inside grp shp
                                    // printf("found a pic inside grp_shp!");
                                    
                                    entry_dummy = pics_handler_new (element_node,bloks_created,i,shape_counter, working_folder);
                                    
                                    if (entry_dummy == 99) {
                                        strcpy(tmp_str,Bloks[working_folder][bloks_created].relationship);
                                        
                                        // printf("in pic handler - tmp_str-%d-%s \n", working_folder,tmp_str);
                                        
                                        image_name = rels_handler_pic_new(slide_rels_fp,tmp_str,working_folder,bloks_created,ws_fp);
                                        
                                        //printf("found matching image name %s-%s \n",tmp_str, image_name);
                                        
                                        strcpy(Bloks[working_folder][bloks_created].relationship,image_name);
                                        new_bloks_created ++;
                                        bloks_created ++;}
                                }
                                element_node = element_node->next;}}
                    }
                }}
        
        //printf("about to xmlFreeDoc!");
        //xmlCleanupParser();
        //xmlFreeDoc(doc);
        
            xmlMemFree(cur_node);
            xmlMemFree(sptree_node);
            xmlMemFree(element_node);
            xmlMemFree(root_element);
            xmlMemFree(sld_node);
            xmlMemFree(outer_node);
            
            xmlFreeDoc(doc);
            
        slide_stop = bloks_created;
        
            if (debug_mode == 1) {
                //printf("update:  finished slide-wf-%d-start-%d-stop-%d \n", working_folder,slide_start, slide_stop);
            }
            
        if (slide_stop > slide_start) {
        
            entry_dummy = post_slide_handler(slide_start, slide_stop, working_folder);
            
            // created a new slide
            // increment global total_pages_added tracker
            
            global_total_pages_added ++;
            
            
        }
        else
        {
            if (debug_mode==1) {
                printf("warning: office_parser - pptx_handler - no new content found - skipping slide. \n");
            }
        }
        }}
    
    return slide_stop;
}


/*
int pptx_build_index_old_works (int working_folder, int local_slide_count, char *ws_fp) {

    int i = 0;
    char slide_fp[200];
    char slide_rels_fp[200];
    xmlNode *cur_node = NULL;
    const char *sp = "sp";
    const char *pic = "pic";
    const char *gf = "graphicFrame";
    const char *grpsp = "grpSp";
    int shape_counter = 0;
    int new_bloks_created = 0;
    int entry_dummy = 0;
    int slide_start = 0;
    int slide_stop = 0;
    int dummy;
    
    xmlNode *root_element = NULL;
    xmlNode *sptree_node = NULL;
    xmlNode *element_node = NULL;
    xmlDoc *doc = NULL;

    xmlNode *sld_node = NULL;
    
    int bloks_created = 0;
    char *image_name = NULL;
    char tmp_str[100];
    char fp_main2[200];
    char fp_str_tmp[100];
    char fp2_str_tmp[100];
    char tmp_str2[100];

    dummy = pptx_meta_handler(working_folder,ws_fp);
    
    strcpy(slide_fp,"");
    strcpy(slide_rels_fp,"");
    
    strcpy(fp_main2, ws_fp);
    
    sprintf(tmp_str2,"%d/",working_folder);
    strcat(fp_main2,tmp_str2);
    
    // Main Loop - Build Index - Cycles through all of the Slides
    
    if (debug_mode == 1) {
        printf ("update: Starting Build Index Main Loop: %d - %s - %d \n", working_folder, fp_main2,local_slide_count);
    }
    
    for (i=1; i <local_slide_count+1; i++) {
        
        strcpy(slide_fp,fp_main2);
        sprintf(fp_str_tmp,"slide%d.xml",i);
        strcat(slide_fp,fp_str_tmp);
        
        strcpy(slide_rels_fp,fp_main2);
        sprintf(fp2_str_tmp,"slide%d.xml.rels",i);
        strcat(slide_rels_fp,fp2_str_tmp);
        
        if (debug_mode == 1) {
            printf("update: on slide-%d-%d-%s \n", working_folder,i,slide_rels_fp);
        }
        
        // Inner Loop - parses every slide
        // Finds every shape & then passes to handler for further processing
        // Each handler writes to the global variable Slide_Bloks
        // passed to int entry counter
        // returns int success factor
        
        
        // ***for pptx slides, sptree_node is always two layers down from root***
        // sptree_node has all of the shapes on the slide as direct children
        
        doc = xmlReadFile(slide_fp,NULL,0);
        
        if (doc == NULL) {
            printf ("error:  could not read xml file-skipping-%s \n",slide_fp);
        }
        
        else {
            // this is the main process - only initiate if doc != NULL
            
            root_element = xmlDocGetRootElement(doc);
            slide_start = bloks_created;
            
            sptree_node = root_element->children->children->children;
        
        //sld_node = root_element->children;
        
        // start tracking first blok @ start of slide
        slide_start = bloks_created;
        
            if (debug_mode == 1) {
                //printf("update:  sptree_node = %s \n", sptree_node->name);
                //printf("update:  node_uptree_1 = %s \n", root_element->children->name);
                //printf("update:  node_uptree_2 = %s \n", root_element->children->children->name);
            }
        
        for (cur_node = sptree_node; cur_node; cur_node=cur_node->next) {
            
            shape_counter ++;
            
            if (debug_mode == 1) {
                //printf("update:  iterating thru slide-%s \n", cur_node->name);
            }
            
            entry_dummy = 0;
            
            if (strcmp(cur_node->name,sp) == 0) {
                
                
                element_node = cur_node->children;
                entry_dummy = sp_handler_new (element_node, bloks_created,bloks_created, i,shape_counter, working_folder, ws_fp);

                if (entry_dummy == 99) {
                    new_bloks_created ++;
                    bloks_created ++;
                }

            }
            
            if (strcmp(cur_node->name,pic) == 0) {
                element_node = cur_node->children;
                entry_dummy = pics_handler_new (element_node,bloks_created,i,shape_counter, working_folder);

                
                //useful for debugging:  printf("survived pics handler -%d - %d \n", working_folder, entry_dummy);
                
                if (entry_dummy == 99) {
                    strcpy(tmp_str,Bloks[working_folder][bloks_created].relationship);
                    
                    //useful for debugging:  printf("pics handler- going to rels-%s-%s \n",slide_rels_fp,tmp_str);
                    
                    image_name = rels_handler_pic_new(slide_rels_fp,tmp_str,working_folder,bloks_created,ws_fp);
                    strcpy(Bloks[working_folder][bloks_created].relationship,image_name);
                    
                    new_bloks_created ++;
                    bloks_created ++;
                }
        
            }
            
            if (strcmp(cur_node->name,gf) == 0) {
                element_node = cur_node->children;
                entry_dummy = gf_handler(element_node,bloks_created,i,shape_counter, working_folder);
                
                if (entry_dummy == 99) {
                    new_bloks_created ++;
                    bloks_created ++;
                }
            }
            
            if (strcmp(cur_node->name,grpsp) == 0) {
                element_node = cur_node->children;
                
                // in group shape handler
                // useful for debugging:  printf("in grp_shp_handler-%d \n",bloks_created);
                
                while (element_node != NULL) {
                    if (strcmp(element_node->name,"sp")==0) {
                        
                        // will iterately call sp handler each time found inside grp_shp
                        
                        entry_dummy = sp_handler_new(element_node,bloks_created,bloks_created, i,shape_counter, working_folder, ws_fp);
                       
                        if (entry_dummy == 99) {
                            new_bloks_created ++;
                            bloks_created ++;
                        }
                    }
                    
                    if (strcmp(element_node->name,"pic")==0) {
                        
                        // found a pic inside grp shp
                        // printf("found a pic inside grp_shp!");
                        
                        entry_dummy = pics_handler_new (element_node,bloks_created,i,shape_counter, working_folder);
                        
                        if (entry_dummy == 99) {
                            strcpy(tmp_str,Bloks[working_folder][bloks_created].relationship);
                            
                            // printf("in pic handler - tmp_str-%d-%s \n", working_folder,tmp_str);
                            
                            image_name = rels_handler_pic_new(slide_rels_fp,tmp_str,working_folder,bloks_created,ws_fp);
                            
                            //printf("found matching image name %s-%s \n",tmp_str, image_name);
                            
                            strcpy(Bloks[working_folder][bloks_created].relationship,image_name);
                            new_bloks_created ++;
                            bloks_created ++;}
                        }
                    element_node = element_node->next;}}
        }
        
        //printf("about to xmlFreeDoc!");
        //xmlCleanupParser();
        //xmlFreeDoc(doc);
        
        xmlMemFree(cur_node);
        xmlMemFree(sptree_node);
        xmlMemFree(element_node);
        xmlMemFree(root_element);
        xmlFree(doc);
            
        slide_stop = bloks_created;
        
            if (debug_mode == 1) {
                printf("update:  finished slide-wf-%d-start-%d-stop-%d \n", working_folder,slide_start, slide_stop);
            }
            
        if (slide_stop > slide_start) {
        
            entry_dummy = post_slide_handler(slide_start, slide_stop, working_folder);
            
        }
        else
        {
            printf("error: no new content found - %d - skipping slide!", working_folder);
        }
        }}
    
    return slide_stop;
}
*/



int write_to_db (int start_blok,int stop_blok, char *account, char *library,int doc_number,int blok_number, int wf, char *time_stamp)
{
    // const char *uri_string = "mongodb://localhost:27017";
    const char *uri_string = global_mongo_db_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_t *insert = NULL;
    bson_error_t error;
    
    int i=0;
    int j=0;
    int k=0;
    int found_problem_char = 0;
    char text_search[100000];
    char ft [50000];
    char table_text[100000];
    
    char tmp[10];
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
    
    database = mongoc_client_get_database (client, account);
    coll = mongoc_client_get_collection (client, account, library);
    
    if (debug_mode == 1) {
        //printf("update: writing to db-start-%d-stop-%d \n", start_blok, stop_blok);
    }
    
    // create text_search in loop - derived from text_run + formatted_text
    
    for (i=start_blok; i < stop_blok; i++) {
        
        // cleans the text_run sample for utf8 validation -> write to MongoDB will fail if bad char
        
        // iterates through all of the text content
        // useful for debugging:   printf("Bloks_text_run len-%d \n",strlen(Bloks[wf][i].text_run));
        
        
        // brute force clean up of text1_core
        
        strcpy(text_search,"");
        
        for (j=0; j < strlen(Bloks[wf][i].text_run); j++) {
            if (Bloks[wf][i].text_run[j] > 128) {
                found_problem_char = 1;
                break;
            }
        }
        
        
        if (found_problem_char == 1) {
            for (j=0; j < strlen(Bloks[wf][i].text_run); j++) {
                if (Bloks[wf][i].text_run[j] >= 32 && Bloks[wf][i].text_run[j] <= 128) {
                    sprintf(tmp,"%c",Bloks[wf][i].text_run[j]);
                    strcat(text_search, tmp);
                }
                
                else {
                    if (Bloks[wf][i].text_run[j] == 9) {
                        strcat(text_search, " ");
                    }
                    if (Bloks[wf][i].text_run[j] == 13) {
                        strcat(text_search, " ");
                    }
                }
                
            }
        }
        else {
            strcat(text_search, Bloks[wf][i].text_run);
        }

        
        // brute force clean up of formatted_text
        
        strcpy(ft,"");
        
        if (strcmp(Bloks[wf][i].content_type,"image")==0) {
            strcat(text_search," ");
        }
        
        found_problem_char = 0;
        for (j=0; j < strlen(Bloks[wf][i].formatted_text); j++) {
            if (Bloks[wf][i].formatted_text[j] > 128) {
                found_problem_char = 1;
                break;
            }
        }
        
        if (found_problem_char == 1) {
            for (j=0; j < strlen(Bloks[wf][i].formatted_text); j++) {
                
                if (Bloks[wf][i].formatted_text[j] >= 32 && Bloks[wf][i].formatted_text[j] <= 128) {
                    sprintf(tmp,"%c",Bloks[wf][i].formatted_text[j]);
                    strcat(ft,tmp);
                    
                    if (strcmp(Bloks[wf][i].content_type, "image") == 0) {
                        strcat(text_search, tmp);
                    }
                }
                else {
                    if (Bloks[wf][i].formatted_text[j] == 9) {
                        strcat(ft, " ");
                        
                        if (strcmp(Bloks[wf][i].content_type, "image") == 0) {
                            strcat(text_search, " ");
                        }
                    }
                    if (Bloks[wf][i].formatted_text[j] == 13) {
                        strcat(ft, " ");
                        
                        if (strcmp(Bloks[wf][i].content_type, "image") == 0) {
                            strcat(text_search, " ");
                        }
                    }
                }
            }
        }
        else {
            strcat(ft, Bloks[wf][i].formatted_text);
        }
            
        
        // brute force clean of table text
        
        strcpy(table_text, "");
        found_problem_char = 0;
            
        if ( strlen(Bloks[wf][i].table_text) > 0) {
                
            for (j=0; j < strlen(Bloks[wf][i].table_text); j++) {
                if (Bloks[wf][i].table_text[j] > 128) {
                    found_problem_char = 1;
                    break;
                }
            }
                
            if (found_problem_char == 1) {
                for (j=0; j < strlen(Bloks[wf][i].table_text); j++) {
                    if (Bloks[wf][i].table_text[j] >= 32 && Bloks[wf][i].table_text[j] <= 128) {
                        sprintf(tmp, "%c", Bloks[wf][i].table_text[j]);
                        strcat(table_text, tmp);
                    }
                        
                    else {
                        if (Bloks[wf][i].table_text[j] == 9) {
                            strcat(table_text, " ");
                            }
                        if (Bloks[wf][i].table_text[j] == 13) {
                            strcat(table_text, " ");
                            }
                        }
                    }
            }
                
            else {
                strcat(table_text, Bloks[wf][i].table_text);
                }
        }
            
        if (strcmp(Bloks[wf][i].content_type,"image")==0) {
                
            if (strlen(text_search) < 25) {
                if (strlen(global_headlines) > 0) {
                    strcat(text_search, " ");
                    strcat(text_search, global_headlines);
                    }
                }
            }
        
        
            

    //}
        
        // multiple options to enhance 'search index'
        
        // not active - but listed below - could add all formatted_text
        // ... also interesting to add file_name and author
        
        //strcat(text_search, " ");
        //strcat(text_search, Bloks[wf][i].linked_text);
        
        //strcat(text_search, " ");
        
        //strcat(text_search, Bloks[wf][i].formatted_text);
        //strcat(text_search, " ");
        
        //strcat(text_search, my_doc[wf].file_name);
        //strcat(text_search, " ");
        
        //strcat(text_search, my_doc[wf].author);
        
        // end - not active options to enhance text_search index
        
        
        //printf("update: write_to_db: %s - %d - %d - %s \n", my_doc[wf].file_name, i, strlen(Bloks[wf][i].formatted_text), Bloks[wf][i].formatted_text);
        
        //printf("update: core_text: %d - %s \n", strlen(text_search),text_search);
        
        
        //  below is the 'canonical' set of info saved with each DB record
        //  used in several places with identical set of fields:
        //          --pdf parser
        //          --library - html, csv/txt, ocr images
        //
        //  note:  there is simplification opportunity with at least 4 elements:
        //          --text2_spatial & text3_format
        //          --content2_spatial & content3_format
        //
        //          these elements are NOT used anywhere
        
        if (debug_mode == 1) {
            printf("update: office_parser - writing block to db - %d - %d - %s \n", blok_number, Bloks[wf][i].slide_num,
                   text_search);
        }
        
        insert = BCON_NEW (
                           "block_ID", BCON_INT64(blok_number),
                           "doc_ID", BCON_INT64(doc_number),
                           "content_type",BCON_UTF8(Bloks[wf][i].content_type),
                           "file_type", BCON_UTF8(Bloks[wf][i].file_type),
                           "master_index", BCON_INT64(Bloks[wf][i].slide_num),
                           "master_index2", BCON_INT64(0),
                           "coords_x", BCON_INT64(Bloks[wf][i].position.x),
                           "coords_y", BCON_INT64(Bloks[wf][i].position.y),
                           "coords_cx", BCON_INT64(Bloks[wf][i].position.cx),
                           "coords_cy", BCON_INT64(Bloks[wf][i].position.cy),
                           "author_or_speaker", BCON_UTF8(my_doc[wf].author),
                           "added_to_collection", BCON_UTF8(time_stamp),
                           "modified_date", BCON_UTF8(my_doc[wf].modified_date),
                           "created_date", BCON_UTF8(my_doc[wf].created_date),
                           "creator_tool", BCON_UTF8(""),
                           "file_source", BCON_UTF8(my_doc[wf].file_short_name),
                           "table", BCON_UTF8(table_text),
                           "external_files", BCON_UTF8(Bloks[wf][i].relationship),
                           "text", BCON_UTF8(text_search),
                           "header_text", BCON_UTF8(ft),
                           "text_search", BCON_UTF8(text_search),
                           "user_tags", BCON_UTF8(""),
                           "special_field1", BCON_UTF8(""),
                           "special_field2", BCON_UTF8(""),
                           "special_field3", BCON_UTF8(""),
                           "graph_status", BCON_UTF8("false"),
                           "dialog", BCON_UTF8("false")
                           );
        
        blok_number ++;
        
        if (!mongoc_collection_insert_one (coll, insert, NULL, NULL, &error)) {
          fprintf (stderr, "%s\n", error.message);
        }
        
        bson_destroy(insert);
        
        // reset Blok in memory
        strcpy(Bloks[wf][i].text_run,"");
        strcpy(Bloks[wf][i].formatted_text,"");
        strcpy(Bloks[wf][i].table_text,"");

    }
    
    strcpy(global_headlines,"");
    //bson_destroy (insert);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    if (debug_mode == 1) {
        printf ("update: office_parser- created db collection successfully \n");
    }
    
   return blok_number;
}



int pull_new_doc_id (char*account_name, char*library_name) {
    
    const char *library_collection = "library";
    
    const char *uri_string = global_mongo_db_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;

    bson_error_t error;
    bson_t *filter;
    bson_t reply;
    bson_t *update = NULL;
    bson_iter_t iter;
    bson_iter_t sub_iter;
    // char *str;
    
    int new_doc_id = -1;
    
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
    
    database = mongoc_client_get_database (client, account_name);
    coll = mongoc_client_get_collection (client, account_name, library_collection);

    filter = BCON_NEW("library_name", library_name);
    update = BCON_NEW("$inc", "{","unique_doc_id", BCON_INT64(1), "}");

    if (!mongoc_collection_find_and_modify(coll,filter,NULL,update,NULL,false,false,true, &reply, &error)) {
        fprintf(stderr, "%s\n", error.message);
    }
       
    if (bson_iter_init_find(&iter, &reply, "value") &&
        (BSON_ITER_HOLDS_DOCUMENT(&iter) || BSON_ITER_HOLDS_ARRAY (&iter)) && bson_iter_recurse(&iter,&sub_iter)) {

            if (bson_iter_find(&sub_iter, "unique_doc_id")) {
                
                // printf("update: pulled new unique doc_id value - %d \n", bson_iter_int64(&sub_iter));
                
                new_doc_id = bson_iter_int64(&sub_iter);
            }
        }
    

    // str = bson_as_canonical_extended_json(&reply, NULL);
    // printf("update: doc_id output - %s \n", str);
    // bson_free(str);
    
    /*
    if (!mongoc_collection_update_one (coll, filter,insert, NULL, NULL, &error)) {
      fprintf (stderr, "%s\n", error.message);
    }
     */
    
        
    bson_destroy (&reply);
    bson_destroy (filter);
    bson_destroy (update);
    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();


    return new_doc_id;
}



int update_library_inc_totals(char*account_name, char*library_name, int inc_docs, int inc_bloks, int inc_img, int inc_pages, int inc_tables) {
    
    const char *library_collection = "library";
    
    const char *uri_string = global_mongo_db_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_t *insert = NULL;
    bson_error_t error;
    bson_t *filter;
    
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
    
    database = mongoc_client_get_database (client, account_name);
    coll = mongoc_client_get_collection (client, account_name, library_collection);

    filter = BCON_NEW("library_name", library_name);
    
    insert = BCON_NEW("$inc",
                      "{",
                            "documents", BCON_INT64(inc_docs),
                            "blocks", BCON_INT64(inc_bloks),
                            "images", BCON_INT64(inc_img),
                            "pages", BCON_INT64(inc_pages),
                            "tables", BCON_INT64(inc_tables),
                      "}"
                      );
                      
    
    if (!mongoc_collection_update_one (coll, filter,insert, NULL, NULL, &error)) {
      fprintf (stderr, "%s\n", error.message);
    }


    bson_destroy (insert);
    bson_destroy (filter);
    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();

    
    return 0;
}

// new function - option to write bloks output to text file

int write_to_file (int start_blok, int stop_blok, char *account, char *library, int doc_number, int blok_number, char *time_stamp, char *filename, int wf) {
    
    if (debug_mode == 1) {
        printf("update: office_parser - writing parsing output to file.\n");
    }
    
    int i=0;
    char text_search[100000];
    
    char fp[500];
    
    FILE* bloks_out;
    
    strcpy(fp,"");
    strcat(fp,global_image_fp);
    strcat(fp,filename);
    
    if ((bloks_out = fopen(fp,"r")) != NULL) {
        
        // file already exists
        fclose(bloks_out);
        
        bloks_out = fopen(fp,"a");
        
        if (debug_mode == 1) {
            printf("update: office_parser - parsing output file already started -> opening in 'a' mode to append \n");
        }
    }
    
    else {
        bloks_out = fopen(fp,"w");
        
        if (debug_mode == 1) {
            printf("update: office_parser - creating new parsing output file -> opening in 'w' write \n");
        }
    }
        
    
    // create text_search in loop - derived from text_run + formatted_text
    
    for (i=start_blok; i < stop_blok; i++) {
        
        // create text_search
        
        strcpy(text_search,Bloks[wf][i].text_run);

        if (strcmp(Bloks[wf][i].content_type,"image") == 0) {
            
            // only add linked text to search index if content_type == "image"
            
            strcat(text_search, Bloks[wf][i].formatted_text);
            
            if (strlen(text_search) < 10) {
                if (strlen(global_headlines) > 0) {
                    strcat(text_search, " ");
                    strcat(text_search, global_headlines);
                }
            }
        }
        
        // correct office vars
        fprintf(bloks_out,"\n<block_ID>: %d,",blok_number);
        fprintf(bloks_out, "\n<doc_ID>: %d,", doc_number);
        fprintf(bloks_out, "\n<content_type>: %s,", Bloks[wf][i].content_type);
        fprintf(bloks_out, "\n<file_type>: %s,", Bloks[wf][i].file_type);
        fprintf(bloks_out, "\n<master_index>: %d,", Bloks[wf][i].slide_num + 1);
        fprintf(bloks_out, "\n<master_index2>: %d,", 0);
        fprintf(bloks_out, "\n<coords_x>: %d,", Bloks[wf][i].position.x);
        fprintf(bloks_out, "\n<coords_y>: %d,", Bloks[wf][i].position.y);
        fprintf(bloks_out, "\n<coords_cx>: %d,", Bloks[wf][i].position.cx);
        fprintf(bloks_out, "\n<coords_cy>: %d,", Bloks[wf][i].position.cy);
        fprintf(bloks_out, "\n<author_or_speaker>: %s,", my_doc[wf].author);
        
        fprintf(bloks_out, "\n<added_to_collection>: %s,", time_stamp);
        fprintf(bloks_out, "\n<file_source>: %s,", my_doc[wf].file_short_name);
        fprintf(bloks_out, "\n<table>: %s,", Bloks[wf][i].table_text);
        
        fprintf(bloks_out, "\n<modified_date>: %s,", my_doc[wf].modified_date);
        fprintf(bloks_out, "\n<created_date>: %s,", my_doc[wf].created_date);
        fprintf(bloks_out, "\n<creator_tool>: %s,", my_doc[wf].creator_tool);
        fprintf(bloks_out, "\n<external_files>: %s,", Bloks[wf][i].relationship);
        fprintf(bloks_out, "\n<text>: %s,", Bloks[wf][i].text_run);
        fprintf(bloks_out, "\n<header_text>: %s,", Bloks[wf][i].formatted_text);
        fprintf(bloks_out, "\n<text_search>: %s,", text_search);
        fprintf(bloks_out, "\n<user_tags>: %s,", "");
        fprintf(bloks_out, "\n<special_field1>: %s,", "");
        fprintf(bloks_out, "\n<special_field2>: %s,", "");
        fprintf(bloks_out, "\n<special_field3>: %s,", "");
        fprintf(bloks_out, "\n<graph_status>: false,");
        fprintf(bloks_out, "\n<dialog>: false,");
        fprintf(bloks_out, "%s\n", "<END_BLOCK>");
        
        
        blok_number ++;
        
    }
    
    fclose(bloks_out);
       
   return blok_number;
}




// register_ae_add_office_event is WIP
//  ... can be developed further to register key processing events from Office XML parsing
//  ... can also be a valuable debugging tool to figure out what went wrong
//  ... most useful application is likely to identify a file that was not saved / processed correctly

int register_ae_add_office_event(char *ae_type, char *event, char *account,char *library,char *fp, char *time_stamp, char *current_file_name) {
    
    // const char *ae_type = "ADD_OFFICE_LOG";
    // const char *ae_type = "REJECTED_FILE_OFFICE";
    
    const char *library_collection = "account_events";
    
    //const char *uri_string = "mongodb://localhost:27017";
    
    const char *uri_string = global_mongo_db_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_t *insert = NULL;
    bson_error_t error;
 
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
    
    database = mongoc_client_get_database (client, account);
    
    coll = mongoc_client_get_collection (client, account, library_collection);

    insert = BCON_NEW (
                       "event_type", BCON_UTF8(ae_type),
                       "account_ID", BCON_UTF8(account),
                       "library_name", BCON_UTF8(library),
                       "blok", BCON_UTF8(""),
                       "search_query", BCON_UTF8(event),
                       "permission_scope", BCON_UTF8(""),
                       "user_IP_address", BCON_UTF8(""),
                       "file_name", BCON_UTF8(current_file_name),
                       "time_stamp", BCON_UTF8(time_stamp),
                       "user_session_ID", BCON_UTF8(""),
                       "comment", BCON_UTF8(event)
                       );
    
    if (!mongoc_collection_insert_one (coll, insert, NULL, NULL, &error)) {
      fprintf (stderr, "%s\n", error.message);
    }


    bson_destroy (insert);

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();

    
    return 0;
}




int save_images (int start_blok, int stop_blok, int image_count, char *account,char*corpus,int wf, char *ws_fp) {
    
    int i = 0;
    char file_type[200];
    char new_image[200];
    char current_img[200];
    FILE *fileptr, *writeptr;
    int filelen;
    char *buffer;
    char new_fp[200];
    int new_img_counter=0;
    char tmp_str[100];
    
    // start at current image_count
    
    new_img_counter += image_count;
    
    for (i=start_blok; i< stop_blok; i++) {
        
        if (strcmp(Bloks[wf][i].content_type,"image") == 0) {
            
            strcpy(file_type,get_file_type(Bloks[wf][i].relationship));
            sprintf(new_image,"image%d.",new_img_counter);
            strcat(new_image,file_type);
            
            strcpy(current_img, ws_fp);
            sprintf(tmp_str,"%d/%s", wf, Bloks[wf][i].relationship);
            strcat(current_img,tmp_str);
            
            // helpful:   printf("in save image loop-%d-%s \n", i,current_img);
            // helpful:   printf("relationship: %s \n", Bloks[wf][i].relationship);
            
            strcpy(new_fp,global_image_fp);
            
            // representative path:  bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
            
            strcpy(Bloks[wf][i].relationship,new_image);
            
            fileptr = fopen(current_img, "rb");
            
            // error on some files in fseek - can't find end of file???
            // may be NULL pointer error
            // may be error in .relationship, e.g., "266.png" - added 6???
            
            
            fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
            filelen = ftell(fileptr);             // Get the current byte offset in the file
            rewind(fileptr);                      // Jump back to the beginning of the file

            buffer = (char *)malloc(filelen * sizeof(char)); // Enough memory for the file
            fread(buffer, filelen, 1, fileptr); // Read in the entire file
            fclose(fileptr); // Close the file    FILE *fout = fopen(fp, "rb");
            
            // now, need to write the file to the new location
            
            strcat(new_fp,new_image);
            
            writeptr = fopen(new_fp,"wb");
            fwrite(buffer, filelen,1, writeptr);
            fclose(writeptr);
            
            free(buffer);
            
            new_img_counter ++;
            
            if (debug_mode == 1) {
                ("update: created new image-%d-%d-%s \n",i, new_img_counter, new_image);
            }
            
            strcpy(Bloks[wf][i].relationship,new_image);
        }
    }
    
    return new_img_counter;
}



int save_images_alt (int start_blok, int stop_blok, int image_count, char *account,char*corpus,int wf, char *ws_fp) {
    
    int i = 0;
    char file_type[200];
    char new_image[200];
    char current_img[200];
    FILE *fileptr, *writeptr;
    int filelen;
    char *buffer;
    char new_fp[200];
    int new_img_counter=0;
    char tmp_str[100];
    
    // start at current image_count
    
    new_img_counter += image_count;
    
    for (i=start_blok; i< stop_blok; i++) {
        
        if (strcmp(Bloks[wf][i].content_type,"image") == 0) {
            
            strcpy(file_type,get_file_type(Bloks[wf][i].relationship));
            
            //  * inserting key change from save_images *
            sprintf(new_image,"image%d_%d.",master_doc_tracker,new_img_counter);
            //  * end - key change - *
            
            strcat(new_image,file_type);
            
            strcpy(current_img, ws_fp);
            sprintf(tmp_str,"%d/%s", wf, Bloks[wf][i].relationship);
            strcat(current_img,tmp_str);
            
            // helpful:   printf("in save image loop-%d-%s \n", i,current_img);
            // helpful:   printf("relationship: %s \n", Bloks[wf][i].relationship);
            
            strcpy(new_fp,global_image_fp);
            
            // representative path:  bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
            
            strcpy(Bloks[wf][i].relationship,new_image);
            
            fileptr = fopen(current_img, "rb");
            
            // error on some files in fseek - can't find end of file???
            // may be NULL pointer error
            // may be error in .relationship, e.g., "266.png" - added 6???
            
            
            fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
            filelen = ftell(fileptr);             // Get the current byte offset in the file
            rewind(fileptr);                      // Jump back to the beginning of the file

            buffer = (char *)malloc(filelen * sizeof(char)); // Enough memory for the file
            fread(buffer, filelen, 1, fileptr); // Read in the entire file
            fclose(fileptr); // Close the file    FILE *fout = fopen(fp, "rb");
            
            // now, need to write the file to the new location
            
            strcat(new_fp,new_image);
            
            writeptr = fopen(new_fp,"wb");
            fwrite(buffer, filelen,1, writeptr);
            fclose(writeptr);
            
            free(buffer);
            
            new_img_counter ++;
            
            if (debug_mode == 1) {
                ("update: created new image-%d-%d-%s \n",i, new_img_counter, new_image);
            }
            
            strcpy(Bloks[wf][i].relationship,new_image);
        }
    }
    
    return new_img_counter;
}





int update_counters (char *account, char *corpus, int blok_count, int doc_count, int image_count, char *account_fp) {
 
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_fp);
    
    // representative path:
    //      /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",account,corpus);
    
    // updates master_count file associated with the library
    // convenient way to 'pass note' between python and C
    // ultimately - this could be replaced with count table in database
    
    FILE *fout = fopen(fp_path_to_master_counter,"w");
    fprintf(fout,"%s,","account_name");
    fprintf(fout,"%s,","corpus_name");
    fprintf(fout,"%s,","block_ID");
    fprintf(fout,"%s,","doc_ID");
    fprintf(fout,"%s\n","image_ID");
    
    fprintf(fout,"%s,",account);
    fprintf(fout,"%s,",corpus);
    fprintf(fout,"%d,",blok_count);
    fprintf(fout,"%d,",doc_count);
    fprintf(fout,"%d\n",image_count);
    
    fclose(fout);
    
    return 0;
}



//                                  ZIP HANDLER
//
//      Used as the first step in processing for all Office documents
//


int handle_zip (char *fp, int t, char *ws_fp) {
    
    char *my_error=NULL;
    zip_file_t *my_file;
    
    // NOTE:  MAX LIMIT hard-coded - 80M
    // ...could be increased or explore other alternatives
    
    int COPY_BUF_SIZE = 80000000;
    
    
    char *my_file_buffer;
    
    zip_t* my_zip;
    int zip_entries;
    char *zip_name;
    int i=0;
    char t_str[20];
    int bytes_read = 0;
    FILE *write_ptr;
    
    int slide_count = 0;
    char short_name[100];
    char working_folder[500];
    char working_fp_tmp[500];
    char fp_tmp_copy[500];
    my_file_buffer = (char*)malloc(COPY_BUF_SIZE);
    
    strcpy(working_folder,ws_fp);
    
    sprintf(t_str,"%d",t);
    strcat(working_folder,t_str);
    strcat(working_folder,"/");
    
    my_zip = zip_open(fp,0,&my_error);
    
    zip_entries = zip_get_num_entries(my_zip,&my_error);

    my_doc[t].xml_files = zip_entries;
    
    // adapt my_doc[t].file_name = should be file_name only, not full_path

    strcpy(fp_tmp_copy,fp);
    strcpy(my_doc[t].file_name,get_file_name(fp_tmp_copy));
    
    // Step 2 - iterate through all zip entries and save as needed
    
    for (i=0; i < zip_entries; i++) {
        zip_name = zip_get_name(my_zip,i,&my_error);
        
        // Useful to print out to get understanding of the xml components
        //printf("iter zip-%s-%d \n",zip_name,i);
        
        if (
            (strstr(zip_name,"ppt/slides/slide") != NULL) ||
            (strstr(zip_name,"ppt/slides/_rels/slide") != NULL) ||
            (strstr(zip_name,"ppt/slideLayouts/") != NULL) ||
            (strstr(zip_name,"ppt/slideMasters/") != NULL) ||
            (strstr(zip_name,"ppt/media/image") != NULL) ||
            (strstr(zip_name,"docProps/") != NULL)  ||
            (strcmp(zip_name,"word/document.xml")==0) ||
            (strstr(zip_name,"word/media/image") != NULL) ||
            (strcmp(zip_name,"word/_rels/document.xml.rels")==0) ||
            (strcmp(zip_name,"xl/sharedStrings.xml")==0) ||
            (strcmp(zip_name,"xl/workbook.xml")==0) ||
            (strcmp(zip_name,"xl/styles.xml")==0) ||
            (strcmp(zip_name,"xl/tables/table.xml")==0) ||
            (strstr(zip_name,"xl/worksheets/sheet") != NULL) ||
            (strstr(zip_name,"xl/worksheets/_rels/sheet") != NULL) ||
            (strcmp(zip_name,"word/settings.xml")==0) ||
            (strcmp(zip_name,"word/styles.xml")==0)
             )
            
        {
            my_file = zip_fopen_index(my_zip,i,0);
            
            if (my_file == NULL) {
                
                if (debug_mode == 1) {
                    printf ("warning: office_parser - problem opening file name-%s-index-%d \n",zip_name,i);
                    
                }
            }
            
            if (strstr(zip_name,"ppt/slides/slide") != NULL) {
                slide_count ++;}
            
            if (strstr(zip_name,"xl/worksheets/sheet") != NULL) {
                
                if (strcmp(zip_name,"xl/worksheets/sheet.xml")==0) {
                    // may save one sheet without number
                    strcpy(zip_name,"xl/worksheets/sheet1.xml");
                }
                
                slide_count ++;
                
            }
            
            bytes_read = zip_fread(my_file,my_file_buffer,COPY_BUF_SIZE);
            
            if (bytes_read > 79000000) {
                
                // this is semi-arbitrary cap - it could be increased
                // potential safeguard against a user maliciously attacking the account with mega-files
                
                printf("warning: office_parser - file exceeds 80MB - too large to process - will skip this file.");
                
                my_doc[t].xml_files = -2;
            }
            
            strcpy(short_name,get_file_name(zip_name));
            strcpy(working_fp_tmp,working_folder);
            strcat(working_fp_tmp,short_name);
            
            write_ptr = fopen(working_fp_tmp,"wb");
            fwrite(my_file_buffer, bytes_read, 1,write_ptr);
            fclose(write_ptr);
            zip_fclose(my_file);
            
            }
        }
    
    my_doc[t].slide_count = slide_count;
    
    free(my_file_buffer);
    zip_discard(my_zip);
    
    return slide_count;
}


//
//                          PARSE ONE DOCUMENT - MAIN ROUTER

//      builder is the core router based on document type


int builder(char *fp,int thread_number,int slide_count, char *ws_fp) {
    
    char file_type[200];
    int bloks_out =0;
    int found = 0;
    
    strcpy(file_type,get_file_type(fp));
    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
        
        bloks_out = pptx_build_index(thread_number, slide_count, ws_fp);
        //xmlCleanupMemory();
        found = 1;
        
    }
    
    if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {

        bloks_out = doc_build_index(thread_number, slide_count, ws_fp);
        //xmlCleanupMemory();
        found = 1;
        
    }
    
    if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {

        bloks_out = xl_build_index(thread_number, slide_count, ws_fp);
        //xmlCleanupMemory();
        found = 1;
        
    }

    return bloks_out;
}



/*
int builder_old_works (char *fp,int thread_number,int slide_count, char *ws_fp) {
    
    char file_type[200];
    int bloks_out =0;
    int found = 0;
    
    strcpy(file_type,get_file_type(fp));
    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
        
        bloks_out = pptx_build_index(thread_number, slide_count, ws_fp);
        xmlMemoryDump();
        found = 1;
        
    }
    
    if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {

        bloks_out = doc_build_index(thread_number, slide_count, ws_fp);
        xmlMemoryDump();
        found = 1;
        
    }
    
    if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {

        bloks_out = xl_build_index(thread_number, slide_count, ws_fp);
        xmlMemoryDump();
        found = 1;
        
    }

    return bloks_out;
}
*/

/*
int *thread_function(document *my_args, char *ws_fp) {
    
    // initialize variables
    
    int dummy=0;
    int thread_number;
    int xml_files;
    int slide_count;
    int bloks_created;
    int iter_count;
    int new_images = 0;
    
    char *event_comment;
    char *ae_type;
    
    // get thread_number
    thread_number = my_args->thread_num;
    xml_files = my_args->xml_files;

    if (xml_files > 0) {
        
        slide_count = my_args->slide_count;
        iter_count = my_args->iter;
        
        if (debug_mode == 1) {
            printf("update:  in thread function- calling builder-%d-%d-%d \n",thread_number,slide_count,xml_files);
        }
        
        
        // builder is the main call
        
        bloks_created = builder(my_args->file_name,thread_number,slide_count,ws_fp);
        
        // success = bloks_created > 0
        
        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update:  bloks_created - %d \n", bloks_created);
            }
            
            pthread_mutex_lock( &mutex1 );
            
            new_images = save_images (0, bloks_created, master_image_tracker, my_args->account_name,my_args->corpus_name, thread_number,ws_fp);

            dummy = write_to_db(0,bloks_created,my_args->account_name,my_args->corpus_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);

            master_doc_tracker ++;
            master_blok_tracker = master_blok_tracker + bloks_created;
            master_image_tracker = new_images;
            
            pthread_mutex_unlock ( &mutex1 );
        }
        
        else {
            
            // bloks_created == 0 -> file is essentially being skipped
            // not clear why
            
            event_comment = "no bloks identified in document - could be empty or corrupted document or parsing error";
            
            ae_type = "REJECTED_FILE_OFFICE";
            
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comment,
                                                 my_args->account_name,
                                                 my_args->corpus_name,
                                                 "",
                                                 time_stamp,
                                                 my_args->file_name);
            
        }
        }
    
    // multi-threading mutex options below (commented-out)- NOT USED
    //          pthread_mutex_lock( &mutex1 );
    //          thread_counter ++;
    //          my_local_counter = thread_counter;
    //          pthread_mutex_unlock( &mutex1 );
    
    return 0;
}
*/


// Our own XML Error Handler to filter out the XML Free/Malloc Errors that were polluting our logs
void xmlErrorHandler(void *ctx, const char * msg, ...) {
    if (strstr(msg,"xmlMemFree") == NULL && strstr(msg,"xmlMallocBreakpoint") == NULL && strstr(msg,"Memory tag error occurs") == NULL) {
        printf("%s",msg);
    }
}


//                          MAIN FUNCTION - add_files_main - entry point
//

int add_files_main  (char *input_account_name,
                     char *input_library_name,
                     char *input_fp,
                     char *workspace_fp,
                     char *account_master_fp,
                     char *input_mongodb_path,
                     char *image_fp,
                     int input_debug_mode) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    GLOBAL_WRITE_TO_DB = 1;
    
    global_write_to_filename = "office_text_blocks_out.txt";
    
    global_total_pages_added = 0;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    //initGenericErrorDefaultFunc(do_nothing_function);
    
    //Register our own XML Error Handler
    xmlSetGenericErrorFunc(NULL, xmlErrorHandler);

    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input library name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    //ws_fp = workspace_fp;
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: office_parser - opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    
    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                // if file in upload_folder is not identified, then it is skipped
                
                if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
                    pptx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
                    docx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
                    xl_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    upload_files_count ++; }
                
                else {
                    
                    // file extension type not found
                    // key action:  skip the file from any further parsing
                    // key action:  register as account event
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - did not find a validate file type - %s \n", ent->d_name);
                    }
                    
                    event_comments = "file type not valid";
                    
                    ae_type = "REJECTED_FILE_OFFICE";
                    
                    dummy = register_ae_add_office_event(ae_type,
                                                         event_comments,
                                                         input_account_name,
                                                         input_library_name,
                                                         "",
                                                         time_stamp,
                                                         ent->d_name);
                    
                }
                
            }}}
    
    closedir (web_dir);
    free(ent);
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    for (i=0; i < upload_files_count ; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%d-%s \n", i,files[i]);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(files[i],thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 50 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 files[iter_count]);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, files[i]);
            
            bloks_created = builder(files[i],thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update:  office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - processed docx files-%d \n", docx_counter);
        printf("summary: office_parser - processed xlsx files-%d \n", xl_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}


int add_files_main_customize  (char *input_account_name,
                               char *input_library_name,
                               char *input_fp,
                               char *workspace_fp,
                               char *account_master_fp,
                               char *input_mongodb_path,
                               char *image_fp,
                               int input_debug_mode,
                               int write_to_db_on,
                               char *write_to_filename,
                               char *file_source_path) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    global_write_to_filename = write_to_filename;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    //initGenericErrorDefaultFunc(do_nothing_function);
    
    //Register our own XML Error Handler
    xmlSetGenericErrorFunc(NULL, xmlErrorHandler);

    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input library name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    //ws_fp = workspace_fp;
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    
    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                // if file in upload_folder is not identified, then it is skipped
                
                if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
                    pptx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
                    docx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
                    xl_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    upload_files_count ++; }
                
                else {
                    
                    // file extension type not found
                    // key action:  skip the file from any further parsing
                    // key action:  register as account event
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - did not find a validate file type - %s \n", ent->d_name);
                    }
                    
                    event_comments = "file type not valid";
                    
                    ae_type = "REJECTED_FILE_OFFICE";
                    
                    dummy = register_ae_add_office_event(ae_type,
                                                         event_comments,
                                                         input_account_name,
                                                         input_library_name,
                                                         "",
                                                         time_stamp,
                                                         ent->d_name);
                    
                }
                
            }}}
    
    closedir (web_dir);
    free(ent);
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    for (i=0; i < upload_files_count ; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%d-%s \n", i,files[i]);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(files[i],thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 files[i]);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, files[i]);
            
            bloks_created = builder(files[i],thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                

                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
 
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - processed docx files-%d \n", docx_counter);
        printf("summary: office_parser - processed xlsx files-%d \n", xl_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}



// new entry point to support unique_doc_number - passed to FOLDER and parse multiple files in folder
// *** note: this is the main entry point used by the worker ***

int add_files_main_customize_parallel  (char *input_account_name,
                                        char *input_library_name,
                                        char *input_fp,
                                        char *workspace_fp,
                                        char *account_master_fp,
                                        char *input_mongodb_path,
                                        char *image_fp,
                                        int input_debug_mode,
                                        int write_to_db_on,
                                        char *write_to_filename,
                                        char *file_source_path,
                                        int unique_doc_num) {
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    global_write_to_filename = write_to_filename;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    //initGenericErrorDefaultFunc(do_nothing_function);
    
    //Register our own XML Error Handler
    xmlSetGenericErrorFunc(NULL, xmlErrorHandler);

    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    

    // char fp_path_to_master_counter[200];
    
    //ws_fp = workspace_fp;
    
    //strcpy(fp_path_to_master_counter,"");
    //strcat(fp_path_to_master_counter,account_master_fp);
    
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    int total_docs_created_local = 0;
    int total_bloks_created_local = 0;
    int total_img_created_local = 0;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    //strcpy(f_in, fp_path_to_master_counter);
    
    
    //  * key changes start here *
    
    master_blok_tracker = 0;
    master_doc_tracker = 0;
    master_image_tracker = 0;
    
    //  * end - key changes - *
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    
    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                // if file in upload_folder is not identified, then it is skipped
                
                if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
                    pptx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
                    docx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
                    xl_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    upload_files_count ++; }
                
                else {
                    
                    // file extension type not found
                    // key action:  skip the file from any further parsing
                    // key action:  register as account event
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - did not find a validate file type - %s \n", ent->d_name);
                    }
                    
                    event_comments = "file type not valid";
                    
                    ae_type = "REJECTED_FILE_OFFICE";
                    
                    dummy = register_ae_add_office_event(ae_type,
                                                         event_comments,
                                                         input_account_name,
                                                         input_library_name,
                                                         "",
                                                         time_stamp,
                                                         ent->d_name);
                    
                }
                
            }}}
    
    closedir (web_dir);
    free(ent);
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    for (i=0; i < upload_files_count ; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%d-%s \n", i,files[i]);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(files[i],thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 files[i]);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, files[i]);
            
            master_doc_tracker = pull_new_doc_id(input_account_name,input_library_name);
                
            // very simple triage - will need to enhance over time
            
            if (master_doc_tracker < 1) {
                // something went wrong - should return +1 at a minimum as "after" response
                // setting a large, arbitrary integer so we can identify a problem post-parsing...
                master_doc_tracker = 1000000;
            }
            
            if (debug_mode == 1) {
                printf("update: office_parser - pull_new_doc_id call - master_doc_tracker - new doc id = %d \n", master_doc_tracker);
            }
            
            bloks_created = builder(files[i],thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images_alt (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                

                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
 
                //master_doc_tracker ++;          // starts at passed unique num
                total_docs_created_local ++;    // starts at 0
                
                total_bloks_created_local += bloks_created;
                master_blok_tracker = 0;
                
                total_img_created_local += new_images;
                master_image_tracker = 0;
                
                //master_blok_tracker = master_blok_tracker + bloks_created;
                //master_image_tracker = new_images;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    //doc_count = master_doc_tracker;
    //image_count = master_image_tracker;
    //blok_count = master_blok_tracker;
    
    dummy = update_library_inc_totals(input_account_name,input_library_name,total_docs_created_local,total_bloks_created_local, total_img_created_local, global_total_pages_added, 0);
    

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - processed docx files-%d \n", docx_counter);
        printf("summary: office_parser - processed xl files-%d \n", xl_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", total_bloks_created_local);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}



int add_one_office_main  (char *input_account_name,
                        char *input_library_name,
                        char *input_fn,
                        char *input_fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0) || (strcmp(file_type,"xlsx") ==0) || (strcmp(file_type,"XLSX")==0) || (strcmp(file_type,"DOCX")==0) || (strcmp(file_type,"docx")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 tmp_fp_iter);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}


// new main entry point - 1 file | any type of doc (PPTX/DOCX/XLSX) | parallel - no master_count dependency
//
//     note: new variable -> unique_doc_num

int add_one_office_main_parallel  (char *input_account_name,
                                   char *input_library_name,
                                   char *input_fn,
                                   char *input_fp,
                                   char *workspace_fp,
                                   char *account_master_fp,
                                   char *input_mongodb_path,
                                   char *image_fp,
                                   int input_debug_mode,
                                   int write_to_db_on,
                                   char *write_to_filename,
                                   char *file_source_path,
                                   int unique_doc_num) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    
    //  * key changes start here *
    
    master_blok_tracker = 0;
    master_doc_tracker = unique_doc_num;
    master_image_tracker = 0;
    
    //  * end - key changes - *
    

    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0) || (strcmp(file_type,"xlsx") ==0) || (strcmp(file_type,"XLSX")==0) || (strcmp(file_type,"DOCX")==0) || (strcmp(file_type,"docx")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 tmp_fp_iter);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images_alt (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                
                // master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    // dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}



int add_one_pptx_main  (char *input_account_name,
                        char *input_library_name,
                        char *input_fn,
                        char *input_fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 50 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 tmp_fp_iter);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}



int add_one_docx_main  (char *input_account_name,
                        char *input_library_name,
                        char *input_fn,
                        char *input_fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 tmp_fp_iter);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}


int add_one_xlsx_main  (char *input_account_name,
                        char *input_library_name,
                        char *input_fn,
                        char *input_fp,
                        char *workspace_fp,
                        char *account_master_fp,
                        char *input_mongodb_path,
                        char *image_fp,
                        int input_debug_mode,
                        int write_to_db_on,
                        char *write_to_filename,
                        char *file_source_path) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: offie_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .xlsx or .XSLX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 50 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 tmp_fp_iter);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker, master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}


// new entry point - nov 25, 2022 - not currently invoked
// adds parameter for target blok_size

int add_files_main_customize_new_blok_size_config  (char *input_account_name,
                                                    char *input_library_name,
                                                    char *input_fp,
                                                    char *workspace_fp,
                                                    char *account_master_fp,
                                                    char *input_mongodb_path,
                                                    char *image_fp,
                                                    int input_debug_mode,
                                                    int write_to_db_on,
                                                    char *write_to_filename,
                                                    char *file_source_path,
                                                    int input_target_blok_size) {
    

    // Main overall function call for "add_files_to_directory"
    // ...currently invoked in library_builder (python)
    
    // Multiple parameters to maximize flexibility of use
    //
    // account_name & library_name
    // input_fp == local file path to the directory that contains files to be added
    // workspace_fp -> typically linked to session_user_fp
    // account_master_fp -> path to find master files lookup
    
    // debug_mode -> determines print-out of info to console for debugging

    // Designed to accept different file types and route to appropriate handler
    

    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    //
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    global_write_to_filename = write_to_filename;
    
    // new parameter - global target blok size - used in both PPTX and DOCX
    GLOBAL_BLOK_SIZE = input_target_blok_size;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    //initGenericErrorDefaultFunc(do_nothing_function);
    
    //Register our own XML Error Handler
    xmlSetGenericErrorFunc(NULL, xmlErrorHandler);

    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    char fp_path_to_master_counter[200];
    
    //ws_fp = workspace_fp;
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter,account_master_fp);
    
    /* representative path:  /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_library_name);
    */
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    strcpy(f_in, fp_path_to_master_counter);
    
    // lookup master counters from file path
    
    FILE *master_counter = fopen(f_in,"r");
    fscanf(master_counter,"%[^,],",header_account);
    fscanf(master_counter,"%[^,],",header_collection);
    fscanf(master_counter,"%[^,],",header_blok_count);
    fscanf(master_counter,"%[^,],",header_doc_count);
    fscanf(master_counter,"%s\n",header_image_count);
    
    fscanf(master_counter,"%[^,],",account);
    fscanf(master_counter,"%[^,],",collection);
    fscanf(master_counter,"%d,",&blok_count);
    fscanf(master_counter,"%d,",&doc_count);
    fscanf(master_counter,"%d,",&image_count);
    
    fclose(master_counter);
    
    // closing - master counters file
    
    if (debug_mode == 1) {
        //printf("update: opened master counter-account-%s-collection-%s-blok-%d-doc-%d-image-%d \n",account,collection,blok_count,doc_count,image_count);
    }
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    
    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                // if file in upload_folder is not identified, then it is skipped
                
                if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
                    pptx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
                    docx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
                    xl_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    upload_files_count ++; }
                
                else {
                    
                    // file extension type not found
                    // key action:  skip the file from any further parsing
                    // key action:  register as account event
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - did not find a validate file type - %s \n", ent->d_name);
                    }
                    
                    event_comments = "file type not valid";
                    
                    ae_type = "REJECTED_FILE_OFFICE";
                    
                    dummy = register_ae_add_office_event(ae_type,
                                                         event_comments,
                                                         input_account_name,
                                                         input_library_name,
                                                         "",
                                                         time_stamp,
                                                         ent->d_name);
                    
                }
                
            }}}
    
    closedir (web_dir);
    free(ent);
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    for (i=0; i < upload_files_count ; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%d-%s \n", i,files[i]);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(files[i],thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
            event_comments = "file ZIP size greater than 80 MB max";
            ae_type = "REJECTED_FILE_OFFICE";
            dummy = register_ae_add_office_event(ae_type,
                                                 event_comments,
                                                 input_account_name,
                                                 input_library_name,
                                                 "",
                                                 time_stamp,
                                                 files[i]);
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, files[i]);
            
            bloks_created = builder(files[i],thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
 
                master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count, account_master_fp);

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - processed docx files-%d \n", docx_counter);
        printf("summary: office_parser - processed xlsx files-%d \n", xl_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}


// new entry point starts here - september 13, 2023

int add_one_office (char *input_account_name,
                    char *input_library_name,
                    char *input_fp,
                    char *input_fn,
                    char *workspace_fp,
                    char *image_fp,
                    char *write_to_filename) {
    
 
    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    //char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;

    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    
    //global_mongo_db_path = input_mongodb_path;
    
    debug_mode = 0;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input corpus name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    // *** Based on the inputs, lookup the master_counters file
    // key information:  blok_counter, image_counter, doc_counter
    
    // note: these counters will be updated at the end of processing
    
    //char fp_path_to_master_counter[200];
    //strcpy(fp_path_to_master_counter,"");
    //strcat(fp_path_to_master_counter,account_master_fp);
    
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int go_ahead_check = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    FILE *bloks_out;
    
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    //strcpy(f_in, fp_path_to_master_counter);
    
    
    //  * key changes start here *
    
    master_blok_tracker = 0;
    master_doc_tracker = 0;
    master_image_tracker = 0;
    
    //  * end - key changes - *
    

    global_total_pages_added = 0;
    
    GLOBAL_WRITE_TO_DB = 0;
    global_write_to_filename = write_to_filename;
    
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_fn));
                    
    if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0) || (strcmp(file_type,"xlsx") ==0) || (strcmp(file_type,"XLSX")==0) || (strcmp(file_type,"DOCX")==0) || (strcmp(file_type,"docx")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_fn);
        
        //printf("processing fp-%s \n",tmp_fp_iter);
        
        go_ahead_check = 1;
        
    }
    
    else {
        go_ahead_check = -1;
    }
    
    // safety check #2 - confirm that file exists in file path
    
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        //printf("error: file not found - %s \n", tmp_fp_iter);
        
        go_ahead_check = -2;
    }
    
    fclose(bloks_out);
    
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    if (go_ahead_check > 0) {

        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%s \n", tmp_fp_iter);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        slide_count = handle_zip(tmp_fp_iter,thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, tmp_fp_iter);
            
            bloks_created = builder(tmp_fp_iter,thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser - blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images_alt (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                
                
                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
                
                
                // master_doc_tracker ++;
                master_blok_tracker = master_blok_tracker + bloks_created;
                master_image_tracker = new_images;
                
                upload_files_count ++;
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    doc_count = master_doc_tracker;
    image_count = master_image_tracker;
    blok_count = master_blok_tracker;
    
    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary: office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary: office_parser - total blocks created - %d \n", blok_count);
        printf("summary: office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: office_parser - Office XML Parsing - Finished - add_docs - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;

}


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
                            char *db_pw) {
    
    // clock variables to track processing time
    clock_t start_t, end_t;
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);

    // initialize multiple file path local variables
    int dummy = 0;
    char file_type[100];
    int slide_count;
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    char doc_file_fp[300];
    char f_in[200];
    
    int blok_count, doc_count, image_count;
    
    char* file_name_tmp;
    
    // debug_mode defined as a global variable, so does not need to be passed to each function
    // debug_mode is defined by the input_debug_mode
    //      = 1 -> verbose print-out to console
    //      = 0 -> minimal print-out to console
    
    // assign the input filepaths to global variables
    global_image_fp = image_fp;
    global_workspace_fp = workspace_fp;
    global_mongo_db_path = input_mongodb_path;
    
    debug_mode = input_debug_mode;
    
    global_total_pages_added = 0;
    global_total_tables_added = 0;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    global_write_to_filename = write_to_filename;
    
    // set time
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    //initGenericErrorDefaultFunc(do_nothing_function);
    
    //Register our own XML Error Handler
    xmlSetGenericErrorFunc(NULL, xmlErrorHandler);

    
    // *** INPUT START ***

    if (debug_mode == 1) {
        printf ("update: office_parser - input account name-%s \n", input_account_name);
        printf ("update: office_parser - input library name-%s \n", input_library_name);
        printf ("update: office_parser - input file path-%s \n", input_fp);
        printf ("update: office_parser - input workspace path-%s \n", workspace_fp);
    }
    
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    int new_images = 0;
    int thread_number = 1;
    int pptx_counter = 0;
    int docx_counter = 0;
    int xl_counter = 0;
    int found = 0;
    int i,j = 0;
    int bloks_created = 0;
    
    int loops = 0;
    int remainder = 0;
    int iter_count = 0;
    
    char *event_comments;
    char *ae_type;
    
    int total_docs_created_local = 0;
    int total_bloks_created_local = 0;
    int total_img_created_local = 0;
    
    // start change here
    char local_short_name_copy[300];
    // end change here
        
    // theoretical MAX of 5000 files -> this can be expanded easily
    char files[5000][300];
    
    
    //  * key changes start here *
    
    master_blok_tracker = 0;
    master_doc_tracker = 0;
    master_image_tracker = 0;
    
    // start change here
    strcpy(local_short_name_copy,"");
    // end change here
    
    //  * end - key changes - *
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    //                      *** Processing Starts here ***

    // Main Loop through document
    // Need to add safety check to confirm file type = .docx or .DOCX
    
    
    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                // if file in upload_folder is not identified, then it is skipped
                
                if ((strcmp(file_type,"pptx")==0) || (strcmp(file_type,"PPTX")==0)) {
                    pptx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"docx")==0) || (strcmp(file_type,"DOCX")==0)) {
                    docx_counter ++;
                    found = 1;}
                
                if ((strcmp(file_type,"xlsx")==0) || (strcmp(file_type,"XLSX")==0)) {
                    xl_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    upload_files_count ++; }
                
                else {
                    
                    // file extension type not found
                    // key action:  skip the file from any further parsing
                    // key action:  register as account event
                    
                    if (debug_mode == 1) {
                        printf("update: office_parser - did not find a validate file type - %s \n", ent->d_name);
                    }
                    
                }
                
            }}}
    
    closedir (web_dir);
    free(ent);
    
    // start clock with main loop processing
    start_t = clock();
    
    thread_number = 0;
    
    for (i=0; i < upload_files_count ; i++) {
        
        if (debug_mode == 1) {
            printf("update: office_parser - processing fp-%d-%s \n", i,files[i]);
        }
        
        // unzip the file and unpack into work space
        // key function call -> opens the office xml document using zip
        //      --unpacks the .pptx/.xlsx/.docx into multiple .xml files
        
        // start change here
        strcpy(local_short_name_copy,files[i]);
        strcpy(my_doc[0].file_short_name, get_file_name(local_short_name_copy));
        // end change here
        
        slide_count = handle_zip(files[i],thread_number, workspace_fp);
        
        // error_handling - if problem with zip - need to catch here and skip processing
        
        if (my_doc[thread_number].xml_files == -2) {
            
            // first error:   zip too large - sends -2 error
            printf("warning: office_parser - problem with ZIP file MAX size exceeded \n");
            
        }
        
        else {
            
            
            // save key variables into my_doc global variable
            
            strcpy(my_doc[thread_number].corpus_name,input_library_name);
            strcpy(my_doc[thread_number].account_name,input_account_name);
            my_doc[thread_number].thread_num = thread_number;
            my_doc[thread_number].image_count_start = image_count;
            my_doc[thread_number].iter  = iter_count;
            strcpy(my_doc[thread_number].file_name, files[i]);
            
            
            // start - key input parameter - determines behavior for allocating document ids
            
            if (unique_doc_num < 0) {
                master_doc_tracker = pull_new_doc_id(input_account_name,input_library_name);
            }
            else {
                master_doc_tracker = unique_doc_num + total_docs_created_local;
            }
            
            // end - key input parameter
            
            
            // very simple triage - will need to enhance over time
            
            if (master_doc_tracker < 1) {
                // something went wrong - should return +1 at a minimum as "after" response
                // setting a large, arbitrary integer so we can identify a problem post-parsing...
                master_doc_tracker = 1000000;
            }
            
            if (debug_mode == 1) {
                printf("update: office_parser - pull_new_doc_id call - master_doc_tracker - new doc id = %d \n", master_doc_tracker);
            }
            
            bloks_created = builder(files[i],thread_number,slide_count, workspace_fp);
            
            if (bloks_created > 0) {
                
                if (debug_mode == 1) {
                    printf("update: office_parser -blocks_created - %d \n", bloks_created);
                }
                
                new_images = save_images_alt (0, bloks_created, master_image_tracker, input_account_name,input_library_name, thread_number,workspace_fp);
                

                if (GLOBAL_WRITE_TO_DB == 1) {
                    dummy = write_to_db(0,bloks_created,input_account_name,input_library_name,master_doc_tracker,master_blok_tracker, thread_number, time_stamp);
                }
                
                else {
                    dummy = write_to_file(0,bloks_created,input_account_name,input_library_name, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename,0);
                }
 
                total_docs_created_local ++;    // starts at 0
                total_bloks_created_local += bloks_created;
                total_img_created_local += new_images;
                
                master_blok_tracker = 0; // starts at zero with each new document
                master_image_tracker = 0;  // starts at zero with each new document
                
                
            }
            
        }}
    
    // iterated through entire processing file -> processed all files
    // all artifacts saved in database + images in file directory
    
    // last step = update and save counters
    // each of the master trackers are GLOBAL VARIABLES updated in different steps
    
    //doc_count = master_doc_tracker;
    //image_count = master_image_tracker;
    //blok_count = master_blok_tracker;
    
    if (GLOBAL_WRITE_TO_DB == 1) {
        
        dummy = update_library_inc_totals(input_account_name,input_library_name,total_docs_created_local,total_bloks_created_local, total_img_created_local, global_total_pages_added, global_total_tables_added);
    }
    

    // time @ end
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary:  office_parser - processed pptx files-%d \n", pptx_counter);
        printf("summary:  office_parser - processed docx files-%d \n", docx_counter);
        printf("summary:  office_parser - processed xlsx files-%d \n", xl_counter);
        printf("summary:  office_parser - total processed upload files-%d \n", upload_files_count);
        printf("summary:  office_parser - total blocks created - %d \n", total_bloks_created_local);
        printf("summary:  office_parser - total images creates - %d \n", total_img_created_local);
        printf("summary:  office_parser - total tables created - %d \n", global_total_tables_added);
        printf("summary:  office_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary:  office_parser - Office XML Parsing - Finished - add files - time elapsed - %f \n",total_t);
    }
    
    xmlCleanupParser();
    
    return global_total_pages_added;
}
