
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

//  pdf_parser_llmware.c
//  pdf_parser_llmware
//
//  Created by Darren Oberst on 2/8/22.


#include "pdf_parser_llmware.h"


// new function -september 2022 - common rules to evaluate text distance

int evaluate_text_distance (float x, float y, float x_last, float y_last, int font_size_state, float tm_scaler) {
    
    float distance_x, distance_y;
    int action = 0;
    
    //float bt_char_text_space = 0;
    //int char_buffer_assumption = 50;
    
    // 3 possible outcomes
    //  0 - no action
    //  1 - add space
    //  2 - start new block
    
    //distance_x = fabsf(x - x_last);
    
    distance_x = x - x_last;
    
    distance_y = fabsf(y - y_last);
    
    if (distance_y > 100) {
        action = 2;
        //printf("update: evaluate_text_distance-> BIG: %f-%f \n", y,y_last);
    }
    
    else {
        
        // what is correct distance_y to insert space?
        // 10 is stable
        // experimenting with 5
        
        if (distance_y > 5 || distance_y > (font_size_state*tm_scaler)) {
            action = 1;
            //printf("update: evaluate text distance-> SPACE: %f-%f \n", y, y_last);
        }
        else {
            
            // *** experiment - march 17, 2023 - remove 'x' test ***
            //  -- will use different distance_x tests inside () | <> | [] handling for space insertion
            
            // bt_char_text_space = (char_size * bt_char_count) + char_buffer_assumption;
            // experiment - with 25 or more ?
            
            // check for extreme negative case as well - experiment
            
            // solid rule
            // e.g., distance_x > 50 || distance_x < -50
            
            // steady rule = 5*
            // experiment - on nov 12 - try 2*
            // used 2* from nov 12 - jan 26
            
            /*
            if (distance_x > (5*font_size_state*tm_scaler)) {
                action = 1;
            }
             */
            
            /*
            if (distance_x > (5*font_size_state*tm_scaler) || distance_x < (-5*font_size_state*tm_scaler)) {
                action = 1;
                
                //printf("update: distance_x > 50 or -50 - %f-%f \n", x, x_last);
            }
            */
        
        }
    }
    
    // safety check
    
    if (x==0 && y==0) {
        // may have been reset or not captured
        action = 0;
    }
    
    return action;
    
}



int create_new_buffer (int buffer_start, int buffer_stop) {
    
    int x=0;
    int y=0;
    int counter=0;
    int stopper = 0;
    int max_len = 1000000;
    int start_len = 15;
    int start_loop = 15;
    int stream_found = -1;
    
    int dst_len = 15000000;
    flate_dst_tmp_buffer = (unsigned char*)malloc(dst_len * sizeof(unsigned char));
    
    //printf("update: in create_new_buffer - %d - %d \n", buffer_start, buffer_stop);
    
    if ((buffer_stop - buffer_start) > max_len) {
        stopper = buffer_start + max_len;
    }
    else {
        stopper = buffer_stop;
    }
    
    if ((buffer_stop - buffer_start) > start_len) {
        start_loop = start_len;
    }
    else {
        start_len = buffer_stop - buffer_start;
    }
    
    for (y=buffer_start; y < buffer_start + start_len; y++) {
        
        //printf("update: buffer - %d \n", buffer[y]);
        
        if (buffer[y] == 115) {
            if ((y+5) < (buffer_start + start_len)) {
                if (buffer[y+1] == 116) {
                    if (buffer[y+2] == 114) {
                        if (buffer[y+3] == 101) {
                            if (buffer[y+4] == 97) {
                                if (buffer[y+5] == 109) {
                                    
                                    stream_found = y+6-buffer_start;
                                    //printf("update: found stream seq! \n");
                                    
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (stream_found > -1) {
        
        // printf("update: found stream_found starter - %d \n", stream_found);
        
        for (x=buffer_start + stream_found; x < stopper; x++) {
            
            //printf("update: looping - %d - %d - %c \n", x, counter,buffer[x]);
            
            if (buffer[x] == 101) {
                //printf("update: found 101 'e' \n");
                
                if ((x+4) < stopper) {
                    if (buffer[x+1]==110) {
                        if (buffer[x+2] ==100) {
                            if (buffer[x+3] == 115) {
                                // endstream found
                                // printf("update: found endstream seq! \n");
                                break;
                            }
                        }
                    }
                }
            }
            
            flate_dst_tmp_buffer[counter] = buffer[x];
            counter ++;
        }
    }
    
    //printf("update: exiting create_new_buffer - %d \n", counter);
    
    return counter;
}


// new function - get text_state from TD/Td operator
ts* get_text_state_from_td (char* td_window, int td_len) {
    
    // td_window is approximately 25 char string
    
    ts text_state;
    ts* text_state_ptr;
    
    int s;
    int dec_ignore = 0;
    int* num_builder[100];
    int n = 0;
    int z= 0;
    float x=0;
    float y=0;
    float stack[25];
    int st = 0;
    float num_float = 0;
    
    for (s=0; s < td_len ; s++) {
        
        //printf("%c", td_window[s]);
        
        if (td_window[s] > 47 && td_window[s] < 58 && dec_ignore == 0) {
            
            num_builder[n] = td_window[s];
            n++;
        }
        
        if (td_window[s] == 45 && n==0) {
            
            num_builder[n] = td_window[s];
            n++;
        }
        
        if (td_window[s] == 46) {
            
            num_builder[n] = td_window[s];
            n++;
        }
        
        
        if (td_window[s] == 32 || td_window[s] == 10 || td_window[s] == 13) {
            
            num_builder[n] = 0;
            for (z=0; z <= n; z++) {
                
            }
            if (n > 0) {
                
                num_float = get_float_from_byte_array(num_builder);
                
                stack[st] = num_float;
                
                st ++;
                dec_ignore = 0;
                n = 0;
            }
        }
        
        if (td_window[s] == 0) {
            break;
        }
    }
    
    if (n > 0) {
        num_builder[n] = 0;
        num_float = get_float_from_byte_array(num_builder);
        stack[st] = num_float;
        st ++;
    }
    
    if (st >= 2) {
        y = stack[st-1];
        x = stack[st-2];

    }
    
    text_state.x = x;
    text_state.y = y;
    text_state.scaler = 0;
    
    text_state_ptr = &text_state;
    
    return text_state_ptr;
    

}


ts* get_text_state (char* tm_window, int tm_len) {
    
    // tm_window is approximately 25 char string
    
    ts text_state;
    ts* text_state_ptr;
    
    int s;
    int dec_ignore = 0;
    int* num_builder[100];
    int n = 0;
    int z= 0;
    float x=0;
    float y=0;
    float scaler = 0;
    float stack[25];
    int st = 0;
    float num_float = 0;
    
    for (s=0; s < tm_len ; s++) {
        
        if (tm_window[s] > 47 && tm_window[s] < 58 && dec_ignore == 0) {
            
            num_builder[n] = tm_window[s];
            n++;
        }
        
        if (tm_window[s] == 45 && n==0) {
            
            num_builder[n] = tm_window[s];
            n++;
        }
        
        if (tm_window[s] == 46) {
            
            num_builder[n] = tm_window[s];
            n++;
        }
        
        
        if (tm_window[s] == 32 || tm_window[s] == 10 || tm_window[s] == 13) {
            
            num_builder[n] = 0;
            for (z=0; z <= n; z++) {
                
            }
            if (n > 0) {
                
                num_float = get_float_from_byte_array(num_builder);
                stack[st] = num_float;
                
                st ++;
                dec_ignore = 0;
                n = 0;
            }
        }
        
        if (tm_window[s] == 0) {
            break;
        }
    }
    
    if (n > 0) {
        num_builder[n] = 0;
        num_float = get_float_from_byte_array(num_builder);
        stack[st] = num_float;
        st ++;
    }
    
    if (st > 2) {
        y = stack[st-1];
        x = stack[st-2];
        scaler = stack[st-3];

    }
    
    text_state.x = x;
    text_state.y = y;
    text_state.scaler = fabs(scaler);
    
    text_state_ptr = &text_state;
    
    return text_state_ptr;
}



int is_new_font (char* font_name, int ref_num) {
    
    // returns +999 if not on existing Font list
    // returns matching_font_number if already exists on Font list
    
    int new = 999;
    int z = 0;
    
    for (z=0 ; z < global_font_count; z++) {
        if (strcmp(Font_CMAP[z].font_name, font_name) == 0) {
            if (Font_CMAP[z].obj_ref1 == ref_num) {
                new = z;
                break;
            }}
    }
    
    // returns 999 if new entry
    // else returns the font_number of the match
    
    return new;
}


char* get_file_name (char*longer_string) {
    
    // longer_string input, e.g., /ppt/media/image32.png
    
    // char fn[200];
    
    // printf("update: longer_string - %s \n", longer_string);
    
    char *fn = NULL;
    char *tokens;
    
    tokens = strtok(longer_string,"/");
    while (tokens != NULL) {
        
        //printf("tokens: %s \n",tokens);
        //strcpy(fn,tokens);
        
        fn = tokens;
        tokens = strtok(NULL,"/");
    }
    
    // printf("update: get_file_name - %s \n", fn);
    
    
    return fn;
}


char *get_file_type (char*full_name) {
    
    //char file_type[200];
    
    char *file_type = NULL;
    
    char local_copy[500];
    char *tokens;
    
    strcpy(local_copy,full_name);
    tokens = strtok(local_copy,".");
    
    while (tokens !=NULL) {
        
        //strcpy(file_type,tokens);
        
        file_type = tokens;
        tokens = strtok(NULL,".");
    }

    return file_type;
}


char* get_string_from_buffer (int buffer_start, int buffer_stop)
{
    
    int max_string_len = 500000;
    char my_string[500025];
    
    char *my_string_ptr;
    
    strcpy(my_string,"");
    char tmp[10];
    int z=0;
    
    int buffer_stop_local = 0;
    
    // safety check -> cap loop at max_string_len
    
    if ((buffer_stop - buffer_start) > max_string_len) {
        
        buffer_stop_local = buffer_start + max_string_len;
        
        //printf("update: in get_string_from_buffer - capped size of get_string-%d-%d-%d \n", buffer_start,buffer_stop, buffer_stop_local);
        
    }
    else {
        buffer_stop_local = buffer_stop;
    }
    
    for (z=buffer_start; z < buffer_stop_local ; z++) {
        
        sprintf(tmp,"%c",buffer[z]);
        strcat(my_string,tmp);
        
    }
    
    my_string_ptr = &my_string;
    
    return my_string_ptr;
}


char* get_string_from_byte_array (int*b[], int len_b)
{
    
    // may need to increase the max string len
    // 10k very stable
    
    int max_string_len = 10000;
    char my_string[10025];
    char* my_string_ptr;
    
    strcpy(my_string,"");
    
    char tmp[20];
    int z=0;
    
    int len_local = 0;
    
    // safety check
    
    if (len_b > max_string_len) {
        len_local = max_string_len;
    }
    else {
        len_local = len_b;
    }
    
    strcpy(tmp,"");
    
    for (z=0; z < len_local; z++) {

        sprintf(tmp,"%c",b[z]);
        strcat(my_string,tmp);
    }
    
    my_string_ptr = &my_string;
    
    return my_string_ptr;
}


int get_int_from_buffer (int start, int stop)
{

    int x=0;
    int max_string_len = 50;
    
    char my_num_str [60];
    
    int my_num;
    strcpy(my_num_str,"");
    char tmp[10];
    
    int stop_local = 0;
    
    // add safety check
    if ((stop - start) > max_string_len) {
        stop_local = start + max_string_len;
    }
    else {
        stop_local = stop;
    }
    
    for (x=start; x <= stop_local; x++) {
        
        if (buffer[x] == 0) {
            break;
        }
        
        if (buffer[x] != 32) {
            sprintf(tmp, "%c", buffer[x]);
            strcat(my_num_str,tmp);
        }

        }
    
    if (strlen(my_num_str) > 0) {
        my_num = atoi(my_num_str);
    }
    else {
        
        // did not find a number - return -1 to signal
        my_num = -1;
    }
    
    return my_num;
}


int nearby_text (int blok_start, int blok_stop, int x, int y, int blok_number) {
    
    // iterate thru Bloks between blok_start & blok_stop
    // get Bloks x/y coords - if nearyby x/y
    // then add all nearby text as core_text and add to Bloks[blok_number]
    
    int b;
    int x_blok;
    int y_blok;
    float distance;
    char core_text[50000];
    char formatted_text[10000];
    
    strcpy(core_text,"");
    strcpy(formatted_text,"");
    
    for (b= blok_start; b <= blok_stop; b++) {
        
        if (b != blok_number) {
            x_blok = Bloks[b].position.x;
            y_blok = Bloks[b].position.y;
            distance = sqrt((x_blok - x)*(x_blok - x) + (y_blok - y) * (y_blok - y));
            
            // can look at alternative ways to measure the distance between bloks
            // useful check for debugging:   printf("checking blok distances - %d - %d -%d - %d - %d \n",x,y, x_blok,y_blok,distance);
            
            if (abs(x_blok - x) < 300 || abs(y_blok - y) < 300 || distance < 700) {
                if ((strlen(core_text) + strlen(Bloks[b].text_run)) < 49999) {
                    strcat(core_text,Bloks[b].text_run);
                    strcat(core_text, " ");
                }
            }
        }}
    
    
    // next, capture the formatted text from the slide
    
    for (b= blok_start; b <= blok_stop; b++) {

        if (strlen(Bloks[b].formatted_text) > 0) {
            if ((strlen(core_text)+strlen(Bloks[b].formatted_text)) < 49000) {
                strcat(core_text,Bloks[b].formatted_text);
                strcat(core_text, " ");
            }
            
            if ((strlen(formatted_text) + strlen(Bloks[b].formatted_text)) < 9900) {
                strcat(formatted_text,Bloks[b].formatted_text);
                strcat(formatted_text, " ");
            }
                
        }
    }

    if (strlen(core_text) > 0 ) {
        strcpy(Bloks[blok_number].text_run,core_text);
    }
    else {
        
        if (strlen(global_headlines) > 0) {
            strcpy(Bloks[blok_number].text_run, global_headlines);
        }
        else {
            strcpy(Bloks[blok_number].text_run, "");
        }
    }
    
    if (strlen(formatted_text) > 0) {
        strcpy(Bloks[blok_number].formatted_text, formatted_text);
    }
    else {
        
        if (strlen(global_headlines) > 0) {
            strcpy(Bloks[blok_number].formatted_text, global_headlines);
        }
        else {
            strcpy(Bloks[blok_number].formatted_text,"");
        }
    }
    
    if (debug_mode == 1) {
        //printf("update: in nearby_text adding to image - %s \n", core_text);
    }
    
    return strlen(core_text);
}


int get_hex_one_digit (int h) {
    
    int d= 0;
    
    //hex digit mapping to ASCII
    // hex "A" - 65 or "a" - 97
    // hex "B" - 66 or "b" - 98
    // hex "C" - 67 or "c" - 99
    // hex "D" - 68 or "d" - 100
    // hex "E" - 69 or "e" - 101
    // hex "F" - 70 or "f" - 102
    
    // digits 0-9 - ASCII - 48-57
    
    if ((h==65) || (h == 97)) {d= 10;}
    if ((h==66) || (h == 98)) {d= 11;}
    if ((h==67) || (h == 99)) {d= 12;}
    if ((h==68) || (h == 100)) {d= 13;}
    if ((h==69) || (h == 101)) {d= 14;}
    if ((h==70) || (h == 102)) {d= 15;}
    
    if ((h > 47) && (h < 58)) {d = h-48;}
    
    return d;
}


int get_hex (int*b[], int len_b) {
    
    int dec = 0;
    
    int dec1 = 0;
    int dec2 = 0;
    int dec3 = 0;
    char dec_char[20];
    char dec1_char[20];
    char dec2_char[20];
    char dec3_char[20];
    
    if (len_b == 2) {
        
        dec = 16 * get_hex_one_digit(b[0]) + get_hex_one_digit(b[1]);
    }
    
    if (len_b == 4) {
        
        if (b[0] == 48 && b[1] == 48) {
            dec = 16 * get_hex_one_digit(b[2]) + get_hex_one_digit(b[3]);
        }
        else
        {
            // useful check to look for exception cases in terms of length of hex field
            // debugging:  printf("first two digits are not zero - will be dec > 255 - %d-%d \n", b[0],b[1]);
            // e.g.,  alternative:  dec = (16 * get_hex_one_digit(b[2])) + get_hex_one_digit(b[3]);
            
            dec = (16*256 * get_hex_one_digit(b[0])) + (256 * get_hex_one_digit(b[1])) + (16 * get_hex_one_digit(b[2])) + get_hex_one_digit(b[3]);
            
            // printf("update: in get_hex - 4 digits - non-zero - %d \n", dec);
            
            // useful check for debugging:  printf("4 digit get_hex - hex out - %d \n", dec);
        }
    }
    
    if (len_b != 2 && len_b != 4) {
        
        // useful for debugging:   printf("unusual hex length - %d \n", len_b);
        
        if (len_b == 8) {
            
            if (b[0] == 48 && b[1] == 48 && b[4] == 48 && b[5] == 48) {
                
                // looks like different pattern, e.g., 00660069
                dec1 = 16 * get_hex_one_digit(b[2]) + get_hex_one_digit(b[3]);
                dec2 = 16 * get_hex_one_digit(b[6]) + get_hex_one_digit(b[7]);
                
                strcpy(dec_char,"");
                strcat(dec_char,"1");
                if (dec1 < 100) {
                    strcat(dec_char,"0");
                }
                if (dec1 < 10) {
                    strcat(dec_char,"0");
                }
                
                sprintf(dec1_char,"%d",dec1);
                strcat(dec_char,dec1_char);
                
                if (dec2 < 100) {
                    strcat(dec_char,"0");
                }
                if (dec2 < 10) {
                    strcat(dec_char,"0");
                }
                
                sprintf(dec2_char,"%d",dec2);
                strcat(dec_char,dec2_char);
                
                dec = atoi(dec_char);
                
                //printf("update: found 8 char hex pattern: %d - ", dec);
                
            }
            
            //useful for debugging:  printf("found 8 byte hex - may be double byte!!");
            
            /*
            dec =
            (256*256*256*16 * get_hex_one_digit(b[0])) +
            (256*256*256 * get_hex_one_digit(b[1])) +
            (256*256*16* get_hex_one_digit(b[2])) +
            (256*256 * get_hex_one_digit(b[3])) +
            (256*16 * get_hex_one_digit(b[4])) +
            (16*16 * get_hex_one_digit(b[5])) +
            (16 * get_hex_one_digit(b[6])) +
            (get_hex_one_digit(b[7]));
            */
            
            // useful for debugging:   printf("outputing two byte big - %d \n", dec);
        }
    }
    
    if (len_b == 12) {
        
        if (debug_mode == 1) {
            //printf("update: found a long 12 digit hex - special encoding!!!! \n");
        }
        
        //dec = 64259;
        
        if (b[0] == 48 && b[1] == 48 && b[4] == 48 && b[5] == 48 && b[8] == 48 && b[9] == 48) {
            
            // looks like different pattern, e.g., 006600690069
            
            dec1 = 16 * get_hex_one_digit(b[2]) + get_hex_one_digit(b[3]);
            dec2 = 16 * get_hex_one_digit(b[6]) + get_hex_one_digit(b[7]);
            dec3 = 16 * get_hex_one_digit(b[10]) + get_hex_one_digit(b[11]);
            
            strcpy(dec_char,"");
            strcat(dec_char,"1");
            if (dec1 < 100) {
                strcat(dec_char,"0");
            }
            sprintf(dec1_char,"%d",dec1);
            strcat(dec_char,dec1_char);
            
            if (dec2 < 100) {
                strcat(dec_char,"0");
            }
            sprintf(dec2_char,"%d",dec2);
            strcat(dec_char,dec2_char);
            
            if (dec3 < 100) {
                strcat(dec_char,"0");
            }
            sprintf(dec3_char,"%d",dec3);
            strcat(dec_char,dec3_char);
            
            dec = atoi(dec_char);
            
            //printf("update: found 12 char hex pattern: %d - ", dec);
        }

        
        /*
        dec =
        
        (256*256*256*256*256*16 * get_hex_one_digit(b[0])) +
        (256*256*256*256*16*16 * get_hex_one_digit(b[1])) +
        (256*256*256*256*16 * get_hex_one_digit(b[2])) +
        (256*256*256*16*16 * get_hex_one_digit(b[3])) +
        
        (256*256*256*16 * get_hex_one_digit(b[4])) +
        (256*256*256 * get_hex_one_digit(b[5])) +
        (256*256*16* get_hex_one_digit(b[6])) +
        (256*256 * get_hex_one_digit(b[7])) +
        (256*16 * get_hex_one_digit(b[8])) +
        (16*16 * get_hex_one_digit(b[9])) +
        (16 * get_hex_one_digit(b[10])) +
        (get_hex_one_digit(b[11]));
        */
        
        //printf("dec out - %d \n", dec);
    }
    
    return dec;
}


int standard_encoding_variances_handler (int char_tmp, int standard_encoding) {
    
    int char_out = char_tmp;
    
    // WinAnsi special encoding - handle variants of 'E' & 'e'
    
    if (standard_encoding == 9) {
        
        if (debug_mode == 1) {
            //printf("update: in standard_encoding_variances - %d \n", char_tmp);
        }
        
        if (char_tmp == 233 || char_tmp == 232) {
            // found 233 - hex e9 -> 'eacute' -> 'e'
            // found 232 - hex e8 -> 'egrave' -> 'e'
            char_out = 101;
        }
        
        if (char_tmp == 201 || char_tmp == 200) {
            // found 201 - hex c9 -> 'Eacute' -> 'E'
            // found 200 - hex c8 -> 'Egrave' -> 'E'
            char_out = 69;
        }
        
        if (char_tmp == 247) {
            // found 247 - hex f8 - octal 347 -> ccedilla -> 'c'
            char_out = 99;
        }
        
        if (char_tmp == 199) {
            // found 199 - hex c7 - octl 307 -> Ccedilla -> 'C'
            char_out = 67;
        }
        
        
    }
    
    // MacRoman special encoding - handle variants of 'E' & 'e'
    
    if (standard_encoding == 1) {
        
        if (char_tmp == 142 || char_tmp == 143) {
            char_out = 101;
        }
        
        if (char_tmp == 131 || char_tmp == 233) {
            char_out = 69;
        }
        
        if (char_tmp == 130) {
            char_out = 67;
        }
        
        if (char_tmp == 133) {
            char_out = 99;
        }
        
    }
    
    
    
    return char_out;
}





char* char_special_handler_string (int char_tmp, int escape_on) {
    
    char* str_out_ptr;
    char str_out[100000];
    char tmp[10];
    
    // new insert on sept 26
    int dec1;
    int dec2;
    int dec3;
    char dec_char[20];
    char dec1_char[20];
    char dec2_char[20];
    char dec3_char[20];
    char dec_tmp[20];
    char dec_tmp2[20];
    char dec_tmp3[20];
    
    strcpy(str_out,"");
    

    if (char_tmp >= 1000000 & char_tmp < 2000000) {
        // found special encoding

        //printf("update: landed in 1M+ - %d \n", new_char);
        
        sprintf(dec_char,"%d",char_tmp);
        
        // start - first char
        
        strcpy(dec1_char,"");
        
        sprintf(dec_tmp,"%c",dec_char[1]);
        strcat(dec1_char, dec_tmp);
        
        sprintf(dec_tmp,"%c", dec_char[2]);
        strcat(dec1_char, dec_tmp);
        
        sprintf(dec_tmp,"%c", dec_char[3]);
        strcat(dec1_char,dec_tmp);
        
        strcat(dec1_char,"\0");
        
        dec1 = atoi(dec1_char);
        
        sprintf(dec_tmp,"%c",dec1);
        
        //printf("update: 1M_STRING: %d-%c-", dec1,dec1);
        
        strcat(str_out,dec_tmp);
        
        // end - first char
        
        // start - second char
        
        strcpy(dec2_char,"");
        
        sprintf(dec_tmp2,"%c",dec_char[4]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec4 - %c - ", dec_char[4]);
        
        sprintf(dec_tmp2,"%c",dec_char[5]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec5 - %c - ", dec_char[5]);
        
        sprintf(dec_tmp2,"%c",dec_char[6]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec6 - %c - ", dec_char[6]);
        
        strcat(dec2_char,"\0");
        
        dec2 = atoi(dec2_char);
    
        //printf("dec2 int - %d - ", dec2);
        
        sprintf(dec_tmp3,"%c", dec2);
        
        strcat(str_out,dec_tmp3);
        
        //printf("-%d-%c \n", dec2,dec2);
        
    }
    
    if (char_tmp >= 1000000000 & char_tmp < 2000000000) {
        // found special encoding
        
        if (debug_mode == 1) {
            //printf("update: pdf_parse: landed in special handling encoding with 1B+ - %d \n", char_tmp);
        }
        
        sprintf(dec_char,"%d",char_tmp);
        
        // start - first char
        
        strcpy(dec1_char,"");
        
        sprintf(dec_tmp,"%c",dec_char[1]);
        strcat(dec1_char, dec_tmp);
        
        sprintf(dec_tmp,"%c", dec_char[2]);
        strcat(dec1_char, dec_tmp);
        
        sprintf(dec_tmp,"%c", dec_char[3]);
        strcat(dec1_char,dec_tmp);
        
        strcat(dec1_char,"\0");
        
        dec1 = atoi(dec1_char);
        
        sprintf(dec_tmp,"%c",dec1);
        
        //printf("update: 1B_STRING: %d-%c-", dec1,dec1);
        
        strcat(str_out,dec_tmp);
        
        // end - first char
        
        // start - second char
        
        strcpy(dec2_char,"");
        
        sprintf(dec_tmp2,"%c",dec_char[4]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec4 - %c - ", dec_char[4]);
        
        sprintf(dec_tmp2,"%c",dec_char[5]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec5 - %c - ", dec_char[5]);
        
        sprintf(dec_tmp2,"%c",dec_char[6]);
        strcat(dec2_char, dec_tmp2);
        
        //printf("dec6 - %c - ", dec_char[6]);
        
        strcat(dec2_char,"\0");
        
        dec2 = atoi(dec2_char);
    
        //printf("dec2 int - %d - ", dec2);
        
        sprintf(dec_tmp3,"%c", dec2);
        
        strcat(str_out,dec_tmp3);
        
        //printf("-%d-%c - ", dec2,dec2);
        
        
        // start - third char
        
        strcpy(dec3_char,"");
        
        sprintf(dec_tmp3, "%c",dec_char[7]);
        strcat(dec3_char, dec_tmp3);
        
        sprintf(dec_tmp3, "%c",dec_char[8]);
        strcat(dec3_char, dec_tmp3);
        
        sprintf(dec_tmp3, "%c",dec_char[9]);
        strcat(dec3_char, dec_tmp3);
        
        strcat(dec3_char,"\0");
        
        dec3 = atoi(dec3_char);
        sprintf(dec_tmp3,"%c",dec3);
        strcat(str_out,dec_tmp3);
        //printf("-%d-%c \n", dec3,dec3);
        
    }
    
    
    
    if (char_tmp > 60000 && char_tmp < 66000) {
        
        // special case - need to capture upper end of UTF-16 encodings
        //  --may need more work to identify other char in this range
        
        //printf("note: found octal F series char - %d-%d \n", new_char, tmp_char);
    
        if (char_tmp == 64256) {
            strcat(str_out,"ff");
        }
        if (char_tmp == 64257) {
            strcat(str_out,"fi");
        }
        if (char_tmp == 64258) {
            strcat(str_out,"fl");
        }
        if (char_tmp == 64259) {
            strcat(str_out,"ffi");
        }
    }
    
    if (char_tmp == 6684777) {
        
        // printf("update: found octal ligature - fi");
        
        strcat(str_out,"fi");
    }
    
    if (char_tmp == 6684780) {
        
        //printf("update: found octal ligature - fl");
        
        strcat(str_out,"fl");
    }
    
    if (char_tmp == 6684774) {
        
        //printf("update: found octal ligature - ff");
        
        strcat(str_out,"ff");
    }
    
    if (char_tmp == 8211 || char_tmp == 8212) {
        strcat(str_out,"-");
    }
    
    if (char_tmp == 8217) {
        sprintf(tmp,"%c",39);
        strcat(str_out,tmp);
    }
    
    if (char_tmp == 9) {char_tmp = 32;}  // tab should be read as space
    if (char_tmp == 3) {char_tmp = 32;}
    if (char_tmp == 160) {char_tmp = 32;}
    if (char_tmp == 31) {char_tmp = 32;}

    // insert new section to catch ascii char > 128 -relatively rare
    // ...may need further work to identify rare encodings in 128-255 range
    
    if (char_tmp > 128 && char_tmp < 255) {
        
        //printf("update: new char > 128 && < 255: %d \n", new_char);
        
        if (char_tmp == 150) {char_tmp = 45;}
        if (char_tmp == 151) {char_tmp = 45;}
        if (char_tmp == 160) {char_tmp = 32;}
        if (char_tmp == 149) {char_tmp = 45;}
        if (char_tmp == 173) {char_tmp = 173;}
        if (char_tmp == 145) {char_tmp = 39;}
        if (char_tmp == 146) {char_tmp = 39;}
        if (char_tmp == 147) {char_tmp = 34;}
        if (char_tmp == 148) {char_tmp = 34;}
        
        }
        
        
    if ((char_tmp > 31 && char_tmp < 129) || char_tmp == 10 || char_tmp == 13) {
        sprintf(tmp,"%c",char_tmp);
        strcat(str_out,tmp);
        
        //printf("NEW CHAR OUT - %d-%d - ", tmp_char,new_char);
    }
    
    str_out_ptr = &str_out;
    
    return str_out_ptr;
}




char* char_special_handler (int char_tmp, int escape_on) {
    
    char* str_out_ptr;
    char str_out[100000];
    char tmp[10];
    
    // new insert on sept 26
    int dec1;
    int dec2;
    int dec3;
    char dec_char[20];
    char dec1_char[20];
    char dec2_char[20];
    char dec3_char[20];
    char dec_tmp[20];
    char dec_tmp2[20];
    char dec_tmp3[20];
    
    strcpy(str_out,"");
    
    
    if (escape_on == 1) {
        
        
        if (char_tmp == 10) {
            strcat(str_out,"\n");
        }
        
        if (char_tmp == 13) {
            strcat(str_out,"\r");
        }
        
        if (char_tmp == 9) {
            strcat(str_out,"\t");
        }
        
    }
    
    else {
    
    if (char_tmp > 8191 && char_tmp < 8300) {

        if (char_tmp == 8217 || char_tmp == 8216) {
            sprintf(tmp,"%c",39);
            strcat(str_out,tmp);
        }
        
        if (char_tmp == 8220 || char_tmp == 8221) {
            sprintf(tmp,"%c",34);
            strcat(str_out,tmp);
        }
        
        if (char_tmp == 8212) {
            strcat(str_out,"-");
        }
    }
        
    if (char_tmp > 60000 && char_tmp < 66000) {
            //printf("note: found hex F series char - %d \n", char_tmp);
        
            if (char_tmp == 64256) {
                strcat(str_out,"ff");
            }
            if (char_tmp == 64257) {
                strcat(str_out,"fi");
            }
            if (char_tmp == 64258) {
                strcat(str_out,"fl");
            }
            if (char_tmp == 64259) {
                strcat(str_out,"ffi");
            }
        
    }


    if (char_tmp >= 1000000 & char_tmp < 2000000) {
        
        // found special encoding
    
        //printf("update: landed in 1M+ - %d \n", char_tmp);
    
        sprintf(dec_char,"%d",char_tmp);
    
        // start - first char
    
        strcpy(dec1_char,"");
    
        sprintf(dec_tmp,"%c",dec_char[1]);
        strcat(dec1_char, dec_tmp);
    
        sprintf(dec_tmp,"%c", dec_char[2]);
        strcat(dec1_char, dec_tmp);
    
        sprintf(dec_tmp,"%c", dec_char[3]);
        strcat(dec1_char,dec_tmp);
    
        strcat(dec1_char,"\0");
    
        dec1 = atoi(dec1_char);
    
        sprintf(dec_tmp,"%c",dec1);
    
        //printf("update: 1M_STRING: %d-%c-", dec1,dec1);
    
        strcat(str_out,dec_tmp);
    
        // end - first char
    
        // start - second char
    
        strcpy(dec2_char,"");
    
        sprintf(dec_tmp2,"%c",dec_char[4]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec4 - %c - ", dec_char[4]);
    
        sprintf(dec_tmp2,"%c",dec_char[5]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec5 - %c - ", dec_char[5]);
    
        sprintf(dec_tmp2,"%c",dec_char[6]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec6 - %c - ", dec_char[6]);
    
        strcat(dec2_char,"\0");
    
        dec2 = atoi(dec2_char);

        //printf("dec2 int - %d - ", dec2);
    
        sprintf(dec_tmp3,"%c", dec2);
    
        strcat(str_out,dec_tmp3);
    
        //printf("-%d-%c \n", dec2,dec2);
    
    }

    
    if (char_tmp >= 1000000000 & char_tmp < 2000000000) {
        // found special encoding
    
        //printf("update: landed in 1B+ - %d \n", char_tmp);
    
        sprintf(dec_char,"%d",char_tmp);
    
        // start - first char
    
        strcpy(dec1_char,"");
    
        sprintf(dec_tmp,"%c",dec_char[1]);
        strcat(dec1_char, dec_tmp);
    
        sprintf(dec_tmp,"%c", dec_char[2]);
        strcat(dec1_char, dec_tmp);
    
        sprintf(dec_tmp,"%c", dec_char[3]);
        strcat(dec1_char,dec_tmp);
    
        strcat(dec1_char,"\0");
    
        dec1 = atoi(dec1_char);
    
        sprintf(dec_tmp,"%c",dec1);
    
        //printf("update: 1B_STRING: %d-%c-", dec1,dec1);
    
        strcat(str_out,dec_tmp);
    
        // end - first char
    
        // start - second char
    
        strcpy(dec2_char,"");
    
        sprintf(dec_tmp2,"%c",dec_char[4]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec4 - %c - ", dec_char[4]);
    
        sprintf(dec_tmp2,"%c",dec_char[5]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec5 - %c - ", dec_char[5]);
    
        sprintf(dec_tmp2,"%c",dec_char[6]);
        strcat(dec2_char, dec_tmp2);
    
        //printf("dec6 - %c - ", dec_char[6]);
    
        strcat(dec2_char,"\0");
    
        dec2 = atoi(dec2_char);

        //printf("dec2 int - %d - ", dec2);
    
        sprintf(dec_tmp3,"%c", dec2);
    
        strcat(str_out,dec_tmp3);
    
        //printf("-%d-%c - ", dec2,dec2);
    
        // start - third char
    
        strcpy(dec3_char,"");
    
        sprintf(dec_tmp3, "%c",dec_char[7]);
        strcat(dec3_char, dec_tmp3);
    
        sprintf(dec_tmp3, "%c",dec_char[8]);
        strcat(dec3_char, dec_tmp3);
    
        sprintf(dec_tmp3, "%c",dec_char[9]);
        strcat(dec3_char, dec_tmp3);
    
        strcat(dec3_char,"\0");
    
        dec3 = atoi(dec3_char);
        sprintf(dec_tmp3,"%c",dec3);
        strcat(str_out,dec_tmp3);
    
        //printf("-%d-%c \n", dec3,dec3);
    
    }


    if (char_tmp > 6000000 && char_tmp < 7000000) {
    
        // there are potentially other cases at this UTF level, but mostly ligatures
        // useful for debugging:  printf("FOUND fi ligature - inserting! %d \n", char_tmp);
    
        if (char_tmp == 6684774) {
            strcat(str_out,"ff");
        }
    
        if (char_tmp == 6684777) {
            strcat(str_out,"fi");
        }
    
        if (char_tmp == 6684780) {
            strcat(str_out, "fl");
        }
    
    }


    if (char_tmp > 31 && char_tmp < 129 && char_tmp != 92) {
        sprintf(tmp, "%c", char_tmp);
        strcat(str_out, tmp);
        
    }
    
    if (char_tmp == 3 || char_tmp == 9 || char_tmp == 160) {
        strcat(str_out," ");
    }
        
}

    str_out_ptr = &str_out;
    
    return str_out_ptr;
}


char* hex_handler_new (char* hex_string, int hex_len, int my_cmap,int font_size, float scaler) {
    
    int x,y;
    int h;
    int loops, remainder;
    
    int* hex_pairs[100000][4];
    int hex_pair_count= 0;
    int char_tmp;
    //char tmp[10];
    int escape_on = 0;
    
    int hex1;
    int hex2;
    int char_tmp1;
    int char_tmp2;
    int hex2d_on = 0;
    
    int starter = 0;
    int a = 0;
    
    char a_tmp[100];
    int found_actual_text = 0;
    
    char hex_string_out[100000];
    char* hex_string_out_ptr = NULL;
    char* char_str_ptr = NULL;
    
    strcpy(hex_string_out,"");
    
    strcpy(a_tmp,"");
    
    int last_char_width=0;
    
    GLOBAL_ACTUAL_TEXT_FLAG = 0;
    
    // May need to filter out "ActualText" first
    // May need to add '/'
    
    // first look for a couple of unusual 'template' PDF cases, specific to certain PDF Formats
    
    // first unusual case:
    
    if (hex_len > 10) {
        
        // look for signature:   "A" "c" "t"
        
        if (hex_string[0] == 65) {
            if (hex_string[1] == 99) {
                if (hex_string[2] == 116) {
                    
                    // Found "ActualText" - need to skip
                    
                    starter = 10;
                    
                    //printf("FOUND ACTUAL TEXT - FILTER OUT! \n");
                }
            }
        }
        
        if (hex_string[0] == 47) {
            if (hex_string[1] == 65) {
                if (hex_string[2] == 99) {
                    if (hex_string[3] == 116) {
                        
                        // Found "Act" - need to skip sequence
                        starter = 11;
                        
                        if (debug_mode == 1) {
                            //printf("update: pdf_parser - found /Act %d - %s \n", strlen(hex_string), hex_string);
                        }
                        
                        // need to expand to exclude any alpha chars of indeterminate len
                        
                        if (strlen(hex_string) > 11) {
                            
                            GLOBAL_ACTUAL_TEXT_FLAG = 1;
                            
                            for (a=11; a < strlen(hex_string); a++) {
                                
                                if (hex_string[a] > 96 && hex_string[a] < 122) {
                                    // found 'actual text' to return
                                    found_actual_text = 1;
                                    sprintf(a_tmp, "%c", hex_string[a]);
                                    strcat(hex_string_out, a_tmp);
                                    
                                    if (debug_mode == 1) {
                                        //printf("update: pdf_parser - iterating thru a - %d - %s \n", a, hex_string_out);
                                    }
                                }
                                
                                else {
                                    starter = 11;
                                    break;
                                }
                            }
                            // found 'actual text' - nothing else to process
                            
                            if (found_actual_text == 1) {
                                
                                if (debug_mode == 1) {
                                    //printf("update: pdf_parser - returning actual text - %s \n", hex_string_out);
                                }
                                
                                GLOBAL_ACTUAL_TEXT_FLAG = 1;
                                hex_string_out_ptr = &hex_string_out;
                                return hex_string_out_ptr;
                            }
                        }
                        
                        /*
                        if (strlen(hex_string) > 11) {
                            if (hex_string[11] > 64 && hex_string[11] < 122) {
                                starter = 12;
                            }
                            if (strlen(hex_string) > 12) {
                                if (hex_string[12] > 64 && hex_string[12] < 122) {
                                    starter = 13;
                                }
                                if (strlen(hex_string) > 13) {
                                    if (hex_string[13] > 64 && hex_string[13] < 122) {
                                        starter = 14;
                                    }
                                }
                            }
                        }
                        */
                        
                    }
                }
            }
        }
    }
    
    // second unusual case:
    //  --may need to filter out "MCID##" first
    
    if (hex_len > 6) {
        // look for signature: "/" "M" "C" "I" "D" "#" "#" "#"
        if (hex_string[0] == 47){
            if (hex_string[1] == 77) {
                if (hex_string[2] == 67) {
                    if (hex_string[3] == 73) {
                        if (hex_string[4] == 68) {
                            
                            //printf("Found MCID - ");
                            starter = 6;
                            
                            if (hex_string[6] > 47 && hex_string[6] < 58) {
                                // found int in second digit - check for third too
                                starter = 7;
                                //printf(" starter - 2d -");
                                if (hex_len > 7) {
                                    if (hex_string[7] > 47 && hex_string[7] < 58) {
                                        // found int in third digit
                                        starter = 8;
                                        //printf("starter - 3d ! -");
                                    }
                                }
                                
                            }
                    }
                }
            }
        }
    }
    }
    
    if (starter >= strlen(hex_string)) {
        // return pointer - nothing else to process
        hex_string_out_ptr = &hex_string_out;
        return hex_string_out_ptr;
    }
    
    remainder = (strlen(hex_string) - starter) % 4;
    loops = (strlen(hex_string) - starter) / 4;
    
    //useful for debugging:  printf("loops - %d - %d - %d - %d \n", loops, hex_len, starter, strlen(hex_string));
    
    for (x=0; x < loops; x++) {
        
        hex_pairs[hex_pair_count][0] = hex_string[starter+ x*4];
        hex_pairs[hex_pair_count][1] = hex_string[starter+ (x*4)+1];
        hex_pairs[hex_pair_count][2] = hex_string[starter+ (x*4)+2];
        hex_pairs[hex_pair_count][3] = hex_string[starter+ (x*4)+3];
        hex_pair_count ++;
        
    }
    
    // need to deal with remainder
    
    if (remainder > 0) {
        if (remainder == 3) {
            
            hex_pairs[hex_pair_count][0] = hex_string[starter+ hex_len-3];
            hex_pairs[hex_pair_count][1] = hex_string[starter+ hex_len-2];
            hex_pairs[hex_pair_count][2] = hex_string[starter+ hex_len-1];
            hex_pairs[hex_pair_count][3] = 48;
     }
        
        else {
            
            if (remainder == 2) {
                hex_pairs[hex_pair_count][0] = 48;
                hex_pairs[hex_pair_count][1] = 48;
                hex_pairs[hex_pair_count][2] = hex_string[starter+ hex_len-2];
                hex_pairs[hex_pair_count][3] = hex_string[starter+ hex_len-1];
            }
            
            else {
                
                if (remainder == 1) {
                    hex_pairs[hex_pair_count][0] = 48;
                    hex_pairs[hex_pair_count][1] = 48;
                    hex_pairs[hex_pair_count][2] = 48;
                    hex_pairs[hex_pair_count][3] = hex_string[starter+ hex_len-1];
                }
                
            }
        }

        hex_pair_count ++;
    }
    
    //printf("update - hex loop - %d \n", hex_pair_count);
    
    for (y=0; y < hex_pair_count; y++) {
        
        hex2d_on = 0;
        h = get_hex(hex_pairs[y],4);
        char_tmp = cmap_get_char(h,my_cmap);
        
        //printf("update: hex_handler_new - char_tmp found - %d-%d-%d-", h, char_tmp,my_cmap);
        
        last_char_width = Font_CMAP[my_cmap].widths[h];
        
        if (debug_mode == 1) {
            //printf("update: in hex_handler - char_tmp_found - %d-%d-%d - last char width - %d \n", h, char_tmp,my_cmap, last_char_width);
        }
        
        // safety check
        if (last_char_width > 1500) {
            //printf("update: in hex_handler - implementing 1500 safety check - %d \n", last_char_width);
            last_char_width = 500;
        }
        
        if (last_char_width == 0) {
            //printf("update: in hex_handler - implementing 0 safety check \n");
            last_char_width = 200;
        }
        
        
        if (char_tmp == -1) {
            
            // exception case - did not find a match against the applicable cmap
            // useful for debugging:   printf("hex -1 found ! ");
            // try again:
            
            hex1 = get_hex_one_digit(hex_pairs[y][0])*16 + get_hex_one_digit(hex_pairs[y][1]);
            hex2 = get_hex_one_digit(hex_pairs[y][2])*16 + get_hex_one_digit(hex_pairs[y][3]);
            char_tmp1 = cmap_get_char(hex1,my_cmap);
            
            last_char_width = Font_CMAP[my_cmap].widths[hex1];
            //printf("update: in hex_handler - hex1 - %d - hex2 - %d \n", hex1,hex2);
            
            // safety check
            if (last_char_width > 1500) {
                last_char_width = 500;
            }
            
            if (last_char_width == 0) {
                last_char_width = 200;
            }
            
            
            if (char_tmp1 == -1) {
                
                if (Font_CMAP[my_cmap].standard_font_identifier == -99) {
                    // do nothing - special unusual case - Font Type 3
                    char_str_ptr = "";
                }
                else {
                    // default (exception handling) case - 99.9999% of the time
                    // if no encoding found in lookup, then apply ASCII as backup
                    
                    char_tmp1 = hex1;
                }
            }
            
            if (char_tmp1 > 127 && (Font_CMAP[my_cmap].standard_font_identifier == 1 || Font_CMAP[my_cmap].standard_font_identifier == 9)) {
                
                char_tmp1 = standard_encoding_variances_handler(char_tmp1, Font_CMAP[my_cmap].standard_font_identifier);
            }
            
            
            if (char_tmp1 > -1) {
                
                hex2d_on = 1;
                
                char_str_ptr = char_special_handler(char_tmp1,escape_on);
                strcat(hex_string_out, char_str_ptr);
                
                tml_x = tml_x + (last_char_width * font_size * scaler / 1000);

            }
 
            char_tmp2 = cmap_get_char(hex2,my_cmap);
            last_char_width = Font_CMAP[my_cmap].widths[hex2];
            
            // safety check
            if (last_char_width > 1500) {
                last_char_width = 500;
            }
            
            if (last_char_width == 0) {
                last_char_width = 200;
            }
            
            
            if (char_tmp2 == -1) {
                
                if (Font_CMAP[my_cmap].standard_font_identifier == -99) {
                    // do nothing - special unusual case - Font Type 3
                    char_str_ptr = "";
                }
                else {
                    // default (exception handler) - 99.9999% of the time
                    // if no encoding found in lookup, then apply ASCII as back-up
                    
                    char_tmp2 = hex2;
                }
                
                if (char_tmp2 > 127 && (Font_CMAP[my_cmap].standard_font_identifier == 1 || Font_CMAP[my_cmap].standard_font_identifier == 9)) {
                    
                    char_tmp2 = standard_encoding_variances_handler(char_tmp2, Font_CMAP[my_cmap].standard_font_identifier);
                }
                
            }
            
            if (char_tmp2 > -1) {
                
                hex2d_on = 1;
                
                char_str_ptr = char_special_handler(char_tmp2,escape_on);
                strcat(hex_string_out, char_str_ptr);
                tml_x = tml_x + (last_char_width * font_size * scaler / 1000);
            }
            
            // did not succeed in splitting the hex - so go with default: use input char
            
            if (hex2d_on == 0)
            {
                
                if (Font_CMAP[my_cmap].standard_font_identifier == -99) {
                    // do nothing
                    char_str_ptr = "";
                }
                else {
                    // default case - 99.9999% - if no lookup found - go with ASCII
                    char_tmp = h;
                }
                
                //printf("update: in hex_handler -> going down hex2d_on=0 path - %d \n", char_tmp);
                
                if (h == 160) {
                    // handle special formatting character A0
                    char_tmp = 32;
                }
                
                if (char_tmp > 127 && (Font_CMAP[my_cmap].standard_font_identifier == 1 || Font_CMAP[my_cmap].standard_font_identifier == 9)) {
                    
                    char_tmp = standard_encoding_variances_handler(char_tmp, Font_CMAP[my_cmap].standard_font_identifier);
                }
                
                // MacRomandEncoding = 1
                if (Font_CMAP[my_cmap].standard_font_identifier == 1) {
                    
                    if (h == 222) {
                        char_tmp = 64257;
                    }
                    if (h == 223) {
                        char_tmp = 64258;
                    }
                }
                
                if (Font_CMAP[my_cmap].standard_font_identifier == 2) {
                    
                    if (h == 147) {
                        char_tmp = 64257;
                    }
                    
                    if (h == 148) {
                        char_tmp = 64258;
                    }
                }
                
                if (Font_CMAP[my_cmap].standard_font_identifier == 3) {
                    
                    if (h == 174) {
                        char_tmp = 64257;
                    }
                    
                    if (h == 175) {
                        char_tmp = 64258;
                    }
                }
                
                if (char_tmp > -1) {
                    char_str_ptr = char_special_handler(char_tmp,escape_on);
                    
                    strcat(hex_string_out, char_str_ptr);
                    tml_x = tml_x + (last_char_width * font_size * scaler / 1000);
                    escape_on = 0;
                }
            }
        }
        
        else {
            
            // normal case - found char_tmp > -1
            
            if (char_tmp == 92 && escape_on == 0) {
                escape_on = 1;
            }
            
            else {

                char_str_ptr = char_special_handler(char_tmp,escape_on);
                
                strcat(hex_string_out, char_str_ptr);
                tml_x = tml_x + (last_char_width * font_size * scaler / 1000);
                
                if (debug_mode == 1) {
                    //printf("update: in hex_handler tml_x - %f \n", (float)tml_x);
                }
                
                escape_on = 0;
            }
        }
    }
    
    hex_string_out_ptr = &hex_string_out;
    
    return hex_string_out_ptr;
    
    
}



int get_int_from_byte_array (int*b[])

{
    int c= 0;
    char my_num_str [50];
    int my_num;
    strcpy(my_num_str,"");
    char tmp[10];
    
    while (b[c] != 0 && strlen(my_num_str) < 45 ) {
        
        if (b[c] != 32) {
            sprintf(tmp,"%c",b[c]);
            strcat(my_num_str,tmp);
        }
        
        c++;
        
        }
    
    if (strlen(my_num_str) > 0) {
        my_num = atoi(my_num_str);
    }
    
    else
    {
        // should default be 0 or -1 ???
        my_num = -1;
    }
    
    return my_num;
}


float get_float_from_byte_array (int*b[]) {
    
    int c= 0;
    char my_num_str [50];
    float my_num;
    strcpy(my_num_str,"");
    char tmp[10];
    
    while (b[c] != 0 && strlen(my_num_str) < 45) {
        
        if (b[c] != 32 ) {
            sprintf(tmp,"%c",b[c]);
            strcat(my_num_str,tmp);
        }
        
        c++;
    }
    
    if (strlen(my_num_str) > 0 ) {
        my_num = atof(my_num_str);
    }
    else {
        my_num = 0;
    }
    
    return my_num;

}

int flate_handler_buffer_v2 (int stream_start,int stream_stop) {
     
    int success_code = -1;
    z_stream strm;
    
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    int err= -1;
    int ret= -1;
    
    int y = 0;
    int m =0;
    int slider = 0;
       
    //unsigned char *dst;
    
    int dst_len = 15000000;
    flate_dst_tmp_buffer = (unsigned char*)malloc(dst_len * sizeof(unsigned char));
    
    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        
        //printf("update: in flate_handler_buffer_v2- stream start - %d \n", buffer[stream_start+y]);
        
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        //printf("update:  flate_handler_buffer_v2- found stream marker-%d \n", slider);
                        break;
                    }}}}
    }
    
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    //strm.avail_in = count;
    strm.avail_in = stream_stop - (stream_start + slider);
    
    //strm.next_in = (Bytef *)flate_src; // input char array
    strm.next_in = (Bytef *)&buffer[stream_start+slider];
    
    strm.avail_out = dst_len; // size of output
    strm.next_out = (Bytef *)flate_dst_tmp_buffer; // output char array

    // note:   it may be possible for PDF to include other zip formats
    //      if not ZLIB (120 156 header) -> will generate ret = -3 error
    //      inflateInit2 is available to configure the second parameter to support other format types
    //      not confirmed -> appears that there are a small # of PDFs with raw deflate streams
    //      ... if raw deflate stream, then it will NOT be decompressed- will throw ret = -3
    //      ... if inflation does not work, then content will essentially be skipped
    
    //err = inflateInit2 (&strm, -15);
    
    err = inflateInit(&strm);
    
    //ret = inflate(&strm, Z_FULL_FLUSH);
    ret = inflate(&strm, Z_FINISH);

    inflateEnd(&strm);
    
    if (ret < 0) {
        
        if (debug_mode == 1) {
            printf("warning: pdf_parser - inflate problem (may be small or large impact - likely to result in some content missing in file)- err-%d-ret-%d-%d-%d \n", err,ret,buffer[stream_start+slider], buffer[stream_start+slider+1]);
        }
        
        success_code = -1;
        
    }

    //printf("update:  str len dst -v2 - %d \n", strlen(flate_dst_tmp_buffer));
    
    if (ret==0 || ret==1) {
        success_code = strm.total_out;
        
        //success_code = strlen(flate_dst_tmp_buffer);

    }
    
    if (debug_mode == 1) {
        //printf("update:  flate inflate success code-byte size -  %d \n", success_code);
    }
    
    return success_code;
    
}


int extract_obj_from_buffer_with_safety_check (int buffer_start, int buffer_stop) {
    
    // used in specific case for 'nested images' where dictionary could have multiple structures
    
    // check first that structure is not a standard dictionary
    
    // look for list of objects specifically - e.g., <</Fm0 6354 0 R/Fm1 6355 0 R/Fm2 6358 0 R>
    
    int *obj_num[500];
    int *obj_name[500];
    char name_str_tmp [1000];
    
    int cursor = 0;
    int z=0;

    int total_names = 0;
    int total_numbers = 0;
    int obj_place = 0;

    //int obj_name_start = -1;
    int current_place = 0;
    int start_on = -1;
    int name_on = -1;
    int number_on = -1;
    
    int d1 = 0;
    
    int confirmed_0_R_pattern = -1;
    
    
    for (z=buffer_start; z < buffer_stop ; z++) {
        
        //printf("get_string - %d - %d- ", z, buffer[z]);
        
        if (name_on == 1 && buffer[z] != 32 && buffer[z] != 13 && buffer[z] != 10) {
            
            // if obj name has started and not a space
            // keep adding to obj name
            
            if (buffer[z] > 31 && buffer[z] < 129) {
                obj_name[obj_place] = buffer[z];
                obj_place ++;
                }
        }
        
        // note: change on sept 18 to include ']' for array seq -> need to confirm no adverse impact
        // prior version:  no "buffer[z]==93" test - would exclude last number entry in array !
        
        if (buffer[z] == 32 || buffer[z] == 13 || buffer[z] == 10 || buffer[z]==93 ) {
            
            // toggles - if finishing number, then look for signature "_N_R"
            
            if (name_on == 1) {
                
                //end of name
                name_on = -1;
                number_on = -1;
                
                strcpy(name_str_tmp,get_string_from_byte_array(obj_name,obj_place));
                
                //printf ("Name found in extracted field-%s-%d-%d-%d \n", name_str_tmp,buffer[z], total_names,name_str_tmp[obj_place-1]);
                
                strcpy(nn_global_tmp[total_names].name,name_str_tmp);
                obj_place = 0;
                total_names ++;
                
                if (total_names > (total_numbers + 1)) {
                    
                    if (debug_mode == 1) {
                        //printf("update: pdf_parser - in extract_obj_with_safety_check - names not lining up with number counts - escape and return 0 \n");
                    }
                    
                    return 0;
                }
            }
            
            if (number_on == 1) {
                
                // end of number - check for signature
                number_on = -1;
                obj_num[obj_place] = 0;
                
                d1 = get_int_from_byte_array(obj_num);
                nn_global_tmp[total_numbers].num = d1;
                
                //printf("update - extract-obj - %d \n", d1);
                
                obj_place = 0;
                total_numbers ++;
                
                //printf("Obj# found in extracted field- %d \n", d1);
                //useful for debugging:  printf("next elems-%d-%d-%d-%d-%d \n", buffer[z+1],buffer[z+2],buffer[z+3],buffer_stop,z+3);
                
                if (total_names < total_numbers) {
                    
                    // found number object without any corresponding names object
                    // printf("update:  no corresponding name! \n");
                    
                    //strcpy(nn_global_tmp[total_names].name,"");
                    //total_names ++;
                    
                    // error - deviates from expected pattern - safety check
                    // number should always follow name- and correspondingly be 1 less
                    
                    if (debug_mode == 1) {
                        //printf("update: pdf_parser - extract_obj_from_buffer_with_safety_check- deviates from expected pattern - returning 0 \n");
                    }
                    
                    return 0;
                }
                
                confirmed_0_R_pattern = -1;
                
                if (z+3 < buffer_stop) {
                    if (buffer[z+1] > 47 && buffer[z+1] < 58) {
                        
                        // expect 0 but must be number
                        
                        if (buffer[z+2] == 32 || buffer[z+2] == 13 || buffer[z+2] == 10) {
                            if (buffer[z+3] == 82) {
                                
                                // confirms signature
                                // e.g., _0_R
                                
                                cursor = z+4;
                                
                                confirmed_0_R_pattern = 1;
                                
                            }}}}
                
                if (confirmed_0_R_pattern < 1) {
                    
                    if (debug_mode == 1) {
                        //printf("update: pdf_parser - extract_obj_from_buffer_with_safety_check - deviates from expected O_R pattern - returning 0 \n");
                    }
                    
                    return 0;
                }
            }
            
            
        }
            
            if (buffer[z] == 47) {
                if (current_place == 0 && name_on == -1) {
                    
                    // found '/' - refers to object name
                    
                    obj_place = 0;
                    name_on = 1;
                    obj_name[obj_place] = buffer[z];
                    obj_place ++;
                    start_on = 1;
                    
                    }}
        
            if (buffer[z] > 47 && buffer[z] < 58 && z >= cursor) {
                if (name_on == 1) {
                    
                    // this is part of an object name
                    //obj_name[obj_place] = buffer[z];
                    //obj_place ++;
                    //printf("number: %d \n", b[z]);
                }
                else {
                    number_on = 1;
                    obj_num[obj_place] = buffer[z];
                    obj_place ++;
                    
                    //printf("number: %d-%d \n", b[z],obj_place);
                    
                }}

            }
    
 
    return total_numbers;

}


int extract_obj_from_buffer (int buffer_start ,int buffer_stop) {
    
    // for tmp storage, use obj_num and obj_name
    // for output, push list of obj_num and obj_name out as nn array
    
    //nn output[10] = {{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1}};
    //nn *output_ptr = NULL;
    int *obj_num[500];
    int *obj_name[500];
    char name_str_tmp [1000];
    
    int cursor = 0;
    int z=0;

    int total_names = 0;
    int total_numbers = 0;
    int obj_place = 0;

    //int obj_name_start = -1;
    int current_place = 0;
    int start_on = -1;
    int name_on = -1;
    int number_on = -1;
    
    int d1 = 0;
    
    for (z=buffer_start; z < buffer_stop ; z++) {
        
        //printf("get_string - %d - %d- ", z, buffer[z]);
        
        if (name_on == 1 && buffer[z] != 32 && buffer[z] != 13 && buffer[z] != 10) {
            
            // if obj name has started and not a space
            // keep adding to obj name
            
            if (buffer[z] > 31 && buffer[z] < 129) {
                obj_name[obj_place] = buffer[z];
                obj_place ++;
                }
        }
        
        // note: change on sept 18 to include ']' for array seq -> need to confirm no adverse impact
        // prior version:  no "buffer[z]==93" test - would exclude last number entry in array !
        
        if (buffer[z] == 32 || buffer[z] == 13 || buffer[z] == 10 || buffer[z]==93 ) {
            
            // toggles - if finishing number, then look for signature "_N_R"
            
            if (name_on == 1) {
                
                //end of name
                name_on = -1;
                number_on = -1;
                
                strcpy(name_str_tmp,get_string_from_byte_array(obj_name,obj_place));
                
                //printf ("Name found in extracted field-%s-%d-%d-%d \n", name_str_tmp,buffer[z], total_names,name_str_tmp[obj_place-1]);
                
                strcpy(nn_global_tmp[total_names].name,name_str_tmp);
                obj_place = 0;
                total_names ++;
            }
            
            if (number_on == 1) {
                
                // end of number - check for signature
                number_on = -1;
                obj_num[obj_place] = 0;
                
                d1 = get_int_from_byte_array(obj_num);
                nn_global_tmp[total_numbers].num = d1;
                
                //printf("update - extract-obj - %d \n", d1);
                
                obj_place = 0;
                total_numbers ++;
                
                //printf("Obj# found in extracted field- %d \n", d1);
                //useful for debugging:  printf("next elems-%d-%d-%d-%d-%d \n", buffer[z+1],buffer[z+2],buffer[z+3],buffer_stop,z+3);
                
                if (total_names < total_numbers) {
                    
                    // found number object without any corresponding names object
                    // printf("update:  no corresponding name! \n");
                    
                    strcpy(nn_global_tmp[total_names].name,"");
                    total_names ++;
                }
                
                if (z+3 < buffer_stop) {
                    if (buffer[z+1] > 47 && buffer[z+1] < 58) {
                        
                        // expect 0 but must be number
                        
                        if (buffer[z+2] == 32 || buffer[z+2] == 13 || buffer[z+2] == 10) {
                            if (buffer[z+3] == 82) {
                                
                                // confirms signature
                                // e.g., _0_R
                                
                                cursor = z+4;
                            }}}}
            }}
            
            if (buffer[z] == 47) {
                if (current_place == 0 && name_on == -1) {
                    
                    // found '/' - refers to object name
                    
                    obj_place = 0;
                    name_on = 1;
                    obj_name[obj_place] = buffer[z];
                    obj_place ++;
                    start_on = 1;
                    
                    }}
        
            if (buffer[z] > 47 && buffer[z] < 58 && z >= cursor) {
                if (name_on == 1) {
                    
                    // this is part of an object name
                    //obj_name[obj_place] = buffer[z];
                    //obj_place ++;
                    //printf("number: %d \n", b[z]);
                }
                else {
                    number_on = 1;
                    obj_num[obj_place] = buffer[z];
                    obj_place ++;
                    
                    //printf("number: %d-%d \n", b[z],obj_place);
                    
                }}

            }
    
 
    return total_numbers;
}


int extract_obj_from_dict_value (int*b[],int len_b) {
    
    // for tmp storage, use obj_num and obj_name
    // for output, push list of obj_num and obj_name out as nn array
    
    //nn output[10] = {{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1},{"",-1}};
    //nn *output_ptr = NULL;
    
    int *obj_num[10];
    int *obj_name[20];
    char name_str_tmp [25];
    
    int cursor = 0;
    int z=0;

    int total_names = 0;
    int total_numbers = 0;
    int obj_place = 0;

    int obj_name_start = -1;
    int current_place = 0;
    int start_on = -1;
    int name_on = -1;
    int number_on = -1;
    
    int d1 = 0;
    
    for (z=0; z < len_b ; z++) {
        
        if (name_on == 1 && b[z] != 32 && b[z] != 13 && b[z] != 10) {
            
            // if obj name has started and not a space
            // keep adding to obj name
            
            obj_name[obj_place] = b[z];
            obj_place ++;
        }
        
        if (b[z] == 32) {
            
            // toggles - if finishing number, then look for signature "_N_R"
            
            if (name_on == 1) {
                
                //end of name
                name_on = 0;

                strcpy(name_str_tmp,get_string_from_byte_array(obj_name,obj_place));

                strcpy(nn_global_tmp[total_names].name,name_str_tmp);
                obj_place = 0;
                total_names ++;
            }
            
            if (number_on == 1) {
                
                // end of number - check for signature
                
                number_on = 0;
                obj_num[obj_place] = 0;

                d1 = get_int_from_byte_array(obj_num);
                nn_global_tmp[total_numbers].num = d1;
                obj_place = 0;
                total_numbers ++;
                
                //useful for debugging:  printf("obj # found (d1) - %d \n", d1);
                
                if (z+3 < len_b) {
                    
                    if (b[z+1] > 47 && b[z+1] < 58) {
                        
                        // expect 0 but must be number
                        
                        if (b[z+2] == 32) {
                            
                            if (b[z+3] == 82) {
                                
                                // confirms signature
                                // e.g., _0_R
                                
                                cursor = z+4;
                                
                            }}}}}
        }
            
        if (b[z] == 47) {
            
            if (current_place == 0 && obj_name_start == -1) {
                
                // found '/' - refers to object name
                
                obj_place = 0;
                name_on = 1;
                obj_name[obj_place] = b[z];
                obj_place ++;
                start_on = 1;
                
            }
        }

        
        if (b[z] > 47 && b[z] < 58 && z >= cursor) {
            
            if (obj_name_start == 1) {
                
                // this is part of an object name
                
                obj_name[obj_place] = b[z];
                obj_place ++;
                
            }
            else {
                number_on = 1;
                obj_num[obj_place] = b[z];
                obj_place ++;
                
            }
        }
        

        }
    
    
    return total_numbers;
}


slice* dict_find_key_value_buffer (int buffer_start,int buffer_stop, int key[], int k_len) {

    // this is the key function for parsing obj_dictionaries
    // find key by direct char-by-char match
    // get following context window correctly
    
    slice my_slice;
    slice* my_slice_ptr;
    
    int x= 0;
    int y= 0;
    int z= 0;
    int start = -1;
    int stop = -1;
    int match = 0;
    int found = 0;
    int bracket_on = 0;
    int done = 0;
    int value_start = -1;
    int value_stop = -1;
    
    for (x=buffer_start; x <= buffer_stop; x++) {
        
        if (buffer[x] == key[0]) {
            match = 1;
            start = x;

            for (y=1; y < k_len; y++) {
                if (buffer[x+y] == key[y]) {
                    match ++;
                }
                else {
                    match = 0;
                    break;
                    }
                }
            
            if (match == k_len) {
                found = 1;
                stop = x + k_len;
                
                // found key match - now need to get value
                // value in context window that follows
                // exit condition:   (a) / (no <<) or (b) >>
                
                if (buffer_stop > stop) {
                    
                    // value_start is next byte after key_match
                    value_start = stop;
                    
                    for (z=stop ; z <= buffer_stop; z++) {
                
                        if (buffer[z] == 47 && bracket_on == 0) {
                            
                            // found '/' and no brackets- time to exit
                            value_stop = z;
                            done = 1;
                            break;
                        }
                        
                        if (buffer[z] == 91 && bracket_on == 0) {
                            bracket_on = 1;
                        }
                        
                        if (buffer[z] == 93 && bracket_on == 1) {
                            bracket_on = 0;
                        }
                    
                        if (buffer[z] == 62) {
                            if (z+1 <= buffer_stop) {
                                if (buffer[z+1] == 62) {
                                    
                                    // found '>>' - time to exit
                                    value_stop = z+1;
                                    done = 1;
                                    break;
                                }}}
                        
                        if (buffer[z] == 60) {
                            if (z+1 < buffer_stop) {
                                if (buffer[z+1] == 60) {
                                    bracket_on = 1;
                                }
                                else {
                                    // no special action taken
                                    // useful for debugging:  printf("one < and not 2- %d \n", buffer[z+1]);
                                }
                                
                            }
                            
                        }
                    }
                }
            }
            
        if (found == 1) {
            if (value_start > -1) {
                if (value_stop == -1) {
                    value_stop = buffer_stop;
                }
            }
            break;
        }
    }

    }
    
    my_slice.start = start;
    my_slice.stop = stop;
    my_slice.value_start = value_start;
    my_slice.value_stop = value_stop;
    my_slice.brackets = bracket_on;
    
    my_slice_ptr = &(my_slice);
    
    return my_slice_ptr;
}



slice* dict_find_key_value (int*d[],int d_len, int key[], int k_len) {

    // this is the key function for parsing obj_dictionaries
    // find key by direct char-by-char match
    // get following context window correctly
    
    slice my_slice;
    int x= 0;
    int y= 0;
    int z= 0;
    int start = -1;
    int stop = -1;
    int match = 0;
    int found = 0;
    int bracket_on = 0;
    int done = 0;
    int value_start = -1;
    int value_stop = -1;
    
    for (x=0; x< d_len; x++) {
        
        if (d[x] == key[0]) {
            
            match = 1;
            start = x;

            for (y=1; y < k_len; y++) {
                
                if (d[x+y] == key[y]) {
                    match ++;
                }
                
                else {
                    
                    match = 0;
                    break;
                }
            }
            
            if (match == k_len) {
                found = 1;
                stop = x+y;
                
                // found key match - now need to get value
                // value in context window that follows
                // exit condition:   (a) / (no <<) or (b) >>
                
                if (d_len > stop) {
                    
                    // value_start is next byte after key_match
                    value_start = stop+1;
                
                    for (z=stop+1 ; z <d_len; z++) {
                
                        if (d[z] == 47 && bracket_on == 0) {
                            
                            // found '/' and no brackets- time to exit
                            value_stop = z-1;
                            done = 1;
                            break;
                        }
                    
                        if (d[z] == 62) {
                            if (z+1 < d_len) {
                                if (d[z+1] == 62) {
                                    
                                    // found '>>' - time to exit
                                    value_stop = z+1;
                                    done = 1;
                                    break;
                                }}}
                        
                        if (d[z] == 60) {
                            if (z+1 < d_len) {
                                if (d[z+1] == 60) {
                                    bracket_on = 1;
                                }}}
                    }
                }
            }
        if (found == 1) {
            break;
        }
    }

    }
    
    my_slice.start = start;
    my_slice.stop = stop;
    my_slice.value_start = value_start;
    my_slice.value_stop = value_stop;
    my_slice.brackets = bracket_on;
    
    //printf("key_value - my slice details-%d-%d-%d-%d-%d \n", start,stop,value_start,value_stop,bracket_on);
    
    return &my_slice;
}


/*
int dict_search_hidden_buffer (int buffer_start, int buffer_stop, int key[], int k_len) {
    int x=0;
    int y=0;
    int start = -1;
    int stop = -1;
    int match = 0;
    int found = 0;
    
    //printf("d-%d-%d-%d-%d-%d \n",d[0],d[1],d[2],d[3],d[4]);
    //printf("key - %d-%d \n", key[0],key[1]);
    
    for (x=buffer_start; x< buffer_stop; x++) {
        
        if (hidden_objstm_buffer[x] == key[0]) {
            match = 1;
            start = x;

            for (y=1; y < k_len; y++) {
                if (hidden_objstm_buffer[x+y] == key[y]) {
                    match ++;
                }
                else {
                    match = 0;
                    break;
                }
            }
            if (match == k_len) {
                found = 1;
                stop = x+y;
                }
            }
        if (found == 1) {
            break;
        }
    }
    
    if (found == 1) {
        //printf("dict_search FOUND-%d-%d \n", start,stop);
    }
    
    return found;
    
}
*/

int dict_search_buffer (int buffer_start,int buffer_stop, int key[], int k_len) {
    
    int x=0;
    int y=0;
    int start = -1;
    int stop = -1;
    int match = 0;
    int found = -1;
    
    for (x=buffer_start; x< buffer_stop; x++) {
        
        if (buffer[x] == key[0]) {
            match = 1;
            start = x;

            for (y=1; y < k_len; y++) {
                if (buffer[x+y] == key[y]) {
                    match ++;
                }
                else {
                    match = 0;
                    break;
                }
            }
            if (match == k_len) {
                found = 1;
                stop = x+y;
                }
            }
        if (found == 1) {
            break;
        }
    }
    
    
    return found;
}


int dict_search (int*d[],int d_len, int key[], int k_len) {
    
    int x=0;
    int y=0;
    int start = -1;
    int stop = -1;
    int match = 0;
    int found = 0;
    
    for (x=0; x< d_len; x++) {
        
        if (d[x] == key[0]) {
            
            match = 1;
            start = x;

            for (y=1; y < k_len; y++) {
                if (d[x+y] == key[y]) {
                    match ++;
                }
                else {
                    match = 0;
                    break;
                }
            }
            if (match == k_len) {
                found = 1;
                stop = x+y;
                }
            }
        if (found == 1) {
            break;
        }
    }
    
    return found;
}



int get_obj (int obj_num) {
    
    int z=0;
    int match = -1;
    
    global_tmp_obj.obj_num = -1;
    global_tmp_obj.gen_num = 0;
    global_tmp_obj.start = -1;
    global_tmp_obj.stop = -1;
    global_tmp_obj.dict_start = -1;
    global_tmp_obj.dict_stop = -1;
    global_tmp_obj.hidden = 0;
    global_tmp_obj.stream_start = -1;
    
    for (z=0;z < global_obj_counter; z++) {
        
        if (obj[z].obj_num == obj_num) {
            match = z;
            break;
        }}
    
    if (match > -1) {
        global_tmp_obj.obj_num = obj[match].obj_num;
        global_tmp_obj.gen_num = obj[match].gen_num;
        global_tmp_obj.start = obj[match].start;
        global_tmp_obj.stop = obj[match].stop;
        global_tmp_obj.dict_start = obj[match].dict_start;
        global_tmp_obj.dict_stop = obj[match].dict_stop;
        global_tmp_obj.hidden = obj[match].hidden;
        global_tmp_obj.stream_start = obj[match].stream_start;
        
    }
    
    return match;
}




int catalog_to_page_handler (int start, int stop) {
    
    // start at page 1
    // global_page_count set at 1 in pdf_builder = global variable
    // global_page_count = 0;
    
    // initialize other variables
    int x, x2,x3,x4,x5,x6,x7,x8,x9,x10,x11, t1, t2, c1,v1,v2,v3,v4,v5,v6, v7,v8,v9,v10,v11,v12;
    
    int pages_found_dummy, page_root_obj, page_count, page_confirm_check;
    
    int number_of_kids, more_kids, grandkids, great_grandkids, great_great_grandkids, g3_kids;
    
    int kids_obj_found;
    
    int kids_obj_num_tmp;
    
    int start_tmp, stop_tmp, start_tmp2, stop_tmp2, start_tmp3, stop_tmp3, start_tmp4, stop_tmp4, start_tmp5, stop_tmp5, start_tmp6, stop_tmp6;
    
    int count1,count2, count3, count4, count5, count6;
    
    int is_it_pages, is_it_pages2, is_it_pages3, is_it_pages4, is_it_pages5, is_it_pages6;

    int kids_obj_num_tmp2, kids_obj_found2, page_confirm_check2, page_confirm_check3, page_confirm_check4, page_confirm_check5, page_confirm_check6;
    
    int kids_obj_num_tmp3, kids_obj_found3, kids_obj_num_tmp4, kids_obj_found4, kids_obj_num_tmp5, kids_obj_found5;
    
    int kids_obj_num_tmp6, kids_obj_found6;
    
    //int last_obj_tmp;
    
    int* kids_list[10000];
    int* kids2_list[10000];
    int* kids3_list[10000];
    int* kids4_list[10000];
    int* kids5_list[10000];
    int* kids6_list[10000];
    
    char* catalog_str;
    char* page_root_str;
    slice* pages_slice;
    slice* pages2_slice;
    slice* pages3_slice;
    slice* more_kids_slice;
    slice* grandkids_slice;
    slice* great_grandkids_slice;
    slice* great_great_grandkids_slice;
    slice* last_slice;
    
    // june 2023- added to support registering account event if pages are mis-aligned
    
    char* event;
    char* ae_type;
    int dummy=0;
    
    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    // end - new account event variables
    
    
    
    char* dict_3_str_tmp;
    
    // First step - extract /Pages Root obj number from the /Catalog
    // Will need to provide handler here if /Catalog == HIDDEN
    // May go back to Catalog to extract META info
    
    t1 = catalog.dict_start;
    t2 = catalog.dict_stop;
        
    catalog_str = get_string_from_buffer(t1,t2);
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - Catalog dict str- %s \n", catalog_str);
    }
    
    pages_slice = dict_find_key_value_buffer (t1,t2,pages_seq, 6);
    v1 = pages_slice->value_start;
    v2 = pages_slice->value_stop;

    pages_found_dummy = extract_obj_from_buffer(v1,v2);
    
    if (pages_found_dummy > 0) {
        page_root_obj = nn_global_tmp[0].num;
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - pdf page root obj-  %d \n", page_root_obj);
        }
        
    }
    else {
        page_root_obj = -1;
        printf("error: pdf_parser - pdf could not find page_root_obj. \n");
    }
    
    // End - use of Catalog
    
    // Second step - go to the /Pages root and extract Count + Kids
    
    c1 = get_obj(page_root_obj);
    
    if (c1 == -1) {
        printf("error: pdf_parser - could not find page_root_obj - %d \n", page_root_obj);
        
    }
    
    v1 = global_tmp_obj.dict_start;
    v2 = global_tmp_obj.dict_stop;
    
    // set as default -> will updated based on "Count" key found in dictionary + value of that parameter
    page_count = 0;
    
    if (debug_mode == 1) {
        
        if (v1 > -1 && v2 > -1) {
            
            page_root_str = get_string_from_buffer(v1,v2);
            
            if (debug_mode == 1) {
                //printf("update: /Page root str - %s \n", page_root_str);
            }
        }
        
        else {
            printf("error: pdf_parser - could not find /Page root str. \n");
            return -1;
        }
        
        //printf("update:  dict_start / dict_stop : %d-%d \n", v1,v2);
    }
    
    
    
    pages2_slice = dict_find_key_value_buffer(v1,v2, count_seq,5);
    v1 = pages2_slice->value_start;
    v2 = pages2_slice->value_stop;
    
    //printf("update: pages slice - %d - %d \n", v1,v2);
    
    if (v1 > -1 && v2 > -1) {
        page_count = get_int_from_buffer(v1,v2);
    }
    else {
        page_count = 0;
    }
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - PDF page count -%d \n", page_count);
    }
    
    pages3_slice = dict_find_key_value_buffer(global_tmp_obj.dict_start,global_tmp_obj.dict_stop,kids_seq,4);
    v1 = pages3_slice->value_start;
    v2 = pages3_slice->value_stop;
    
    // End - second step - confirmed found /Pages root + extracted list of Kids - need to iterate thru Kids
    
    if (debug_mode == 1) {
        //printf("update: kids found - %s \n", get_string_from_buffer(v1,v2));
    }
    
    // Third step - iterate thru nested /Pages -> /Pages -> /Page
    // Next:  iterate thru Kids to find /Page entries
    // Objective:  build page_list using global Pages
    
    number_of_kids = extract_obj_from_buffer(v1,v2);  // Kids found in Pages_Root
    
    //useful for debugging:  printf("number of kids in /Pages Root found - %d \n", number_of_kids);
    
    // starting new loop here!!
     

    if (number_of_kids > 0) {
        count1=0;
        for (x=0 ; x < number_of_kids ; x++) {
            if (nn_global_tmp[x].num == 0) {
                
                break;
            }
            
            kids_list[x] = nn_global_tmp[x].num;
            count1 ++; }
        
        for (x=0; x < count1 ; x++) {
            
            kids_obj_num_tmp = kids_list[x];
            kids_obj_found = get_obj(kids_obj_num_tmp);
            
            //this is a multi-layered iteration looking for "Page" entries inside "Pages"
            
            // useful for debugging:   printf("update:  MAIN-OUTER-Kids-Loop - %d-%d \n", x, kids_obj_num_tmp);
            
            if (kids_obj_found > -1) {
                
                start_tmp = global_tmp_obj.dict_start;
                stop_tmp = global_tmp_obj.dict_stop;
                
                is_it_pages = dict_search_buffer(start_tmp,stop_tmp,pages_seq,6);
                
                if (is_it_pages != 1) {
                    
                    page_confirm_check = dict_search_buffer(start_tmp,stop_tmp,page_seq,5);
                    page_confirm_check = 1;
                    
                    if (page_confirm_check == 1) {
                        
                        // Creating new entry in Pages
                        Pages[global_page_count].page_obj = kids_obj_num_tmp;
                        
                        if (global_page_count < GLOBAL_MAX_PAGES) {
                            global_page_count ++;
                        }
                        else {
                            
                            if (debug_mode == 1) {
                                printf("warning: pdf_parser - too many pages - exceeded GLOBAL_MAX_PAGES = %d \n", global_page_count);
                            }
                            
                        }
                        
                    }
                }
                
                // in first list of 'kids' -> found another 'Pages' entry
                if (is_it_pages == 1) {
                    
                    more_kids_slice = dict_find_key_value_buffer(start_tmp,stop_tmp,kids_seq,4);
                    v3 = more_kids_slice->value_start;
                    v4 = more_kids_slice->value_stop;
                    more_kids = extract_obj_from_buffer(v3,v4);
                    
                    if (more_kids > 0) {
                        count2 = 0;
                        for (x2=0; x2 < more_kids ; x2++) {
                            if (nn_global_tmp[x2].num == 0) {
                                break;
                            }
                            
                            kids2_list[x2] = nn_global_tmp[x2].num;
                            count2++;
                        }
                        
                        for (x3=0; x3 < count2 ; x3++) {
                            
                            kids_obj_num_tmp2 = kids2_list[x3];
                            kids_obj_found2 = get_obj(kids_obj_num_tmp2);
                            
                            if (kids_obj_found2 > -1) {
                                start_tmp2 = global_tmp_obj.dict_start;
                                stop_tmp2 = global_tmp_obj.dict_stop;
                                is_it_pages2 = dict_search_buffer(start_tmp2,stop_tmp2,pages_seq,6);
                                
                                if (is_it_pages2 != 1) {
                                    
                                    page_confirm_check2 = dict_search_buffer(start_tmp2,stop_tmp2,page_seq,5);
                                    page_confirm_check2 = 1;
                                    if (page_confirm_check2 == 1) {
                                        
                                        //create new entry in global Pages
                                        Pages[global_page_count].page_obj = kids_obj_num_tmp2;
                                        
                                        if (global_page_count < GLOBAL_MAX_PAGES) {
                                            global_page_count ++;
                                        }
                                        else {
                                            
                                            if (debug_mode == 1) {
                                                printf("warning: pdf_parser - too many pages - exceeded cap of GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                            }
                                         
                                        }

                                    }}
                                
                                 
                                
                                if (is_it_pages2 == 1) {
                                    
                                    grandkids_slice = dict_find_key_value_buffer(start_tmp2, stop_tmp2, kids_seq,4);
                                    v5 = grandkids_slice->value_start;
                                    v6 = grandkids_slice->value_stop;
                                    grandkids = extract_obj_from_buffer(v5,v6);
                                    
                                    if (grandkids > 0) {
                                        
                                        count3 = 0;
                                        for (x4=0; x4 < grandkids ; x4++) {
                                            
                                            if (nn_global_tmp[x4].num==0) {
                                                
                                                break;
                                                
                                            }
                                            
                                            kids3_list[x4] = nn_global_tmp[x4].num;
                                            count3++;
                                            
                                        }
                                        
                                    for (x5=0; x5 < count3 ; x5++) {
                                        
                                        kids_obj_num_tmp3 = kids3_list[x5];
                                        kids_obj_found3 = get_obj(kids_obj_num_tmp3);
                                    
                                        //useful for debugging:  printf("3rd Kids-loop - %d-%d \n", x5,kids_obj_num_tmp3);
                                        
                                        if (kids_obj_found3 > -1) {
                                            start_tmp3 = global_tmp_obj.dict_start;
                                            stop_tmp3 = global_tmp_obj.dict_stop;
                                            dict_3_str_tmp = get_string_from_buffer(start_tmp3,stop_tmp3);
                                            
                                            is_it_pages3 = dict_search_buffer(start_tmp3,stop_tmp3, pages_seq,6);
                                            
                                            if (is_it_pages3 != 1) {
                                                
                                                page_confirm_check3 = dict_search_buffer(start_tmp3, stop_tmp3, page_seq,5);
                                                
                                                page_confirm_check3 = 1;
                                                
                                                if (page_confirm_check3 == 1) {
                                                    
                                                    // create new entry in Pages
                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp3;
                                                    
                                                    if (global_page_count < GLOBAL_MAX_PAGES) {
                                                    
                                                        global_page_count ++;
                                                    }
                                                    else {
                                                        
                                                        if (debug_mode == 1) {
                                                            printf("warning: pdf_parser - too many pages - exceeded cap - stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                        }
                                                        
                                                        //printf("error: page count found - %d \n", page_count);
                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                    }
                                                    
                                                    //useful for debugging:  printf("New page created-3rd-%d-%d \n",global_page_count,kids_obj_num_tmp3);
                                                    
                                                }}
                                            
                                            if (is_it_pages3 == 1) {
                                                
                                                great_grandkids_slice= dict_find_key_value_buffer(start_tmp3,stop_tmp3,kids_seq,4);
                                                
                                                v7 = great_grandkids_slice->value_start;
                                                v8 = great_grandkids_slice->value_stop;
                                                
                                                great_grandkids = extract_obj_from_buffer(v7,v8);
                                                
                                                if (great_grandkids >0) {
                                                    
                                                    count4 = 0;
                                                    for (x6=0; x6 < great_grandkids ; x6++) {
                                                        
                                                        if (nn_global_tmp[x6].num == 0) {
                                                            
                                                            break;
                                                        }
                                                        
                                                        kids4_list[x6] = nn_global_tmp[x6].num;
                                                        count4++;
                                                    }

                                                    for (x7=0; x7 < count4 ; x7++) {
                                                        kids_obj_num_tmp4 = kids4_list[x7];
                                                        kids_obj_found4 = get_obj(kids_obj_num_tmp4);
                                                        
                                                        //useful for debugging:  printf("4th kids-loop - %d-%d \n", x7,kids_obj_num_tmp4);
                                                        
                                                        if (kids_obj_found4 > -1) {
                                                            
                                                            start_tmp4 = global_tmp_obj.dict_start;
                                                            stop_tmp4 = global_tmp_obj.dict_stop;
                                                            
                                                            is_it_pages4 = dict_search_buffer(start_tmp4,stop_tmp4, pages_seq,6);
                                                            
                                                            if (is_it_pages4 != 1) {
                                                                
                                                                page_confirm_check4 = dict_search_buffer(start_tmp4, stop_tmp4, page_seq,5);
                                                                
                                                                //useful for debugging-
                                                                //printf("in 4th loop - not pages - page-confirm-%d \n", page_confirm_check4);
                                                                //printf("dict str - %s-\n", get_string_from_buffer(start_tmp4,stop_tmp4));
                                                                
                                                                page_confirm_check4 = 1;
                                                                
                                                                if (page_confirm_check4 == 1) {
                                                                    
                                                                    // create new Page entry
                                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp4;
                                                                    
                                                                    if (global_page_count < GLOBAL_MAX_PAGES) {
                                                                        
                                                                        global_page_count ++;
                                                                    }
                                                                    else {
                                                                        
                                                                        if (debug_mode == 1) {
                                                                            printf("error: pdf_parser -             too many pages - exceeded           cap - GLOBAL_MAX_PAGES =            %d \n",         global_page_count);
                                                                        }
                                                                        
                                                                        //printf("error: page count found - %d \n", page_count);
                                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                        
                                                                    }
                                                                    
                                                                    
                                                                }}
                                                            if (is_it_pages4 == 1) {
                                                                
                                                                //useful for debugging and further editing:
                                                                //printf("cant believe it -more pages- TBD!!!");
                                                                //printf("dict found- cant believe-%s \n", get_string_from_buffer(start_tmp4,stop_tmp4));
                                                                
                                                                great_great_grandkids_slice= dict_find_key_value_buffer(start_tmp4,stop_tmp4,kids_seq,4);
                                                                v9 = great_grandkids_slice->value_start;
                                                                v10 = great_grandkids_slice->value_stop;
                                                                great_great_grandkids = extract_obj_from_buffer(v9,v10);
                                                                
                                                                if (great_great_grandkids >0) {
                                                                    
                                                                    count5 = 0;
                                                                    
                                                                    for (x8=0; x8 < great_grandkids ; x8++) {
                                                                        
                                                                        if (nn_global_tmp[x8].num == 0) {
                                                                            
                                                                            break;
                                                                        }
                                                                        
                                                                        kids5_list[x8] = nn_global_tmp[x8].num;
                                                                        count5++;
                                                                    }

                                                                    for (x9=0; x9 < count5 ; x9++) {
                                                                        
                                                                        kids_obj_num_tmp5 = kids5_list[x9];
                                                                        
                                                                        kids_obj_found5 = get_obj(kids_obj_num_tmp5);
                                                                        
                                                                        //printf("5th kids-loop - %d-%d \n", x9,kids_obj_num_tmp5);
                                                                        
                                                                        if (kids_obj_found5 > -1) {
                                                                            start_tmp5 = global_tmp_obj.dict_start;
                                                                            stop_tmp5 = global_tmp_obj.dict_stop;
                                                                            
                                                                            is_it_pages5 = dict_search_buffer(start_tmp5,stop_tmp5, pages_seq,6);
                                                                            if (is_it_pages5 != 1) {
                                                                                page_confirm_check5 = dict_search_buffer(start_tmp5, stop_tmp5, page_seq,5);
                                                                                
                                                                                //printf("in 5th loop - not pages - page-confirm-%d \n", page_confirm_check5);
                                                                                
                                                                                //printf("dict str - %s-\n", get_string_from_buffer(start_tmp5,stop_tmp5));
                                                                                
                                                                                page_confirm_check5=1;
                                                                                
                                                                                if (page_confirm_check5 == 1) {
                                                                                    
                                                                                    // creating new Pages entry
                                                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp5;
                                                                                    global_page_count ++;
                                                                                    
                                                                                    if (global_page_count > GLOBAL_MAX_PAGES) {
                                                                                        
                                                                                        if (debug_mode==1) {
                                                                                            printf("warning: pdf_parser - too many pages - exceeded cap - stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                        }
                                                                                        
                                                                                        //printf("error: page count found - %d \n", page_count);
                                                                                        
                                                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                        return 0;
                                                                                    }
                                                                                    
                                                                                    //printf("New page created-5th-%d-%d \n",global_page_count,kids_obj_num_tmp5);
                                                                                    
                                                                                }}
                                                                            
                                                                            if (is_it_pages5 == 1) {
                                                                                
                                                                                if (debug_mode == 1) {
                                                                                    printf("warning: pdf_parser - catalog_to_page handler - need 6th loop - unusual nested loop -more pages- handling");
                                                                                }
                                                                                
                                                                                if (debug_mode == 1) {
                                                                                    printf("note:  dict found- very unlikely nesting pattern -%s \n",        get_string_from_buffer(start_tmp5,stop_tmp5));
                                                                                }
                                                                                
                                                                                last_slice = dict_find_key_value_buffer(start_tmp5,stop_tmp5,kids_seq,4);
                                                                                v11 = last_slice->value_start;
                                                                                v12 = last_slice->value_stop;
                                                                                if (v11 > -1 && v12 > -1) {
                                                                                    g3_kids = extract_obj_from_buffer(v11,v12);
                                                                                    
                                                                                    // updating
                                                                                    // replacing g3_kids > -1
                                                                                    // correct check >0
                                                                                    
                                                                                    if (g3_kids > 0) {
                                                                                        count6=0;
                                                                                        for (x10=0; x10 < g3_kids; x10++) {
                                                                                            if (nn_global_tmp[x10].num ==0) {
                                                                                                break;
                                                                                            }
                                                                                            kids6_list[x10] =        nn_global_tmp[x10].num;
                                                                                            count6++;
                                                                                        }
                                                                                        
                                                                                        for (x11=0; x11 < count6; x11++) {
                                                                                            kids_obj_num_tmp6= kids6_list[x11];
                                                                                            kids_obj_found6 = get_obj(kids_obj_num_tmp6);
                                                                                            if (kids_obj_found6 > -1) {
                                                                                                start_tmp6 = global_tmp_obj.dict_start;
                                                                                                stop_tmp6 = global_tmp_obj.dict_stop;
                                                                                                is_it_pages6 = dict_search_buffer(start_tmp6,stop_tmp6,pages_seq,6);
                                                                                                if (is_it_pages6 != 1) {
                                                                                                    page_confirm_check6 = dict_search_buffer(start_tmp6,stop_tmp6, page_seq,5);
                                                                                                    page_confirm_check6=1;
                                                                                                    if (page_confirm_check6 == 1) {
                                                                                                        Pages[global_page_count].page_obj = kids_obj_num_tmp6;
                                                                                                        global_page_count ++;
                                                                                                        
                                                                                                        if (global_page_count > GLOBAL_MAX_PAGES) {
                                                                             
                                                                                                            if (debug_mode == 1) {
                                                                                                                printf("warning: pdf_parser - too many pages - exceeded cap \n");
                                                                                                            }
                                                                                                            //printf("error: page count found - %d \n", page_count);
                                                                                                            //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                                            return 0;
                                                                                                        }
                                                                                                        
                                                                                                        if (debug_mode == 1) {
                                                                                                            printf("warning:  pdf_parser - unusual case- found Pages deep nested loop - Found last page - %s \n", get_string_from_buffer(start_tmp6,stop_tmp6));
                                                                                                        }}
                                                                                            }
                                                                                                if (is_it_pages6 == 1) {
                                                                                                    printf("error:  pdf_parser - unusual case - did not find all pages - still more pages  nested at layer 7 - not currently handled - %d \n", get_string_from_buffer(start_tmp6,stop_tmp6));
                                                                                                }
                                                                                        }
                                                                                    }
                                                                                    
                                                                                    }}
                                                                                
                                                                            }
                                                                            
                                                                                
                                                                        }}}
                                                                
                                                            }
                                                        }
                                                    }}}}}}}}}}} //}}
            }
        }
    }
     
    if (debug_mode == 1) {
        printf("update: pdf_parser - page count - %d- pages_found - %d \n", page_count, global_page_count);
    }
    
    // if there is mis-alignment, then we have not found all of the objects corresponding to pages
    // note:  this is very unusual - but can happen if embedded form, scanned docs, etc.
    
    if (page_count > global_page_count) {
        // register account event
        
        if (debug_mode == 1) {
            printf("update: pdf_parser - potential error - page_count not the same as global_page_count - %d - %d \n", page_count, global_page_count);
        }
        
        // note: this error is very rare
        
        /*
        event = "MISSING_PAGES";
        ae_type = "REJECTED_FILE_PDF";
        strftime(time_stamp, sizeof(time_stamp), "%c", tm);
                
        dummy = register_ae_add_pdf_event(ae_type, event, my_doc.account_name,my_doc.corpus_name, my_doc.file_name, time_stamp);
        */
        
    }
    
    return 0;

    
}

/*
int catalog_to_page_handler_old_works (int start, int stop) {
    
    // start at page 1
    // global_page_count set at 1 in pdf_builder = global variable
    // global_page_count = 0;
    
    // initialize other variables
    int x, x2,x3,x4,x5,x6,x7,x8,x9,x10,x11, t1, t2, c1,v1,v2,v3,v4,v5,v6, v7,v8,v9,v10,v11,v12;
    
    int pages_found_dummy, page_root_obj, page_count, page_confirm_check;
    
    int number_of_kids, more_kids, grandkids, great_grandkids, great_great_grandkids, g3_kids;
    
    int kids_obj_found;
    
    int kids_obj_num_tmp;
    
    int start_tmp, stop_tmp, start_tmp2, stop_tmp2, start_tmp3, stop_tmp3, start_tmp4, stop_tmp4, start_tmp5, stop_tmp5, start_tmp6, stop_tmp6;
    
    int count1,count2, count3, count4, count5, count6;
    
    int is_it_pages, is_it_pages2, is_it_pages3, is_it_pages4, is_it_pages5, is_it_pages6;

    int kids_obj_num_tmp2, kids_obj_found2, page_confirm_check2, page_confirm_check3, page_confirm_check4, page_confirm_check5, page_confirm_check6;
    
    int kids_obj_num_tmp3, kids_obj_found3, kids_obj_num_tmp4, kids_obj_found4, kids_obj_num_tmp5, kids_obj_found5;
    
    int kids_obj_num_tmp6, kids_obj_found6;
    
    //int last_obj_tmp;
    
    int* kids_list[10000];
    int* kids2_list[10000];
    int* kids3_list[10000];
    int* kids4_list[10000];
    int* kids5_list[10000];
    int* kids6_list[10000];
    
    char* catalog_str;
    char* page_root_str;
    slice* pages_slice;
    slice* pages2_slice;
    slice* pages3_slice;
    slice* more_kids_slice;
    slice* grandkids_slice;
    slice* great_grandkids_slice;
    slice* great_great_grandkids_slice;
    slice* last_slice;
    
    
    char* dict_3_str_tmp;
    
    // First step - extract /Pages Root obj number from the /Catalog
    
    t1 = catalog.dict_start;
    t2 = catalog.dict_stop;
        
    catalog_str = get_string_from_buffer(t1,t2);
    
    if (debug_mode == 1) {
        printf("update:  Catalog dict str- %s \n", catalog_str);
    }
    
    pages_slice = dict_find_key_value_buffer (t1,t2,pages_seq, 6);
    v1 = pages_slice->value_start;
    v2 = pages_slice->value_stop;

    pages_found_dummy = extract_obj_from_buffer(v1,v2);
    
    if (pages_found_dummy > 0) {
        page_root_obj = nn_global_tmp[0].num;
        
        if (debug_mode == 1) {
            printf("update: pdf page root obj-  %d \n", page_root_obj);
        }
        
    }
    else {
        page_root_obj = -1;
        printf("error: pdf_parser - pdf could not find page_root_obj - can not process! \n");
        // error code
        return -1;
    }
    
    // End - use of Catalog
    
    // Second step - go to the /Pages root and extract Count + Kids
    
    c1 = get_obj(page_root_obj);
    
    if (c1 == -1) {
        printf("error: pdf_parser - could not find page_root_obj - can not process - %d \n", page_root_obj);
        // error code
        return -1;
    }
    
    v1 = global_tmp_obj.dict_start;
    v2 = global_tmp_obj.dict_stop;
    
    // set as default -> will updated based on "Count" key found in dictionary + value of that parameter
    page_count = 0;
    
    if (debug_mode == 1) {
        
        if (v1 > -1 && v2 > -1) {
            
            page_root_str = get_string_from_buffer(v1,v2);
            
            if (debug_mode == 1) {
                printf("update: /Page root str - %s \n", page_root_str);
            }
        }
        
        else {
            printf("error:  pdf_parser did not find /Page root str! \n");
        }
        
        //printf("update:  dict_start / dict_stop : %d-%d \n", v1,v2);
    }
    
    
    
    pages2_slice = dict_find_key_value_buffer(v1,v2, count_seq,5);
    v1 = pages2_slice->value_start;
    v2 = pages2_slice->value_stop;
    
    //printf("update: pages slice - %d - %d \n", v1,v2);
    
    if (v1 > -1 && v2 > -1) {
        page_count = get_int_from_buffer(v1,v2);
    }
    else {
        page_count = 0;
    }
    
    if (debug_mode == 1) {
        printf("update: PDF page count -%d \n", page_count);
    }
    
    pages3_slice = dict_find_key_value_buffer(global_tmp_obj.dict_start,global_tmp_obj.dict_stop,kids_seq,4);
    v1 = pages3_slice->value_start;
    v2 = pages3_slice->value_stop;
    
    // End - second step - confirmed found /Pages root + extracted list of Kids - need to iterate thru Kids
    
    if (debug_mode == 1) {
        //printf("update: kids found - %s \n", get_string_from_buffer(v1,v2));
    }
    
    // Third step - iterate thru nested /Pages -> /Pages -> /Page
    // Next:  iterate thru Kids to find /Page entries
    // Objective:  build page_list using global Pages
    
    number_of_kids = extract_obj_from_buffer(v1,v2);  // Kids found in Pages_Root
    
    //useful for debugging:  printf("number of kids in /Pages Root found - %d \n", number_of_kids);
    
    // starting new loop here!!
     

    if (number_of_kids > 0) {
        count1=0;
        for (x=0 ; x < number_of_kids ; x++) {
            if (nn_global_tmp[x].num == 0) {
                
                break;
            }
            
            kids_list[x] = nn_global_tmp[x].num;
            count1 ++; }
        
        for (x=0; x < count1 ; x++) {
            
            kids_obj_num_tmp = kids_list[x];
            kids_obj_found = get_obj(kids_obj_num_tmp);
            
            //this is a multi-layered iteration looking for "Page" entries inside "Pages"
            
            // useful for debugging:   printf("update:  MAIN-OUTER-Kids-Loop - %d-%d \n", x, kids_obj_num_tmp);
            
            if (kids_obj_found > -1) {
                
                start_tmp = global_tmp_obj.dict_start;
                stop_tmp = global_tmp_obj.dict_stop;
                
                is_it_pages = dict_search_buffer(start_tmp,stop_tmp,pages_seq,6);
                
                if (is_it_pages != 1) {
                    
                    page_confirm_check = dict_search_buffer(start_tmp,stop_tmp,page_seq,5);
                    page_confirm_check = 1;
                    
                    if (page_confirm_check == 1) {
                        
                        // Creating new entry in Pages
                        Pages[global_page_count].page_obj = kids_obj_num_tmp;
                        
                        
                        // GLOBAL_MAX_PAGES = maximum number of pages that will be read
                        //  ... set at 2000 currently
                        //  ... can be increased ...
                        
                        if (global_page_count < GLOBAL_MAX_PAGES) {
                            global_page_count ++;
                        }
                        else {
                            
                            // Note: safety check for GLOBAL_MAX_PAGES
                            // *** should be adjusted to handle docs > 2000 pages ***
                            
                            printf("error: pdf_parser - too many pages - exceeded GLOBAL_MAX_PAGES = %d \n", global_page_count);
                            
                            //printf("error: page count found - %d \n", page_count);
                            //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                        }
                        
                        //useful for debugging:  printf("New page created-1st-%d-%d \n",global_page_count,kids_obj_num_tmp);
                        
                    }}
                
                // insert break here - test
                is_it_pages = 19;
                
                if (is_it_pages == 19) {
                    return 0;
                }
                // end - break
                
                
                if (is_it_pages == 1) {
                    
                    more_kids_slice = dict_find_key_value_buffer(start_tmp,stop_tmp,kids_seq,4);
                    v3 = more_kids_slice->value_start;
                    v4 = more_kids_slice->value_stop;
                    more_kids = extract_obj_from_buffer(v3,v4);
                    
                    if (more_kids > 0) {

                        count2 = 0;
                        
                        for (x2=0; x2 < more_kids ; x2++) {
                            
                            if (nn_global_tmp[x2].num == 0) {
                                
                                break;
                            }
                            
                            kids2_list[x2] = nn_global_tmp[x2].num;
                            
                            count2++;
                            
                        }
                        
                        for (x3=0; x3 < count2 ; x3++) {
                            
                            kids_obj_num_tmp2 = kids2_list[x3];
                            kids_obj_found2 = get_obj(kids_obj_num_tmp2);
                            
                            //iterating through second level looking for "Page" entries
                            //useful for debugging:  printf("2nd Kids-loop - %d-%d \n", x3,kids_obj_num_tmp2);
                            
                            if (kids_obj_found2 > -1) {
                                start_tmp2 = global_tmp_obj.dict_start;
                                stop_tmp2 = global_tmp_obj.dict_stop;
                                is_it_pages2 = dict_search_buffer(start_tmp2,stop_tmp2,pages_seq,6);
                                
                                if (is_it_pages2 != 1) {
                                    
                                    page_confirm_check2 = dict_search_buffer(start_tmp2,stop_tmp2,page_seq,5);
                                    page_confirm_check2 = 1;
                                    if (page_confirm_check2 == 1) {
                                        
                                        //create new entry in global Pages
                                        Pages[global_page_count].page_obj = kids_obj_num_tmp2;
                                        
                                        if (global_page_count < GLOBAL_MAX_PAGES) {
                                            global_page_count ++;
                                        }
                                        else {

                                            printf("error: pdf_parser - too many pages - exceeded cap of GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                            
                                            //printf("error: page count found - %d \n", page_count);
                                            //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                         
                                        }
                                        
                                        //usefulf for debugging:   printf("New page created-2nd-%d-%d \n",global_page_count,kids_obj_num_tmp2);
                                        
                                    }}
                                
                                if (is_it_pages2 == 1) {
                                    
                                    grandkids_slice = dict_find_key_value_buffer(start_tmp2, stop_tmp2, kids_seq,4);
                                    v5 = grandkids_slice->value_start;
                                    v6 = grandkids_slice->value_stop;
                                    grandkids = extract_obj_from_buffer(v5,v6);
                                    
                                    if (grandkids > 0) {
                                        
                                        count3 = 0;
                                        for (x4=0; x4 < grandkids ; x4++) {
                                            
                                            if (nn_global_tmp[x4].num==0) {
                                                
                                                break;
                                                
                                            }
                                            
                                            kids3_list[x4] = nn_global_tmp[x4].num;
                                            count3++;
                                            
                                        }
                                        
                                    for (x5=0; x5 < count3 ; x5++) {
                                        
                                        kids_obj_num_tmp3 = kids3_list[x5];
                                        kids_obj_found3 = get_obj(kids_obj_num_tmp3);
                                    
                                        //useful for debugging:  printf("3rd Kids-loop - %d-%d \n", x5,kids_obj_num_tmp3);
                                        
                                        if (kids_obj_found3 > -1) {
                                            start_tmp3 = global_tmp_obj.dict_start;
                                            stop_tmp3 = global_tmp_obj.dict_stop;
                                            dict_3_str_tmp = get_string_from_buffer(start_tmp3,stop_tmp3);
                                            
                                            is_it_pages3 = dict_search_buffer(start_tmp3,stop_tmp3, pages_seq,6);
                                            
                                            if (is_it_pages3 != 1) {
                                                
                                                page_confirm_check3 = dict_search_buffer(start_tmp3, stop_tmp3, page_seq,5);
                                                
                                                page_confirm_check3 = 1;
                                                
                                                if (page_confirm_check3 == 1) {
                                                    
                                                    // create new entry in Pages
                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp3;
                                                    
                                                    if (global_page_count < GLOBAL_MAX_PAGES) {
                                                    
                                                        global_page_count ++;
                                                    }
                                                    else {

                                                        printf("error: pdf_parser - too many pages - exceeded cap- stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                        
                                                        //printf("error: page count found - %d \n", page_count);
                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                    }
                                                    
                                                    //useful for debugging:  printf("New page created-3rd-%d-%d \n",global_page_count,kids_obj_num_tmp3);
                                                    
                                                }}
                                            
                                            if (is_it_pages3 == 1) {
                                                
                                                great_grandkids_slice= dict_find_key_value_buffer(start_tmp3,stop_tmp3,kids_seq,4);
                                                
                                                v7 = great_grandkids_slice->value_start;
                                                v8 = great_grandkids_slice->value_stop;
                                                
                                                great_grandkids = extract_obj_from_buffer(v7,v8);
                                                
                                                if (great_grandkids >0) {
                                                    
                                                    count4 = 0;
                                                    for (x6=0; x6 < great_grandkids ; x6++) {
                                                        
                                                        if (nn_global_tmp[x6].num == 0) {
                                                            
                                                            break;
                                                        }
                                                        
                                                        kids4_list[x6] = nn_global_tmp[x6].num;
                                                        count4++;
                                                    }

                                                    for (x7=0; x7 < count4 ; x7++) {
                                                        kids_obj_num_tmp4 = kids4_list[x7];
                                                        kids_obj_found4 = get_obj(kids_obj_num_tmp4);
                                                        
                                                        //useful for debugging:  printf("4th kids-loop - %d-%d \n", x7,kids_obj_num_tmp4);
                                                        
                                                        if (kids_obj_found4 > -1) {
                                                            
                                                            start_tmp4 = global_tmp_obj.dict_start;
                                                            stop_tmp4 = global_tmp_obj.dict_stop;
                                                            
                                                            is_it_pages4 = dict_search_buffer(start_tmp4,stop_tmp4, pages_seq,6);
                                                            
                                                            if (is_it_pages4 != 1) {
                                                                
                                                                page_confirm_check4 = dict_search_buffer(start_tmp4, stop_tmp4, page_seq,5);
                                                                
                                                                //useful for debugging-
                                                                //printf("in 4th loop - not pages - page-confirm-%d \n", page_confirm_check4);
                                                                //printf("dict str - %s-\n", get_string_from_buffer(start_tmp4,stop_tmp4));
                                                                
                                                                page_confirm_check4 = 1;
                                                                
                                                                if (page_confirm_check4 == 1) {
                                                                    
                                                                    // create new Page entry
                                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp4;
                                                                    
                                                                    if (global_page_count < GLOBAL_MAX_PAGES) {
                                                                        
                                                                        global_page_count ++;
                                                                    }
                                                                    else {
                                                                    
                                                                        printf("error: pdf_parser - too many pages - exceeded cap - GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                        
                                                                        //printf("error: page count found - %d \n", page_count);
                                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                        
                                                                    }
                                                                    
                                                                    
                                                                }}
                                                            if (is_it_pages4 == 1) {
                                                                
                                                                //useful for debugging and further editing:
                                                                //printf("cant believe it -more pages- TBD!!!");
                                                                //printf("dict found- cant believe-%s \n", get_string_from_buffer(start_tmp4,stop_tmp4));
                                                                
                                                                great_great_grandkids_slice= dict_find_key_value_buffer(start_tmp4,stop_tmp4,kids_seq,4);
                                                                v9 = great_grandkids_slice->value_start;
                                                                v10 = great_grandkids_slice->value_stop;
                                                                great_great_grandkids = extract_obj_from_buffer(v9,v10);
                                                                
                                                                if (great_great_grandkids >0) {
                                                                    
                                                                    count5 = 0;
                                                                    
                                                                    for (x8=0; x8 < great_grandkids ; x8++) {
                                                                        
                                                                        if (nn_global_tmp[x8].num == 0) {
                                                                            
                                                                            break;
                                                                        }
                                                                        
                                                                        kids5_list[x8] = nn_global_tmp[x8].num;
                                                                        count5++;
                                                                    }

                                                                    for (x9=0; x9 < count5 ; x9++) {
                                                                        
                                                                        kids_obj_num_tmp5 = kids5_list[x9];
                                                                        
                                                                        kids_obj_found5 = get_obj(kids_obj_num_tmp5);
                                                                        
                                                                        //printf("5th kids-loop - %d-%d \n", x9,kids_obj_num_tmp5);
                                                                        
                                                                        if (kids_obj_found5 > -1) {
                                                                            start_tmp5 = global_tmp_obj.dict_start;
                                                                            stop_tmp5 = global_tmp_obj.dict_stop;
                                                                            
                                                                            is_it_pages5 = dict_search_buffer(start_tmp5,stop_tmp5, pages_seq,6);
                                                                            if (is_it_pages5 != 1) {
                                                                                page_confirm_check5 = dict_search_buffer(start_tmp5, stop_tmp5, page_seq,5);
                                                                                
                                                                                //printf("in 5th loop - not pages - page-confirm-%d \n", page_confirm_check5);
                                                                                
                                                                                //printf("dict str - %s-\n", get_string_from_buffer(start_tmp5,stop_tmp5));
                                                                                
                                                                                page_confirm_check5=1;
                                                                                
                                                                                if (page_confirm_check5 == 1) {
                                                                                    
                                                                                    // creating new Pages entry
                                                                                    Pages[global_page_count].page_obj = kids_obj_num_tmp5;
                                                                                    global_page_count ++;
                                                                                    
                                                                                    if (global_page_count > GLOBAL_MAX_PAGES) {
                                                                                        printf("error: pdf_parser - too many pages - exceeded cap- stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                        
                                                                                        //printf("error: page count found - %d \n", page_count);
                                                                                        
                                                                                        //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                        return 0;
                                                                                    }
                                                                                    
                                                                                    //printf("New page created-5th-%d-%d \n",global_page_count,kids_obj_num_tmp5);
                                                                                    
                                                                                }}
                                                                            
                                                                            if (is_it_pages5 == 1) {
                                                                                printf("error: pdf_parser - catalog_to_page handler - need 6th loop- unusual nested loop -more pages- handling");
                                                                                
                                                                                if (debug_mode == 1) {
                                                                                    printf("note:  dict found-          cant believe-%s \n",        get_string_from_buffer(start_tmp5,stop_tmp5));
                                                                                }
                                                                                
                                                                                last_slice = dict_find_key_value_buffer(start_tmp5,stop_tmp5,kids_seq,4);
                                                                                v11 = last_slice->value_start;
                                                                                v12 = last_slice->value_stop;
                                                                                if (v11 > -1 && v12 > -1) {
                                                                                    g3_kids = extract_obj_from_buffer(v11,v12);
                                                                                    
                                                                                    // updating
                                                                                    // replacing g3_kids > -1
                                                                                    // correct check >0
                                                                                    
                                                                                    if (g3_kids > 0) {
                                                                                        count6=0;
                                                                                        for (x10=0; x10 < g3_kids; x10++) {
                                                                                            if (nn_global_tmp[x10].num ==0) {
                                                                                                break;
                                                                                            }
                                                                                            kids6_list[x10] =        nn_global_tmp[x10].num;
                                                                                            count6++;
                                                                                        }
                                                                                        
                                                                                        for (x11=0; x11 < count6; x11++) {
                                                                                            kids_obj_num_tmp6= kids6_list[x11];
                                                                                            kids_obj_found6 = get_obj(kids_obj_num_tmp6);
                                                                                            if (kids_obj_found6 > -1) {
                                                                                                start_tmp6 = global_tmp_obj.dict_start;
                                                                                                stop_tmp6 = global_tmp_obj.dict_stop;
                                                                                                is_it_pages6 = dict_search_buffer(start_tmp6,stop_tmp6,pages_seq,6);
                                                                                                if (is_it_pages6 != 1) {
                                                                                                    page_confirm_check6 = dict_search_buffer(start_tmp6,stop_tmp6, page_seq,5);
                                                                                                    page_confirm_check6=1;
                                                                                                    if (page_confirm_check6 == 1) {
                                                                                                        Pages[global_page_count].page_obj = kids_obj_num_tmp6;
                                                                                                        global_page_count ++;
                                                                                                        
                                                                                                        if (global_page_count > GLOBAL_MAX_PAGES) {
                                                                                                            printf("error: too many pages - exceeded cap \n");
                                                                                                            //printf("error: page count found - %d \n", page_count);
                                                                                                            //printf("error: stopping at GLOBAL_MAX_PAGES = %d \n", global_page_count);
                                                                                                            return 0;
                                                                                                        }
                                                                                                        
                                                                                                        if (debug_mode == 1) {
                                                                                                            printf("error:  pdf_parser - unusual case- found Pages deep nested loop - Found last page - %s \n", get_string_from_buffer(start_tmp6,stop_tmp6));
                                                                                                        }}
                                                                                            }
                                                                                                if (is_it_pages6 == 1) {
                                                                                                    printf("error:  unusual case - did not find all pages - still more pages  nested at layer 7 - need to build out TBD - %d \n", get_string_from_buffer(start_tmp6,stop_tmp6));
                                                                                                }
                                                                                        }
                                                                                    }
                                                                                    
                                                                                    }}
                                                                                
                                                                            }
                                                                            
                                                                                
                                                                        }}}
                                                                
                                                            }
                                                        }
                                                    }}}}}}}}}}} //}}
            }
        }
    }
     
    if (debug_mode == 1) {
        printf("update:  page count - %d- pages_found - %d \n", page_count, global_page_count);
    }
    
    return 1;

    
}
*/





int objstm_handler (int start, int stop) {

    int x,y;
    int count2;
    int tmp1;
    int tmp2;
    
    // increasing limits - 5K works
    int obj_num[50000];
    int o = 0;
    int byte_offsets[50000];
    int b = 0;
    
    // increasing limits - 1K works
    int* tmp [10000];
    
    int string_on = 0;
    int string_count = 0;
    int my_number;
    int alternator = 1;
    int start_point = 0;
    int new_objs_created = 0;
    
    int dict_on = 0;
    int bracket_counter = 0;
    
    int entry_global_buffer_cursor;
    
    // Do we need to check if Objstm is Flate - is it possible not compressed??
    
    int stream_len;
    
    //printf("IN OBJSTM HANDLER! %d-%d \n",start,stop);
    
    stream_len = flate_handler_buffer_v2(start,stop);
    
    if (stream_len <= 0) {
        
        if (debug_mode == 1) {
            printf("warning: pdf_parser - no stream found - could not decompress Flate ObjStm- skipping. \n");
        }
        
        free(flate_dst_tmp_buffer);
        return -1;
    }
    
    else {
        if (stream_len > 1000000) {
            if (debug_mode == 1) {
                printf("warning: pdf_parser - large stream found in OBJSTM HANDLER - %d \n", stream_len);
            }
        }
    }
    
    //printf("update: stream_len_found - %d \n", stream_len);
    
    /*
    for (x=0; x < stream_len; x++) {
        printf("%c",flate_dst_tmp_buffer[x]);
    }
     */
    
    for (x=0; x< stream_len; x++) {
        
        //printf("%c", flate_dst_tmp_buffer[x]);
        
        
        if ((string_on == 1) && (flate_dst_tmp_buffer[x] == 32 || flate_dst_tmp_buffer[x] == 10 || flate_dst_tmp_buffer[x] == 13)) {
            
            string_on = 0;
            tmp[string_count] = 0;
            my_number = get_int_from_byte_array(tmp);
            string_count = 0;
            
            if (alternator == 1) {
                obj_num[o] = my_number;
                o++;
                alternator = 0;
            }
            else {
                byte_offsets[b] = my_number;
                b++;
                alternator = 1;
            }
            }
        
        // obj #s and byte offset section will terminate with start of content
        // most files:  start of content signal is '<' (ascii 60)
        // there are at least some files where content start could be '[' (ascii 91)
        // need to further test ascii 91 start if widespread, or if creates other problems
        
        if (flate_dst_tmp_buffer[x] == 60 || flate_dst_tmp_buffer[x] == 91) {
            start_point = x;
            break;
        }
        
        if ((flate_dst_tmp_buffer[x] > 47) && (flate_dst_tmp_buffer[x] < 58)) {
            tmp[string_count] = flate_dst_tmp_buffer[x];
            string_count ++;
            string_on = 1;
        }
        
    }
    
    entry_global_buffer_cursor = global_buffer_cursor;
    
    //printf("update: objstm object count found - %d \n", o);
    //printf("update: objstm byte offset count found - %d \n", b);
    
    for (x=0; x < o ; x++) {
        
        //printf("update: objstm_handler - obj iter - %d - %d \n", x, obj_num[x]);
        
        if (byte_offsets[x] >= 0) {
            tmp2 = start_point + byte_offsets[x];
        }
        else {
            // edge case - if byte offsets is negative value
            tmp2 = start_point;
        }
        
        // new fix - jan 15
        // add safety check: if tmp2 longer than stream_len -> cap tmp2
        if (tmp2 > stream_len) {
            tmp2 = stream_len;
        }
        // end - new fix
        
        if (x+1 < o)
        {
            
            // new update - jan 15
            if (byte_offsets[x+1] >= 0) {
                tmp1 = byte_offsets[x+1] + start_point;
            }
            else {
                // edge case - if byte_offsets is negative
                tmp1 = start_point;
            }
        
            // new fix - jan 15
            if (tmp1 > stream_len) {
                tmp1 = stream_len;
            }
            
            // end - new fix
            
            //printf("update: iter thru objects -%d-%d-%d \n", x,byte_offsets[x], byte_offsets[x+1]);
        }
        
        else
        {
            tmp1 = stream_len;
            
            if ((stream_len - tmp2) > 10000) {
                tmp1 = tmp2 + 10000;
            }
            
        }
        
        count2 = 0;
        
        obj[global_obj_counter].obj_num = obj_num[x];
        obj[global_obj_counter].gen_num = 0;
        
        // new approach - add to global buffer - starts here
        obj[global_obj_counter].hidden = 1;
        obj[global_obj_counter].start = global_buffer_cursor;
        obj[global_obj_counter].dict_start = global_buffer_cursor;
        obj[global_obj_counter].dict_stop = -1;
        
        if (global_obj_counter > 199000) {
            printf("warning:  pdf_parser - global obj counter exceeeded MAX - 200,000 objects- unusual case - %d - %d \n", global_obj_counter, global_buffer_cursor);
        }
        
        //printf("update: tmp2 - %d - tmp1 - %d \n", tmp2, tmp1);
        
        for (y=tmp2; y < tmp1; y++) {
            
            // new approach - add to buffer directly @ global_buffer_cursor
            
            //printf("%c", flate_dst_tmp_buffer[y-100]);
            
            //printf("global buffer cursor: %d \n", global_buffer_cursor);
            
            buffer[global_buffer_cursor] = flate_dst_tmp_buffer[y];
            
            if (buffer[global_buffer_cursor] == 60) {
                if (y+1 < tmp1) {
                    if (flate_dst_tmp_buffer[y+1] == 60) {
                        dict_on = 1;
                        bracket_counter ++;
                    }
                }
            }
            
            if (buffer[global_buffer_cursor] == 62 && dict_on) {
                if (y+1 < tmp1) {
                    if (flate_dst_tmp_buffer[y+1] == 62) {
                        bracket_counter --;
                        if (bracket_counter <= 0) {
                            dict_on = 0;
                            obj[global_obj_counter].dict_stop = global_buffer_cursor+1;
                            obj[global_obj_counter].stream_start = global_buffer_cursor+2;
                            
                            // objstm_debug
                            //printf("\nupdate: objstm_handler - %s \n", get_string_from_buffer(obj[global_obj_counter].dict_start,obj[global_obj_counter].dict_stop));
                            
                        }
                        
                    }
                }
            }
            
            global_buffer_cursor ++;
            
        }
        
        // new approach
        if (obj[global_obj_counter].dict_stop == -1) {
            obj[global_obj_counter].dict_stop = global_buffer_cursor-1;
        }
        obj[global_obj_counter].stop = global_buffer_cursor-1;
        
        // end - new approach
        
        //obj[global_obj_counter].stop = hidden_buffer_cursor-1;
        //obj[global_obj_counter].dict_stop = hidden_buffer_cursor-1;
        // will need to build out way to track end of dict in hidden buffer
        
        // do not exceed global max obj count = 100000
        if (global_obj_counter < 199998) {
            global_obj_counter ++;
            new_objs_created ++;
        }
        
        //useful for debugging:  printf("new obj created - %d - %d \n", global_obj_counter, new_objs_created);
        
    }
    
    //printf("SURVIVED TO END OF LOOP IN OBJSTM! \n");
    
    free(flate_dst_tmp_buffer);
    
    return new_objs_created;
    
}


int page_resource_handler (int page_count) {
    
    int w, x, y, p, p2, p3, p4, p5, p6, p7, p8,p9, p10, p11,p12,p14;
    int f1,f2;
    
    int start1, stop1, start2, stop2;
    int dict_start, dict_stop;
    
    int o, o1,o2, o3,o4,o5,o6;
    
    int img_dict_start, img_dict_stop;
    
    char* page_obj_dict_tmp_str;
    
    char* contents_str;
    
    char* font_str;
    
    char* resources_str;
    
    char* tmp_str;
    
    // testing
    char* images_str_test;
    // end - testing
    
    char* f_str;
    
    int new_font;
    int number_of_content_entries_found ;
    int number_of_fonts_found;
    int number_of_fonts_found2;
    
    int number_of_resources_found;
    
    int obj_tmp, obj_tmp2;
      
    int number_of_images_found;
    int number_of_l2_images_found;
    
    int image_max_add;
    int image_create_counter;
    
    // insert - test
    int test1, test2;
    int obj_test1;
    // end - test
    
    slice* contents_slice;
    slice* font_slice;
    slice* resources_slice;
    slice* images_slice;
    slice* xobj_slice;
    slice* form_slice;
    slice* img_object_slice;
    slice* img_object_slice2;
    
    global_font_count = 0;
    
    //pages_found_counter = global_page_count;
    
    //useful for debugging:  printf("GLOBAL PAGE COUNT - %d \n", global_page_count);
    
    for (x=0; x < global_page_count ; x++) {
        
        //printf("update: PAGE RESOURCE HANDLER - Pages - %d - %d \n ", x,Pages[x].page_obj);
        
        p = get_obj(Pages[x].page_obj);
        
        if (p < 0) {
            if (debug_mode == 1) {
                printf("warning: pdf_parser - problem page - unusual - %d - NOT FOUND. \n", x);
            }
        }
        
        if (p > -1) {
            
            // found page object in list
            dict_start = global_tmp_obj.dict_start;
            dict_stop = global_tmp_obj.dict_stop;
            
            //printf("update: Page found - dict start-stop-%d-%d-%d-%d \n", dict_start,dict_stop,global_tmp_obj.start,global_tmp_obj.stop);
            
            if (dict_stop - dict_start < 100000) {
                
                page_obj_dict_tmp_str = get_string_from_buffer(dict_start,dict_stop);
                
                //printf("update: Page found - dict str-%s \n", page_obj_dict_tmp_str);
                
            }
            else {
                
                if (debug_mode == 1) {
                    printf("warning: pdf_parser -  page dictionary mis-mapping- %d - %d \n", dict_start,dict_stop);
                }
                
                dict_start = global_tmp_obj.start;
                dict_stop = global_tmp_obj.stop;
            }
            
            // Step 1- CONTENTS
            
            contents_slice = dict_find_key_value_buffer(dict_start,dict_stop,contents_seq,8);
            p2 = contents_slice->value_start;
            p3 = contents_slice->value_stop;
            
            if (p2 > - 1 && p3 > -1) {
                
                
                if (debug_mode == 1) {
                    //contents_str = get_string_from_buffer(p2,p3);
                    //printf("update: contents_str: %s \n", contents_str);
                }
                
                
                number_of_content_entries_found = extract_obj_from_buffer(p2,p3);
                
                // updating - replacing number_of_content_entries_found > -1
                // correct check:   number_of_conten_entries_found > 0
                
                if (number_of_content_entries_found > 0) {
                    
                    if (debug_mode == 1) {
                        //printf("update: number of content entries-%d \n", number_of_content_entries_found);
                    }
                    
                    Pages[x].content_entries = number_of_content_entries_found;
                    for (y=0; y< number_of_content_entries_found ; y++) {
                        Pages[x].content[y] = nn_global_tmp[y].num;
                        
                        if (debug_mode == 1) {
                            //printf("update: content objects - %d \n", nn_global_tmp[y].num);
                        }
                        
                        }
                }
                else {
                    //  could not find contents!
                    //  contents_str = get_string_from_buffer(dict_start,dict_stop);
                    
                    if (debug_mode == 1) {
                        printf("warning: pdf_parser - page -no content found - %s \n", contents_str);
                    }
                }
            }
            
            // Step 2- IMAGES
            
            Pages[x].image_entries = 0;
            xobj_slice = dict_find_key_value_buffer(dict_start,dict_stop,xobject_seq,7);
            p8 = xobj_slice->value_start;
            p9 = xobj_slice->value_stop;
            if (p8 > -1 && p9 > -1) {
                
                // testing
                if (debug_mode == 1) {
                    images_str_test = get_string_from_buffer(p8,p9);
                    printf("update: pdf_parser - gathering images- xobject str-%s-%d-%d \n", images_str_test,p8,p9);
                }
                // end-testing
                
                number_of_images_found = extract_obj_from_buffer(p8,p9);
                
                // update
                // replacing:  number_of_images_found > -1
                // correct check:  number_of_images_found > 0
                
                if (number_of_images_found > 0) {
                    
                    //printf("update:  number of image entries-%d \n", number_of_images_found);
                    
                    // MAX of 500 images per page
                    
                    if (number_of_images_found > 499) {
                        image_max_add = 499;
                        printf("warning: pdf_parser - found more than 500 images on single page - capping page- %d \n", number_of_images_found);
                    }
                    else {
                        image_max_add = number_of_images_found;
                    }
                    
                    // new master counter for image_create_counter
                    image_create_counter = 0;
                    
                    //Pages[x].image_entries = image_max_add;
                    
                    for (y=0; y < image_max_add; y++) {
                    
                        // insert new check - look for XObject now in the indirect pointer
                        // if found -> then save the reference
                        //     else -> go one level deeper
                        
                        if (debug_mode == 1) {
                            //printf("update: in image loop - 1st indirect ref - %d \n", nn_global_tmp[y].num);
                        }
                        
                        o = get_obj(nn_global_tmp[y].num);
                        
                        img_dict_start = global_tmp_obj.dict_start;
                        img_dict_stop = global_tmp_obj.dict_stop;
                        
                        o1 = dict_search_buffer(img_dict_start,img_dict_stop,xobject_seq,7);
                        o2 = dict_search_buffer(img_dict_start,img_dict_stop,form_seq,4);
                        
                        /*
                        img_object_slice = dict_find_key_value_buffer(img_dict_start,img_dict_stop,xobject_seq,7);
                        o1 = xobj_slice->value_start;
                        o2 = xobj_slice->value_stop;
                        */
                        
                        if (debug_mode == 1) {
                            //printf("update img obj dict - %s \n", get_string_from_buffer(img_dict_start,img_dict_stop));
                        }
                        
                        // if keys "XObject" or "Form" found -> save as image
                        
                        if (o1 > -1 || o2 > -1) {
                            
                            // found xobject reference
                            
                            if (debug_mode == 1) {
                                //printf("update: found xobject - saving image! \n");
                            }
                            
                            if (image_create_counter < 499) {
                            
                                Pages[x].images[image_create_counter] = nn_global_tmp[y].num;
                                strcpy(Pages[x].image_names[image_create_counter],nn_global_tmp[y].name);
                            
                                //printf("update: saving images list - %d-%d-%s \n", y,nn_global_tmp[y].num,nn_global_tmp[y].name);
                            
                                Pages[x].image_x_coord[image_create_counter] = 0;
                                Pages[x].image_y_coord[image_create_counter] = 0;
                                Pages[x].image_cx_coord[image_create_counter] = 0;
                                Pages[x].image_cy_coord[image_create_counter] = 0;
                                Pages[x].image_found_on_page[image_create_counter] = 0;
                            
                                image_create_counter ++;
                            }
                            
                        }
                        
                        else {
                            
                            if (debug_mode == 1) {
                                //printf("update: pdf_parser - did not find keys for Form or XObject- need to keep analyzing at deeper level of dict structure ! \n");
                            }
                            
                            if (img_dict_start > -1 && img_dict_stop > -1) {
                            
                                //printf("get_string - %s \n", get_string_from_buffer(img_dict_start,img_dict_stop));
                                
                                number_of_l2_images_found = extract_obj_from_buffer_with_safety_check(img_dict_start,img_dict_stop);
                                
                                //printf("update: obj extraction result - %d \n", number_of_l2_images_found);

                            if (number_of_l2_images_found > 0) {
                                
                                for (o3=0; o3 < number_of_l2_images_found; o3++) {
                                    
                                    if (debug_mode == 1) {
                                        //printf("update - found objects - %s-%d \n", nn_global_tmp[o3].name, nn_global_tmp[o3].num);
                                    }
                                    
                                    o = get_obj(nn_global_tmp[y].num);
                                    
                                    img_dict_start = global_tmp_obj.dict_start;
                                    img_dict_stop = global_tmp_obj.dict_stop;
                                    
                                    if (debug_mode == 1) {
                                        //printf("update - obj dict - %s \n", get_string_from_buffer(img_dict_start, img_dict_stop));
                                    }
                                    
                                    img_object_slice = dict_find_key_value_buffer(img_dict_start,img_dict_stop,xobject_seq,7);
                                    o1 = xobj_slice->value_start;
                                    o2 = xobj_slice->value_stop;
                                    
                                    img_object_slice2 = dict_find_key_value_buffer(img_dict_start,img_dict_stop, image_seq,5);
                                    
                                    if ((o1 > -1 && o2 > -1) || (img_object_slice2->value_start > -1 && img_object_slice2->value_stop > -1)) {
                                        
                                        //printf("update- confirmed XObj found - saving image! \n");
                                        
                                        if (image_create_counter < 499) {
                                            
                                            Pages[x].images[image_create_counter] = nn_global_tmp[o3].num;
                                            strcpy(Pages[x].image_names[image_create_counter],nn_global_tmp[o3].name);
                                        
                                            //printf("update: saving images list - %d-%d-%s \n", y,nn_global_tmp[y].num,nn_global_tmp[y].name);
                                        
                                            Pages[x].image_x_coord[image_create_counter] = 0;
                                            Pages[x].image_y_coord[image_create_counter] = 0;
                                            Pages[x].image_cx_coord[image_create_counter] = 0;
                                            Pages[x].image_cy_coord[image_create_counter] = 0;
                                            Pages[x].image_found_on_page[image_create_counter] = 0;
                                        
                                        image_create_counter ++;
                                        }
                                    }
                                }

                                }
                            else {
                                
                                if (debug_mode == 1) {
                                    //printf("update: pdf_parser - no image found - skipping! \n");
                                }
                                
                            }
                            }
                            
                        }
                        
                        
                        Pages[x].image_entries = image_create_counter;
                        
                        /*
                        Pages[x].images[y] = nn_global_tmp[y].num;
                        strcpy(Pages[x].image_names[y],nn_global_tmp[y].name);
                        
                        //printf("update: saving images list - %d-%d-%s \n", y,nn_global_tmp[y].num,nn_global_tmp[y].name);
                        
                        Pages[x].image_x_coord[y] = 0;
                        Pages[x].image_y_coord[y] = 0;
                        Pages[x].image_cx_coord[y] = 0;
                        Pages[x].image_cy_coord[y] = 0;
                        Pages[x].image_found_on_page[y] = 0;
                        */
                    }
                }
                else {
                    //printf("xobj found - but - no images found on page \n");
                }
            }
            else {
                //printf("no xobj found \n");
            }
    
            
            // Step 3- FONTS
            
            //printf("iterating to FONTS - %d - %d \n", dict_start, dict_stop);
            
            font_slice = dict_find_key_value_buffer(dict_start,dict_stop,font_seq,4);
            p4 = font_slice->value_start;
            p5 = font_slice->value_stop;
            
            //printf("p4: %d - p5: %d \n", p4, p5);
            
            if (p4 > - 1 && p5 > -1) {
                
                // found "Font" in obj dictionary
                
                number_of_fonts_found = extract_obj_from_buffer(p4,p5);
                 
                //printf("number of fonts found: %d \n", number_of_fonts_found);
                
                if (number_of_fonts_found > 0) {
                      
                    for (y=0; y< number_of_fonts_found ; y++) {
                        
                        Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[y].num;
                        
                        //printf("iterating thru fonts: %d %d %s \n", y, nn_global_tmp[y].num, nn_global_tmp[y].name);
                        
                        if (strlen(nn_global_tmp[y].name) == 0) {
                            
                            if (debug_mode == 1) {
                                //printf("update: pdf_parser - PATH - FONT found - no name found - need to iterate on font obj num to get to name! \n");
                            }
                            
                            obj_tmp = get_obj(nn_global_tmp[y].num);
                            
                            if (obj_tmp > -1) {
                                start1 = global_tmp_obj.dict_start;
                                stop1 = global_tmp_obj.dict_stop;
                                font_slice = dict_find_key_value_buffer(start1,stop1, font_seq,4);
                                p6 = font_slice->value_start;
                                p7 = font_slice->value_stop;
                                
                                if (p6 == -1 || p7 == -1) {
                                    
                                    f_str = get_string_from_buffer(start1,stop1);
                                    
                                    number_of_fonts_found2 = extract_obj_from_buffer(start1,stop1);
                                    
                                    //printf("update: num of fonts found - %d \n", number_of_fonts_found2);
                                    
                                    if (number_of_fonts_found2 > 0) {
                                        
                                        //printf("found name/num values- %d \n", number_of_fonts_found2);
                                        
                                        for (w=0; w < number_of_fonts_found2 ; w++) {
                                            
                                            //printf("found - %d - %s \n", nn_global_tmp[w].num,nn_global_tmp[w].name);
                                            
                                            if (strlen(nn_global_tmp[w].name) > 0) {
                                                
                                                // CHECK FOR NEW FONT TO ADD TO LIST
                                                
                                                new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                
                                                if (new_font == 999) {
                                                    strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                    Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                    Font_CMAP[global_font_count].page_apply_counter = 1;
                                                    Font_CMAP[global_font_count].pages[0] = x;
                                                    
                                                    // adding default setting for cmap_apply = 0
                                                    Font_CMAP[global_font_count].cmap_apply = 0;
                                                    
                                                    global_font_count ++;
                                                    
                                                    //printf("FOUND FONT - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                                    
                                                }
                                                else {
                                                    Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                    Font_CMAP[new_font].page_apply_counter ++;
                                                    
                                                    // adding default setting for cmap_apply = 0
                                                    Font_CMAP[new_font].cmap_apply = 0;
                                                }
                                            }}
                                    }
                                }
                                else {
                                    
                                    //printf("In secondary iteration - found Font object-%d-%d-%d \n", nn_global_tmp[y].num,p6,p7);
                            
                                    tmp_str = get_string_from_buffer(p6,p7);
                                    
                                    //printf("update: FONT - %s \n", tmp_str);
                                    
                                    number_of_fonts_found2 = extract_obj_from_buffer(p6,p7);
                                    
                                    // update
                                    // replacing old check:   number_of_fonts_found2 > -1
                                    // with correct check:  number_of_fonts_found2 > 0
                                    
                                    if (number_of_fonts_found2 > 0) {
                                        
                                        for (w=0; w < number_of_fonts_found2 ; w++) {
                                            if (strlen(nn_global_tmp[w].name) > 0) {
                                                
                                                // CHECK FOR NEW FONT TO ADD TO LIST
                                                
                                                new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                if (new_font == 999) {
                                                    
                                                    strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                    Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                    
                                                    Font_CMAP[global_font_count].page_apply_counter = 1;
                                                    Font_CMAP[global_font_count].pages[0] = x;
                                                    
                                                    // insert default setting
                                                    Font_CMAP[global_font_count].cmap_apply = 0;

                                                    global_font_count ++;
                                                    
                                                    //printf("FOUND FONT - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                            }
                                                else {
                                                    
                                                    Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                    Font_CMAP[new_font].page_apply_counter ++;
                                                    
                                                    // new default setting
                                                    Font_CMAP[new_font].cmap_apply = 0;
                                                }
                                                
                                                
                                                
                                            }}}}
                            }}
                        
                        else {
                            
                            // CHECK FOR NEW FONT TO ADD TO LIST
                            
                            new_font = is_new_font(nn_global_tmp[y].name, nn_global_tmp[y].num);
                            
                            if (new_font == 999) {
                                
                                if (strlen(nn_global_tmp[y].name) > 50) {
                                    printf("warning: pdf_parser - unusual - font name too long - can not save - %d \n", strlen(nn_global_tmp[y].name));
                                }
                                
                                else
                                {
                                    
                                    strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[y].name);
                                    Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[y].num;
                                    
                                    Font_CMAP[global_font_count].page_apply_counter = 1;
                                    Font_CMAP[global_font_count].pages[0] = x;
                                    
                                    // adding new default setting
                                    Font_CMAP[global_font_count].cmap_apply = 0;
                                    
                                    global_font_count ++;
                                    
                                    //printf("survived to add new font - %d \n", global_font_count);
                                }
                            }
                            
                            else {
                                
                                Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                Font_CMAP[new_font].page_apply_counter ++;
                                                                
                                // adding new default setting
                                Font_CMAP[new_font].cmap_apply = 0;
                                
                                
                            }
                        }}
                    }
                }
                
            else {
                
                // did not find "Font" in obj dictionary
                // look for "Resources"

                //printf("PATH - could not find FONT - looking now for Resources! \n");
                
                resources_slice = dict_find_key_value_buffer(dict_start,dict_stop,resources_seq,9);
                p4 = resources_slice->value_start;
                p5 = resources_slice->value_stop;
                
                if (p4 > - 1 && p5 > -1) {
                    
                    // found "Resources" in obj_dictionary
                    
                    resources_str = get_string_from_buffer(p4,p5);
                    
                    if (debug_mode == 1) {
                        //printf("update:  page-RESOURCES found - %s \n", resources_str);
                    }
                    
                    // new - check for rare case where /xobject is in resources- and contains the fonts
                    
                    images_slice = dict_find_key_value_buffer(p4,p5,xobject_seq,7);
                    p8 =images_slice->value_start;
                    p9 =images_slice->value_stop;
                    
                    if (p8 > -1 && p9 > -1) {
                        
                        if (debug_mode == 1) {
                            //printf("update: edge case - need to look inside XObject to find font! \n");
                        }
                        
                        number_of_resources_found = extract_obj_from_buffer(p8,p9);
                        
                        if (number_of_resources_found > 0) {
                            
                            for (y=0; y < number_of_resources_found; y++) {
                                
                                obj_tmp = get_obj(nn_global_tmp[y].num);
                                
                                if (debug_mode == 1) {
                                    //printf("update: obj nums found - %d \n", nn_global_tmp[y].num);
                                    //printf("update: object tmp - %d \n", obj_tmp);
                                }
                                
                                if (obj_tmp > -1) {
                                    
                                    // Looking for object references inside XObject
                                    
                                    start1 = global_tmp_obj.dict_start;
                                    stop1 = global_tmp_obj.dict_stop;
                                    
                                    // Look for /Font key
                                    form_slice = dict_find_key_value_buffer(start1,stop1,font_seq,4);
                                    f1 = form_slice->value_start;
                                    f2 = form_slice->value_stop;
                                    
                                    if (f1 > -1 && f2 > -1) {
                                        
                                        if (debug_mode == 1) {
                                            //printf("update: found Font- %s \n", get_string_from_buffer(f1,f2));
                                        }
                                        
                                        number_of_fonts_found2 = extract_obj_from_buffer(f1,f2);
                                        
                                        if (number_of_fonts_found2 > -1) {
                                            
                                            for (w=0; w < number_of_fonts_found2 ; w++) {
                                                
                                                if (strlen(nn_global_tmp[w].name) > 0) {
                                                    
                                                    // CHECK IF NEW FONT AND ADD TO LIST
                                                    
                                                    new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                    
                                                    if (new_font == 999) {
                                                        
                                                        strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                        Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                        
                                                        Font_CMAP[global_font_count].page_apply_counter = 1;
                                                        Font_CMAP[global_font_count].pages[0] = x;
                                                        
                                                        // adding new default setting
                                                        Font_CMAP[global_font_count].cmap_apply = 0;
                                                        
                                                        global_font_count ++;
                                                        
                                                        //printf("global font count - %d \n", global_font_count);
                                                        //printf("update:  FONT FOUND - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                                    }
                                                    
                                                    else {
                                                        Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                        Font_CMAP[new_font].page_apply_counter ++;
                                                        
                                                        // adding new default setting
                                                        Font_CMAP[new_font].cmap_apply = 0;
                                                        
                                                        
                                                    }
                                                }
                                            }
                                        }
                                        
                                        
                                        
                                    }
                                    
                                    
                                }
                            }
                            
                        }
                    }
                    
                    else {
                        
                        number_of_resources_found = extract_obj_from_buffer(p4,p5);

                    }
                    
                    if (debug_mode == 1) {
                        //printf("update: number of resources found - %d \n", number_of_resources_found);
                    }
                    
                    if (number_of_resources_found > 0) {
                        
                        for (y=0; y< number_of_resources_found ; y++) {
                            
                            if (strlen(nn_global_tmp[y].name) == 0) {
                                
                                if (debug_mode == 1) {
                                    //printf("update:  PATH - RESOURCES - no name found - need to iterate on resource obj num to get to name - %d - %d  \n", y, nn_global_tmp[y].num);
                                }
                                
                                obj_tmp = get_obj(nn_global_tmp[y].num);
                                
                                if (obj_tmp > -1) {
                                
                                    start1 = global_tmp_obj.dict_start;
                                    stop1 = global_tmp_obj.dict_stop;
                                    
                                    if (debug_mode == 1) {
                                        //printf("found obj - %s \n", get_string_from_buffer(start1,stop1));
                                    }
                                    
                                    // first -look for Font seq inside Resources dictionary
                                    
                                    font_slice = dict_find_key_value_buffer(start1,stop1, font_seq,4);
                                    p6 = font_slice->value_start;
                                    p7 = font_slice->value_stop;
                                    
                                    if (p6 > -1 && p7 > -1) {
                                        
                                        //Found Font in Resource Dict - now get details
        
                                        tmp_str = get_string_from_buffer(p6,p7);
                                        
                                        if (debug_mode == 1) {
                                            //printf("update: getting font dict - %s \n", tmp_str);
                                        }
                                        
                                        number_of_fonts_found2 = extract_obj_from_buffer(p6,p7);
                                        
                                        if (number_of_fonts_found2 > 0) {
                                        
                                            if (strlen(nn_global_tmp[0].name) == 0) {
                                            
                                                obj_tmp2 = get_obj(nn_global_tmp[0].num);
                                            
                                                if (obj_tmp2 > -1) {
                                                    start2 = global_tmp_obj.dict_start;
                                                    stop2 = global_tmp_obj.dict_stop;
                                                    //tmp_str = get_string_from_buffer(start2,stop2);
                                                    number_of_fonts_found2 = extract_obj_from_buffer(start2,stop2);
                                                }
                                            }
                                        
                                        if (number_of_fonts_found2 > 0) {
                                            
                                        for (w=0; w < number_of_fonts_found2 ; w++) {
                                            
                                            if (strlen(nn_global_tmp[w].name) > 0) {
                                                
                                                // CHECK IF NEW FONT AND ADD TO LIST
                                                
                                                new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                if (new_font == 999) {
                                                    
                                                    strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                    Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                    
                                                    Font_CMAP[global_font_count].page_apply_counter = 1;
                                                    Font_CMAP[global_font_count].pages[0] = x;
                                                    
                                                    // adding new default setting
                                                    Font_CMAP[global_font_count].cmap_apply = 0;

                                                    global_font_count ++;
                                                    
                                                    if (debug_mode == 1) {
                                                        //printf("global font count - %d \n", global_font_count);
                                                        //printf("FONT FOUND - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                                    }
                                                    
                                                }
                                                else {
                                                    Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                    Font_CMAP[new_font].page_apply_counter ++;
                                                    
                                                    // adding new default setting
                                                    Font_CMAP[new_font].cmap_apply = 0;
                                                    
                                                }
                                            }

                                        }
                                        }
                                        }}
   
                                    
                                    // second - look for Xobject reference
                                    
                                    form_slice = dict_find_key_value_buffer(start1,stop1,xobject_seq,7);
                                    f1 = form_slice->value_start;
                                    f2 = form_slice->value_stop;
                                    
                                    if (f1 > -1 && f2 > -1) {
                                        
                                        if (debug_mode == 1) {
                                            //printf("update:  found Xobject form!");
                                            //printf("update:  found Xobject-form - %s \n",get_string_from_buffer(f1,f2));
                                        }
                                        
                                        // look at the form and check if any fonts loaded in form dictionary
                                        
                                        number_of_fonts_found2 = extract_obj_from_buffer(f1,f2);
                                        
                                        if (number_of_fonts_found2 > 0) {
                                            
                                            obj_tmp2 = get_obj(nn_global_tmp[0].num);
                                                
                                            if (obj_tmp2 > -1) {
                                                
                                                start2 = global_tmp_obj.dict_start;
                                                stop2 = global_tmp_obj.dict_stop;
                                                tmp_str = get_string_from_buffer(start2,stop2);
                                                
                                                if (debug_mode == 1) {
                                                    //printf("update: xobject form dictionary - %s \n", tmp_str);
                                                }
                                                
                                                font_slice = dict_find_key_value_buffer(start2,stop2,font_seq,4);
                                                
                                                p10 = font_slice->value_start;
                                                p11 = font_slice->value_stop;
                                            
                                                if (p10 > -1 && p11 > -1) {
                                                
                                                    if (debug_mode == 1) {
                                                        //printf("update: found font - %s \n", get_string_from_buffer(p10,p11));
                                                    }
                                                    
                                                    number_of_fonts_found2 = extract_obj_from_buffer(p10, p11);
                                                
                                                    if (number_of_fonts_found2 > 0) {
                                                    
                                                    // check if new font and add
                                                                
                                                    for (w=0; w < number_of_fonts_found2 ; w++) {
                                                          
                                                        if (debug_mode == 1) {
                                                            //printf("update: name/num - %d - %s - %d \n", w, nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                        }
                                                        
                                                        if (strlen(nn_global_tmp[w].name) > 0) {
                                                            
                                                            // CHECK IF NEW FONT AND ADD TO LIST
                                                            
                                                            new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                            
                                                            if (new_font == 999) {
                                                                
                                                                strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                                
                                                                Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                                
                                                                
                                                                Font_CMAP[global_font_count].page_apply_counter = 1;
                                                                
                                                                Font_CMAP[global_font_count].pages[0] = x;
                                                                
                                                                
                                                                // adding new default setting
                                                                
                                                                Font_CMAP[global_font_count].cmap_apply = 0;
                                                                
                                                                
                                                                global_font_count ++;
                                                                
                                                                if (debug_mode == 1) {
                                                                    //printf("update:  global font count - %d \n", global_font_count);
                                                                    
                                                                    //printf("update:  FONT FOUND - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                                                }
                                                            }
                                                            
                                                            else {
                                                                
                                                                Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                                
                                                                Font_CMAP[new_font].page_apply_counter ++;
                                                                
                                                                
                                                                // adding new default setting
                                                                
                                                                Font_CMAP[new_font].cmap_apply = 0;
                                                                
                                                            }
                                                        }
                                                        }
                                                    }
                                                }
                                            }
                                            }
                                        
                                                
                                        
                                                                            
                                        number_of_images_found = extract_obj_from_buffer(f1,f2);
                                        
                                        if (number_of_images_found > 0) {
                                            
                                            //printf("number of image entries-%d \n", number_of_images_found);
                                            
                                            // MAX of 500 images per page
                                            
                                            if (number_of_images_found > 499) {
                                                image_max_add = 499;
                                            }
                                            else {
                                                image_max_add = number_of_images_found;
                                            }
                                            
                                            //Pages[x].image_entries += image_max_add;
                                            
                                            for (y=Pages[x].image_entries; y < image_max_add; y++) {
                                                Pages[x].images[y] = nn_global_tmp[y].num;
                                                strcpy(Pages[x].image_names[y],nn_global_tmp[y].name);
                                                
                                                //printf("saving images list - %d-%d-%s- ", y,nn_global_tmp[y].num,nn_global_tmp[y].name);
                                                
                                                Pages[x].image_x_coord[y] = 0;
                                                Pages[x].image_y_coord[y] = 0;
                                                Pages[x].image_cx_coord[y] = 0;
                                                Pages[x].image_cy_coord[y] = 0;
                                                Pages[x].image_found_on_page[y] = 0;
                                                Pages[x].image_entries ++;
                                            }
                                        }
                                        
                                    }
                                    
                                    /*
                                    font_slice = dict_find_key_value_buffer(start1,stop1, font_seq,4);
                                    p6 = font_slice->value_start;
                                    p7 = font_slice->value_stop;
                                    
                                    if (p6 > -1 && p7 > -1) {
                                        
                                        //Found Font in Resource Dict - now get details
        
                                    //tmp_str = get_string_from_buffer(p6,p7);
                                       
                                    number_of_fonts_found2 = extract_obj_from_buffer(p6,p7);
                                        
                                    if (number_of_fonts_found2 > 0) {
                                        
                                        if (strlen(nn_global_tmp[0].name) == 0) {
                                            
                                            obj_tmp2 = get_obj(nn_global_tmp[0].num);
                                            
                                            if (obj_tmp2 > -1) {
                                                start2 = global_tmp_obj.dict_start;
                                                stop2 = global_tmp_obj.dict_stop;
                                                //tmp_str = get_string_from_buffer(start2,stop2);
                                                number_of_fonts_found2 = extract_obj_from_buffer(start2,stop2);
                                                }
                                            }
                                        
                                        if (number_of_fonts_found2 > 0) {
                                            
                                        for (w=0; w < number_of_fonts_found2 ; w++) {
                                            
                                            if (strlen(nn_global_tmp[w].name) > 0) {
                                                
                                                // CHECK IF NEW FONT AND ADD TO LIST
                                                
                                                new_font = is_new_font(nn_global_tmp[w].name, nn_global_tmp[w].num);
                                                if (new_font == 999) {
                                                    
                                                    strcpy(Font_CMAP[global_font_count].font_name, nn_global_tmp[w].name);
                                                    Font_CMAP[global_font_count].obj_ref1 = nn_global_tmp[w].num;
                                                    
                                                    Font_CMAP[global_font_count].page_apply_counter = 1;
                                                    Font_CMAP[global_font_count].pages[0] = x;
                                                    
                                                    // adding new default setting
                                                    Font_CMAP[global_font_count].cmap_apply = 0;

                                                    global_font_count ++;
                                                    
                                                    //printf("global font count - %d \n", global_font_count);
                                                    //printf("FONT FOUND - %s-%d \n", nn_global_tmp[w].name,nn_global_tmp[w].num);
                                                }
                                                else {
                                                    Font_CMAP[new_font].pages[Font_CMAP[new_font].page_apply_counter]=x;
                                                    Font_CMAP[new_font].page_apply_counter ++;
                                                    
                                                    // adding new default setting
                                                    Font_CMAP[new_font].cmap_apply = 0;
                                                    
                                                }
                                            }

                                        }
                                        }
                                        }}
                                    */
                                    
                                    images_slice = dict_find_key_value_buffer(start1,stop1,xobject_seq,7);
                                    p8 =images_slice->value_start;
                                    p9 =images_slice->value_stop;
                                    number_of_images_found = extract_obj_from_buffer(p8,p9);
                                    
                                    // replacing old check:  number_of_images_found > -1
                                    // with correct new check:  number_of_images_found > 0
                                    
                                    if (number_of_images_found > 0) {
                                        
                                        //printf("number of image entries-%d \n", number_of_images_found);
                                        
                                        Pages[x].image_entries = number_of_images_found;
                                        for (w=0; w < number_of_images_found; w++) {
                                            Pages[x].images[w] = nn_global_tmp[w].num;
                                            strcpy(Pages[x].image_names[w],nn_global_tmp[w].name);
                                            
                                            //printf("saving images list - %d-%d-%s \n", w,nn_global_tmp[w].num,nn_global_tmp[w].name);
                                        }
                                    }
                                    else {
                                        //printf("no images found on page \n");
                                    }
                                }
                            }
                            //printf("going down found resources path - %d - %d \n", number_of_resources_found, nn_global_tmp[y].num);
                        }
                    }
                }
            }
        }
    }
    
    //printf("\nGLOBAL PAGE COUNT-%d \n", global_page_count);
    
    for (x=0; x < global_font_count; x++) {
        
        // may be duplicative - set all cmap_apply to 0 as default
        
        Font_CMAP[x].cmap_apply = 0;
        
        if (debug_mode == 1) {
            //printf("update: FONT MASTER LIST-%d-%s-%d-%d \n", x,Font_CMAP[x].font_name,Font_CMAP[x].obj_ref1,Font_CMAP[x].page_apply_counter);
        }
        
    }
    
    /*
    for (x=0; x < global_page_count; x++) {
        printf("PAGE CONTENT - %d-%d-%d \n", x, Pages[x].content_entries, Pages[x].content[0]);
        if (Pages[x].image_entries > 0) {
            for (y=0; y < Pages[x].image_entries; y++) {
                printf("images on page-%d-%d-%d-", x,y,Pages[x].image_entries);
            }
        }
    }
     */
    
    if (debug_mode == 1) {
        printf("update: pdf_parser - global font count- %d \n", global_font_count);
    }
    
    return 0;
}



int build_obj_master_list (int filelen)

    // Step 1 of parsing PDF - reads off ***buffer***
    // Extracts <obj> ... <endobj>
    // Stores in master global *** obj *** list + global_obj_counter
    // Parses object dictionaries concurrently

{
    int bracket_dict_count = 0;
    int y,z,t1,t2;
    int counter =0;
    int start = -1;
    int stop = -1;
    int found_start = -1;
    int found_stop = -1;
    int dict_start = -1;
    int dict_stop = -1;
    int dict_on = -1;
    int pre_start = -1;
    int b_on, b1, b2;
    int last_obj_end = -1;
    int* obj_num[10];
    int* gen_num[10];

    for (z=0; z < filelen; z++) {
        
        // look for obj
        if ((z > counter) && (found_start == -1) && (buffer[z] == 111)) {
            if (z+3 < filelen) {
                
                if (buffer[z+1] == 98 && buffer[z+2] == 106) {
                    // found obj
                    start = z+3;
                    found_start = 1;
                    counter = z+2;
                    bracket_dict_count = 0;
                }}}
        
        // look for obj_dict if found_start on
        if ((z > counter) && (found_start == 1) && (dict_stop == -1) && buffer[z] == 60){
            if (z+1 < filelen) {
                
                if (buffer[z+1] == 60) {
                    // found << inside obj
                    if (dict_start == -1) {
                        dict_start = z;
                    }
                    
                    dict_on = 1;
                    counter = z+1;
                    bracket_dict_count ++;
                }}}
        
        // look for obj_dict close if dict_start on
        if ((z > counter) && (found_start == 1) && (dict_on == 1) && buffer[z] == 62) {
            if (z+1 < filelen) {
                
                if (buffer[z+1] == 62) {
                    // found >> inside obj
                    bracket_dict_count --;
                    if (bracket_dict_count <= 0) {
                        dict_stop = z+1;
                        dict_on = -1;
                    }
                    else {
                        //printf(" \n\nFOUND BRACKET DICT COUNT HIGH - %d - %d \n\n", global_obj_counter, bracket_dict_count);
                    }
                    counter = z+1;
                }}}
        
        //if found_start on
        if ((z > counter) && (found_start == 1) && (buffer[z] == 101)) {
            
            if (z+6 < filelen) {
                
                if (buffer[z+1] == 110 && buffer[z+2] == 100) {
                    
                    if (buffer[z+3] == 111 && buffer[z+4] == 98 && buffer[z+5] == 106) {
                        
                        // found endobj
                        stop = z-1;
                        found_stop = 1;
                        counter = z+6;
                        
                        // get obj_num and gen_num - look back before start of obj
                        
                        // modify check -> start at 9 not 0
                        // if go back to 0, then may pick up PDF version # instead of obj #
                        
                        // previous - stable:  (start-12) > 0
                        
                        if ((start-12) > 9) {
                            
                            pre_start = start-12;
                        }
                        
                        else
                            
                        {
                            // edge case - looking for first object
                            // the first 8 chars are always %PDF-1.x
                            // start @ 9
                            
                            // change on sept 1, 2022
                            // previous stable:  pre_start = 0
                            
                            pre_start = 9;
                            
                        }
                        
                        // if pre_start is before last_obj_end - set to the first char after obj_end
                        if ((pre_start < last_obj_end) && (last_obj_end > -1)) {
                            pre_start = last_obj_end +1; }
                        
                        b_on = 0;
                        b1=0;
                        b2=0;
                        
                        //printf("obj num window - %s \n", get_string_from_buffer(pre_start,start));
                        
                        for (y=pre_start;y < start; y++) {
                            
                            if ((47 < buffer[y]) && (buffer[y] < 58)) {
                                
                                if (b_on==0) {b_on=1;}
                                
                                if (b_on==1) {
                                    obj_num[b1] = buffer[y];
                                    b1 ++; }
                                
                                if (b_on==2) {
                                    gen_num[b2] = buffer[y];
                                    b2 ++;}
                            }
                            
                            if (buffer[y] == 32 || buffer[y] == 13 || buffer[y] == 10) {
                                if (b_on == 2) {
                                    b_on = 3;}
                                if (b_on == 1) {
                                    b_on = 2;}
                            }
                        }
                        
                        // save obj
                        obj_num[b1] = 0;
                        gen_num[b2] = 0;
                             
                        
                        obj[global_obj_counter].stop = stop;
                        obj[global_obj_counter].start = start;
                        
                        if (dict_start > -1) {
                            obj[global_obj_counter].dict_start = dict_start;
                        }
                        else {
                            obj[global_obj_counter].dict_start = start;
                            
                        }
                        if (dict_stop > -1) {
                            obj[global_obj_counter].dict_stop = dict_stop;
                        }
                        else {
                            obj[global_obj_counter].dict_stop = stop;
                        }
                        
                        last_obj_end = stop;

                        t1 = get_int_from_byte_array(obj_num);
                        t2 = get_int_from_byte_array(gen_num);
                        
                        //printf("found obj numbers - t1- %d - t2 - %d \n", t1,t2);
                        
                        obj[global_obj_counter].obj_num = t1;
                        obj[global_obj_counter].gen_num = t2;
                        
                        obj[global_obj_counter].stream_start = dict_stop;
                        obj[global_obj_counter].hidden = 0;
                        
                        
                        global_obj_counter ++;
                        
                        // printf("update:  found obj - %d-%d-%d-%d-%d-%s \n", t1,dict_start,dict_stop, start, stop, get_string_from_buffer(dict_start,dict_stop));
                        
                        
                        //printf("obj num / gen num- %d-%d \n", t1,t2);
                        start = -1;
                        stop = -1;
                        dict_start = -1;
                        dict_stop = -1;
                        found_start = -1;
                        found_stop = -1;
                        
                        //  Note:  important GLOBAL_MAX_OBJ
                        //  very few PDF files > 100K objects, but not impossible for large files
                        //  caps content @ GLOBAL_MAX and does not read any additional objects
                        //  *** objects are usually interconnected, so ...
                        //      ... not reading past 100K objects will potentially create errors in those files
                        //
                        // future direction:  need to insert more safety checks
                        
                        if (global_obj_counter >= GLOBAL_MAX_OBJ) {
                            printf("warning: pdf_parser - too many objects in file -  unusual - exceeding cap-%d \n", global_obj_counter);
 
                            return global_obj_counter;
                        }
                        
                    }
                }
            }
        }
    }
    
    return global_obj_counter;
}


// new handler to parse font files

int fontfile_handler (char* fontfile, int font_number) {
    
    int l;
    int x,y;
    
    int num_on = 0;
    int* num_str[10];
    int num_tmp_counter = 0;
    int name_tmp_counter = 0;
    int number_index = 0;
    int match_glyph;
    char tmp[100];
    char tmp_char[10];
    
    int nums[500];
    char names[500][50];
    int new_entries = 0;
    
    int index_counter = 0;
    int counter = 0;
    
    int dup_on = -1;
    int name_on = 0;
    
    int starter = -1;
    
    l = strlen(fontfile);
    strcpy(tmp,"");
    
    index_counter += Font_CMAP[font_number].cmap_apply;

    for (x=0; x < l; x++) {
        
        // look for Encoding = {69,110,99,111,100,105,110,103} -> else, skip
        
        if (fontfile[x] == 69) {
            if (x+7 < l) {
                if (fontfile[x+1] == 110) {
                    if (fontfile[x+2] == 99) {
                        if (fontfile[x+3] == 111) {
                            if (fontfile[x+4] == 100) {
                                if (fontfile[x+5] == 105) {
                                    if (fontfile[x+6] == 110) {
                                        if (fontfile[x+7] == 103) {
                                            starter = x+8;
                                            
                                            if (debug_mode == 1) {
                                                //printf("update: pdf_parser - found Encoding in FontFile - %d \n", starter);
                                            }
                                            
                                            break;
                                        }
                                    }}}}}}}}
    }
    
    if (starter > -1) {
        
        for (x=starter; x < l; x++) {
            
            //printf("x-found-%d \n", fontfile[x]);
            
            // look for 'dup' sequence
            
            if (fontfile[x] == 100) {
                if (x+5 < l) {
                    if (fontfile[x+1] == 117) {
                        if (fontfile[x+2] == 112) {
                            // found 'dup' sequence
                            dup_on = 1;
                            
                            //printf("update: found dup - %d \n", x);
                            
                        }}}}
            
            
            // look for 'put' sequence
            
            if (fontfile[x] == 112) {
                if (x+2 < 1) {
                    if (fontfile[x+1] == 117) {
                        if (fontfile[x+2] == 116) {
                            // found 'put' sequence
                            
                            dup_on = -1;
                            
                            //printf("update: found put - %d \n", x);
                            
                        }}}}
            
            
            // found number
            
            if (dup_on == 1 && fontfile[x] > 47 && fontfile[x] < 58) {
                
                num_on = 1;
                num_str[num_tmp_counter] = fontfile[x];
                num_tmp_counter ++;
                
            }
            
            // found font name
            
            if (dup_on == 1 && fontfile[x] == 47) {
                name_on = 1;
                strcpy(tmp,"");
                //printf("update: found name start - %d \n", x);
                
            }
            
            if (name_on == 1 && (fontfile[x] > 64 && fontfile[x] < 127)) {
                // build up font name
                //printf("update: building up font name - %d \n", x);
                sprintf(tmp_char,"%c", fontfile[x]);
                strcat(tmp,tmp_char);
                name_tmp_counter ++;
                
            }
            
            // ending token
            
            if (dup_on == 1 && (fontfile[x] == 32 || fontfile[x] == 13 || fontfile[x] == 10)) {
                
                if (num_on == 1) {
                    
                    num_str[num_tmp_counter] = 0;
                    number_index = get_int_from_byte_array(num_str);
                    
                    //printf("update: getting number index - %d \n", number_index);
                    
                    num_on = 0;
                    num_tmp_counter = 0;
                    
                    nums[counter] = number_index;
                }
                
                if (name_on == 1) {
                    name_on = 0;
                    strcpy(names[counter], tmp);
                    
                    //printf("update: getting name index-  %s \n", tmp);
                    
                    match_glyph = -1;
                    
                    for (y=0; y < adobe_glyph_count; y++) {
                        
                        if (strcmp(tmp,glyph_names[y]) == 0) {
                            match_glyph = glyph_lookup[y];
                            break;
                        }
                    }
                    
                    if (match_glyph > -1) {
                        
                        if (debug_mode == 1) {
                            //printf("update:  Found Differences - Match Glyph-%s-%d-%d-%d-%d \n", tmp,match_glyph,index_counter,nums[counter],font_number);
                        }
                        
                        Font_CMAP[font_number].cmap_in[index_counter] = nums[counter];
                        Font_CMAP[font_number].cmap_out[index_counter] = match_glyph;
                        
                        index_counter ++;
                        
                    }
                    
                    strcpy(tmp,"");
                    counter ++;
                    
                }
            }
            
            
        }
    }
    
    if (debug_mode == 1) {
        printf("update: pdf_parser - counter total - %d - index counter - %d \n", counter, index_counter);
    }
    
    return index_counter;
}




int differences_handler (char *differences, int font_number) {
    
    int l;
    int x,y;
    //int z;
    
    int bracket_on = 0;
    int num_on = 0;
    int* num_str[100];
    int num_tmp_counter = 0;
    int number_index = 0;
    int match_glyph;
    char tmp[1000];
    
    //char* diff_table[200];
    
    int diff_counter = 0;
    char d[10];
    
    l = strlen(differences);
    strcpy(tmp,"");
    
    diff_counter += Font_CMAP[font_number].cmap_apply;

    // printf("update: diff_counter @ start: %d \n", diff_counter);
    
    for (x=0; x < l; x++) {
        
        //  iterate through list of differences
        
        if (debug_mode == 1) {
            //printf("update:  iter thru differences - %d-%d \n", x,differences[x]);
        }
        
        // look for bracket -> usually the start of a differences sequence, but can be nested
        
        if (differences[x] == 91) {
            
            bracket_on = 1;
            num_on = 0;
        }
        
        
        // look for a number digit and build up number string
        
        if (bracket_on == 1 && differences[x] >= 48 && differences[x] <= 57 && strlen(tmp)==0) {
            
            // found NUMBER in differences list
            // keep adding to build up num_str until space or special char reached
            
            if (num_tmp_counter < 100) {
                num_str[num_tmp_counter] = differences[x];
                num_tmp_counter ++;
                num_on = 1;
            }
        }
        
        
        //  if inside bracket, not building a number string, and not special: / [ ]
        //  build tmp
        
        if (bracket_on == 1 && num_on == 0 && differences[x] != 47 && differences[x] != 91 && differences[x] !=92 && differences[x] !=93) {
            
            if (differences[x] > 32 && differences[x] < 129 && strlen(tmp) < 1000) {
                sprintf(d,"%c",differences[x]);
                strcat(tmp,d);
                
            }
        }
        
        
        // if inside bracket, and either white space / ]  ... reached end of element
        // multiple nested options ...
        
        if (bracket_on == 1 && (differences[x] == 47 || differences[x] == 93) || differences[x] == 10 || differences[x] == 13 || differences[x] == 32) {
            
            //reached end of element

            match_glyph = -1;

            // if not a number, then compare the tmp string with lookup in adobe glyph table
            
            if (num_on == 0 && strlen(tmp) > 0) {
                
                // not number & tmp > 0
                
                //  search for element match on the adobe glyph_names lookup table in header
                //  ***note:  will need to expand this list - right now it is only the most common glyphs***
                
                for (y=0; y < adobe_glyph_count; y++) {
                    
                    if (strcmp(tmp,glyph_names[y]) == 0) {
                        match_glyph = glyph_lookup[y];
                        break;
                    }
                }
                
                if (match_glyph > - 1) {
                    
                    // found a matching glyph in the list - need to add to Font CMAP
                    
                    // differences_debug
                    
                    if (debug_mode == 1) {
                        //printf("update: pdf_parser - found Differences - Match Glyph-%s-%d-%d-%d-%d \n", tmp,match_glyph,diff_counter,number_index,font_number);
                    }
                    
                    Font_CMAP[font_number].cmap_in[diff_counter] = number_index;
                    Font_CMAP[font_number].cmap_out[diff_counter] = match_glyph;
                    diff_counter ++;
                    number_index ++;
                }
                
                else {
                    
                    if (strlen(tmp) > 2) {
                        if (tmp[0] == 103 && tmp[1] == 48) {
                            
                            // special case of differences format used in small number of files
                            //      e.g., 'g0' format
                            //      printf("found g0 format - %s \n", tmp);
                            
                            match_glyph = atoi(&tmp[2]);
                            
                            //printf("update: Found g0 special case - %s-%d-%d \n", tmp, match_glyph,number_index);
                            
                            Font_CMAP[font_number].cmap_in[diff_counter] = number_index;
                            Font_CMAP[font_number].cmap_out[diff_counter] = match_glyph;
                            diff_counter ++;
                            number_index ++;
                        }
                        else {
                            // still need to catch and update the number index
                            
                            // Font_CMAP[font_number].cmap_in[diff_counter] = number_index;
                            // Font_CMAP[font_number].cmap_out[diff_counter] = 32;
                            
                            number_index ++;
                            
                            
                        }
                    }
                    
                    
                    else {
                        
                        // no action - this should not be here!
                        
                        // if no glyph name found, then keep entry and assume 32 space
                        
                        //printf("update: Differences - in the middle of sequence -> did not find glyph, but need to increment ! %d \n", differences[x]);
                        
                        //Font_CMAP[font_number].cmap_in[diff_counter] = number_index;
                        //Font_CMAP[font_number].cmap_out[diff_counter] = 32;
                        //diff_counter ++;
                        number_index ++;
                    }
                    
                }
            }
            
            else
                
                {
                    
                    if (num_on > 0) {
                        
                        //printf("update: number on == 1 - %d-%d \n", num_tmp_counter,num_str[0]);
                        
                        num_str[num_tmp_counter] = 0;
                        number_index = get_int_from_byte_array(num_str);
                        
                        //printf("update: getting number index - %d \n", number_index);
                        
                        num_on = 0;
                        num_tmp_counter = 0;
                    }
                
            }
            
            // printf("update:  differences - landing here? - %d \n", differences[x]);
            
            strcpy(tmp,"");
            num_on = 0;
        }
        
    }
    
    return diff_counter;
    
}



// note: this function (process_cmap_buffer_alt) has significant redundancy with process_cmap function
//  --this is a different function since the input is very different
//  --this is for a case in which the CMAP encoding table is not in flate unzip
//  --this is less often used, but not infrequently the CMAP table is directly in the main PDF buffer ...
//      ...without using flate compression

int process_cmap_buffer_alt (int buffer_start, int buffer_stop, int font_number) {
    
    int w, x,y,z, t, l, y2, y3;
    
    int tr1,tr2,tr3;
    char tmp[10];
    char cmap_buf_str[200000];
    int* bf [5000];
    char bf_str[5000];
    int* bf_nums[5000];
    int* num_tmp[10];
    int* num2_tmp[10];
    int* brackets[200];
    int bracket_tmp_save_0;
    int bracket_tmp_save_1;
    
    int bracket_counter;
    int bracket_start;
    
    int bracket_cursor;
    int b=0;
    int b2=0;
    int b3=0;
    int h_on = 0;
    int hbr_on = 0;
    int c = 0;
    
    int* start[200];
    int* stop[200];
    
    int* start_char[200];
    int* stop_char[200];
    int char_count = 0;
    int char_match = -1;
    int char_start_on = -1;
    int first_number = 1;
    
    int count = 0;
    int cursor = -1;
    int place_in_triple = 0;
    int start_on = -1;
    int match = -1;
    
    int bracket_on = 0;
    int bracket_same_len = 0;
    
    strcpy(cmap_buf_str,"");
    
    for (x=buffer_start; x < buffer_stop; x++) {
        sprintf(tmp,"%c",buffer[x]);
        strcat(cmap_buf_str,tmp);
    }
    
    //printf("found cmap ***BUFFER*** str- %s \n", cmap_buf_str);
    //printf("loop size - %d \n", loop_size);
    
    //stream_buffer_len = buffer_stop - buffer_start;
    
    for (x=buffer_start; x < buffer_stop; x++) {
        
        if ((x > cursor) && start_on < 1 && buffer[x] == beginbfchar_seq[0]) {
            char_match = 1;
            if (buffer_stop > x + 11) {
                for (y=1 ; y< 12; y++) {
                    if (buffer[x+y] == beginbfchar_seq[y]) {
                        char_match ++;
                    }
                else {
                    char_match = -1;
                    break;
                }
                if (char_match == 11) {
                    char_start_on = 1;
                    start_char[char_count] = x+11;
                    cursor = x+11;
                    char_match = -1;
                    
                    //printf("update:  found beginbfchar - %d \n", x);
                }
            }
            }}
        
        if ((x > cursor) && char_start_on == 1 && buffer[x] == endbfchar_seq[0]) {
            char_match = 1;
            
            if (buffer_stop > x + 9) {
                
                for (y=1; y<9; y++) {
                    
                    if (buffer[x+y] == endbfchar_seq[y]) {
                        char_match++;
                    }
                    else {
                        char_match = -1;
                        break;
                    }
                    if (char_match == 9) {
                        char_start_on = -1;
                        stop_char[char_count] = x-1;
                        cursor = x+9;
                        char_match = -1;
                        char_count ++;
                    }
                }
            }
        }
        
        
        if ((x > cursor) && char_start_on < 1 && buffer[x] == beginbfrange_seq[0]) {
            match = 1;
            if (buffer_stop > x+12) {
                
                for (y=1; y < 12 ; y++) {
                    
                    if (buffer[x+y] == beginbfrange_seq[y]) {
                        match ++;
                    }
                    else {
                        match = -1;
                        break;
                    }
                    if (match == 12) {
                        start_on = 1;
                        start[count] = x+12;
                        cursor = x+12;
                        match = -1;
                    }
                }
            }
        }
        
        if ((x > cursor) && start_on == 1 && buffer[x] == endbfrange_seq[0]) {
            
            match = 1;
            
            if (buffer_stop > x+10) {
                
                for (y=1; y < 12 ; y++) {
                    
                    if (buffer[x+y] == endbfrange_seq[y]) {
                        match ++;
                    }
                    else {
                        match = -1;
                        break;
                    }
                    if (match == 10) {
                        start_on = -1;
                        stop[count] = x-1;
                        cursor = x+10;
                        count ++;
                        match = -1;
                    }
                }
            }
        }
        
    }
    
    //  useful readout to check if bfrange and bfchar sequences have been correctly identified
    //  printf("bfrange seq found - %d \n", count);
    //  printf("bfchar seq found - %d \n", char_count);
    
    b=0;
    b2=0;
    
    // First - extract hex numbers from bfchar fields
    
    for (z=0; z < char_count; z++) {
        
        l=0;

        for (t=start_char[z]; t < stop_char[z]; t++) {
        
            bf[l] = buffer[t];
            l++;
        }
        
        for (y=0; y<l; y++) {
            
            if (h_on == 1 && bf[y] != 62 && bf[y] !=13 && bf[y] !=10 && bf[y] != 32) {
                num_tmp[b2] = bf[y];
                b2++;
            }
            
            if (bf[y] == 60) {
                h_on = 1;
            }
            
            if (bf[y] == 62) {
                
                h_on = -1;
                bf_nums[b] = get_hex(num_tmp,b2);
                
                b++;
                if (first_number == 1) {
                    
                    bf_nums[b] = get_hex(num_tmp,b2);
                    b++;
                    first_number = 0;
                }
                else {
                    first_number = 1;
                }
                b2=0;
            }
        }
    }
    
    // carry over value of b from bfchar
    
    b2=0;
    
    // Second - extract numbers from bf_range
    
    for (z=0; z < count; z++) {
        
        //printf("in range loop - bf start/stop - %d-%d \n", start[z], stop[z]);
        
        l = 0;
        for (t=start[z]; t < stop[z]; t++) {
            
            bf[l] = buffer[t];
            l++;
        }
        
        strcpy(bf_str, get_string_from_byte_array(bf,l));
        
        bracket_cursor = -1;
        bracket_counter = 0;
        
        for (y=0; y<l ; y++) {
            
            if (bf[y] == 91 && y > bracket_cursor) {
                bracket_on = 1;
                
                //  printf("bracket on - %d \n", y);
                //  special case - handle in sub-loop
                
                bracket_start = y;
                bracket_counter = 0;
                
                for (y2=bracket_start; y2 < l; y2++) {
                    
                    // end special inner loop
                    
                    if (bf[y2] == 93) {
                        
                        //printf("bracket off - %d \n", y2);
                        
                        bracket_on = 0;
                        bracket_cursor = y2;
                        
                        //printf("bracket off - numbers found inside - %d - %d \n", bracket_counter,b);
                        
                        if ((b-bracket_counter + 1) >= -1000) {
                            
                            // need to check whether this is needed
                            // there is scenario of <0001> <001f> [<A> <B> ... <Z>]
                            // in that scenario b = 2 and bracket-counter = <001f> - <0001>
                            
                            bracket_tmp_save_0 = bf_nums[b-1];
                            bracket_tmp_save_1 = bf_nums[b-2];
                            
                            //  useful checks for debugging [ ] handling
                            //  printf("bracket tmp save [0] - %d [1] - %d - br count - %d \n",     bracket_tmp_save_0, bracket_tmp_save_1, bracket_counter);
                            //  printf("equal test - %d \n", bracket_tmp_save_0-bracket_tmp_save_1);
                            
                            if ((bracket_tmp_save_0 - bracket_tmp_save_1 +1) == bracket_counter) {
                                bracket_same_len = 1;
                            }
                            else {
                                bracket_same_len = 0;
                            }
                            
                            b = b - 1;

                            // first entry
                            //printf("zero entry before re-write - %d \n", bf_nums[b-1]);
                            
                            bf_nums[b] = bf_nums[b-1];
                            //printf("first re-write - %d \n", bf_nums[b]);
                            
                            b++;
                            bf_nums[b] = brackets[0];
                            //printf("second re-write - %d \n", bf_nums[b]);
                            
                            b++;
                            
                            for (y3=0; y3 < bracket_counter-1; y3++) {
                                if (bracket_same_len == 1) {
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    
                                    b++;
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                }
                                else {
                                    // may need to be safe here - apply error check
                                    
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                    
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                }
                                
                                // useful debugging check:  printf("loop-2nd - %d \n", bf_nums[b]);
                                
                                bf_nums[b] = brackets[y3+1];
                                //printf("loop-3rd - %d \n", bf_nums[b]);
                                b++;
                            }
                        break;
                    }
                    }
                    // get all of the numbers in sub-loop
                    
                    if (hbr_on == 1 && bf[y2] != 62 && bf[y2] != 13 && bf[y2] != 32) {
                        
                        num2_tmp[b3] = bf[y2];
                        b3++;
                    }
                    
                    if (bf[y2] == 60) {
                        hbr_on = 1;
                    }
                    
                    if (bf[y2] == 62) {
                        
                        hbr_on = -1;
                        brackets[bracket_counter] = get_hex(num2_tmp,b3);
                        
                        bracket_counter ++;
                        b3 = 0;
                    }
                    
                }
                
            }
            
            
            if (h_on == 1 && y > bracket_cursor && bf[y] != 62 && bf[y] != 13 && bf[y] != 10 && bf[y] !=32) {
                
                num_tmp[b2] = bf[y];
                b2++;
            }
            
            if (bf[y] == 60 && y > bracket_cursor) {
                h_on = 1;
            }
            
            if (bf[y] == 62 && y > bracket_cursor) {
                
                if (bracket_on == 0) {
                    h_on = -1;
                    bf_nums[b] = get_hex(num_tmp,b2);
                    
                    b++;
                    b2=0;
                }

            }}
        
    }
    
    // next - need to parse inside each start:stop window to build in/out mappings
    // each bf entry will consist of 3 paired hex numbers - <03EC> <03EE> <0030> with /r or /n after 3rd number
    // need hex converter too
    

    for (w=0; w < b; w++) {
        
        
        //  Note: cap on size of encoding table -> very unlikely to reach this limit
        //  If this limit is reached, good chance that it is a parsing error due to some unusual element
        
        if (c > 890) {
            // printf("update:  exceed max encodings - error! \n");
            break;
        }
        
        if (place_in_triple == 0) {
            tr1 = bf_nums[w];
            place_in_triple ++;
        }
        else {
            if (place_in_triple == 1) {
                tr2 = bf_nums[w];
                place_in_triple ++;
            }
            else {
                if (place_in_triple == 2) {
                    tr3 = bf_nums[w];
                    place_in_triple = 0;
                    if (tr1 == tr2) {
                        
                        // simple case
                        // in = tr1 - out = tr3
                        
                        Font_CMAP[font_number].cmap_in[c] = tr1;
                        Font_CMAP[font_number].cmap_out[c] = tr3;
                        
                        //  useful readout to check for encoding table errors:
                        //  printf("cmap out - %d - %d \n", tr1,tr3);
                        
                        c++;
                    }
                    else {
                        for (y=0; y <= (tr2-tr1); y++) {
                            
                            //printf("multiple entry path in cmap-%d-%d-%d \n", tr1,tr2,y);
                            
                            Font_CMAP[font_number].cmap_in[c] = tr1+y;
                            Font_CMAP[font_number].cmap_out[c] = tr3+y;
                            
                            //  useful readout to check for encoding table errors:
                            //  printf("cmap out - range - %d - %d \n", tr1+y,tr3+y);
                            
                            c++;
                        }
                    }
                    
                }
            }
        }
        
    }
    

    Font_CMAP[font_number].cmap_apply = c;
    
    //free(flate_dst_tmp_buffer);
    //free(cmap_buf_str);
    
    // Note:  this is a critical read-out of the CMAP created
    
    if (debug_mode == 1) {
        /*
        for (z=0; z < c; z++) {
            printf("update:  CMAP -BUFFER- In/Out- %s - %d-%d \n", Font_CMAP[font_number].font_name, Font_CMAP[font_number].cmap_in[z], Font_CMAP[font_number].cmap_out[z]);
        }
         */
    }
    
    return c;
}



int process_cmap (int stream_buffer_len, int font_number) {
    
    int w, x,y,z, t, l, y2, y3;
    
    int tr1,tr2,tr3;
    char tmp[10];
    char cmap_buf_str[200000];
    int* bf [5000];
    char bf_str[5000];
    int* bf_nums[5000];
    
    int* num_tmp[100];
    int* num2_tmp[100];
    
    
    int* brackets[200];
    int bracket_tmp_save_0;
    int bracket_tmp_save_1;
    
    int bracket_counter;
    int bracket_start;
    
    int bracket_cursor;
    int b=0;
    int b2=0;
    int b3=0;
    int h_on = 0;
    int hbr_on = 0;
    int c = 0;
    
    int* start[200];
    int* stop[200];
    
    int* start_char[200];
    int* stop_char[200];
    int char_count = 0;
    int char_match = -1;
    int char_start_on = -1;
    int first_number = 1;
    
    int count = 0;
    int cursor = -1;
    int place_in_triple = 0;
    int start_on = -1;
    int match = -1;
    int MAX_SIZE = 500000;
    int loop_size;
    int bracket_on = 0;
    int bracket_same_len = 0;
    
    int bfchar_local_counter = 0;
    
    strcpy(cmap_buf_str,"");
    
    // Note: safety checks, just in case there was an error in inflating the stream
    //      -- caps at MAX_SIZE
    
    if (stream_buffer_len > MAX_SIZE) {
        loop_size = stream_buffer_len;
    }
    else {
        loop_size = stream_buffer_len;
    }
    
    /*
    for (x=0; x < loop_size; x++) {
        
        //sprintf(tmp,"%c",flate_dst_tmp_buffer[x]);
        //strcat(cmap_buf_str,tmp);
        
        if (flate_dst_tmp_buffer[x] > 31 && flate_dst_tmp_buffer[x] < 128) {
            printf("%c",flate_dst_tmp_buffer[x]);
        }
        
    }
    */
    
    //  useful for debugging if error in flate opening
    
    //  printf("update:  found cmap str- %s \n", cmap_buf_str);
    //  printf("update:  loop size - %d \n", loop_size);

    
    for (x=0; x < loop_size; x++) {
        
        if ((x > cursor) && start_on < 1 && flate_dst_tmp_buffer[x] == beginbfchar_seq[0]) {
            char_match = 1;
            
            if (stream_buffer_len > x + 11) {
                
                for (y=1 ; y< 12; y++) {
                    if (flate_dst_tmp_buffer[x+y] == beginbfchar_seq[y]) {
                        char_match ++;
                    }
                    
                else {
                    char_match = -1;
                    break;
                }
                if (char_match == 11) {
                    char_start_on = 1;
                    start_char[char_count] = x+11;
                    cursor = x+11;
                    char_match = -1;
                    
                    //printf("update:  found beginbfchar - %d \n", x);
                }
            }
            }}
        
        if ((x > cursor) && char_start_on == 1 && flate_dst_tmp_buffer[x] == endbfchar_seq[0]) {
            char_match = 1;
            
            if (stream_buffer_len > x + 9) {
                for (y=1; y<9; y++) {
                    
                    if (flate_dst_tmp_buffer[x+y] == endbfchar_seq[y]) {
                        char_match++;
                    }
                    else {
                        char_match = -1;
                        break;
                    }
                    if (char_match == 9) {
                        char_start_on = -1;
                        stop_char[char_count] = x-1;
                        cursor = x+9;
                        char_match = -1;
                        char_count ++;
                    }
                }
            }
        }
        
        if ((x > cursor) && char_start_on < 1 && flate_dst_tmp_buffer[x] == beginbfrange_seq[0]) {
            match = 1;
            if (stream_buffer_len > x+12) {
                
                for (y=1; y < 12 ; y++) {
                    
                    if (flate_dst_tmp_buffer[x+y] == beginbfrange_seq[y]) {
                        match ++;
                    }
                    else {
                        match = -1;
                        break;
                    }
                    if (match == 12) {
                        start_on = 1;
                        start[count] = x+12;
                        cursor = x+12;
                        match = -1;
                    }
                }
            }
        }
        
        if ((x > cursor) && start_on == 1 && flate_dst_tmp_buffer[x] == endbfrange_seq[0]) {
            match = 1;
            if (stream_buffer_len > x+10) {
                for (y=1; y < 12 ; y++) {
                    if (flate_dst_tmp_buffer[x+y] == endbfrange_seq[y]) {
                        match ++;
                    }
                    else {
                        match = -1;
                        break;
                    }
                    if (match == 10) {
                        start_on = -1;
                        stop[count] = x-1;
                        cursor = x+10;
                        count ++;
                        match = -1;
                    }
                }
            }
        }
        
    }
    
    //  useful readout to check if bfrange and bfchar sequences have been correctly identified
    //printf("update:  bfrange seq found - %d \n", count);
    //printf("update:  bfchar seq found - %d \n", char_count);
    
    b=0;
    b2=0;
    
    // First - extract hex numbers from bfchar fields
    
    for (z=0; z < char_count; z++) {
        
        l=0;
        
        //printf("update: iterating thru bfchar seq # %d \n", z);
        
        for (t=start_char[z]; t < stop_char[z]; t++) {
        
            bf[l] = flate_dst_tmp_buffer[t];
            
            // printf("update: iterating thru beginbfchar-  %d - %d \n", l,bf[l]);
            
            l++;
        }
        
        bfchar_local_counter = 0;
        
        for (y=0; y<l; y++) {
            
            //printf("update: iterating thru beginbfchar - %d - %d \n", y, bf[y]);
            
            // future direction -> right now, this only grabs well-formed triple
            //      ... which is the standard case -> there may be exceptions
            
            
            if (h_on == 1 && bf[y] != 62 && bf[y] !=13 && bf[y] !=10 && bf[y] != 32) {
                
                num_tmp[b2] = bf[y];
                b2++;
                
            }
            
            if (bf[y] == 60) {
                h_on = 1;
            }
            
            //  works on most docs - stable -> bf[y] == 62 without check for 32/10/13
            //  additional check for 32/10/13 - cuts off if multiple numbers found
            //  note:  right now, it does not handle that case with multiple numbers - just stops
            
            if (bf[y] == 62 || ((b2 > 0) && (bf[y] ==32 || bf[y]==10 || bf[y]==13))) {
                
                h_on = -1;
                
                if (b2 > 0) {
                    
                    bf_nums[b] = get_hex(num_tmp,b2);
                    
                    
                    if (font_number == 15) {
                        //printf("update - bf char - %d - %d \n", b,bf_nums[b]);
                        //printf("update - bf[0] - %d \n", bf_nums[0]);
                    }
                
                    b++;
                }
                
                if (first_number == 1) {
                    
                    if (b2 > 0) {
                        bf_nums[b] = get_hex(num_tmp,b2);
                    
                        //printf("update - bf char2 - %d - %d \n", b,bf_nums[b]);
                        
                        if (font_number == 15) {
                            //printf("update - bf char2 - %d - %d \n", b,bf_nums[b]);
                            //printf("update - bf[0] - %d \n", bf_nums[0]);
                        }
                        
                        
                        b++;
                    }
                    
                    first_number = 0;
                }
                
                else {
                    first_number = 1;
                }
                
                b2=0;
            }
        }
    }
    
    b2=0;
    
    // Second - extract numbers from bf_range
    
    
    for (z=0; z < count; z++) {
        
        //printf("in range loop - bf start/stop - %d-%d \n", start[z], stop[z]);
        
        l = 0;
        
        for (t=start[z]; t < stop[z]; t++) {
            bf[l] = flate_dst_tmp_buffer[t];
            l++;
        }
        
        strcpy(bf_str, get_string_from_byte_array(bf,l));
        
        bracket_cursor = -1;
        bracket_counter = 0;
        
        for (y=0; y<l ; y++) {
            

            if (bf[y] == 91 && y > bracket_cursor) {
                bracket_on = 1;
                
                //printf("bracket on - %d \n", y);
                // special case - handle in sub-loop
                
                bracket_start = y;
                bracket_counter = 0;
                
                for (y2=bracket_start; y2 < l; y2++) {
                    
                    // end special inner loop
                    
                    if (bf[y2] == 93) {
                        
                        //printf("bracket off - %d \n", y2);
                        
                        bracket_on = 0;
                        bracket_cursor = y2;
                        
                        //printf("bracket off - numbers found inside - %d \n", bracket_counter);
                        // re-write start = b - bracket_counter + 1;
                        
                        if ((b-bracket_counter + 1) >= 0) {
                            
                            bracket_tmp_save_0 = bf_nums[b-1];
                            bracket_tmp_save_1 = bf_nums[b-2];
                            
                            //  useful readouts for debugging bracket handling
                            //  printf("bracket tmp save [0] - %d [1] - %d - br count - %d \n", bracket_tmp_save_0, bracket_tmp_save_1, bracket_counter);
                            //  printf("equal test - %d \n", bracket_tmp_save_0-bracket_tmp_save_1);
                            
                            if ((bracket_tmp_save_0 - bracket_tmp_save_1 +1) == bracket_counter) {
                                bracket_same_len = 1;
                            }
                            else {
                                bracket_same_len = 0;
                            }
                            
                            b = b - 1;

                            // first entry
                            
                            //printf("zero entry before re-write - %d \n", bf_nums[b-1]);
                            
                            bf_nums[b] = bf_nums[b-1];
                            //printf("first re-write - %d \n", bf_nums[b]);
                            b++;
                            
                            bf_nums[b] = brackets[0];
                            //printf("second re-write - %d \n", bf_nums[b]);
                            b++;
                            
                            for (y3=0; y3 < bracket_counter-1; y3++) {
                                
                                if (bracket_same_len == 1) {
                                    
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    
                                    b++;
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                }
                                else {
                                    // may need to be safe here - apply error check
                                    
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                    
                                    bf_nums[b] = bracket_tmp_save_1 + 1 + y3;
                                    b++;
                                }
                                
                                bf_nums[b] = brackets[y3+1];
                                //printf("loop-3rd - %d \n", bf_nums[b]);
                                b++;
                            }
                        break;
                    }
                    }
                    // get all of the numbers in sub-loop
                    
                    if (hbr_on == 1 && bf[y2] != 62 && bf[y2] != 13 && bf[y2] != 32) {
                        num2_tmp[b3] = bf[y2];
                        b3++;
                    }
                    if (bf[y2] == 60) {
                        hbr_on = 1;
                    }
                    
                    if (bf[y2] == 62) {
                        
                        hbr_on = -1;
                        brackets[bracket_counter] = get_hex(num2_tmp,b3);
                        
                        //useful debugging check:  printf("bracket num found - %d \n", brackets[bracket_counter]);
                        
                        bracket_counter ++;
                        b3 = 0;
                    }
                    
                }
                
            }
            
            if (h_on == 1 && y > bracket_cursor && bf[y] != 62 && bf[y] != 13 && bf[y] != 10 && bf[y] !=32) {
                num_tmp[b2] = bf[y];
                b2++;
            }
            
            if (bf[y] == 60 && y > bracket_cursor) {
                h_on = 1;
            }
            
            if (bf[y] == 62 && y > bracket_cursor) {
                
                if (bracket_on == 0) {
                    h_on = -1;
                    bf_nums[b] = get_hex(num_tmp,b2);
                    
                    //printf("bf range out - %d - ", bf_nums[b]);
                    
                    b++;
                    b2=0;
                }

            }}
        
        //printf("bf_str- %s \n", bf_str);
        //printf("bf nums entries - %d \n", b);
        
    }
    
    // next - need to parse inside each start:stop window to build in/out mappings
    // each bf entry will consist of 3 paired hex numbers - <03EC> <03EE> <0030> with /r or /n after 3rd number
    // need hex converter too
    
    //printf("b count end - %d \n", b);
    

    
    for (w=0; w < b; w++) {
        
        /*
        if (font_number == 2) {
            printf("bf_nums- %d-%d \n", w,bf_nums[w]);
        }
        */
        
        
        // Note: MAXIMUM number of entries in CMAP encoding
        //  --very unlikely to reach this limit - usually indicates a parsing error
        
        // max clip level @ 2000
        // works stably at 890 - but some encoding tables are larger!
        // need to experiment/test/confirm this level
        
        if (c > 1890) {
            
            if (debug_mode == 1) {
                printf("warning: pdf_parser - unusual - may have encoding impact (none, small or large) - exceed max encodings found > 1890 ! \n");
            }
            
            break;
        }
        
        if (place_in_triple == 0) {
            tr1 = bf_nums[w];
            place_in_triple ++;
        }
        else {
            if (place_in_triple == 1) {
                tr2 = bf_nums[w];
                place_in_triple ++;
            }
            else {
                if (place_in_triple == 2) {
                    tr3 = bf_nums[w];
                    place_in_triple = 0;
                    if (tr1 == tr2) {
                        
                        // simple case
                        // in = tr1 - out = tr3
                        
                        Font_CMAP[font_number].cmap_in[c] = tr1;
                        Font_CMAP[font_number].cmap_out[c] = tr3;
                        
                        //  useful readout to debug CMAP table creation
                        if (font_number == 15) {
                            //printf("cmap out - %d - %d \n", tr1,tr3);
                        }
                        
                        c++;
                        
                
                    }
                    else {
                        for (y=0; y <= (tr2-tr1); y++) {
                            
                            //printf("multiple entry path in cmap-%d-%d-%d \n", tr1,tr2,y);
                            
                            Font_CMAP[font_number].cmap_in[c] = tr1+y;
                            Font_CMAP[font_number].cmap_out[c] = tr3+y;
                            
                            //  useful readout to debug CMAP table creation
                            //  printf("cmap out - range - %d - %d \n", tr1+y,tr3+y);
                            
                            c++;
                        }
                    }
                    
                }
            }
        }
        
    }
    
    //  additional info useful for detailed debugging of font encoding errors
    //  printf("finishing cmap processing \n");
    //  printf("update:  font number - %d \n", font_number);
    //  printf("update:  global font number - %d \n", global_font_count);
    //  printf("update:  cmap apply = c - %d \n", c);
    
    Font_CMAP[font_number].cmap_apply = c;
    
    //free(flate_dst_tmp_buffer);
    //free(cmap_buf_str);

    //  Key view of CMAP table created with inputs / outputs
    
    
    // printf("update: c = %d \n", c);
    
    // experiment -> there is a pattern of 'implicit' 3/32 spans in some Font0 fonts
    // if a CMAP with single 3 / 32 entry is found, then 'fill in' the rest of the map with +29 mapping
    
    if (c > 0 && c < 5) {
        
        if ( ( Font_CMAP[font_number].cmap_in[0] == 3 && Font_CMAP[font_number].cmap_out[0] == 32) ) {
            
            // may apply to other patterns, e.g. 17-46
            
            if (debug_mode == 1) {
                //printf("update: pdf_parser- CMAP 3-32 implicit fill-in-  %d %d %d \n", c, Font_CMAP[font_number].cmap_in[0],Font_CMAP[font_number].cmap_out[0]);
            }
            
            for (z=1; z < 150; z++) {
                Font_CMAP[font_number].cmap_in[z] = Font_CMAP[0].cmap_in[z];
                Font_CMAP[font_number].cmap_out[z] = Font_CMAP[0].cmap_out[z];
                //Font_CMAP[font_number].cmap_in[z] = 3 + z;
                //Font_CMAP[font_number].cmap_out[z] = 32 + z;
                //printf("updating cmap - %d - %d - %d \n", z, Font_CMAP[font_number].cmap_in[z], Font_CMAP[font_number].cmap_out[z]);
            }
            
            c += 145;
            Font_CMAP[font_number].cmap_apply = c;
        }
    }
    
    // experiment ends here
                      
    if (debug_mode == 1) {
        
        /*
        for (z=0; z < c; z++) {
            printf("update:  CMAP In/Out- %s - %d-%d \n", Font_CMAP[font_number].font_name, Font_CMAP[font_number].cmap_in[z], Font_CMAP[font_number].cmap_out[z]);
        }
        */
    }
    
    
    free (flate_dst_tmp_buffer);
    
    return c;
}


int font_encoding_cmap_handler (int font_len) {
    
    int x;
    
    char* font_name;
    int font_num;
    int found_obj, found_obj_tou;
    int start, start2, start3;
    int stop, stop2, stop3;
    int enc_obj, enc_start, enc_stop, enc1,enc2;
    
    int dif1,dif2,dif4;
    char* dif_str;
    
    slice* tou;
    
    slice* dif;
    slice* enc;
    
    int cmap_flate;
    int t1,t2,t3,t4,t5,t6;
    char* tmp1;
    char* tmp2;
    //char* tmp3;
    char* tmp4;
    
    int last_cmap_c = -1;
    int last_iter1 = 0;
    int last_iter2 = 0;
    
    slice* l1;
    int len1= -1;
    int len2= -1;
    int len_int = 0;
    int* len_array[100];
    int len_on = -1;
    
    // new variables related to widths
    int w,w1,w2,w3,w4,w5,w6,w7;
    slice* width_values;
    int fc,fc1,fc2,fc3;
    int lc,lc1,lc2,lc3;
    int w_counter_tmp;
    slice* first_char;
    slice* last_char;
    
    char fontfile_str[100000];
    char ff_tmp[10];
    
    // end - new variables
    
    int encoding_found = -1;
    
    char* flate_str;
    int found_tou;
    
    int c;
    int dummy = 0;
    
    int defined_encoding = 0;
    slice* predefined_encoding;
    slice* type3_encoding;
    slice* char_procs_slice;
    int p1, p2, p3, p4, p5,p6,p7, p8,p9,p10, p12, p14;
    
    int font_type_3_flag = 0;
    int font_type_0_flag = 0;
    slice* type0_encoding;

    int found_standard_font = -1;
    
    //int test_var = 0;
    
    // Font_CMAP - look up font_name & font_resource_obj
    // look in font_resource_obj for "Encoding" // "Differences" // "ToUnicode"
    
    //printf("global font count -%d \n", global_font_count);
    
    for (x=0; x < global_font_count ; x++) {
        
        // at start of each font loop, reset encoding_found = -1
        
        encoding_found = -1;
        
        // useful for debugging:   printf("update: At the top of global_font_count loop - %d-%d \n", x, global_font_count);
        
        font_name = Font_CMAP[x].font_name;
        font_num = Font_CMAP[x].obj_ref1;
        
        Font_CMAP[x].standard_font_identifier = 0;
        
        font_type_3_flag = 0;
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - Font Encodings- %d-%s-%d \n",x,font_name,font_num);
        }
        
        found_obj = get_obj(font_num);
        
        //printf("update:  found obj: %d \n", found_obj);
        
        if (found_obj > -1) {
            
            start = global_tmp_obj.dict_start;
            stop = global_tmp_obj.dict_stop;
            
            if (debug_mode == 1) {
                //printf("update:  font dict start- %d - stop - %d \n", start,stop);
            }
            
            tmp1 = get_string_from_buffer(start,stop);
            
            if (debug_mode == 1) {
                //printf("update: pdf_parser - font obj dict- %s \n", tmp1);
            }
            
            //  note: type3 fonts are very rare
            //  font type 3 standard started in 1984 - the font is a binary stored in charprocs files
            //  if type3 font does not provide a separate encoding table, we will not be able to process
            
            type3_encoding = dict_find_key_value_buffer(start, stop,type3_seq,5);
            
            p12 = type3_encoding->value_start;
            p14 = type3_encoding->value_stop;
            
            if (p12 > -1 && p14 > -1) {
                if (debug_mode == 1) {
                    printf("warning: pdf_parser - found Type3 font - likely will not be able to encode this font accurately - may be no, small or large impact. \n");
                    font_type_3_flag = 1;
                    
                    //  special flag for Type 3 fonts
                    //  this is a note that will be picked up in the text processor
                    //  -- if encoding not found (e.g., cmap lookup = -1)
                    //  -- then skip the character (avoid writing noise)
                    //  -- will need to also flag in account event and run OCR
                    
                    Font_CMAP[x].standard_font_identifier = -99;
                }
            }
            
            //  first - extract Widths - common to (almost) all font dictionaries
            //      --enhancement:  critical to capture to optimize spacing and text placement
            
            // look for any predefined standard fonts - with particular encodings
            
            predefined_encoding = dict_find_key_value_buffer(start,stop,mac_roman_encoding_seq,16);
            
            if (predefined_encoding->value_start > -1 && predefined_encoding->value_stop > -1) {
                
                if (debug_mode == 1) {
                    //printf("update: pdf_parser - found MacRomanEncoding! \n");
                }
                
                Font_CMAP[x].standard_font_identifier = 1;
                
            }
            
            predefined_encoding = dict_find_key_value_buffer(start,stop,win_ansi_encoding_seq,15);
            
            if (predefined_encoding->value_start > -1 && predefined_encoding->value_stop > -1) {
                
                if (debug_mode == 1) {
                    //printf("update: pdf_parser - found WinAnsiEncoding! \n");
                }
                
                Font_CMAP[x].standard_font_identifier = 9;
            }

            // start with getting /FirstChar & /LastChar
            
            fc = -1;
            lc = -1;
            
            first_char = dict_find_key_value_buffer(start,stop,first_char_seq,9);
            fc1 = first_char->value_start;
            fc2 = first_char->value_stop;
            
            if (fc1 > -1 && fc2 > -1) {
                fc = get_int_from_buffer(fc1,fc2);
                
                if (debug_mode == 1) {
                    //printf("update: found first_char- %d \n", fc);
                }
            }
            
            last_char = dict_find_key_value_buffer(start,stop,last_char_seq,8);
            lc1 = last_char->value_start;
            lc2 = last_char->value_stop;
            
            if (lc1 > -1 && lc2 > -1) {
                lc = get_int_from_buffer(lc1,lc2);
                
                if (debug_mode == 1) {
                    //printf("update: found last_char - %d \n", lc);
                }
            }
            
            // next - get /Widths seq - could be array or indirect reference
            
            width_values = dict_find_key_value_buffer(start,stop,widths_seq,7);
            w1 = width_values->value_start;
            w2 = width_values->value_stop;
            
            if (debug_mode == 1) {
                //printf("update: width values - %s \n", get_string_from_buffer(w1,w2));
            }
            
            if (w1 > -1 && w2 > -1) {
                
                // look for indirect reference - 'hard-coded' one-off check
                w5= 1;
                
                for (w4=w1; w4 < w2; w4++) {
                    
                    // marker:  if '[' found -> then go straight to process list
                    if (buffer[w4] == 91 || buffer[w4] == 93) {
                        w5 = 1;
                        break;
                    }
                    
                    // marker: if 'R" found -> indirect reference -> need to iterate
                    if (buffer[w4] == 82) {
                        w5 = 2;
                        break;
                    }
                }
                
                if (w5 == 2) {
                    
                    w = extract_obj_from_buffer(w1,w2);
                    if (w==1) {
                        w6 = nn_global_tmp[0].num;
                        //printf("update: found indirect reference - %d \n", w6);
                        w7 = get_obj(w6);
                        if (w7 > -1) {
                            
                            if (debug_mode == 1) {
                                //printf("update: widths obj found - %s \n", get_string_from_buffer(global_tmp_obj.dict_start, global_tmp_obj.dict_stop));
                            }
                            
                            w1 = global_tmp_obj.dict_start;
                            w2 = global_tmp_obj.dict_stop;
                            
                        }
                    }
                    
                }

            
                if (lc > -1 && fc > -1) {
                    //printf("update: found widths - %s \n", get_string_from_buffer(w1,w2));
                    w = extract_obj_from_buffer(w1,w2);
                    //printf("update: # of char widths found - %d \n", w);
                
                    if (w > 0) {
                        
                        w_counter_tmp = 0;
                        for (w3=0; w3 < fc; w3++) {
                            Font_CMAP[x].widths[w3] = 0;
                        }
                        
                        if (debug_mode == 1) {
                            //printf("update: safety check - lc-fc-%d-widths-found-%d \n", (1+lc-fc),w);
                        }
                        
                        for (w3=fc; w3 < (fc + w); w3++) {
                            //printf("update: widths - %d \n", nn_global_tmp[w_counter_tmp].num);
                            Font_CMAP[x].widths[w3] = nn_global_tmp[w_counter_tmp].num;
                            
                            //printf("update: saving width - %d - %d - %d - %d \n", x,w_counter_tmp,w3,nn_global_tmp[w_counter_tmp].num);
                            
                            w_counter_tmp ++;
                        }
                    }}}
            else
                {
                    found_standard_font = -1;
                    
                    if ((dict_search_buffer(start,stop,times_roman_seq,11) == 1) || dict_search_buffer(start,stop,times_new_roman_seq,13) == 1) {
                        
                        if (debug_mode == 1) {
                            //printf("update: Found standard 14 font - Times-Roman ! \n");
                        }
                        
                        found_standard_font = 1;
                        for (w3=0; w3 < 32; w3++) {
                            Font_CMAP[x].widths[w3] = 0;
                        }
                        for (w3=32; w3 < 123; w3++) {
                            Font_CMAP[x].widths[w3] = times_roman_widths[w3-32];
                        }
                    }
                    
                    if (found_standard_font == -1) {
                        if (dict_search_buffer(start,stop,arial_mt_seq,7) == 1) {
                            
                            if (debug_mode == 1) {
                                //printf("update: Found common Microsoft font - ArialMT ! \n");
                            }
                            
                            found_standard_font = 1;
                            for (w3=0; w3 < 32; w3++) {
                                Font_CMAP[x].widths[w3] = 0;
                            }
                            for (w3=32; w3 < 123; w3++) {
                                Font_CMAP[x].widths[w3] = arial_mt_widths[w3-32];
                            }
                                                      
                        }
                    }
                    
                    if (found_standard_font == -1) {
                        if (dict_search_buffer(start,stop,symbol_seq,6) == 1) {
                            
                            if (debug_mode == 1) {
                                //printf("update: Found common Microsoft font - Symbol ! \n");
                            }
                            
                            found_standard_font = 1;
                            for (w3=0; w3 < 32; w3++) {
                                Font_CMAP[x].widths[w3] = 0;
                            }
                            for (w3=32; w3 < 123; w3++) {
                                Font_CMAP[x].widths[w3] = symbol_widths[w3-32];
                            }
                                                      
                        }
                        
                    }
                    
                    if (found_standard_font == -1) {
                        
                        if (debug_mode == 1) {
                            //printf("update: NO WIDTH FOUND PATH - setting default width ! \n");
                        }
                        
                        for (w3=0; w3 < 500; w3++) {
                            Font_CMAP[x].widths[w3] = GLOBAL_DEFAULT_WIDTH_SIZE;
                        }
                    }
                }
            
            //  end - widths capture
            
            
            // First look for "ToUnicode" sequence
            
            tou = dict_find_key_value_buffer(start,stop,tounicode_seq,9);
            t1 = tou->value_start;
            t2 = tou->value_stop;
            
            if (t1 > -1 && t2 > -1) {
                
                tmp1 = get_string_from_buffer(t1,t2);
                
                if (debug_mode == 1) {
                    //printf("update:  ToUnicode Found- %s \n", tmp1);
                }
                
                found_tou = extract_obj_from_buffer(t1,t2);
                
                // update - replacing with correct check
                // old check:  found_tou > -1
                // new correct check:   found_tou > 0
                
                if (found_tou > 0) {
                    
                    encoding_found = 1;
                    
                    Font_CMAP[x].obj_ref2 = nn_global_tmp[0].num;
                    
                    //printf("Cmap obj- %d \n", nn_global_tmp[0].num);
                    found_obj_tou = get_obj(nn_global_tmp[0].num);
                    
                    //printf("foundobj tou - %d \n", found_obj_tou);
                    
                    if (found_obj_tou > -1) {
                        start2 = global_tmp_obj.dict_start;
                        stop2 = global_tmp_obj.dict_stop;
                        cmap_flate = dict_search_buffer(start2,stop2, flatedecode_seq,11);
                        
                        //printf("cmap flate val - %d \n", cmap_flate);
                        

                        if (cmap_flate > -1) {
                            
                            //printf("PATH - looks like flate cmap \n");
                            
                            flate_str = get_string_from_buffer(start2,stop2);
                            
                            //printf("update:  flate obj dict str- %s \n", flate_str);
                            
                            start3 = global_tmp_obj.stream_start;
                            stop3 = global_tmp_obj.stop;
                            
                            //printf("update:  start-flate / stop-flate - %d - %d \n", start3,stop3);
                            
                            // printf("calling on flate_handler_buffer_v2 @ 7841 \n");
                            
                            c = flate_handler_buffer_v2(start3,stop3);
                            
                            // printf("output of flate = %d \n", c);
                            
                            if (c > -1) {
                                
                                //printf("update: going to process_cmap! \n");
                                
                                // process_cmap will free flate_dst_tmp_buffer
                                
                                dummy = process_cmap(c,x);
                                
                                //printf("update:  survived process_cmap- created in/out cmap len - %d \n", dummy);
                                
                            }
                            else {
                                
                                if (debug_mode == 1) {
                                    printf("warning: pdf_parser - failed to inflate - releasing flate_dst_tmp_buffer \n");
                                }
                                
                                if (flate_dst_tmp_buffer != NULL) {
                                    free(flate_dst_tmp_buffer);
                                }
                                
                                if (debug_mode == 1) {
                                    printf("warning: pdf_parser - font_encoding_cmap_handler - unusual case - could not inflate stream to find cmap \n");
                                }
                                
                            }
                        }
                        
                        else {
                            
                            if (debug_mode == 1) {
                                //printf("update: pdf_parser - font_encoding_cmap_handler - unusual case (handling) - CMAP not in Flate - ***CALLING PROCESS_CMAP_BUFFER*** ! \n");
                            }
                            
                            start3 = global_tmp_obj.stream_start;
                            stop3 = global_tmp_obj.stop;
                            flate_str = get_string_from_buffer(start3, stop3);
                            
                            //printf("cmap obj - %s \n", flate_str);
                            
                            if (start3 > -1 && stop3 > -1) {
                                dummy = process_cmap_buffer_alt(start3,stop3,x);
                            }}
                    }
                }
                
            }
            
            
            
            // at this point of processing -> looking for differences
        
            tmp4 = get_string_from_buffer(start,stop);
            
            //printf("NEW-Found-Font Dict - %s \n", tmp4);
            
            dif = dict_find_key_value_buffer(start,stop,differences_seq,11);
            
            t3 = dif->value_start;
            t4 = dif->value_stop;
            
            if  (t3 > -1 && t4 > -1) {
                
                tmp2 = get_string_from_buffer(t3,t4);
                
                //printf("update:  Differences FOUND AT TOP - %s \n", tmp2);
                
                encoding_found = 1;
                
                dif4 = differences_handler(tmp2,x);
                
                // useful for debugging:   printf("update: completed differences_hander -1- Font_CMAP created- cmap_apply = %d \n", dif4);
                
                if (dif4 > -1) {
                    Font_CMAP[x].cmap_apply = dif4;
                }
                else {
                    Font_CMAP[x].cmap_apply = 0;
                }
            }
            
            else {
               
                
                enc = dict_find_key_value_buffer(start,stop,encoding_seq,8);
                t5 = enc->value_start;
                t6 = enc->value_stop;
                
                if (t5 > -1 && t6 > -1) {
                
                    // tmp3 = get_string_from_buffer(t5,t6);
                
                    //printf("update: Encoding - %d-%d \n", t5,t6);
                
                    enc_obj = extract_obj_from_buffer(t5,t6);
                    
                    // useful for debugging - printf("update: completed extract_obj_from_buffer - value = %d \n", enc_obj);
                    
                    //  update - change - replacing:   enc_obj > -1
                    
                    if (enc_obj > 0) {
                        
                        encoding_found = 1;
                        
                        enc1 = nn_global_tmp[0].num;
                    
                        // useful for debugging - printf("update: Enc Obj#- %d \n", enc1);
                    
                        if (enc1 > -1) {
                        
                            enc2 = get_obj(enc1);
                            
                            if (enc2 > -1) {
                                // found matching object
                            
                                enc_start = global_tmp_obj.dict_start;
                                enc_stop = global_tmp_obj.dict_stop;
                                tmp4 = get_string_from_buffer(enc_start,enc_stop);
                        
                                if (debug_mode == 1) {
                                    //printf("update: pdf_parser -  Encoding Dict - %d-%d-%s \n", enc_start,enc_stop,tmp4);
                                }
                                
                                dif = dict_find_key_value_buffer(enc_start,enc_stop,differences_seq,11);
                        
                                dif1 = dif->value_start;
                                dif2 = dif->value_stop;
                        
                                if (dif1 > -1 && dif2 > -1) {
                            
                                    dif_str = get_string_from_buffer(dif1,dif2);
                            
                                    dif4 = differences_handler(dif_str,x);
                            
                                    //printf("update: differences -2- Font_CMAP created- cmap_apply = %d \n", dif4);
                            
                                    if (dif4 > -1) {
                                        Font_CMAP[x].cmap_apply = dif4;
                                    }
                                    else {
                                        Font_CMAP[x].cmap_apply = 0;
                                    }
                                }
                        }}
                    }
            }
            }
            
            
        }
        
        if (encoding_found == -1) {
            
            if (debug_mode == 1) {
                //printf("update: pdf_parser - did not find Encoding - will keep looking! \n");
            }
            
            
            // insert - jan 24 2023 - look for Type0 Font and apply special handling
            
            type0_encoding = dict_find_key_value_buffer(start, stop,type0_seq,5);
            
            p12 = type3_encoding->value_start;
            p14 = type3_encoding->value_stop;
            
            if (p12 > -1 && p14 > -1) {
                if (debug_mode == 1) {
                    //printf("update: pdf_parser - found type0 font - apply special 3/32 encoding \n");
                }
                for (last_iter1=0; last_iter1 < 150; last_iter1++) {
                    Font_CMAP[x].cmap_in[last_iter1] = 3 + last_iter1;
                    Font_CMAP[x].cmap_out[last_iter1] = 32 + last_iter1;
                }
                Font_CMAP[x].cmap_apply = 140;
                
            }
            
            // end - insert - check for Type0 font
            
            
            dif = dict_find_key_value_buffer(start,stop,font_descriptor_seq,14);
            t3 = dif->value_start;
            t4 = dif->value_stop;
            
            if (t3 > -1 && t4 > -1) {
                
                if (debug_mode == 1) {
                    //printf("update: looking for encoding in font_descriptor! \n");
                }
                
                enc_obj = extract_obj_from_buffer(t3,t4);
                
                enc1 = nn_global_tmp[0].num;
            
                if (enc1 > -1) {
                
                    enc2 = get_obj(enc1);
                    
                    if (enc2 > -1) {
                        // found matching object
                    
                        enc_start = global_tmp_obj.dict_start;
                        enc_stop = global_tmp_obj.dict_stop;
                        tmp4 = get_string_from_buffer(enc_start,enc_stop);
                        
                        if (debug_mode == 1) {
                            //printf("update: pdf_parser - FontDescriptor Dict - %d-%d-%s \n", enc_start,enc_stop,tmp4);
                        }
                        
                        dif = dict_find_key_value_buffer(enc_start,enc_stop,font_file_seq,8);
                
                        dif1 = dif->value_start;
                        dif2 = dif->value_stop;
                        
                        // check if FontFile2 or FontFile3 and skip
                        
                        if (dif1 > -1) {
                            if (buffer[dif1] == 50 || buffer[dif1] == 51) {
                                //looks like fontfile2 or fontfile3 -> skip
                                //fail the test
                                dif1= -1;
                                // dif1 ++;
                                
                                if (debug_mode == 1) {
                                    //printf("warning: pdf_parser - found FontFile2/3 - may have encoding issues if can not find embedded CMAP ! \n");
                                }
                                
                                // EXPERIMENT - STARTS HERE
                                // this is WIP - currently shut-off
                                
                                
                                if (x > 10000000) {
                                    /*
                                    if (Font_CMAP[0].cmap_apply > 0) {
                                        
                                        // apply the 'state' from the last cmap
                                        // there seems to be a 'fall-back' in some files that are missing CMAPs in which the PDF Processor goes back to the state of a previous CMAP when it is not found
                                        // this is WIP to build comprehensive handling for edge cases
                                        
                                        if (debug_mode == 1) {
                                            printf("update: EXPERIMENT- assigning 'state' to font! %d-%d \n",
                                                   x, Font_CMAP[0].cmap_apply);
                                        }
                                        
                                        Font_CMAP[x].cmap_apply = Font_CMAP[0].cmap_apply;
                                        
                                        for (last_iter1=0; last_iter1 < Font_CMAP[0].cmap_apply; last_iter1++) {
                                            
                                            Font_CMAP[x].cmap_in[last_iter1] = Font_CMAP[0].cmap_in[last_iter1];
                                            Font_CMAP[x].cmap_out[last_iter1] = Font_CMAP[0].cmap_out[last_iter1];
                                            
                                            printf("update: cmap - %d - %d - %d \n", last_iter1, Font_CMAP[x].cmap_in[last_iter1], Font_CMAP[x].cmap_out[last_iter1]);
                                        }
                                    }
                                     */
                                }
                                else {
                                    if (debug_mode == 1) {
                                        //printf("update: pdf_parser - could not find CMAP - try from previous font - this is shut off - no action taken ! \n");
                                    }
                                    
                                    /*
                                    for (last_iter1=0; last_iter1 < 150; last_iter1++) {
                                        Font_CMAP[x].cmap_in[last_iter1] = 3 + last_iter1;
                                        Font_CMAP[x].cmap_out[last_iter1] = 32 + last_iter1;
                                    }
                                    Font_CMAP[x].cmap_apply = 140;
                                     */
                                    
                                }
                                
                                // EXPERIMENT - ENDS HERE
                                
                            }
                        }
                        
                        if (dif1 > -1 && dif2 > -1) {
                            
                            if (debug_mode == 1) {
                                //printf("update: pdf_parser - Found FontFile (and excluded FontFile 2 ! \n");
                            }
                            
                            enc_obj = extract_obj_from_buffer(dif1,dif2);
                            enc1 = nn_global_tmp[0].num;
                            if (enc1 > -1) {
                                enc2 = get_obj(enc1);
                                if (enc2 > -1) {
                                    enc_start = global_tmp_obj.dict_start;
                                    enc_stop = global_tmp_obj.dict_stop;
                                    
                                    if (debug_mode == 1) {
                                        //printf("update: pdf_parser - FontFile - %s \n", get_string_from_buffer(enc_start,enc_stop));
                                    }
                                    
                                    l1 = dict_find_key_value_buffer(enc_start,enc_stop,length1_seq,7);
                                    
                                    len1 = l1->value_start;
                                    len2 = l1->value_stop;
                                    
                                    //printf("update- len1 string - %s \n", get_string_from_buffer(len1,len2));
                                    
                                    if (len1 > -1 && len2 > -1) {
                                        
                                        len_on = 1;
                                        
                                        for (dif1=0; dif1 < (len2-len1); dif1++) {
                                            len_array[dif1] = buffer[len1+dif1];
                                            
                                            //printf("len_array-%d-%d \n", dif1,len_array[dif1]);
                                            
                                            // check for obj not length1 count
                                            
                                            if (buffer[len1+dif1] == 82) {
                                                if (dif1 > 3) {
                                                    if (buffer[len1+dif1-1] == 32) {
                                                        if (buffer[len1+dif1-2] == 48) {
                                                            if (buffer[len1+dif1-3] == 32) {
                                                                // found signature '_0_R"
                                                                len_on = -1;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        
                                        len_array[(len2-len1)] = 0;
                                        
                                        len_int = 0;
                                        
                                        if (len_on == 1) {
                                            len_int = get_int_from_byte_array(len_array);
                                            
                                            //printf("got int from array - %d \n", len_int);
                                            
                                        }
                                    }
                                    
                                    else {
                                        len_int = 0;
                                        
                                        // printf("hitting else \n");
                                        
                                    }
                                    
                                    start3 = global_tmp_obj.stream_start;
                                    stop3 = global_tmp_obj.stop;
                                    
                                    c = flate_handler_buffer_v2(start3,stop3);
                                    
                                    // printf("update: len_int - %d - flate outcome - %d @ 8124 \n", len_int, c);
                                    
                                    if (c > -1) {
                                        strcpy(fontfile_str, "");
                                        
                                        for (dif1=0; dif1 < len_int; dif1++) {
                                            
                                            
                                            if (debug_mode == 1) {
                                                //printf("%c",flate_dst_tmp_buffer[dif1]);
                                            }
                                            
                                            sprintf(ff_tmp,"%c",flate_dst_tmp_buffer[dif1]);
                                            strcat(fontfile_str,ff_tmp);
                                        }
                                        
                                        c = fontfile_handler(fontfile_str,x);
                                        
                                        // new insert
                                        
                                        if (debug_mode == 1) {
                                            //printf("update: unusual case - post fontfile_handler - releasing flate_dst_tmp_buffer @ 8144");
                                        }
                                        
                                        free(flate_dst_tmp_buffer);
                                        
                                        
                                        if (c > -1) {
                                            Font_CMAP[x].cmap_apply = c;
                                            
                                            if (debug_mode == 1) {
                                                //printf("update: Font_CMAP- %d-%d \n", x,c);
                                            }
                                            
                                        }
                                        else {
                                            Font_CMAP[x].cmap_apply = 0;
                                        }
                                        
                                    }
                                    
                                }
                            }
                        }
                
                    }}
                
                }
            }
        
    }
    
    // useful for debugging - printf("update: font_encoding_cmap_handler - finished processing- returning to main loop \n");
    
    
    return 0;
}


int cmap_get_char(int tmp_char,int my_cmap) {
    
    int x;
    int char_out;
    char_out = -1;
    
    // three conditions-
    // 1.  if found font, e.g., my_cmap > -1
    // 2.  if corresponding font has a cmap to apply, e.g., .cmap_apply > -1
    // 3.  if cmap_in list finds entry
    
    if (my_cmap > -1) {
        if (Font_CMAP[my_cmap].cmap_apply > -1) {
            
            //printf("applying CMAP - %d-%d-%d \n", my_cmap,Font_CMAP[my_cmap].cmap_apply,tmp_char);
            
            for (x=0; x < Font_CMAP[my_cmap].cmap_apply; x++) {
                
                if (tmp_char == Font_CMAP[my_cmap].cmap_in[x]) {
                    char_out = Font_CMAP[my_cmap].cmap_out[x];
                    
                    /*
                    if (tmp_char < 500) {
                        printf("update: Found match on cmap- char-%d-width-%d-out-%d- ", tmp_char,Font_CMAP[my_cmap].widths[tmp_char],char_out);
                    }
                    */
                    
                    //printf("update:  Found match on cmap- %d-%d-%d \n", tmp_char,char_out,x);
                    
                    break;
                    
                }}}}

    return char_out;
}


int text_processor_page (int page_buffer_size, int page_number) {
    
    ts *text_state_cm;
    ts *text_state_tm;
    
    float cm_x = 0;
    float cm_y = 0;
    float cm_scaler = 1;
    
    // insert - new - oct 1, 2022
    char* char_str_ptr;
    
    float tm_scaler = 1;
    
    float last_tm_x = 0;
    float last_tm_y = 0;
    float last_tml_x = 0;
    
    int first_coords_set = -1;
    
    // assume as start default a font_size of 10
    int last_font_size_state = 10;
    
    // new variable - last_char_width
    int last_char_width = 0;
    float last_char_text_adder = 0;
    float total_char_width = 0;
    
    int distance_too_far = 0;
    
    int p,x,y,z,t;
    
    int octal_on = -1;
    int octal_count = 0;
    
    // new insert on sept 26
    int dec1;
    int dec2;
    int dec3;
    char dec_char[20];
    char dec1_char[20];
    char dec2_char[20];
    char dec3_char[20];
    char dec_tmp[20];
    char dec_tmp2[20];
    char dec_tmp3[20];
    
    // new - jan 15
    int img_on_page_max = 500;
    
    // iterate thru inflated buffer for page
    // store inflated page buffer - pointer should be input to text_processor_page
    
    char core_text_out[50000];
    
    char core_text_copy_tmp[50000];
    char core_text_final[50000];
    char core_period_tmp[100];
    
    // update: 031823 - change in formatted text: 10K - 50K (safety)
    char formatted_text[50000];
    int ft_x = 0;
    int ft_y = 0;
    
    int core_text_copy_go_ahead = 0;
    int period_break_marker = 0;
    
    int MAX_TEXT_BOX_COUNT = 10000;
    
    int* BT[MAX_TEXT_BOX_COUNT];
    int* ET[MAX_TEXT_BOX_COUNT];
    int* ts_start[MAX_TEXT_BOX_COUNT];
    int* ts_stop[MAX_TEXT_BOX_COUNT];
    char font_apply[MAX_TEXT_BOX_COUNT][50];
    char tm[200];
    char tm_tmp[10];
    
    int* my_font[20];
    int font_name_on = 0;
    int* my_font_size[20];
    int font_size_on = 0;
    int fs=0;
    
    // change - jan 25, 2023
    int font_size_state = GLOBAL_FONT_SIZE;
    
    int s_start_new = 0;
    int f_found = 0;
    
    int text_box_count=0;
    int text_on = 0;
    int s;
    int i, j;
    int text_state_size_to_get = 50;
    
    char* tmp_str2;
    int ss=0;
    char out_str_tmp[MAX_TEXT_BOX_COUNT];
    char ts[MAX_TEXT_BOX_COUNT][100];
    char ost_tmp[10];
    int ost_counter = 0;
    
    char cm[MAX_TEXT_BOX_COUNT][100];
    int cm_on[MAX_TEXT_BOX_COUNT];
    int cm_found = 0;
    
    int tmp_char;
    int new_char;
    
    int found_tj = -1;
    int bt_start = 0;
    int et_start = 0;
    
    int bloks_created = 0;
    
    int hex_len = 0;
    int bracket_on;
    int string_on;
    int hex_on;
    int escape_on;
    int escape_count;
    char tmp_str[100000];
    char hex_tmp_str[100000];
    char* hex_output_str;
    char text_str[500000];
    
    // experiment - oct 29
    char table_text_str[100000];
    char table_text_tmp[10000];
    int table_count_indicator = 0;
    char tt_tmp[100];
    char table_text_out[100000];
    char table_text_out_final[100000];
    int table_success = 0;
    int table_row = 0;
    int table_col = 0;
    int table_cols_sorter[100];
    
    int found_table_cell_content = 0;
    int max_col_count = 0;
    int tab_i = 0;
    int tab_j = 0;
    int tab_k = 0;
    int tab_l = 0;
    int tab_tmp = 0;
    int tab_most_common = -1;
    
    int tab_num_count = 0;
    int tab_total_count = 0;
    int tab_iter_nums = 0;
    
    int my_col_locations[100];
    int tab_tmp2 = 0;
    int tab_tmp3 = 0;
    int tab_tmp4 = 0;
    int tab_tmp5 = 0;
    int tab_col_current_tmp = 0;
    table_cells my_table[150][50];
    char table_cell[100000];
    int table_row_y[150];
    int table_tmp_go_ahead =0;
    int table_copy_row = 0;
    int table_copy_col = 0;
    int new_row_found = 0;
    char table_formatting_copy[10000];
    
    int dupe_space_check = 0;
    int not_spaces_only = 0;
    
    // end - experiment
    
    // new var - multi-part test for writing blok
    int create_blok_go_ahead = 0;
    int ii;
    
    char tmp[10];
    int tj_look_ahead;
    int tml = 25;
    char new_tmp[10];
    int image_on_page_count;
    char img_name_tmp [100];
    char img_do_str[100];
    int img_do[100];
    
    // insert new variable - july 11
    int img_do_len = 20;
    
    int text_written_in_bt = -1;
    int text_space_action = 0;
    
    int add_hex_space = 0;
    
    int escape_keep = -1;
    
    
    // insert new variable to track count of displayed char in BT
    // will use this for spacing approximation
    // will reset every time that a new TM is found in BT box
    
    int bt_char_out_count = 0;
    int char_buffer_assumption = 50;
    int bt_char_text_space = 0;
    int char_size = 20;
    int TEST_PAGE_NUMBER = 1;
    
    int s_start = 0;
    
    int cursor=0;
    
    int oct1,oct2,oct3;
    
    int BDC_FLAG = 0;
    
    // new -added - march 17, 2023
    int last_hex_str_len = 0;
    int last_char_str_len = 0;
    int last_bracket_len = 0;
    
    char space_count[10];
    int space_count_num = 0;
    int space_counter = 0;
    int space_decimal_stopper = 0;
    strcpy(space_count,"");
    
    strcpy(new_tmp,"");
    strcpy(text_str,"");
    strcpy(core_text_out,"");
    strcpy(formatted_text,"");
    
    strcpy(table_text_str," <tr> <th> <A1> ");  // experiment - oct 29
    //table_col ++;
    //table_row ++;
    strcpy(tt_tmp,"");          // experiment - oct 29
    strcpy(table_text_tmp,"");  // experiment - oct 29
    strcpy(table_text_out, ""); // experiment - oct 29
    strcpy(table_text_out_final,"");
    strcpy(table_cell,"");
    int table_cell_counter = 0;
    strcpy(table_formatting_copy,"");
    
    for (tab_i=0; tab_i < 50; tab_i ++) {
        pdf_table[tab_i].col_count = 0;
    }
    
    char tf_window_tmp[100];
    char tf_tmp[10];
    
    //int my_cmap = -1;
    int my_cmap = GLOBAL_PAGE_CMAP;
    
    int img_match = 0;
    
    strcpy(out_str_tmp,"");
    
    image_on_page_count = Pages[page_number].image_entries;
    
    cm_found = 0;
    
    //  Getting started HERE with processing of page_buffer
    
    // printf("update: initiating start of text_process - %d ! \n", page_buffer_size);
    
    for (x=0; x < page_buffer_size; x++) {
        
        // start at the top of the page buffer
        // identify BT/ET blocks for further processing
        // capture information outside of BT/ET blocks, especially font state info
        
        //  for 'deep' debugging - useful to do char-by-char view of each byte in the page buffer
        //  printf("page buffer size - %d - %d - %d - %d \n", x, page_buffer_size, text_on,page_buffer[x]);
        
        // fullpagedeepdive
        
        /*
        if (debug_mode == 1) {
            
            if (page_number == 1) {
                
                if (flate_dst_tmp_buffer[x] < 32 || flate_dst_tmp_buffer[x] > 128) {
                    if (flate_dst_tmp_buffer[x] == 13) {
                        printf("\n");
                    }
                    else {
                        printf("\n", flate_dst_tmp_buffer[x]);
                    }
                }
                else {
                    printf("%c", flate_dst_tmp_buffer[x]);
                }
                
            }
            
        }
        */
        
        
        //  look for "B" "T" = 66 84 seq
        
        if (flate_dst_tmp_buffer[x] == 66 && text_on == 0) {
            
            if (x+2 < page_buffer_size) {
                
                if (flate_dst_tmp_buffer[x+1] == 84) {
                    
                    // change from <= 32 to specific 10/13/32
                    // adding '-' = 45 on jan 27
                    
                    if (flate_dst_tmp_buffer[x+2] == 32 || flate_dst_tmp_buffer[x+2] == 10 || flate_dst_tmp_buffer[x+2] == 13 || flate_dst_tmp_buffer[x+2] == 45) {
                        
                        /*
                         if (page_number == 3) {
                         printf("\nFOUND BT - %d \n", x+2);
                         }
                         */
                        
                        text_on = 1;
                        BT[text_box_count] = x+2;
                        ts_stop[text_box_count] = x-1;
                        strcpy(ts[text_box_count],out_str_tmp);
                        strcpy(out_str_tmp,"");
                        ost_counter = 0;
                        
                    }}}}
        
        
        // look for "E "T" = 69 84 seq
        
        if (flate_dst_tmp_buffer[x] == 69 && text_on == 1) {
            
            if (x+1 < page_buffer_size) {
                
                if (flate_dst_tmp_buffer[x+1] == 84) {
                    
                    text_on = 0;
                    ET[text_box_count] = x;
                    if (cm_found == 0) {
                        cm_on[text_box_count] = 0;
                        strcpy(cm[text_box_count],"");
                    }
                    
                    text_box_count ++;
                    if (text_box_count > MAX_TEXT_BOX_COUNT-1) {
                        
                        // safety check -> usually means that something has gone very wrong in the parsing
                        //  ... but there can also be extremes in documents
                        // MAX_TEXT_BOX_COUNT defined within this function locally @ 10,000
                        
                        printf("warning:  pdf_parser - unusual - exceeded max text box count -  %d \n", text_box_count);
                    }
                    
                    ts_start[text_box_count] = x+2;
                    cm_found = 0;
                }}}
        
        
        // look for image object - referenced by start with "/"
        
        if (text_on == 0 && flate_dst_tmp_buffer[x] == 47 && image_on_page_count > 0) {
            
            //printf("update: found / %d - ", x);
            
            if (x+20 < page_buffer_size) {
                img_do_len = 20;
            }
            else {
                img_do_len = page_buffer_size - x;
                if (img_do_len < 0) {
                    img_do_len =0;
                }
            }
            
            for (j=0; j < img_do_len; j++) {
                img_do[j] = flate_dst_tmp_buffer[x+j];
            }
            img_do[img_do_len] = 0;
            strcpy(img_do_str,get_string_from_byte_array(img_do,img_do_len+1));
            
            if (debug_mode == 1) {
                //printf("update:  Image Do str - %s \n", img_do_str);
            }
            
            if (image_on_page_count > img_on_page_max) {
                
                if (debug_mode == 1) {
                    printf("warning: pdf_parser - potential error - img_on_page_count > max - %d ", image_on_page_count);
                }
                
                image_on_page_count = img_on_page_max;
            }
            
            for (i=0; i < image_on_page_count; i++) {
                
                if (strlen(Pages[page_number].image_names[i]) > 0) {
                    
                    strcpy(img_name_tmp,Pages[page_number].image_names[i]);
                    img_match = 0;
                    
                    //printf("CHECKING IMG FOUND - %s-%d-%d \n", img_name_tmp,i,image_on_page_count);
                    
                    for (j=0; j < strlen(img_name_tmp); j++) {
                        if (img_name_tmp[j] == flate_dst_tmp_buffer[x+j]) {
                            img_match ++;
                        }
                        else {
                            img_match = 0;
                            break;
                        }
                    }
                    
                    if (img_match == strlen(img_name_tmp) && img_match > 0) {
                        
                        if (debug_mode == 1) {
                            //printf("update: found Image name match - %s-%f-%f-%f-%d \n", img_name_tmp,cm_x,cm_y,cm_scaler, i);
                        }
                        
                        Pages[page_number].image_x_coord[i] = cm_x;
                        Pages[page_number].image_y_coord[i] = cm_y;
                        Pages[page_number].image_found_on_page[i] = 1;
                    }
                    else {
                        // no action taken
                        // no match found
                    }
                }}
            
            
        }
        
        // get text state info before BT
        if (text_on == 0 && ost_counter < 900) {
            
            if (flate_dst_tmp_buffer[x] != 10 && flate_dst_tmp_buffer[x] != 13) {
                
                sprintf(ost_tmp,"%c",flate_dst_tmp_buffer[x]);
                strcat(out_str_tmp,ost_tmp);
                ost_counter ++;
                
            }}
        
        // new - look for Tf window to load font -> possible outside of BT / ET
        if (text_on == 0 && flate_dst_tmp_buffer[x] == 84) {
            if (x+1 < page_buffer_size) {
                if (flate_dst_tmp_buffer[x+1] == 102) {
                    
                    //printf("update: FOUND Tf OUTSIDE of BT/ET \n");
                    
                    ss=0;
                    fs=0;
                    strcpy(tf_window_tmp,"");
                    font_size_on = 0;
                    font_name_on = 0;
                    
                    if (x < 25) {
                        s_start = 0;
                    }
                    else {
                        s_start = x-25;
                    }
                    
                    // new insert - safety check in Tf font window look-back
                    
                    s_start_new = s_start;
                    
                    for (s= x-1; s >= s_start; s--) {
                        
                        if (flate_dst_tmp_buffer[s] == 47) {
                            // stop lookback here - at first / found
                            s_start_new = s;
                            break;
                        }
                    }
                    
                    // end - new insert - safety check in Tf font window look-back
                    
                    
                    for (s= s_start_new; s < x; s++) {
                        
                        //printf("%c", page_buffer[s]);
                        
                        if (flate_dst_tmp_buffer[s] != 10 && flate_dst_tmp_buffer[s] != 13) {
                            sprintf(tf_tmp,"%c",flate_dst_tmp_buffer[s]);
                            strcat(tf_window_tmp,tf_tmp);
                        }
                        
                        if (font_name_on == 1) {
                            if (flate_dst_tmp_buffer[s] == 32 || flate_dst_tmp_buffer[s] == 10 || flate_dst_tmp_buffer[s] == 13) {
                                font_name_on = 2;
                                strcpy(font_apply[text_box_count],my_font);
                            }
                            else
                            {
                                my_font[ss] = flate_dst_tmp_buffer[s];
                                ss++;
                            }}
                        
                        if (font_size_on == 1) {
                            
                            // stop at decimal point '.'
                            if (flate_dst_tmp_buffer[s] == 32 || flate_dst_tmp_buffer[s] == 46 || flate_dst_tmp_buffer[s] == 10 || flate_dst_tmp_buffer[s] == 13) {
                                font_size_on = 2;
                                my_font_size[fs] = 0;
                                last_font_size_state = font_size_state;
                                
                                // set font_size_state from Tf invocation
                                
                                font_size_state = get_int_from_byte_array(my_font_size);
                                
                                // if font_size_state == 1 -> apply tm_scaler
                                // otherwise, reset tm_scaler -> 1
                                
                                if (font_size_state > 1) {
                                    tm_scaler = 1;
                                }
                                
                                // set font size and apply tm_scaler, if any - default set to 1
                                //font_size_state = (int)(font_size_state * tm_scaler);
                                
                                
                                fs=0;
                            }
                        }
                        
                        if (flate_dst_tmp_buffer[s] > 47 && flate_dst_tmp_buffer[s] < 58 && font_name_on == 2) {
                            
                            if (font_size_on < 2) {
                                
                                font_size_on = 1;
                                my_font_size[fs] = flate_dst_tmp_buffer[s];
                                fs++;
                            }}
                        
                        if (flate_dst_tmp_buffer[s] == 47) {
                            
                            //font name will start with /
                            
                            ss=0;
                            my_font[ss] = flate_dst_tmp_buffer[s];
                            font_name_on = 1;
                            ss++;
                        }
                    }
                    
                    
                    tmp_str2 = get_string_from_byte_array(my_font,ss);
                    
                    if (debug_mode == 1) {
                        if (page_number < 7) {
                            //printf("update:  TEXT PROCESSOR - Font found in Tf window outside BT/ET - %s \n", tmp_str2);
                        }
                    }
                    
                    for (z=0; z < global_font_count; z++) {
                        
                        if (strcmp(Font_CMAP[z].font_name,tmp_str2) == 0) {
                            
                            if (page_number == 1) {
                                //printf("update:  FOUND MATCHING FONT NAME outside BT/ET -%d- %s ! \n", z, Font_CMAP[z].font_name);
                            }
                            
                            for (p=0; p < Font_CMAP[z].page_apply_counter; p++) {
                                
                                // need to test with many docs whether pages capture is +1
                                if (Font_CMAP[z].pages[p] == page_number ) {
                                    
                                    my_cmap = z;
                                    
                                    // very useful check if Font loaded correctly
                                    
                                    if (debug_mode == 1) {
                                        if (page_number >= 0 && page_number < 4) {
                                            //printf("update: pdf_parser - TEXT PROCESSOR - Found matching font outside BT-ET -%d-%s-CMAP val- %d \n", z,tmp_str2,Font_CMAP[z].cmap_apply);
                                        }
                                    }
                                    
                                    break;
                                }
                            }
                        }
                    }
                    
                    
                }
            }
        }
        
        
        // look for Tm window to set text_state
        if (text_on == 0 && flate_dst_tmp_buffer[x] == 84) {
            
            if (x+1 < page_buffer_size) {
                
                if (flate_dst_tmp_buffer[x+1] == 109) {
                    
                    if (page_number == TEST_PAGE_NUMBER) {
                        
                        //printf("Found Tm window OUTSIDE BT/ET Box -%d \n", x);
                    }
                    
                    ss=0;
                    strcpy(tm,"");
                    if (x-25 >= 0) {
                        tml = 25;}
                    else {
                        tml = x;
                    }
                    for (s=x-tml; s < x; s++) {
                        sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[s]);
                        strcat(tm,tm_tmp);
                    }
                    if (page_number == TEST_PAGE_NUMBER) {
                        //printf("Tm window (outside BT/ET)- %s - ",tm);
                    }
                }
            }
        }
        
        // look for cm window to set text_state
        if (text_on == 0 && flate_dst_tmp_buffer[x] == 99) {
            
            if (x+1 < page_buffer_size) {
                if (flate_dst_tmp_buffer[x+1] == 109) {
                    
                    if (page_number < 5) {
                        //printf("Found cm window (outside BT/ET) -%d \n", x);
                    }
                    
                    ss=0;
                    strcpy(tm,"");
                    cm_found = 1;
                    
                    if (x-50 < 0) {
                        text_state_size_to_get = x;
                    }
                    else {
                        text_state_size_to_get = 50;
                    }
                    
                    for (s=x-text_state_size_to_get; s < x; s++) {
                        sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[s]);
                        strcat(tm,tm_tmp);
                    }
                    
                    strcpy(cm[text_box_count],tm);
                    cm_on[text_box_count] = 1;
                    
                    text_state_cm = get_text_state(tm,text_state_size_to_get);
                    cm_x = text_state_cm->x;
                    cm_y = text_state_cm->y;
                    cm_scaler = text_state_cm->scaler;
                    
                    
                    if (page_number < 5) {
                        //printf("cm window-outside- %s - %f - %f - %f \n",tm, cm_x,cm_y,cm_scaler);
                    }
                    
                }
            }
        }
        
        
        
    }
    
    
    //
    //  first step in processing page completed - identified BT/ET boxes
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - BT/ET text boxes found-%d \n", text_box_count);
    }
    
    // insert new triage process -> if no BT/ET pairs found
    //  --very unusual pattern, but there are some documents with 'implicit' BT/ET
    
    if (text_box_count == 0 ) {
        BT[0] = 0;
        
        // check if unusually large page_buffer, and cap
        
        if (page_buffer_size > 10000) {
            page_buffer_size = 10000;
        }
        
        ET[0] = page_buffer_size;
        cm_on[0] = 0;
        
        text_box_count += 1;
    }
    
    //
    
    // initialize last tm state @ top of page
    
    // special default marker to indicate that no text yet written to text_str in this BT-ET
    last_tm_x = 0;
    last_tm_y = 0;
    
    text_written_in_bt = -1;
    text_space_action = 0;
    
    
    //printf("update: text box count - %d \n", text_box_count);
    
    for (x=0; x < text_box_count; x++) {
        
        if (debug_mode == 1) {
            //printf("update: back up at top of text box iteration - %d - %d - %d - %d \n", text_box_count, x, table_row, table_col);
        }
        
        bracket_on = 0;
        string_on = 0;
        hex_on = 0;
        hex_len = 0;
        escape_on = 0;
        escape_count =0;
        //my_cmap = -1;
        strcpy(tmp_str,"");
        strcpy(hex_tmp_str,"");
        octal_on = 0;
        octal_count = 0;
        cursor = 0;
        
        bt_start = BT[x];
        et_start = ET[x];
        
        
        // check if cm found and update text state - otherwise, keep the last text state found!
        if (cm_on[x] == 1) {
            
            text_state_cm = get_text_state(cm[x],50);
            cm_x = text_state_cm->x;
            cm_y = text_state_cm->y;
            cm_scaler = text_state_cm->scaler;
            
            if (debug_mode == 1) {
                if (page_number < 6) {
                    //printf("update: found CM @ TOP OF BT - cm x - %f - y - %f - scaler - %f \n", text_state_cm->x, text_state_cm->y, text_state_cm->scaler);
                }
            }
            
        }
        
        // start parsing individual BT-ET text box
        
        // text position matrix is always reset at start of new BT-ET box
        
        //last_tml_x = tml_x;
        
        tml_x = 0;
        tm_x = 0;
        tm_y = 0;
        
        
        
        for (y= bt_start; y < et_start; y++) {
            
            // deepdive
            
            /*
            if (debug_mode == 1) {
            
                 if (page_number >= 2 && page_number < 26) {
                     if (flate_dst_tmp_buffer[y] > 31 && flate_dst_tmp_buffer[y] < 128) {
                         printf("%c",flate_dst_tmp_buffer[y]);
                     }
                     else {
                         if (flate_dst_tmp_buffer[y] == 13 || flate_dst_tmp_buffer[y] == 10) {
                             printf("\n");
                         }
                         else {
                             printf("-%d-",flate_dst_tmp_buffer[y]);
                         }
                     }
                 }
            }
        */
        
            
            // toggle escape_on
            if (escape_on == 1) {
                if (escape_count == 1) {
                    escape_on = 0;
                    escape_count = 0;
                }
                else {
                    if (escape_count == 0) {
                        escape_count ++;
                    }
                }
            }
            
            
            // if "\" -> escape_on
            if (flate_dst_tmp_buffer[y] == 92 && escape_on == 0){
                escape_on = 1;
                escape_count = 0;
            }
            
            
            // if "(" -> string_on
            if (string_on == 0 && hex_on == 0 && flate_dst_tmp_buffer[y] == 40) {
                
                string_on = 1;
                
                text_space_action = evaluate_text_distance(tml_x,tm_y,last_tm_x,last_tm_y,font_size_state,tm_scaler);
                
                // experiment - adding second 'absolute' test - if distance > 5
                // distance of 5 for typical font of 10 implies 1 full char
                // will be useful with large fonts too
                
                // note: march 17, 2023 - should tml_x test be relative to font size???
                // --added test:  if long string inside char, do not add space
                // --logic: in very long string, it can be difficult to keep track of tml_x
                // --in most cases, when TD used to visually insert space, it is with short strings
                
                if (text_space_action > 0 || ( (last_char_str_len < 5) && (tml_x - last_tm_x) > 5) ) {
                    
                    // add len safety check
                    if (strlen(tmp_str) < 99998) {
                        strcat(tmp_str, " ");
                    }
                    
                    if ((font_size_state * tm_scaler) >= 18) {
                        if (strlen(formatted_text) < 9000) {
                            strcat(formatted_text," ");
                        }
                    }
                    
                    if (debug_mode == 1) {
                        if (page_number == 2 && page_number < 4) {
                            //printf("update: evaluated text distance - adding space - %f-%f-%f-%f-%f-%f-%f \n", tml_x,tm_x,tm_y,last_tm_x, last_tm_y, (float)font_size_state,(float)tm_scaler);
                            //printf("update: last_char_str_len - %d-%d \n", last_char_str_len, text_space_action);
                        }
                    }
                    
                }
                else {
                    
                    // 0.3 is too small if 'i' or 'l'
                    
                    /*
                     // experiment starts here
                     // this test is problematic !
                     
                     // should this test apply outside of a bracket ???
                     
                     if (bracket_on == 1) {
                     if ((tm_x - last_tm_x) > (last_char_text_adder * 0.6)) {
                     strcat(tmp_str, " ");
                     }
                     
                     if (page_number < 5) {
                     printf("WOW -update: tm_x - last_tm_x > 0.6 * last_char_text_adder - %f-%f-%f \n", tm_x, last_tm_x, last_char_text_adder);
                     }
                     }
                     */
                }
                
                
                space_decimal_stopper = 0;
                
                
                // experiment - check if bracket_on - and only apply space_count if inside [ ]
                
                
                if (space_counter > 0 && bracket_on == 1) {
                    
                    //  assess whether to add space
                    //  this space_count still needs work -> does not work consistently on all docs
                    //  'implicit' spacing is a big challenge to solve
                    
                    space_count_num = atoi(space_count);
                    
                    // end - experiment
                    
                    //printf("update: found space_count- %d \n", space_count_num);
                    
                    // note the space_count is subtracted- so negative numbers are +
                    
                    tml_x = tml_x - (font_size_state * tm_scaler * space_count_num / 1000);
                    
                    // experiment - oct 29
                    if (space_count_num >= 500 || space_count_num <= -500) {
                        
                        
                        table_count_indicator ++;
                        
                    }
                    
                    
                    // base space_count insertion on 30% of last_char_width OR > 200
                    //      -- note:  this may be trial-and-error - does not appear to be 'perfect' rule
                    
                    // 175 seems stable but misses spaces
                    
                    if (-(space_count_num) > 155) {
                        
                        if (strlen(tmp_str) < 99998) {
                            strcat(tmp_str," ");
                            
                           // printf("update: inserting space in () due to space count - %d \n", space_count_num);
                        }
                        
                        if ((font_size_state * tm_scaler) >= 18) {
                            if (strlen(formatted_text) < 9000) {
                                strcat(formatted_text," ");
                            }
                        }
                        
                        // experiment
                        
                        //strcat(table_text_str," ");
                        if (strlen(table_cell) < 99998) {
                            strcat(table_cell, " ");
                        }
                        
                        // end - experiemnt
                        
                    }
                    
                    else
                        
                    {
                        if (last_char_width > 0) {
                            
                            // does this test work / needed?
                            // 0.3 seems stable, but 'over-permissive', especially ..
                            // ... with narrow chars like "l" and "i"
                            
                            //printf("update: last_char with path - %d \n", last_char_width);
                            
                            if (-(space_count_num) >= (0.8 * last_char_width)) {
                                
                                if (debug_mode == 1) {
                                    if (page_number == 0 || page_number == 1) {
                                        //printf("update: pdf_parser - space_count_num test passed - %d-%d \n", space_count_num, last_char_width);
                                    }
                                }
                                
                                if (strlen(tmp_str) < 99998) {
                                    strcat(tmp_str, " ");
                                }
                                
                                // experiment
                                
                                if ((font_size_state * tm_scaler) >= 18) {
                                    if (strlen(formatted_text) < 9000) {
                                        strcat(formatted_text," ");
                                    }
                                }
                                
                                if (strlen(table_cell) < 99998) {
                                    strcat(table_cell," ");
                                }
                                
                            }
                        }
                    }
                    
                    
                    total_char_width = 0;
                    bt_char_out_count = 0;
                    
                    space_counter = 0;
                    //space_decimal_stopper = 0;
                    strcpy(space_count,"");
                }
                
                // experiment - capturing table coords
                
                if ( (tml_x > (last_tm_x + 10) && (fabs(tm_y - last_tm_y) <= 2)) || fabs(tm_y - last_tm_y) > 2) {
                    
                    if (found_table_cell_content == 0 | strlen(table_cell) == 0 || strlen(table_cell) > 100) {
                        // no action - skip
                    }
                    else {
                        // one more check -> skip if only spaces
                        
                        table_tmp_go_ahead = 0;
                        for (tab_i=0; tab_i < strlen(table_cell); tab_i ++) {
                            if (table_cell[tab_i] !=32) {
                                table_tmp_go_ahead = 1;
                                break;
                            }
                        }
                        
                        if (table_tmp_go_ahead == 1) {
                            
                                table_tmp_go_ahead = 1;
                                
                                if (table_row > -1) {
                                    for (tab_i=0; tab_i <= table_row; tab_i++) {
                                        
                                        if (fabs(table_row_y[tab_i] - last_tm_y) < 2) {
                                            //printf("update: found table_cell - str ( - from previous row! - %d-%d-%d-%s-%d-%d \n", tab_i,pdf_table[tab_i].col_count, (int)last_tm_y, table_cell, (int)tml_x, (int)last_tm_x);
                                            
                                            //table_copy_col = pdf_table[tab_i].col_count;
                                            //pdf_table[tab_i].col_count ++;
                                            table_tmp_go_ahead = 0;
                                            
                                            strcpy(my_table[tab_i][pdf_table[tab_i].col_count].cell,table_cell);
                                            strcpy(table_cell,"");
                                            //my_table[tab_i][pdf_table[tab_i].col_count].x = (int)tml_x;
                                            my_table[tab_i][pdf_table[tab_i].col_count].x = (int)last_tm_x;
                                            my_table[tab_i][pdf_table[tab_i].col_count].y = (int)table_row_y[tab_i];
                                            pdf_table[tab_i].col_count ++;
                                            pdf_table[tab_i].col_locs[table_col] = (int)tml_x;
                                            table_cell_counter ++;
                                            break;
                                        }
                                    }
                                }
                                
                                if (table_tmp_go_ahead == 1) {
                                    
                                    // no cell match with previous row -> create new row
                                    
                                    if (table_row < 99) {
                                        table_row ++;
                                    }
                                    
                                    table_col = 0;
                                    //printf("update: in str ( -> new row - capturing table cell info - %d-%d-%d-%s-%d-%d-%d \n", table_row, table_col,strlen(table_cell),table_cell, (int)last_tm_y, (int)last_tm_x, (int)tml_x);
                                    
                                    strcpy(my_table[table_row][table_col].cell,table_cell);
                                    strcpy(table_cell,"");
                                    //my_table[table_row][table_col].x = (int)tml_x;
                                    my_table[table_row][table_col].x = (int)last_tm_x;
                                    my_table[table_row][table_col].y = (int)last_tm_y;
                                    pdf_table[table_row].col_count = 1;
                                    pdf_table[table_row].col_locs[table_col] = (int)tml_x;
                                    table_row_y[table_row] = (int)last_tm_y;
                                    table_col ++;
                                    table_cell_counter ++;
                                }
                                }
                                //}
                            }
                    found_table_cell_content = 0;
                }
                    //found_table_cell_content = 0;
            }
            
            // if "<" & no string_on -> hex_on
            if (string_on == 0 && flate_dst_tmp_buffer[y] == 60) {
                hex_on = 1;
                
                //printf("HEX ON found - %d \n", y);
                
                // first test change in (tm_x, tm_y) to evaluate whether to add space
                
                
                text_space_action = evaluate_text_distance(tml_x,tm_y,last_tm_x,last_tm_y,font_size_state,tm_scaler);
                
                // add new test:  if tm_x - last_tm_x > one char size
                // stable @ 0.8 -> experiment, try 1.0
                // add new test: only insert space if last_hex_str_len < 3
                
                if (text_space_action > 0 || (last_hex_str_len < 3) && ((tm_x - last_tm_x) > (0.8 *(float)font_size_state * (float)(fabs(tm_scaler)) ))) {
                    
                    add_hex_space = 1;
                    //strcat(tmp_str, " ");
                    
                    if (debug_mode == 1) {
                        if (page_number >=3 && page_number < 5) {
                            //printf("update: HEX - evaluated text distance - adding space - %f-%f-%f-%f-%f-%d-%f \n", tm_x,tm_y,last_tm_x, last_tm_y, tml_x, font_size_state, tm_scaler);
                        }
                    }
                }
                
                else {
                    add_hex_space = 0;
                }
                
                
                // end - text_space_action test
                
                space_decimal_stopper = 0;
                
                if (space_counter > 0 && bracket_on == 1) {
                    
                    // assess whether to add space
                    space_count_num = atoi(space_count);
                    
                    // conforming hex inside [] with string inside []
                    
                    tml_x = tml_x - (font_size_state * tm_scaler * space_count_num / 1000);
                    
                    // 175 is stable - but misses spaces
                    // experiment - 155
                    
                    if (-(space_count_num) >= 155) {
                        
                        if (strlen(tmp_str) < 99998) {
                            strcat(tmp_str, " ");
                        }
                        
                        if ((font_size_state * tm_scaler) >= 18) {
                            if (strlen(formatted_text) < 9000) {
                                strcat(formatted_text," ");
                            }
                        }
                        
                        // experiment
                        //strcat(table_text_str, " ");
                        
                        if (strlen(table_cell) < 99998) {
                            strcat(table_cell, " ");
                        }
                        
                        // end - experiment
                    }
                    
                    // experiment - oct 29
                    if (space_count_num >= 500 || space_count_num <= -500) {
                        
                        table_count_indicator ++;
                    }
                    
                    // end - experiment
                    
                    space_counter = 0;
                    strcpy(space_count,"");
                }
                
                // experiment - capturing table coords
                
                //printf("update: experiment - start of string ( coords - %d - %d \n", (int)tml_x, (int)tm_y);
                
                // add further check that space gap is greater than 1.5 * font size
                
                if ( (tml_x > (last_tm_x + 10) && ((tml_x - last_tm_x) > 1.5* (float)font_size_state * tm_scaler) && tm_y == last_tm_y) || fabs(tm_y - last_tm_y) > 1) {
                    
                    
                    if (found_table_cell_content == 0 | strlen(table_cell) == 0 || strlen(table_cell) > 100) {
                        // no action - skip
                    }
                    else {
                        // one more check -> skip if only spaces
                        
                        table_tmp_go_ahead = 0;
                        for (tab_i=0; tab_i < strlen(table_cell); tab_i ++) {
                            if (table_cell[tab_i] !=32) {
                                table_tmp_go_ahead = 1;
                                break;
                            }
                        }
                        
                        if (table_tmp_go_ahead == 1) {
                            
                                table_tmp_go_ahead = 1;
                                
                                if (table_row > -1) {
                                    for (tab_i=0; tab_i <= table_row; tab_i++) {
                                        
                                        if (fabs(table_row_y[tab_i] - last_tm_y) < 2) {
                                            
                                            if (page_number < 3) {
                                                //printf("update: found hex table_cell from previous row! - %d-%d-%d-%s \n", tab_i,pdf_table[tab_i].col_count, (int)last_tm_y, table_cell);
                                            }
                                            
                                            //table_copy_col = pdf_table[tab_i].col_count;
                                            //pdf_table[tab_i].col_count ++;
                                            table_tmp_go_ahead = 0;
                                            
                                            strcpy(my_table[tab_i][pdf_table[tab_i].col_count].cell,table_cell);
                                            strcpy(table_cell,"");
                                            //my_table[tab_i][pdf_table[tab_i].col_count].x = (int)tml_x;
                                            my_table[tab_i][pdf_table[tab_i].col_count].x = (int)last_tm_x;
                                            my_table[tab_i][pdf_table[tab_i].col_count].y = (int)table_row_y[tab_i];
                                            pdf_table[tab_i].col_count ++;
                                            pdf_table[tab_i].col_locs[table_col] = (int)tml_x;
                                            table_cell_counter ++;
                                            break;
                                        }
                                    }
                                }
                                
                                if (table_tmp_go_ahead == 1) {
                                    
                                    // no cell match with previous row -> create new row
                                    
                                    if (table_row < 99) {
                                        table_row ++;
                                    }
                                    
                                    table_col = 0;
                                    
                                    if (page_number < 10) {
                                        //printf("update: in hex < -> capturing table cell info - %d-%d-%d-%s-%d \n", table_row, table_col,strlen(table_cell),table_cell, (int)last_tm_y);
                                    }
                                    
                                    strcpy(my_table[table_row][table_col].cell,table_cell);
                                    strcpy(table_cell,"");
                                    //my_table[table_row][table_col].x = (int)tml_x;
                                    my_table[table_row][table_col].x = (int)last_tm_x;
                                    my_table[table_row][table_col].y = (int)last_tm_y;
                                    pdf_table[table_row].col_count = 1;
                                    pdf_table[table_row].col_locs[table_col] = (int)tml_x;
                                    table_row_y[table_row] = (int)last_tm_y;
                                    table_col ++;
                                    table_cell_counter++;
                                }
                                }
                                //}
                            }
                    found_table_cell_content = 0;
                }

                     
            }
            
            // if "[" -> bracket_on
            if (string_on == 0 && flate_dst_tmp_buffer[y] == 91) {
                bracket_on = 1;
                space_counter = 0;
                space_decimal_stopper = 0;
                strcpy(space_count,"");
                
                //printf("BRACKET ON found - %d \n", y);
                
                text_space_action = evaluate_text_distance(tml_x,tm_y,last_tm_x,last_tm_y,font_size_state,tm_scaler);
                
                
                // new test- experiment - mirrors the test from evaluate_text_space
                if (text_space_action > 0 || ( (last_bracket_len <5) && ((tml_x - last_tm_x) > (5* (float)font_size_state * (float) tm_scaler))) ) {
                    
                    /*
                    if (debug_mode == 1) {
                        if (page_number >= 0 && page_number < 2) {
                            printf("update: in [ - adding space - passed text_space_action! \n");
                        }
                    }
                    */
                    
                    strcat(text_str, " ");
                    
                    // experiment
                    //strcat(table_cell," ");
                    //strcat(table_text_str," ");
                    // end - experiment
                    
                }
                
                else {
                    
                    if ( (last_bracket_len < 5) && ((tm_x - last_tm_x) > (0.5 * (float)font_size_state * (float)tm_scaler * 500 / 1000)) ) {
                        
                        if (debug_mode == 1) {
                            if (page_number == 0 || page_number == 1) {
                                //printf("update: in [ - secondary test - inserting space - tml_x-%f-compare-%f \n", tml_x- last_tm_x,(0.25 * (float)font_size_state *tm_scaler));
                            }
                        }
                        
                        if (strlen(tmp_str) < 99998) {
                            strcat(tmp_str, " ");
                        }
                        
                        // experiment
                        //strcat(table_text_str, " ");
                        // end - experiment
                        
                    }
                    
                }
                // update text cursor to current coordinates
                //last_tm_x = tml_x;
            }
            
            // space count search - looking for array of numbers in [ ]
            // need to apply more sophisticated test
            // 250 or similar index following - required
            
            if (bracket_on == 1 && string_on == 0 && hex_on == 0) {
                
                if (flate_dst_tmp_buffer[y] == 45 && space_counter == 0) {
                    sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[y]);
                    strcat(space_count,tm_tmp);
                    space_counter++;
                }
                
                if (flate_dst_tmp_buffer[y] > 47 && flate_dst_tmp_buffer[y] < 58 && space_decimal_stopper == 0) {
                    
                    if (space_counter < 9) {
                        sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[y]);
                        strcat(space_count,tm_tmp);
                        space_counter ++;
                        if (page_number == TEST_PAGE_NUMBER) {
                            //printf("building SPACE COUNT - %s - ", space_count);
                        }
                    }
                }
                if (flate_dst_tmp_buffer[y] == 46) {
                    space_decimal_stopper = 1;
                }
            }
            
            // look for BDC & EMC FLAGS
            
            GLOBAL_ACTUAL_TEXT_FLAG;
            
            if (GLOBAL_ACTUAL_TEXT_FLAG == 2 && string_on ==0 && hex_on ==0 && bracket_on == 0) {
                if (flate_dst_tmp_buffer[y] == 66) {
                    if (y+2 < page_buffer_size) {
                        if (flate_dst_tmp_buffer[y+1] == 68) {
                            if (flate_dst_tmp_buffer[y+2] == 67) {
                                // found 'BDC' flag
                                BDC_FLAG = 1;
                                GLOBAL_ACTUAL_TEXT_FLAG = 0;
                            }
                        }
                    }
                }
            }
            
            if (BDC_FLAG == 1 && string_on ==0 && hex_on ==0 && bracket_on == 0) {
                if (flate_dst_tmp_buffer[y] == 69) {
                    if (y+2 < page_buffer_size) {
                        if (flate_dst_tmp_buffer[y+1] == 77) {
                            if (flate_dst_tmp_buffer[y+2] == 67) {
                                // found 'EMC' flag
                                BDC_FLAG = 0;
                                //GLOBAL_ACTUAL_TEXT_FLAG = 0;
                            }
                        }
                    }
                }
            }
            
            // end - BDC & EMC flags
            
            
            // look for T* and insert space - "move to start of next line"
            
            if (string_on == 0 && hex_on == 0) {
                if (flate_dst_tmp_buffer[y] == 84) {
                    if (y+2 < page_buffer_size) {
                        if (flate_dst_tmp_buffer[y+1] == 42) {
                            if (flate_dst_tmp_buffer[y+2] == 10 || flate_dst_tmp_buffer[y+2] == 32 || flate_dst_tmp_buffer[y+2] == 13) {
                                
                                // found T*
                                
                                // insert space
                                
                                if (strlen(tmp_str) < 99998) {
                                    strcat(tmp_str," ");
                                }
                                
                                // experiment
                                //strcat(table_text_str, " ");
                                
                                //printf("update: found T* - will start new line - no other action taken! \n");
                                
                                /*
                                if (table_row < 100) {
                                    table_row ++;
                                }
                                */
                                
                                //table_count_indicator ++;
                                tm_y = tm_y - (font_size_state * tm_scaler);
                                
                                if (debug_mode == 1) {
                                    //printf("\nupdate: found T* - adjusting tm_y: %f - %f \n", tm_y, (font_size_state * tm_scaler));
                                }
                                
                                // end experiment
                                
                                //td_on = 1;
                            }
                        }
                    }
                }
            }
            
            // looking for "Td" / "TD" -> update text state
            
            if (string_on == 0 && hex_on == 0) {
                if (flate_dst_tmp_buffer[y] == 84) {
                    if (y+2 < page_buffer_size) {
                        if (flate_dst_tmp_buffer[y+1] == 68 || flate_dst_tmp_buffer[y+1] == 100) {
                            
                            // 40 is acceptable separator, e.g., Td(
                            
                            if (flate_dst_tmp_buffer[y+2] == 10 || flate_dst_tmp_buffer[y+2] == 32 || flate_dst_tmp_buffer[y+2] == 13 || flate_dst_tmp_buffer[y+2] == 40 || flate_dst_tmp_buffer[y+2] == 91) {
                                
                                // confirmed - found 'Td ' or 'TD '
                                
                                ss=0;
                                strcpy(tm,"");
                                
                                // safety check for look-back - do not extend past 0 index of page_buffer
                                
                                if (y < 20) {
                                    s_start = 0;
                                }
                                else {
                                    s_start = y-20;
                                }
                                
                                for (s=s_start; s < y; s++) {
                                    sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[s]);
                                    strcat(tm,tm_tmp);
                                }
                                
                                //  get the text state for current text
                                text_state_tm = get_text_state_from_td(tm,20);
                                
                                
                                // TD/Td adjusts the existing text state by adding new params
                                
                                // inserting font_size_state - oct 2
                                
                                //printf("update: in TD - coords - %f - %f - %f - %f - %f \n", tm_x, text_state_tm->x, tm_y, text_state_tm->y, tm_scaler);
                                
                                // why problems with font_size_state ?
                                
                                if (debug_mode == 1) {
                                    if (page_number == 0) {
                                        //printf("update: TD coords found - %f - %f -%f \n", text_state_tm->x, text_state_tm->y, tm_scaler);
                                    }
                                }
                                
                                // experiment - include font_size_state
                                tm_x = tm_x + (tm_scaler * text_state_tm->x);
                                
                                if (debug_mode == 1) {
                                    if (page_number <2) {
                                        //printf("update: in TD - tml_x - %f - %f - %f \n", tml_x, last_tml_x, tm_x);
                                    }
                                }
                                
                                //printf("update - line 8978- SETTING TML_X in TD - %f - %f \n", tml_x, tm_x);
                                
                                tml_x = tm_x;
                                
                                tm_y = tm_y + (tm_scaler * text_state_tm->y);
                                
                                if (debug_mode == 1) {
                                    
                                    if (page_number == 0 || page_number == 1) {
                                        //printf("update: in TD - new coords - %f - %f - %f - %f \n", tm_x, tm_y, last_tm_y, (tm_scaler * text_state_tm->y));
                                    }
                                }
                                
                                
                                // start experiment - table count indicator
                                
                                // add check that td created gap is greater than font_size * 1.5 as min
                                if (((tm_scaler * text_state_tm->x) > 10) && (text_state_tm->x > (1.5*(float)font_size_state)) && (text_state_tm->y == 0 || fabs(tm_y - last_tm_y) < 1)) {
                                    
                                    // two tests for y movement -
                                    //      1.   usual case - td y adjustment = 0
                                    //      2.   ununusal case - each BT has only one element
                                    //              -- at top of each BT, coords reset (0,0)
                                    //              -- Td used to create new (x,y)
                                    //              -- the new (x,y) is a small deviation from last
                                    
                                    // indicator only applies if more than one char written
                                    
                                    table_count_indicator ++;
                                    
                                    
                                    // adjusting strlen(table_cell) test - must be more than 1 char
                                    // previous test: only confirmed if not empty
                                    if (found_table_cell_content == 0 | strlen(table_cell) == 0 || strlen(table_cell) > 100) {
                                        // no action - skip
                                    }
                                    else {
                                        // one more check -> skip if only spaces
                                        
                                        table_tmp_go_ahead = 0;
                                        for (tab_i=0; tab_i < strlen(table_cell); tab_i ++) {
                                            if (table_cell[tab_i] !=32) {
                                                table_tmp_go_ahead = 1;
                                                break;
                                            }
                                        }
                                        
                                        if (table_tmp_go_ahead == 1) {
                                           
                                                table_tmp_go_ahead = 1;
                                                
                                                if (table_row > -1) {
                                                    for (tab_i=0; tab_i <= table_row; tab_i++) {
                                                        
                                                        if (fabs(table_row_y[tab_i] - last_tm_y) < 2) {
                                                            //printf("update: found Td table_cell from previous row! - %d-%d-%d-%s-%d \n", tab_i,pdf_table[tab_i].col_count, (int)last_tm_y, table_cell, (int)last_tm_x);
                                                            
                                                            //table_copy_col = pdf_table[tab_i].col_count;
                                                            //pdf_table[tab_i].col_count ++;
                                                            table_tmp_go_ahead = 0;
                                                            
                                                            strcpy(my_table[tab_i][pdf_table[tab_i].col_count].cell,table_cell);
                                                            strcpy(table_cell,"");
                                                            //my_table[tab_i][pdf_table[tab_i].col_count].x = (int)tml_x;
                                                            my_table[tab_i][pdf_table[tab_i].col_count].x = (int) last_tm_x;
                                                            my_table[tab_i][pdf_table[tab_i].col_count].y = table_row_y[tab_i];
                                                            pdf_table[tab_i].col_count ++;
                                                            pdf_table[tab_i].col_locs[table_col] = (int)tml_x;
                                                            table_cell_counter ++;
                                                            
                                                            break;
                                                            
                                                        }
                                                    }
                                                }
                                                
                                                if (table_tmp_go_ahead == 1) {
                                                    
                                                    // no cell match with previous row -> create new row
                                                    
                                                    if (table_row < 99) {
                                                        table_row ++;
                                                    }
                                                    
                                                    table_col = 0;
                                                    //printf("update: in Td -> new row - capturing table cell info - %d-%d-%d-%s-%d-%d \n", table_row, table_col,strlen(table_cell),table_cell, (int)last_tm_y, (int)last_tm_x);
                                                    
                                                    strcpy(my_table[table_row][table_col].cell,table_cell);
                                                    strcpy(table_cell,"");
                                                    //my_table[table_row][table_col].x = (int)tml_x;
                                                    my_table[table_row][table_col].x = (int)last_tm_x;
                                                    
                                                    my_table[table_row][table_col].y = (int)last_tm_y;
                                                    pdf_table[table_row].col_count = 1;
                                                    pdf_table[table_row].col_locs[table_col] = (int)tml_x;
                                                    table_row_y[table_row] = (int)last_tm_y;
                                                    table_col ++;
                                                    table_cell_counter ++;
                                                }
                                                }
                                                }
                                            //}
                                    found_table_cell_content = 0;
                                }
                                // update last here
                                
                            }}}}}
            
            
            // look for Tm window to set text_state
            
            if (string_on == 0 && hex_on == 0 && flate_dst_tmp_buffer[y] == 84) {
                
                if (y+1 < page_buffer_size) {
                    if (flate_dst_tmp_buffer[y+1] == 109) {
                        
                        ss=0;
                        strcpy(tm,"");
                        
                        // add safety check for look-back - do not extend past 0 index of page_buffer
                        
                        if (y < 50) {
                            s_start = 0;
                        }
                        else {
                            s_start = y-50;
                        }
                        
                        //  Currently increased to 50 -but may need further evaluation
                        
                        for (s=s_start; s < y; s++) {
                            //if (page_buffer[s] !=10 && page_buffer[s] != 13) {
                            sprintf(tm_tmp,"%c",flate_dst_tmp_buffer[s]);
                            strcat(tm,tm_tmp);
                            //}
                            
                        }
                        
                        
                        // save current tm_x / tm_y / tm_scaler as 'last'
                        //last_tm_x = tm_x;
                        //last_tm_y = tm_y;
                        //last_tm_scaler = tm_scaler;
                        
                        //  get the text state for current text
                        text_state_tm = get_text_state(tm,50);
                        
                        // update the text_state with the new location parameters
                        tm_x = text_state_tm->x;
                        tml_x = tm_x;
                        tm_y = text_state_tm->y;
                        
                        // start experiment - table count indicator
                        /*
                        if (debug_mode == 1) {
                            if (page_number == 1) {
                                printf("update: in TM setting coords - tm_y - %f - last_tm_y - %f  - tml_x - %f - last_tm_x - %f - %f - %f \n", tm_y,last_tm_y, tml_x, last_tm_x,
                                       (float)font_size_state, (float)tm_scaler);
                            }}
                        */
                        
                        if ((int)tm_y == (int)last_tm_y && (tm_x > last_tm_x + 10) || first_coords_set == -1) {
                            
                            table_count_indicator ++;
                            
                            if (first_coords_set == -1) {

                                last_tm_x = tm_x;
                                
                                if (debug_mode == 1) {
                                    if (page_number == 17) {
                                        //printf("update: in TM table coords- first_coords_set - setting last_tm_x - %f \n", last_tm_x);
                                    }
                                }
                                
                                last_tm_y = tm_y;
                                first_coords_set = 1;
                            }
                            
                            
                            if (found_table_cell_content == 0 | strlen(table_cell) == 0 || strlen(table_cell) > 100) {
                                // no action - skip
                            }
                            else {
                                // one more check -> skip if only spaces
                                
                                table_tmp_go_ahead = 0;
                                for (tab_i=0; tab_i < strlen(table_cell); tab_i ++) {
                                    if (table_cell[tab_i] !=32) {
                                        table_tmp_go_ahead = 1;
                                        break;
                                    }
                                }
                                
                                if (table_tmp_go_ahead == 1) {
                                    
                                        // different row -> need to check if it maps to any existing row
                                        // if no existing row -> create new row
                                        
                                        table_tmp_go_ahead = 1;
                                        
                                        if (table_row > -1) {
                                            for (tab_i=0; tab_i <= table_row; tab_i++) {
                                                
                                                if (fabs(table_row_y[tab_i] - last_tm_y) < 2) {
                                                    
                                                    if (debug_mode == 1) {
                                                        //printf("update: found table_cell - TM - from previous row! - %d-%d-%d-%s \n", tab_i,pdf_table[tab_i].col_count, (int)last_tm_y, table_cell);
                                                    }
                                                    
                                                    table_tmp_go_ahead = 0;
                                                    
                                                    strcpy(my_table[tab_i][pdf_table[tab_i].col_count].cell,table_cell);
                                                    strcpy(table_cell,"");
                                                    
                                                    my_table[tab_i][pdf_table[tab_i].col_count].x = (int) last_tm_x;
                                                    
                                                    my_table[tab_i][pdf_table[tab_i].col_count].y = (int)table_row_y[tab_i];
                                                    pdf_table[tab_i].col_count ++;
                                                    pdf_table[tab_i].col_locs[table_col] = (int)tml_x;
                                                    
                                                    table_cell_counter ++;
                                                    
                                                    // insert space in tmp_str on writing cell
                                                    //strcat(tmp_str," ");
                                                    
                                                    break;
                                                }
                                            }
                                        }
                                        
                                        if (table_tmp_go_ahead == 1) {
                                            
                                            // no cell match with previous row -> create new row
                                            
                                            if (table_row < 99) {
                                                table_row ++;
                                            }
                                            
                                            table_col = 0;
                                            
                                            if (debug_mode == 1) {
                                                //printf("update: in Tm -> capturing table cell info - %d-%d-%d-%s-%d \n", table_row, table_col,strlen(table_cell),table_cell, (int)last_tm_y);
                                            }
                                            
                                            strcpy(my_table[table_row][table_col].cell,table_cell);
                                            strcpy(table_cell,"");
                                            
                                            //my_table[table_row][table_col].x = (int)tml_x;
                                            my_table[table_row][table_col].x = (int)last_tm_x;

                                            my_table[table_row][table_col].y = (int)last_tm_y;
                                            pdf_table[table_row].col_count = 1;
                                            pdf_table[table_row].col_locs[table_col] = (int)tml_x;
                                            table_row_y[table_row] = (int)last_tm_y;
                                            table_col ++;
                                            
                                            table_cell_counter ++;
                                            
                                            if (strlen(tmp_str) < 99998) {
                                                strcat(tmp_str, " ");
                                            }
                                            
                                            if ((font_size_state * tm_scaler) >= 18) {
                                                
                                                if (strlen(formatted_text) < 9000) {
                                                    strcat(formatted_text," ");
                                                }
                                            }
                                            
                                        }
                                        }
                                        
                                    }
                            found_table_cell_content = 0;
                        }
                        
                        tm_scaler = fabs(text_state_tm->scaler);
                        
                        if (page_number == 1) {
                            //printf("update: tm_scaler - %f \n", tm_scaler);
                        }
                    }
                }
            }
            
            
            // look for Tf window to identify font & apply cmap if needed
            if (string_on == 0 && hex_on == 0 && flate_dst_tmp_buffer[y] == 84) {
                
                if (y+1 < page_buffer_size) {
                    if (flate_dst_tmp_buffer[y+1] == 102) {
                        
                        // found "Tf" seq- need to look back to get the font name
                        //printf("Found Tf window-%d \n", x);
                        
                        ss=0;
                        fs=0;
                        strcpy(tf_window_tmp,"");
                        font_size_on = 0;
                        font_name_on = 0;
                        
                        if (y < 25) {
                            s_start = 0;
                        }
                        else {
                            s_start = y-25;
                        }
                        
                        s_start_new = s_start;
                        
                        for (s= y-1; s >= s_start; s--) {
                            
                            if (flate_dst_tmp_buffer[s] == 47) {
                                // stop lookback here - at first / found
                                s_start_new = s;
                                break;
                            }
                        }
                        
                        for (s= s_start_new; s <y; s++) {
                            
                            if (flate_dst_tmp_buffer[s] != 10 && flate_dst_tmp_buffer[s] != 13) {
                                sprintf(tf_tmp,"%c",flate_dst_tmp_buffer[s]);
                                strcat(tf_window_tmp,tf_tmp);
                            }
                            
                            if (font_name_on == 1) {
                                if (flate_dst_tmp_buffer[s] == 32 || flate_dst_tmp_buffer[s] == 10 || flate_dst_tmp_buffer[s] == 13) {
                                    font_name_on = 2;
                                    strcpy(font_apply[text_box_count],my_font);
                                }
                                else
                                {
                                    my_font[ss] = flate_dst_tmp_buffer[s];
                                    ss++;
                                }}
                            
                            if (font_size_on == 1) {
                                
                                // stop at decimal point '.'
                                if (flate_dst_tmp_buffer[s] == 32 || flate_dst_tmp_buffer[s] == 46 || flate_dst_tmp_buffer[s] == 10 || flate_dst_tmp_buffer[s] == 13)
                                
                                {
                                    font_size_on = 2;
                                    my_font_size[fs] = 0;
                                    last_font_size_state = font_size_state;
                                    
                                    // set font_size_state
                                    font_size_state = get_int_from_byte_array(my_font_size);
                                    
                                    // safety check - if no size found, then set @ 1 by default
                                    
                                    if (font_size_state == 0) {
                                        font_size_state = 1;
                                    }
                                    
                                    // if font_size_state == 1 -> apply tm_scaler
                                    // otherwise, reset tm_scaler -> 1
                                    
                                    if (font_size_state > 1) {
                                        tm_scaler = 1;
                                        
                                    }
                                    
                                    //font_size_state = font_size_state * (int)tm_scaler;
                                    
                                    fs=0;
                                }
                            }
                            
                            if (flate_dst_tmp_buffer[s] > 47 && flate_dst_tmp_buffer[s] < 58 && font_name_on == 2) {
                                
                                if (font_size_on < 2) {
                                    
                                    font_size_on = 1;
                                    my_font_size[fs] = flate_dst_tmp_buffer[s];
                                    fs++;
                                }}
                            
                            if (flate_dst_tmp_buffer[s] == 47) {
                                
                                //font name will start with /
                                
                                ss=0;
                                my_font[ss] = flate_dst_tmp_buffer[s];
                                font_name_on = 1;
                                ss++;
                            }
                        }
                        
                        tmp_str2 = get_string_from_byte_array(my_font,ss);
                        
                        //my_cmap = -1;
                        
                        for (z=0; z < global_font_count; z++) {
                            
                            if (strcmp(Font_CMAP[z].font_name,tmp_str2) == 0) {
                                
                                if (page_number == 0) {
                                    //printf("FOUND MATCHING FONT NAME -%d- %s ! \n", z, Font_CMAP[z].font_name);
                                }
                                
                                for (p=0; p < Font_CMAP[z].page_apply_counter; p++) {
                                    
                                    // need to test with many docs whether pages capture is +1
                                    if (Font_CMAP[z].pages[p] == page_number ) {
                                        
                                        my_cmap = z;
                                        
                                        // very useful check if Font loaded correctly
                                        
                                        if ((font_size_state * tm_scaler) >= 18) {
                                            
                                            if (debug_mode == 1) {
                                                //printf("update: found large font - %f \n", font_size_state * tm_scaler);
                                            }
                                        }
                                        
                                        if ((font_size_state * tm_scaler) <= 8) {
                                            if (debug_mode == 1) {
                                                //printf("update: found **small** font - %f \n", font_size_state * tm_scaler);
                                            }
                                        }
                                        
                                        // FONT_FINDER
                                        /*
                                        if (debug_mode == 1) {
                                            if (page_number < 3) {
                                                printf("\nTEXT PROCESSOR - Found matching font-%d-%s-CMAP val- %d \n", z,tmp_str2,Font_CMAP[z].cmap_apply);
                                            }
                                        }
                                        */
                                        
                                        break;
                                    }
                                }
                            }}
                        
                    }}}
            
            if (string_on == 1) {
                
                if (flate_dst_tmp_buffer[y] > 128) {
                    //printf("WOW - page_buffer[y] in string = %d \n", page_buffer[y]);
                }
                
                // exclude (), <> unless escape_on and then special handling
                
                // deprecating and removing:   page_buffer[y] == 93 && bracket_on == 0
                
                if ((escape_on == 1 && escape_count == 1) || (flate_dst_tmp_buffer[y] != 40 && flate_dst_tmp_buffer[y] != 41 && flate_dst_tmp_buffer[y] !=93 && flate_dst_tmp_buffer[y] !=92) ||
                    (flate_dst_tmp_buffer[y] == 93 || flate_dst_tmp_buffer[y] == 91)) {
                    
                    // 992 is plug that should be replaced - just used to guarantee that if condition passes
                    
                    if (flate_dst_tmp_buffer[y] != 992) {
                        
                        tmp_char = flate_dst_tmp_buffer[y];
                        
                        // default - skip over char unless identified special escape seq
                        escape_keep = -1;
                        
                        
                        // main subloop - if escape_on -> invoke special handling
                        
                        if (escape_on == 1) {
                            
                            if (flate_dst_tmp_buffer[y] == 114)
                            { tmp_char = 13;
                                escape_keep = 1;
                            }
                            
                            if (flate_dst_tmp_buffer[y] == 110)
                            {
                                tmp_char = 10;
                                escape_keep = 1;
                                
                            }
                            
                            // if \t -> interpret as ASCII 9
                            if (flate_dst_tmp_buffer[y] == 116)
                            {
                                tmp_char = 9;
                                escape_keep = 1;
                                
                            }
                            
                            // if \f -> interpret as ASCII 12
                            if (flate_dst_tmp_buffer[y] == 102)
                            {
                                tmp_char = 12;
                                escape_keep = 1;
                                
                            }
                            
                            if (flate_dst_tmp_buffer[y] == 92)
                            {
                                tmp_char = 92;
                                escape_keep = 1;
                            }
                            
                            if (flate_dst_tmp_buffer[y] == 40)
                            {
                                tmp_char = 40;
                                escape_keep = 1;
                                //printf("***40***");
                                
                            }
                            
                            if (flate_dst_tmp_buffer[y] == 41)
                            {
                                tmp_char = 41;
                                escape_keep = 1;
                                //printf("***41***");
                            }
                            
                            // if \b -> interpret as ASCII 8
                            if (flate_dst_tmp_buffer[y] == 98)
                            {
                                tmp_char = 8;
                                escape_keep = 1;
                            }
                            
                            // look for Octal 3 digit codes - all numbers
                            
                            if (flate_dst_tmp_buffer[y] > 47 && flate_dst_tmp_buffer[y] < 58) {
                                
                                escape_keep = 1;
                                
                                if (y+2 < page_buffer_size) {
                                    
                                    /*
                                    if (debug_mode == 1) {
                                        if (page_number < 10) {
                                            printf("update: FOUND OCTAL CODE - %d - %d - %d -%d \n", flate_dst_tmp_buffer[y], flate_dst_tmp_buffer[y+1],flate_dst_tmp_buffer[y+2], my_cmap);
                                        }
                                    }
                                     */
                                    
                                    oct1 = flate_dst_tmp_buffer[y] - 48;
                                    oct2 = flate_dst_tmp_buffer[y+1] - 48;
                                    oct3 = flate_dst_tmp_buffer[y+2] - 48;
                                    octal_count = oct1*64 + oct2*8 + oct3;
                            
                                    tmp_char = octal_count;
                                    
                                    
                                    // every time invoke cmap_get_char -> increment char_count
                                    
                                    new_char = cmap_get_char(tmp_char,my_cmap);
                                    
                                    if (debug_mode == 1) {
                                        if (page_number == 2) {
                                            //printf("OCTAL: %d %d %d - ", new_char, tmp_char, my_cmap);
                                        }}
                                    
                                    // change inserted on June 28, 2022
                                    // insert increment to bt_char_out_count
                                    bt_char_out_count ++;
                                    
                                    if (new_char == -1) {
                                        
                                        if (Font_CMAP[my_cmap].standard_font_identifier == -99) {
                                            // do nothing - keep new_char = -1
                                        }
                                        else {
                                            // default case - 99.999% of the time
                                            // if no encoding found, go with tmp_char (assume ASCII)
                                            
                                            new_char = tmp_char;
                                        }
                                        
                                        // printf("error: octal encoding missed - %d - %d \n", new_char,tmp_char);
                                        
                                        if (tmp_char == 160) {
                                            // handle special formatting character A0
                                            new_char = 32;
                                        }
                                        
                                        // special token for 'fi' -> recognized in Std/Mac/PDF encoding
                                        
                                        // MacRomandEncoding = 1
                                        if (Font_CMAP[my_cmap].standard_font_identifier == 1) {
                                            
                                            if (tmp_char == 222) {
                                                new_char = 64257;
                                            }
                                            if (tmp_char == 223) {
                                                new_char = 64258;
                                            }
                                        }
                                        
                                        if (Font_CMAP[my_cmap].standard_font_identifier == 2) {
                                            
                                            if (tmp_char == 147) {
                                                new_char = 64257;
                                            }
                                            
                                            if (tmp_char == 148) {
                                                new_char = 64258;
                                            }
                                        }
                                        
                                        if (Font_CMAP[my_cmap].standard_font_identifier == 3) {
                                            
                                            if (tmp_char == 174) {
                                                new_char = 64257;
                                            }
                                            
                                            if (tmp_char == 175) {
                                                new_char = 64258;
                                            }
                                        }
                                        
                                    }
                                    
                                    // end change
                                    
                                    // new -insert - needs testing - picks up encodings
                                    
                                    if (new_char > 127 && (Font_CMAP[my_cmap].standard_font_identifier == 1 || Font_CMAP[my_cmap].standard_font_identifier == 9)) {
                                        
                                        new_char = standard_encoding_variances_handler(new_char, Font_CMAP[my_cmap].standard_font_identifier);
                                    }
                                    
                                    // end - new - insert
                                    
                                    if (new_char > -1) {
                                        char_str_ptr = char_special_handler_string(new_char, escape_on);
                                        
                                        // add len safety check - unusual case
                                        if (strlen(tmp_str) < 99998) {
                                            strcat(tmp_str, char_str_ptr);
                                        }
                                    }
                                    else {
                                        char_str_ptr = "";
                                    }
                                    
                                    // experiment - oct 29
                                    //strcat (table_text_tmp, char_str_ptr);
                                    
                                    // only register found_table_cell_content if not empty and not space
                                    if (strlen(char_str_ptr) > 0) {
                                        if (char_str_ptr != 32) {
                                            found_table_cell_content = 1;
                                            //printf("update: found_table_cell_content set- %d-%d-%s \n", found_table_cell_content, strlen(char_str_ptr), char_str_ptr);
                                        }
                                    }
                                    
                                    //found_table_cell_content = 1;
                                    
                                    // add len safety check - unusual edge case
                                    if (strlen(table_cell) < 99998) {
                                        strcat (table_cell, char_str_ptr);
                                    }
                                    // end - experiment
                                    
                                    // if large text -> add to formatted_text
                                    
                                    if ((font_size_state * tm_scaler) >= 18) {
                                        
                                        if (fabs(tm_y - ft_y) < 1) {
                                            // don't insert space if on same line
                                            strcat(formatted_text, char_str_ptr);
                                        }
                                        else {
                                            // if different line, insert space
                                            
                                            if (strlen(formatted_text) < 9000) {
                                                strcat(formatted_text, " ");
                                                strcat(formatted_text, char_str_ptr);
                                            }
                                        }
                                        
                                        ft_y = tm_y;
                                        ft_x = tml_x;
                                        
                                    }
                                        
                                    // end - large text handler
                                    
                                    if (strlen(char_str_ptr) > 0) {
                                        
                                        // apply width + add to tm_x state
                                        
                                        if (my_cmap >= 0) {
                                            last_char_width = Font_CMAP[my_cmap].widths[tmp_char];
                                        }
                                        else {
                                            last_char_width = 500;
                                        }
                                        
                                        last_char_text_adder = (last_char_width * font_size_state * tm_scaler / 1000);
                                        
                                        total_char_width = total_char_width + last_char_text_adder;
                                        
                                        tml_x = tml_x + last_char_text_adder ;
                                        
                                    }
                                    
                                    // end - last_char_width
                                    
                                    
                                    
                                }
                                
                                cursor = y+3;
                            }
                            
                            //escape_on =0;
                        }
                        
                        
                        if (y >= cursor) {
                            
                            
                            // edge case -> check if 'skipped' escape char
                            if (escape_on == 1 && escape_keep == -1) {
                                new_char = -2;
                            }
                            else {
                                // default case -> get char
                                new_char = cmap_get_char(tmp_char,my_cmap);
                                
                                if (page_number == 2) {
                                    //printf("GETTING NORMAL STR CHAR: %c %d %d %d -", new_char, new_char, tmp_char,my_cmap);
                                }
                                
                                if (debug_mode == 1) {
                                    if (page_number == 1 || page_number == 2) {
                                        //printf("update: my_cmap - %d - tmp_char - %d - new_char - %c \n", my_cmap, tmp_char, new_char);
                                    }
                                }
                                
                            }
                            
                            
                            // change - increment bt_char_out_count
                            bt_char_out_count ++;
                            // end - change
                            
                            if (new_char == -1) {
                                
                                if (Font_CMAP[my_cmap].standard_font_identifier == -99)
                                {
                                    // special case - keep tmp_char as -1
                                    if (debug_mode == 1) {
                                        //printf("update: processing text - found Font Type 3 with -1 skipping \n");
                                    }
                                }
                                else {
                                    new_char = tmp_char;
                                }
                                
                                if (tmp_char == 160) {
                                    // handle special formatting character A0
                                    new_char = 32;
                                }
                                
                                // MacRomandEncoding = 1
                                if (Font_CMAP[my_cmap].standard_font_identifier == 1) {
                                    
                                    //printf("update: found char & Mac Encoding - %d \n", tmp_char);
                                    
                                    if (tmp_char == 222) {
                                        new_char = 64257;
                                    }
                                    if (tmp_char == 223) {
                                        new_char = 64258;
                                    }
                                }
                                
                                if (Font_CMAP[my_cmap].standard_font_identifier == 2) {
                                    
                                    if (tmp_char == 147) {
                                        new_char = 64257;
                                    }
                                    
                                    if (tmp_char == 148) {
                                        new_char = 64258;
                                    }
                                }
                                
                                if (Font_CMAP[my_cmap].standard_font_identifier == 3) {
                                    
                                    if (tmp_char == 174) {
                                        new_char = 64257;
                                    }
                                    
                                    if (tmp_char == 175) {
                                        new_char = 64258;
                                    }
                                }
                                
                                
                            }
                            // new -insert - needs testing - picks up encodings
                            
                            if (new_char > 127 && (Font_CMAP[my_cmap].standard_font_identifier == 1 || Font_CMAP[my_cmap].standard_font_identifier == 9)) {
                                
                                new_char = standard_encoding_variances_handler(new_char, Font_CMAP[my_cmap].standard_font_identifier);
                            }
                            
                            // end - new - insert
                            
                            if (new_char == -1) {
                                
                                if (debug_mode == 1) {
                                    //printf("update: new_char == -1");
                                }
                                
                                if (Font_CMAP[my_cmap].standard_font_identifier == -99) {
                                    // special case - Font Type 3 - skip
                                    char_str_ptr = "";
                                }
                            }
                            
                            else {
                                
                                char_str_ptr = char_special_handler_string(new_char,escape_on);
                                //printf("char_str_ptr_out - %d-%s \n", new_char, char_str_ptr);
                                
                                // add len safety check - edge case
                                if (strlen(tmp_str) < 99998) {
                                    strcat(tmp_str,char_str_ptr);
                                }
                                
                                // experiment - oct 29
                                //strcat(table_text_tmp,char_str_ptr);
                                
                                if (strlen(table_cell) < 99998) {
                                    strcat(table_cell, char_str_ptr);
                                }
                                // end - experiment
                                
                                // capture larger font text
                                if ((font_size_state * tm_scaler) >= 18) {
                                    
                                    if (fabs(tm_y - ft_y) < 1) {
                                        
                                        if (strlen(formatted_text) < 9000) {
                                            strcat(formatted_text, char_str_ptr);
                                        }
                                    }
                                    else {
                                        if (strlen(formatted_text) < 9000) {
                                            strcat(formatted_text, " ");
                                            strcat(formatted_text, char_str_ptr);
                                        }
                                    }
                                    
                                    ft_y = tm_y;
                                    ft_x = tml_x;

                                }
                                // end - capture larger font text
                            }
                            
                            if (strlen(char_str_ptr) > 0) {
                                
                                // new - get last_char_width
                                
                                if (my_cmap >= 0) {
                                    last_char_width = Font_CMAP[my_cmap].widths[tmp_char];
                                }
                                else {
                                    last_char_width = 500;
                                }
                                
                                // kludge
                                /*
                                if (font_size_state == 1 || tm_scaler == 0) {
                                    font_size_state = 12;
                                    tm_scaler = 1;
                                }
                                 */
                                // end - kludge
                                
                                // kludge - safety - if last_char_width = 0, keep space count more accurate?
                                
                                
                                last_char_text_adder = (font_size_state * tm_scaler * last_char_width / 1000);
                                
                                total_char_width = total_char_width + last_char_text_adder;
                                
                                tml_x = tml_x + last_char_text_adder;
                                
                                if (page_number == 1 ) {
                                    //printf("update: new char - last_char_text_adder - %f - %f - %f - %d - %d \n", last_char_text_adder, (float)font_size_state, (float)tm_scaler, last_char_width, my_cmap);
                                }
                                
                                // end - last_char_width
                                
                                if (page_number == 1) {
                                    //printf("update: normal char add - %d-%d-%s-%f-%f \n", tmp_char,new_char, tmp_str,last_char_text_adder,tml_x);
                                    
                                }
                            }
                            
                        }
                    }
                }}
            
            else {
                // useful for testing - do nothing on else branch

            }
            
            
            if (hex_on == 1 && flate_dst_tmp_buffer[y] != 60 && flate_dst_tmp_buffer[y] != 62) {
                
                //sprintf(tmp, "%c",new_char);
                // exclude non A-F and non 0-9
                
                if (flate_dst_tmp_buffer[y] > 46 && flate_dst_tmp_buffer[y] < 123 && escape_on == 0) {
                    sprintf(tmp, "%c", flate_dst_tmp_buffer[y]);
                    
                    // add len safety check
                    if (strlen(hex_tmp_str) < 99998) {
                        strcat(hex_tmp_str,tmp);
                        hex_len ++;
                    }
                    
                }
                else {
                    // do nothing

                }}
            
            
            // found ')' -> signals end of string if string_on = 1
            
            if (string_on == 1 && flate_dst_tmp_buffer[y] == 41 && escape_on == 0) {
                
                string_on = 0;
                
                // at end of string ) -> need to save text coordinates
                
                last_tm_x = tml_x;
                last_tm_y = tm_y;
                
                last_char_str_len = strlen(tmp_str);
                
                /*
                if (debug_mode == 1) {
                    if (page_number == 0 || page_number == 1) {
                        //printf("update: last_char_str_len - %s - %d \n", tmp_str, strlen(tmp_str));
                    }
                }
                */
                
                if (debug_mode == 1) {
                    if (page_number == 17) {
                        //printf("update: @ end of ) - setting last_tm_x - %f - last_tm_y -  %f - %s \n", last_tm_x, last_tm_y, tmp_str);
                    }}
                
                // end of save text coordinates
                
                // experiment - oct 29 start
                
            
                if (strlen(table_cell) > 0) {
                    found_table_cell_content = 1;
                    
                    //strcat(table_text_str,table_text_tmp);
 
                    //strcpy(table_text_tmp,"");
                    //found_table_cell_content = 1;
                }
        
                
                // end - experiment
                
                
                // if no bracket -> look immediately for Tj/TJ to keep text
                
                if (bracket_on == 0) {
                    // look for Tj to keep tmp_text
                    tj_look_ahead = 5;
                    
                    found_tj = -1;
                    for (t=1; t < 1+tj_look_ahead; t++) {
                        if (flate_dst_tmp_buffer[t+y] == 84) {
                            if (t+1 < tj_look_ahead) {
                                if (flate_dst_tmp_buffer[y+t+1] == 74 || flate_dst_tmp_buffer[y+t+1] == 106) {
                                    if (page_number < 20) {
                                        //printf("TJ FORMAT- %d-%d \n", page_buffer[y+t+1],page_buffer[y+t+2]);
                                    }
                                    found_tj = 1;
                                    break;
                                }
                                
                                // look for T* independently
                                /*
                                 if (page_buffer[y+t+1] == 42) {
                                 found_tj = 2;
                                 break;
                                 }
                                 */
                            }
                        }
                        
                        if (flate_dst_tmp_buffer[y+t] == 34) {
                            found_tj = 2;
                            //printf("update:  unusual formatting - TJ FORMAT - 34 found \n");
                            break;
                        }
                        if (flate_dst_tmp_buffer[y+t] == 39) {
                            found_tj = 2;
                            //printf("update:  unusual formatting - TJ FORMAT - 39 found \n");
                            break;
                        }
                    }
                    
                    // KEY STEP -> found TJ -> save tmp_str text
                    
                    if (found_tj > 0 && BDC_FLAG == 0) {
                        
                        // evaluate if any special actions need to be taken based on text state
                        
                        if (text_written_in_bt < 0) {
                            //note : this is the first text written from this BT
                            // e.g., 'last' = 'current'
                            text_written_in_bt = 1;
                            text_space_action = 0;
                        }
                        
                        else {
                            //do nothing
                        }
                        
                        // save current text coords in 'last_tm'
                        
                        
                        last_tm_x = tml_x;
                        last_tm_y = tm_y;
                        
                        last_tml_x = tml_x;
                    
                        // add to text_str for saving
                        
                        // add len safety check - text_str max len = 500K
                        if ((strlen(text_str) + strlen(tmp_str)) < 499998) {
                            strcat(text_str,tmp_str);
                            
                            
                            if (debug_mode == 1) {
                                if (page_number < 10) {
                                    //printf("update: tmp_str_out - %s \n", tmp_str);
                                }
                            }
                            
                            
                        }
                        
                        
                        // reset tmp_str
                        strcpy(tmp_str,"");
                        
                        if (found_tj == 2) {
                            strcat(text_str, " ");
                            //printf("update - unusual formatting - found tj=2 - adding space \n");
                        }
                        
                    }
                    else {
                        // unusual case- but if fail TJ or BDC test, then reset tmp_str
                        strcpy(tmp_str,"");
                    }
                    
                    
                    
                }}
            
            
            
            // key action: found ']' -> likely end of string
            // should not be inside ")" , e.g., string_on = 0
            
            if (flate_dst_tmp_buffer[y] == 93) {
                if (string_on == 0 && bracket_on == 1) {
                    bracket_on = 0;
                    
                    tj_look_ahead = 5;
                    found_tj = -1;
                    
                    for (t=1; t < tj_look_ahead+1; t++) {
                        
                        if (flate_dst_tmp_buffer[t+y] == 84) {
                            if (t+1 < tj_look_ahead) {
                                if (flate_dst_tmp_buffer[y+t+1] == 74 || flate_dst_tmp_buffer[y+t+1] == 106) {
                                    found_tj = 1;
                                    break;
                                }
                            }
                        }
                        
                        if (flate_dst_tmp_buffer[y+t] == 34) {
                            found_tj = 2;
                            break;
                        }
                        if (flate_dst_tmp_buffer[y+t] == 39) {
                            found_tj = 2;
                            break;
                        }
                    }
                    
                    
                    // important - found TJ/Tj - save text !
                    
                    if (found_tj > 0) {
                        
                        // evaluate if any special actions need to be taken based on text state
                        
                        if (text_written_in_bt < 0) {
                            //note : this is the first text written from this BT
                            // e.g., 'last' = 'current'
                            text_written_in_bt = 1;
                            text_space_action = 0;
                        }
                        
                        else {
                            // do nothing
                            
                        }
                        
                        // save current text coords in 'last_tm'
                        
                        // EXPERIMENT
                        //last_tm_x = tml_x;
                        //last_tm_y = tm_y;
                        //last_tml_x = tml_x;
                        
                        
                        if (debug_mode == 1) {
                            if (page_number == 1) {
                                //printf("update: in [] end - tml_x - %f - tml_y - %f \n", tml_x, tm_y);
                            }
                        }
                        
                        // add to running text_str for saving
                        
                        if ((strlen(text_str)+strlen(tmp_str)) < 499998) {
                            strcat(text_str,tmp_str);
                            
                            // new - save len of string in last_bracket - will use for spacing
                            last_bracket_len = strlen(tmp_str);
                            
                        }
                        
                        /*
                        if (debug_mode == 1) {
                            if (page_number == 1 ) {
                                printf("update: tmp_str []-end - %s \n", tmp_str);
                            }
                        }
                        */
                        
                        // reset tmp_str
                        strcpy(tmp_str,"");
                        
                        if (found_tj == 2) {
                            strcat(text_str, " ");
                            //printf("update:  FOUND T* - TJ=2 SCENARIO - inserting space \n");
                        }
                        
                    }
                }}
            
            
            
            // important action - found '>' - likely end of hex string
            
            if (flate_dst_tmp_buffer[y] == 62) {
                if (hex_on == 1) {
                    
                    if (debug_mode == 1) {
                        if (page_number == 0) {
                            //printf("update:  hex in-%d - %s - ", my_cmap, hex_tmp_str);
                            //printf("update: tml_x = %f \n", tml_x);
                        }
                    }
                    
                    hex_output_str = hex_handler_new(hex_tmp_str,hex_len,my_cmap,font_size_state,tm_scaler);
                    
                    
                    if (debug_mode == 1) {
                        if (page_number >= 3 && page_number < 5) {
                            
                            //printf("update: hex output str - %d - %s - %s \n", my_cmap, hex_output_str, hex_tmp_str);
                        }
                    }
                    
                    // new - save the len of the last hex str output -> will be used for assessing Td spacing
                    last_hex_str_len = strlen(hex_output_str);
                    
                    
                    // insert change - add hex_len to output char counter
                    bt_char_out_count += hex_len;
                    // end- change
                    
                    //hex_len = 0;
                    hex_on = 0;
                    
                    // experiment starts here - oct 29
                    
                    if (strlen(hex_output_str) > 0) {
                        
                        //strcat(table_text_str,hex_output_str);
                        
                        if (strlen(table_cell) < 99998) {
                            strcat(table_cell,hex_output_str);
                            
                            found_table_cell_content = 1;
                            //printf("update: hex - found_table_table_cell_content-%d \n", strlen(hex_output_str));
                        }
                        
                        // key change - experiment
                        
                        // printf("UPDATING LAST_TM_X - line 10153 - end of > - %f - %f \n", tml_x, last_tm_x);
                        
                        last_tm_x = tml_x;
                        last_tm_y = tm_y;
                        
                        if (page_number == 17) {
                            //printf("update: inside hex > end - updating last_tm_x - %f \n", last_tm_x);
                        }
                        
                    }
                    
                    // end - experiment here
                    
                    
                    // important action - check if inside a bracket
                    // if so, then add to tmp_str & reset hex_tmp_str
                    
                    if (bracket_on == 1) {
                        
                        if ((font_size_state * tm_scaler) >= 18) {
                            
                            //strcat(formatted_text,hex_output_str);
                            
                            if (fabs(tm_y - ft_y) < 1) {
                                
                                if (strlen(formatted_text) < 9000) {
                                    strcat(formatted_text, hex_output_str);
                                }
                            }
                            else {
                                if (strlen(formatted_text) < 9000) {
                                    strcat(formatted_text, " ");
                                    strcat(formatted_text, hex_output_str);
                                }
                            }
                            ft_y = tm_y;
                            ft_x = tml_x;
                            
                        }
                        
                        if (strlen(tmp_str) < 99998) {
                            strcat(tmp_str, hex_output_str);
                        }
                        
                        strcpy(hex_tmp_str,"");
                        strcpy(hex_output_str,"");
                        hex_len = 0;
                        
                        // add hex output to text_str
                        // IMPORTANT - NEED TO CHECK - THIS LOOKS LIKE BUG !!!
                        // note:  code works with lines 7076-7077 active
                        
                        //strcat(text_str,tmp_str);
                        //strcpy(tmp_str,"");
                        
                        // END - CHECK FOR BUG
                        
                        
                    }
                    
                    else {
                        
                        // Important action:  if not inside [ ], then look to Tj/TJ to save
                        
                        tj_look_ahead = 5;
                        found_tj = -1;
                        for (t=1; t < 1 +tj_look_ahead; t++) {
                            if (flate_dst_tmp_buffer[t+y] == 84) {
                                if (t+1 < tj_look_ahead) {
                                    if (flate_dst_tmp_buffer[y+t+1] == 74 || flate_dst_tmp_buffer[y+t+1] == 106) {
                                        found_tj = 1;
                                        break;
                                    }
                                }
                            }
                            
                            if (flate_dst_tmp_buffer[y+t] == 34) {
                                found_tj = 2;
                                //printf("update: unusual formatting - FOUND 34 TJ - ");
                                break;
                            }
                            if (flate_dst_tmp_buffer[y+t] == 39) {
                                found_tj = 2;
                                //printf("update: unusual formatting - FOUND 39 TJ - ");
                                break;
                            }
                        }
                        
                        // important action:  Found TJ/Tj - save text
                        
                        if ((found_tj > 0 && BDC_FLAG==0) || GLOBAL_ACTUAL_TEXT_FLAG == 1) {
                            
                            // evaluate if any special actions need to be taken based on text state
                            
                            if (text_written_in_bt < 0) {
                                //note : this is the first text written from this BT
                                // e.g., 'last' = 'current'
                                text_written_in_bt = 1;
                                text_space_action = 0;
                            }
                            
                            else {
                                // do nothing
                                
                            }
                            
                            // save current text coords in 'last_tm'
                                                    
                            last_tm_x = tml_x;
                            last_tm_y = tm_y;
                            
                            if (add_hex_space == 1) {
                                strcat(text_str," ");
                                add_hex_space = 0;
                            }
                            
                            if ((font_size_state * tm_scaler) >= 18) {
                                
                                if (strlen(formatted_text) < 9000) {
                                    strcat(formatted_text,hex_output_str);
                                }
                            }
                            
                            strcat(text_str,hex_output_str);
                                                       
                            strcpy(hex_tmp_str,"");
                            strcpy(hex_output_str,"");
                            hex_len=0;
                            GLOBAL_ACTUAL_TEXT_FLAG = 2;
                            
                            if (found_tj == 2) {
                                strcat(text_str, " ");
                                
                            }
                            
                        }
                        else {
                            // did not find a TJ - still need to reset hex tmp str
                            strcpy(hex_tmp_str,"");
                            hex_len=0;
                        }
                    }}}
            
        }
        // finished BT/ET loop
        // add tmp_str to total text
        
        if (debug_mode == 1) {
            //printf("update: FINISHED BT \n");
        }
        
        if ((strlen(text_str) + strlen(core_text_out)) < 50000) {
            // this is key step at end of BT/ET - copying text_str to core_text_out
            
            strcat(core_text_out,text_str);
            // key change - no space added after BT/ET
            //strcat(core_text_out, " ");
            strcpy(text_str,"");
            
            // experiment
            
            /*
            if ((strlen(table_text_str) + strlen(table_text_out)) < 100000) {
                strcat(table_text_out,table_text_str);
                strcpy(table_text_str,"");
            }
            */
            
            // end - experiment
            
            // parameterize GLOBAL_BLOK_SIZE
            // standard = 400 chars
            
            if (strlen(core_text_out) > GLOBAL_BLOK_SIZE && global_blok_save_off < 0) {
                
                // insert test - nov 11
                //printf("TESTING HERE - ");
                
                period_break_marker = -1;
                core_text_copy_go_ahead = 1;
                
                strcpy(core_text_copy_tmp,"");
                strcpy(core_text_final,"");

                strcpy(core_period_tmp,"");
                
                dupe_space_check = 0;
                
                create_blok_go_ahead = 0;
                
                for (i=0; i < strlen(core_text_out); i++) {
                    
                    sprintf(core_period_tmp,"%c", core_text_out[i]);
                    strcat(core_text_final, core_period_tmp);
                    
                    /*
                    if (dupe_space_check == 0 || core_text_out[i] != 32) {
                        sprintf(core_period_tmp,"%c", core_text_out[i]);
                        strcat(core_text_final, core_period_tmp);
                        
                        dupe_space_check = 0;
                        
                        if (core_text_out[i] == 32) {
                            dupe_space_check = 1;
                        }
                    }
                    */
                    
                    // if 46, 10 or 13 found after reaching GLOBAL BLOK SIZE target
                    
                    if (i > GLOBAL_BLOK_SIZE & (core_text_out[i] == 46 || core_text_out[i] == 13 || core_text_out[i] == 10)) {
                        
                        // if there is more text ...
                        if ((i+1) < strlen(core_text_out)) {
                            
                            // test #1 - if double /n/r/ pattern -> assume end of para
                            if (core_text_out[i] == 13 || core_text_out[i] ==10) {
                                if (core_text_out[i+1] == 10 || core_text_out[i+1] == 13) {
                                    create_blok_go_ahead = 1;
                                }
                            }
                            
                            // test #2 - if there is 'validated' '.', e.g., space follows, and no immediately preceding '.' -> avoids common "Section 4.6." pattern
                            
                            if (core_text_out[i] == 46) {
                                
                                // test #2A - confirm that there is a space following the 'period'
                                if (core_text_out[i+1] == 32) {
                                    create_blok_go_ahead = 1;
                                    
                                    // test #2B - confirm that there is no other '.' in 5 char look-back
                                    for (ii=i-5; ii < i; ii++) {
                                        if (core_text_out[ii] == 46) {
                                            create_blok_go_ahead = 0;
                                            break;
                                        }
                                    }}
                                
                                // common abbreviations exclusions
                                
                                // 'Mr'
                                if (core_text_out[i-2] == 77 && core_text_out[i-1] == 114) {
                                    create_blok_go_ahead = 0;
                                }
                                
                                // 'Mrs'
                                if (core_text_out[i-3] == 77 && core_text_out[i-2] == 114 && core_text_out[i-1] == 115) {
                                    create_blok_go_ahead = 0;
                                }
                                
                                // "Dr"
                                if (core_text_out[i-2] == 68 && core_text_out[i-1] == 114) {
                                    create_blok_go_ahead = 0;
                                }
                                
                                
                                // exclude common number pattern, e.g., "3." or "4." used commonly in lists and sections of documents
                                
                                if (core_text_out[i-1] > 47 && core_text_out[i-1] < 58) {
                                    create_blok_go_ahead = 0;
                                }
                                
                            }
                                
                        }
                        
                        // if at end of text ... if 46, 13, or 10 and end of the core text out
                        if ((i+1) >= strlen(core_text_out)) {
                            create_blok_go_ahead = 1;
                        }
                        
                        // max threshold for largest blok = 10X GLOBAL_BLOK_SIZE
                        //  --safety for edge cases or unique documents
                        
                        if (i > GLOBAL_BLOK_SIZE * 5) {
                            create_blok_go_ahead = 1;
                        }
                                            
                        //printf(".\n");
                        
                        if (create_blok_go_ahead == 1) {
                            // found section of text, delimited with '.' greater than target blok size
                            // insert break here - > write this part to Bloks
                            // copy the balance and keep in core_text_out
                            period_break_marker = i;
                            core_text_copy_go_ahead = 1;
                            strcpy(core_text_copy_tmp,"");
                            create_blok_go_ahead = 0; // reset
                            //printf("update: found target text blok and '.' - %d \n", strlen(core_text_out));
                            
                            if ((i+1) < strlen(core_text_out)) {
                                for (j=i+1; j < strlen(core_text_out); j++) {
                                    sprintf(core_period_tmp,"%c", core_text_out[j]);
                                    strcat(core_text_copy_tmp,core_period_tmp);
                                }
                            }
                            //printf("survived to line 11659 -%s \n", core_text_copy_tmp);
                            
                            strcpy(core_text_out,"");
                            for (j=0;j < strlen(core_text_copy_tmp); j++) {
                                sprintf(core_period_tmp,"%c", core_text_copy_tmp[j]);
                                strcat(core_text_out,core_period_tmp);
                            }
                            break;
                        }
                        
                    }
                    else {
                        //printf("%c", core_text_out[i]);
                    }
                    
                }
                
                if (period_break_marker == -1) {
                    
                    // do not have a text blok > 400 with nice '.' break
                    // do nothing - keep going
                }
                else {
                    
                    // if core_text_out exceeds target len == 400, then write to blok and start over
                    
                    if (debug_mode == 1) {
                        if (page_number >= 0 && page_number < 10) {
                            printf("update: pdf_parser - core_text_out (block > %d) - %d - %s \n", GLOBAL_BLOK_SIZE, strlen(core_text_final), core_text_final);
                        }
                    }
                    
                    
                    // start new blok
                    strcpy(Bloks[global_blok_counter].content_type,"text");
                    strcpy(Bloks[global_blok_counter].file_type, "PDF");
                    Bloks[global_blok_counter].slide_num = page_number;
                    Bloks[global_blok_counter].shape_num = bloks_created;
                    strcpy(Bloks[global_blok_counter].text_run, core_text_final);
                    Bloks[global_blok_counter].position.x = (int)tm_x;
                    Bloks[global_blok_counter].position.y = (int)tm_y;
                    Bloks[global_blok_counter].position.cx = page_number;
                    Bloks[global_blok_counter].position.cy = x;
                    Bloks[global_blok_counter].slide_num = page_number;
                    Bloks[global_blok_counter].shape_num = x;
                    strcpy(Bloks[global_blok_counter].relationship, "");
                    
                    // if formatted_text is not empty -> add to blok
                    if (strlen(formatted_text) > 10 && strlen(formatted_text) < 49000) {
                        
                        strcpy(Bloks[global_blok_counter].formatted_text, formatted_text);
                            
                        if (strlen(global_headlines) < 1000 && strlen(formatted_text) < 1000) {
                            strcat(global_headlines, formatted_text);
                            strcat(global_headlines, " ");
                        }
                    }
                    
                    else {
                        if (strlen(global_headlines) > 0) {
                            
                            strcat(formatted_text," ");
                            strcat(formatted_text, global_headlines);
                        }
                    }
                    
                    if (strlen(formatted_text) > 0) {
                        strcpy(Bloks[global_blok_counter].formatted_text, formatted_text);
                    }
                    else {
                        strcpy(Bloks[global_blok_counter].formatted_text, "");
                    }
                    
                    if ((strlen(table_formatting_copy) + strlen(formatted_text)) < 9900) {
                        if (strlen(formatted_text) > 0) {
                            strcat(table_formatting_copy, formatted_text);
                            strcat(table_formatting_copy, " ");
                        }
                    }
                    
                    strcpy(formatted_text,"");
                    
                    strcpy(Bloks[global_blok_counter].linked_text,"");
                    strcpy(Bloks[global_blok_counter].table_text,"");
                
                    global_blok_counter ++;
                    bloks_created ++;
                    
                    //printf("update: finished all array blok creation steps ! \n");
                    
                }
            }
        }
        
        
    }
    
    //printf("update: checking for one last blok ! \n");
    
    if (strlen(text_str) > 0 || strlen(core_text_out) > 0) {
        if ((strlen(text_str) + strlen(core_text_out)) < 50000 && global_blok_save_off < 0) {
            strcat(core_text_out, text_str);
            
            // experiment
            /*
            if ((strlen(table_text_str) + strlen(table_text_out)) < 100000) {
                strcat(table_text_out, table_text_str);
            }
             */
            // ends experiment
            
            if (strlen(core_text_out) < 100 && ((strlen(core_text_out) + strlen(Bloks[global_blok_counter-1].text_run)) < 49995) && global_blok_counter >= 1) {
                
                // if final bit of text is short, then consolidate with last blok
                // ... avoids too many small bloks in downstream analysis
                
                if (debug_mode == 1) {
                    //printf("update: found last bit of text - too short - will consolidate - new! \n");
                    //printf("update: len of last bit of text - %d - %s \n", strlen(core_text_out), core_text_out);
                    
                }
                
                //printf("udate: consolidating short last blok! \n");
                
                strcpy(core_period_tmp,"");

                //printf("update: global_blok_counter - %d \n", global_blok_counter);
                //printf("update: strlen last entry - %d \n", strlen(Bloks[global_blok_counter-1].text_run));
                //printf("update: Bloks entry last - %s \n", Bloks[global_blok_counter-1].text_run);
                
                for (i=0; i < strlen(core_text_out); i++) {
                    sprintf(core_period_tmp,"%c",core_text_out[i]);
                    //printf("iter core text - %d - %d \n", i, core_text_out[i]);
                    strcat(Bloks[global_blok_counter-1].text_run,core_period_tmp);
                }
            }
            
            else {
                
                
                not_spaces_only = 0;
                for (i=0; i < strlen(core_text_out); i++) {
                    if (core_text_out[i] != 32) {
                        not_spaces_only = 1;
                        break;
                    }
                }
                
                if (not_spaces_only == 1) {
                    
                    
                    if (debug_mode == 1) {
                        if (page_number < 10) {
                            printf("update: pdf_parser - core_text_out - %d - %s \n", strlen(core_text_out),core_text_out);
                        }
                    }
                    
                    strcpy(Bloks[global_blok_counter].content_type,"text");
                    strcpy(Bloks[global_blok_counter].file_type, "PDF");
                    Bloks[global_blok_counter].slide_num = page_number;
                    Bloks[global_blok_counter].shape_num = bloks_created;
                    strcpy(Bloks[global_blok_counter].text_run, core_text_out);
                    Bloks[global_blok_counter].position.x = (int)tm_x;
                    Bloks[global_blok_counter].position.y = (int)tm_y;
                    Bloks[global_blok_counter].position.cx = page_number;
                    Bloks[global_blok_counter].position.cy = text_box_count-1;
                    Bloks[global_blok_counter].slide_num = page_number;
                    Bloks[global_blok_counter].shape_num = text_box_count-1;
                    strcpy(Bloks[global_blok_counter].relationship, "");
                    
                
                    // if formatted_text is not empty -> add to blok
                    if (strlen(formatted_text) > 10 && strlen(formatted_text) < 49000) {
                        
                        strcpy(Bloks[global_blok_counter].formatted_text, formatted_text);
                        
                        if (strlen(global_headlines) < 1000 && strlen(formatted_text) < 1000) {
                            strcat(global_headlines, formatted_text);
                            strcat(global_headlines, " ");
                        }
                    }
                    
                    else {
                        if (strlen(global_headlines) > 0) {
                            strcat(formatted_text," ");
                            strcat(formatted_text, global_headlines);
                        }
                    }
                    
                    if (strlen(formatted_text) > 0) {
                        strcpy(Bloks[global_blok_counter].formatted_text, formatted_text);
                    }
                    else {
                        strcpy(Bloks[global_blok_counter].formatted_text, "");
                    }
                    
                    if ((strlen(table_formatting_copy) + strlen(formatted_text)) < 9900) {
                        if (strlen(formatted_text) > 0) {
                            strcat(table_formatting_copy, formatted_text);
                            strcat(table_formatting_copy, " ");
                        }
                    }
                    
                    strcpy(formatted_text,"");
                    
                    strcpy(Bloks[global_blok_counter].linked_text,"");
                    strcpy(Bloks[global_blok_counter].table_text,"");
                    
                    global_blok_counter ++;
                    bloks_created ++;
                }
            }
            
            strcpy(text_str,"");
            strcpy(core_text_out,"");
            
            // experiment
            // strcpy(table_text_out,"");
            // ends experiment
            
        }
    }
    
    // REACHED END OF PAGE -> processed all BT/ET text boxes on page
    
    //printf("update: reached end of page successfully! \n");
        
    if (bloks_created > 0) {
        //printf("ADDING BLOKS -%d - on PAGE - %d \n", bloks_created, page_number);
    }
        
    
    
    
    //                          *** INITIATE TABLE PARSER ***
    
    if (debug_mode == 1) {
        if (page_number < 10) {
            //printf("update: pdf_parser - table count indicator - %d - table_rows - %d - table_cell_counter - %d  \n", table_count_indicator, table_row, table_cell_counter);
        }
    }
    
    if (debug_mode == 1) {
        
        if (page_number < 200) {
            
            tab_most_common = 0;
            
            for (tab_i=0; tab_i < table_row; tab_i ++) {
                
                if (page_number < 20) {
                    //printf("update: rows - %d - cell count - %d-%d \n", tab_i, pdf_table[tab_i].col_count, my_table[tab_i][0].y);
                }
                
                tab_most_common += pdf_table[tab_i].col_count;
                for (tab_j=0; tab_j < pdf_table[tab_i].col_count; tab_j++) {
                    //printf("%s - ", my_table[tab_i][tab_j].cell);
                }
            }
            
            if (page_number < 20) {
                //printf("\nupdate: AVG COLUMNS - %d - %d - %d \n", tab_most_common, table_row, (int)(tab_most_common / table_row));
            }
        }
    }
    
    if (table_count_indicator >= 12 && table_cell_counter >= 20) {
        
        tab_most_common = 0;
        tab_tmp = 0;
        
        for (tab_i=0; tab_i < 100; tab_i ++) {
            table_cols_sorter[tab_i] = 0;
        }
        
        for (tab_i=0; tab_i < table_row; tab_i ++) {
            table_cols_sorter[pdf_table[tab_i].col_count] ++;
        }
        for (tab_i=0; tab_i < 20; tab_i++) {
            if (table_cols_sorter[tab_i] > tab_tmp) {
                tab_tmp = table_cols_sorter[tab_i];
                tab_most_common = tab_i;
            }
        }
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - most common col count = %d - %d - %d \n", tab_most_common, tab_tmp, table_row);
        }
        
        
        // only move ahead with table if most common col structure is greater than threshold
        // assume that there must be between 4 - 10 columns that repeat @ least 5+ times
        // ... if 1-2 columns, likely not a table -> could be simple a long row of text with formatting
        // ... if 11+ columnns, it may be a large column, but more likely, it is a string of text
        
        if (tab_most_common > 3 && tab_most_common < 11 && tab_tmp > 4) {
            
            // one last test - look for 'number density' in specific rows, e.g., most common width
            
            tab_num_count = 0;
            tab_total_count = 0;
            
            for (tab_iter_nums=0; tab_iter_nums < table_row; tab_iter_nums ++) {
                
                if (pdf_table[tab_iter_nums].col_count == tab_most_common) {
                    // iterate thru rows with most common col structure
                    
                    for (tab_i=0; tab_i < pdf_table[tab_iter_nums].col_count; tab_i ++) {
                        // iterate thru each col
                        for (tab_j=0; tab_j < strlen(my_table[tab_iter_nums][tab_i].cell); tab_j++) {
                            // iterate thru each cell
                            if (my_table[tab_iter_nums][tab_i].cell[tab_j] > 47 && my_table[tab_iter_nums][tab_i].cell[tab_j] < 58) {
                                tab_num_count ++;
                            }
                            
                            // new test - include 'X' or 'x' as indicators of table density
                            if (my_table[tab_iter_nums][tab_i].cell[tab_j] == 88 || my_table[tab_iter_nums][tab_i].cell[tab_j] == 120) {
                                tab_num_count ++;
                            }
                            // end - new test
                            
                            tab_total_count ++;
                        }
                    }
                }
            }
            
            //printf("CHECKED NUMBER DENSITY TEST: %d - %d - %f \n", tab_num_count, tab_total_count, (float)tab_num_count/(float)tab_total_count);
            
            
            if (tab_total_count > 100) {
                
                // change - level of test - starts here
                //  --current product version @ 0.20
                //  --resetting to 0.15
                
                if ( ((float)tab_num_count / (float)tab_total_count) > 0.15) {
                    
                    // continue
                    
                    if (debug_mode == 1) {
                        //printf("update: pdf_parser - passed numerical density test for table identification - %f \n", (float)tab_num_count/(float)tab_total_count);
                    }
                    
                    tab_tmp = 0;
                    
                    for (tab_i=0; tab_i < 50; tab_i++) {
                        my_col_locations[tab_i] = 0;
                    }
                    
                    for (tab_i=0; tab_i < table_row; tab_i ++) {
                        
                        if (pdf_table[tab_i].col_count == tab_most_common) {
                            tab_tmp ++;
                            //printf("update: found row with right number of columns - %d - %d \n", tab_i, pdf_table[tab_i].col_count);
                            
                            for (tab_j=0; tab_j < pdf_table[tab_i].col_count; tab_j++) {
                                //printf("table cols - %d - %d - %d \n", tab_i,tab_j,pdf_table[tab_i].col_locs[tab_j]);
                                //tab_tmp2 = pdf_table[tab_i].col_locs[tab_j];
                                tab_tmp2 = my_table[tab_i][tab_j].x;
                                //printf("update: iterating - %d - %d - %d \n", tab_i,tab_j, tab_tmp2);
                                my_col_locations[tab_j] += tab_tmp2;
                            }
                        }
                    }
                    
                    // printf("rows with right number of columns - %d \n", tab_tmp);
                    
                    for (tab_i=0; tab_i < tab_most_common; tab_i ++) {
                        tab_tmp3 = my_col_locations[tab_i];
                        my_col_locations[tab_i] = (int) (tab_tmp3 / tab_tmp);
                        
                        if (debug_mode == 1) {
                            //printf("update: table_builder- my col avg locations - %d-%d \n", tab_i, my_col_locations[tab_i]);
                        }
                    }
                    
                    // remediate any mis-aligned columns -> and perfect table_out
                    strcpy(table_text_out_final, "");
                    strcpy(core_text_out, "");
                    
                    for (tab_i=0; tab_i < table_row; tab_i ++) {
                        
                        // iterating through rows
                        if (debug_mode == 1) {
                            //printf("update: row found @ %d-%d \n", tab_i, my_table[tab_i][0].y);
                            //printf("update: table_row_y - %d \n", table_row_y[tab_i]);
                        }
                        
                        if (pdf_table[tab_i].col_count == tab_most_common) {
                            
                            // correct number of columns - write each cell sequentially into master table
                            strcat(table_text_out_final, " <tr> ");
                            for (tab_j=0; tab_j < pdf_table[tab_i].col_count; tab_j++) {
                                strcat(table_text_out_final, "<th> <");
                                // need to insert handling for cols > 26 rows
                                sprintf(tt_tmp,"%c",65+tab_j);
                                strcat(table_text_out_final, tt_tmp);
                                sprintf(tt_tmp,"%d",tab_i);
                                
                                if (strlen(table_text_out_final) < 99000) {
                                    strcat(table_text_out_final, tt_tmp);
                                    strcat(table_text_out_final, "> ");
                                    strcat(table_text_out_final, my_table[tab_i][tab_j].cell);
                                    
                                    if (strlen(core_text_out) + strlen(my_table[tab_i][tab_j].cell) < 49000) {
                                        strcat(core_text_out, my_table[tab_i][tab_j].cell);
                                        strcat(core_text_out, " ");
                                    }
                                    
                                    strcat(table_text_out_final, " </th> ");
                                }
                            }
                            strcat(table_text_out_final, " </tr> ");
                        }
                        
                        if (pdf_table[tab_i].col_count < tab_most_common) {
                            // fewer elements - map up each element to the closest coord
                            strcat(table_text_out_final, " <tr> ");
                            tab_col_current_tmp = 0;
                            
                            for (tab_j=0; tab_j < pdf_table[tab_i].col_count; tab_j++) {
                                tab_tmp3 = my_table[tab_i][tab_j].x;
                                tab_tmp4 = 1000000;
                                tab_tmp5 = 0;
                                //printf("update: found column with too few items - %d-%d-%d-%s \n", tab_j, my_table[tab_i][tab_j].x, my_table[tab_i][tab_j].y, my_table[tab_i][tab_j].cell);
                                
                                for (tab_k=0; tab_k < tab_most_common; tab_k ++) {
                                    //printf("col match loop - %d - %d - %d \n", tab_tmp3, my_col_locations[tab_k], abs(tab_tmp3 - my_col_locations[tab_k]));
                                    
                                    if (abs(tab_tmp3 - my_col_locations[tab_k]) < tab_tmp4) {
                                        tab_tmp4 = abs(tab_tmp3 - my_col_locations[tab_k]);
                                        tab_tmp5 = tab_k;
                                    }
                                }
                                //printf("update: closest col match - %d - %d- col match - %d \n", tab_i,tab_j,tab_tmp5);
                                
                                if (tab_tmp5 > tab_col_current_tmp) {
                                    
                                    //printf("update: inserting empty col - %d - %d - %d \n", tab_i,tab_j,tab_tmp5);
                                    
                                    for (tab_l=tab_col_current_tmp; tab_l < tab_tmp5; tab_l++) {
                                        // insert empty entry
                                        
                                        if (strlen(table_text_out_final) < 99000) {
                                            strcat(table_text_out_final, "<th> <");
                                            // need to insert handling for cols > 26 rows
                                            sprintf(tt_tmp,"%c",65+tab_l);
                                            strcat(table_text_out_final, tt_tmp);
                                            sprintf(tt_tmp,"%d",tab_i);
                                            strcat(table_text_out_final, tt_tmp);
                                            strcat(table_text_out_final, "> ");
                                            strcat(table_text_out_final, "");
                                            strcat(table_text_out_final, " </th> ");
                                        }
                                    }
                                    tab_col_current_tmp = tab_tmp5;
                                }
                                
                                if (strlen(table_text_out_final) < 99000) {
                                    
                                    strcat(table_text_out_final, "<th> <");
                                    // need to insert handling for cols > 26 rows
                                    sprintf(tt_tmp,"%c",65+tab_col_current_tmp);
                                    strcat(table_text_out_final, tt_tmp);
                                    sprintf(tt_tmp,"%d",tab_i);
                                    strcat(table_text_out_final, tt_tmp);
                                    strcat(table_text_out_final, "> ");
                                    strcat(table_text_out_final, my_table[tab_i][tab_j].cell);
                                    strcat(table_text_out_final, " </th> ");
                                    tab_col_current_tmp ++;
                                    
                                    if (strlen(core_text_out) + strlen(my_table[tab_i][tab_j].cell) < 49000) {
                                        strcat(core_text_out, my_table[tab_i][tab_j].cell);
                                        strcat(core_text_out, " ");
                                    }
                                    
                                    
                                }
                                //printf("table cols - %d - %d - %d \n", tab_i, tab_j, pdf_table[tab_i].col_locs[tab_j]);
                                //printf("cells - %d - %d - %s \n", my_table[tab_i][tab_j].x, my_table[tab_i][tab_j].y, my_table[tab_i][tab_j].cell);
                            }
                            
                            strcat(table_text_out_final, " </tr> ");
                        }
                        
                        if (pdf_table[tab_i].col_count > tab_most_common) {
                            // more elements - map up each element to the closest coord
                            strcpy(table_text_tmp,"");
                            strcat(table_text_out_final, " <tr> ");
                            tab_tmp3 = 0;
                            //tab_tmp4 = my_table[tab_i][0].x;
                            //tab_tmp5 = my_table[tab_i][0].y;
                            strcat(table_text_tmp,my_table[tab_i][0].cell);
                            
                            if (strlen(core_text_out) + strlen(my_table[tab_i][0].cell) < 49000) {
                                strcat(core_text_out, my_table[tab_i][0].cell);
                                strcat(core_text_out, " ");
                            }
                            
                            //printf("[0] long column - %s \n", table_text_tmp);
                            tab_tmp4 = 0;
                            
                            for (tab_j=1; tab_j < pdf_table[tab_i].col_count; tab_j++) {
                                
                                //printf("iter long column - table_cell - %d - %s - %d - %d \n", my_table[tab_i][tab_j].x, my_table[tab_i][tab_j].cell, my_table[tab_i][tab_tmp3].x, my_col_locations[tab_j]);
                                
                                tab_tmp4 = my_col_locations[tab_tmp3+1];
                                
                                if ( (abs(my_table[tab_i][tab_j].x - my_table[tab_i][tab_tmp3].x)) < (abs(my_table[tab_i][tab_j].x - tab_tmp4) ) ) {
                                    // cell is closer to current column
                                    strcat(table_text_tmp, my_table[tab_i][tab_j].cell);
                                    
                                    if (strlen(core_text_out) + strlen(my_table[tab_i][tab_j].cell) < 49000) {
                                        strcat(core_text_out, my_table[tab_i][tab_j].cell);
                                        strcat(core_text_out, " ");
                                    }
                                    
                                    //printf("consolidating cells - %d - %d - %s \n", tab_i, tab_j, table_text_tmp);
                                }
                                
                                else {
                                    
                                    if (strlen(table_text_out_final) < 99000) {
                                        strcat(table_text_out_final, "<th> <");
                                        // need to insert handling for cols > 26 rows
                                        sprintf(tt_tmp,"%c",65+tab_tmp3);
                                        strcat(table_text_out_final, tt_tmp);
                                        sprintf(tt_tmp,"%d",tab_i);
                                        strcat(table_text_out_final, tt_tmp);
                                        strcat(table_text_out_final, "> ");
                                        strcat(table_text_out_final, table_text_tmp);
                                        strcat(table_text_out_final, " </th> ");
                                        strcpy(table_text_tmp,"");
                                        tab_tmp3 ++;
                                        strcat(table_text_tmp,my_table[tab_i][tab_j].cell);
                                        
                                        if (strlen(core_text_out) + strlen(my_table[tab_i][tab_j].cell) < 49000) {
                                            strcat(core_text_out, my_table[tab_i][tab_j].cell);
                                            strcat(core_text_out, " ");
                                        }
                                        
                                        
                                        
                                    }
                                }
                            }
                            
                            strcat(table_text_out_final, " </tr> ");
                        }
                    }
                    
                    if (debug_mode == 1) {
                        printf("update: pdf_parser - writing table_text_out_final - %s \n", table_text_out_final);
                    }
                    
                    if (strlen(table_text_out_final) < 99500) {
                        
                        //printf("going down this path - %d-%d \n", strlen(table_text_out_final), strlen(table_text_out));
                        
                        global_table_count ++;
                        
                        strcat(table_text_out," </th> </tr>");
                        
                        strcpy(Bloks[global_blok_counter].content_type,"table");
                        strcpy(Bloks[global_blok_counter].file_type, "pdf");
                        Bloks[global_blok_counter].slide_num = page_number;
                        Bloks[global_blok_counter].shape_num = bloks_created;
                        
                        if (strlen(table_text_out) < 50000) {
                            strcpy(Bloks[global_blok_counter].text_run, core_text_out);
                        }
                        else {
                            strcpy(Bloks[global_blok_counter].text_run, "");
                        }
                        
                        Bloks[global_blok_counter].position.x = (int)tm_x;
                        Bloks[global_blok_counter].position.y = (int)tm_y;
                        Bloks[global_blok_counter].position.cx = page_number;
                        Bloks[global_blok_counter].position.cy = text_box_count-1;
                        Bloks[global_blok_counter].slide_num = page_number;
                        Bloks[global_blok_counter].shape_num = text_box_count-1;
                        strcpy(Bloks[global_blok_counter].relationship, "");
                        
                        if (strlen(table_formatting_copy) > 0) {
                            strcpy(Bloks[global_blok_counter].formatted_text, table_formatting_copy);
                        }
                        else {
                            strcpy(Bloks[global_blok_counter].formatted_text, "");
                        }
                        
                        strcpy(Bloks[global_blok_counter].linked_text,"");
                        
                        strcpy(Bloks[global_blok_counter].table_text,table_text_out_final);
                        
                        global_blok_counter ++;
                        bloks_created ++;
                    }
                }
            }}

        }
    
    // new change - save 'state' availabe to another content item on this page
    GLOBAL_PAGE_CMAP = my_cmap;
    GLOBAL_FONT_SIZE = (int)(font_size_state * tm_scaler);
    
    return bloks_created;
    }
 


//  RGB function not used -> all PNG creation for images left to post-processing
//  kept here commented out for potential future use, if PNG creation added to core processing

/*
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
*/


int image_handler_flate (int height,int width, int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int dct_flag, int no_text_flag, int png_convert_on, int cmyk_flag_on) {
    
    int success_code = -1;
    int blok_added = 0;
    
    z_stream strm;
    
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    int err= -1;
    int ret= -1;
     
    int x1=0;
    int x2=0;
    int first_cell=0;
    int monochrome = 0;
    int save_yes = 1;
    
    int x=0;
    int y=0;
    int count=0;
    int m =0;
    int rgb = -1;
    
    int image_text_len = 0;
    
    int png_count = 0;
    
    // Parameters for pnglib
    
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;
    FILE *fp = NULL;
    
    //int code = 0;
    //char* title;
    
    // End - Parameters for pnglib

    int local_img_counter =0;

    int slider = 0;
    int file_size;
    FILE* writeptr;
    
    char image_account_directory[500];
    char img_name[100];
    
    int img_dst_len;
    int total_file_size_out = -1;
    
    unsigned char *img_dst_buffer;
    
    int t=0;
    char test_tmp[10];
    strcpy(test_tmp,"");
    
    // important tracker
    local_img_counter = master_image_tracker;
    // end - tracker
    
    
    // assess options based on dct_flag & png_convert_on
    
    // if dct_flag > -1 -> this is already packaged as a JPG image -> just write to disk 'as is'
    
    if (dct_flag > -1) {
        // dct image type
        sprintf(img_name,"image%d_%d.jpg",master_doc_tracker,local_img_counter);
    }
    
    else {
        
        // this is a raw binary - will either save 'as is' or convert to png
        
        if (png_convert_on == 1) {
            // will convert to png
            sprintf(img_name,"image%d_%d.png", master_doc_tracker,local_img_counter);
        }
        
        else {
            // 'old way' of saving raw binary directly to disk
            // ... requires post-processing step to convert 'raw' image into 'png'
            // ras image type
            sprintf(img_name,"image%d_%d.ras",master_doc_tracker, local_img_counter);
        }
    }
    
    

    if (dct_flag > -1) {
        strcpy(image_account_directory,"");
        strcat(image_account_directory,global_image_fp);
        strcat(image_account_directory,img_name);
        
    }
    
    else {
        
        strcpy(image_account_directory,"");
        strcat(image_account_directory,global_image_fp);
        strcat(image_account_directory,img_name);
        
    }
    
    // arbitrary, but large cap at ~25 MB
    
    img_dst_len = 25000000;
    img_dst_buffer = (unsigned char*)malloc(img_dst_len * sizeof(unsigned char));
    
    file_size = stream_stop-stream_start;
    
    
    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        break;
                    }}}}
    }
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    //strm.avail_in = count;
    strm.avail_in = stream_stop - (stream_start + slider);
    
    strm.next_in = (Bytef *)&buffer[stream_start+slider];
    
    strm.avail_out = img_dst_len; // size of output
    strm.next_out = (Bytef *)img_dst_buffer; // output char array
    
    err = inflateInit(&strm);
    //ret = inflate(&strm, Z_FULL_FLUSH);
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    
    total_file_size_out = strm.total_out;
    
    //  error-checking on inflate process
    //printf("update:  survived - err-%d-ret-%d \n", err,ret);
    //printf("update:  total out - %d \n", strm.total_out);
    
    if (debug_mode == 1) {
        //printf("update: in image_handler_flate - cmyk_flag = %d \n", cmyk_flag_on);
    }
    
    if (ret==0 || ret==1) {
        if (total_file_size_out >0) {
            
            // success - found flate
            
            success_code = total_file_size_out;
            first_cell = img_dst_buffer[0];
            
            if (total_file_size_out == (3*height*width)) {
                
                //  note: check 3 * height * width works surprisingly well to identify RGB images
                
                if (debug_mode == 1) {
                    
                    //printf("update:  RGB Flate image found - %d - %d \n", total_file_size_out, (3*height*width));
                }
                
                rgb = 1;
                
                // insert new check on content of img_dst_buffer
                
                for (x1=0; x1 < height; x1++) {
                    for (x2=0; x2 < 3*width; x2++) {
                        count = x1*3*width+x2;
                        if (count > 0) {
                            if (img_dst_buffer[x1*3*width+x2] != first_cell) {
                                monochrome = 99;
                                break;
                            }}
                    }}
                
                if (monochrome !=99) {
                    
                    // note: there are potentially a large number of small formatting related images
                    //      --unlike pptx, which stores 'style' separately, a PDF stores as images
                    //      --possible in large PDF to have 100+ of the same image
                    //      --need to enhance filtering
                    //      --one simple check is to remove monochrome images -> low information value
                    
                    //      printf("update: FOUND MONOCHROME IMAGE - DON'T SAVE");
                    
                    save_yes = -1;
                }
                
            }
            else {
                if (total_file_size_out == height*width ) {
                    
                    //  note: height * width = # of pixels in grayscale image
                    //      -- may need to enhance handling of other image types
                    //  printf("update:  Grayscale image found - %d - %d \n", total_file_size_out, height*width);
                    
                    rgb = 2;
                    
                    //insert new check on content of img_dst_buffer
                    
                    for (x1=0; x1 < height; x1++) {
                        for (x2=0; x2 < width; x2++) {
                            count = x1*width + x2;
                            if (count > 0) {
                                if (img_dst_buffer[count] != first_cell) {
                                    monochrome = 99;
                                    break;
                                }}
                        }}
                    
                    if (monochrome !=99) {
                        
                        //printf("update: found GRAYSCALE MONOCHROME IMAGE - DONT SAVE");
                        
                        save_yes = -1;
                    }
                    
                }
                else {
                    if (dct_flag > -1) {
                        
                        //printf("note: this is DCT image type - will save but H*W not match");
                        
                        save_yes = 1;
                    }
                    else {
                        
                        //  note: need to add better handling of other image types
                        //
                        //printf("error: IMAGE - can not match height-width with output size - %d - %d - %d \n", height,width, total_file_size_out);
                        
                        save_yes = -1;
                    }
                    
                }
            }
        }
    }
    
    
    if (png_convert_on == 0) {
        
        if (success_code > -1 && save_yes == 1 && (rgb == 1 || rgb == 2)) {
            writeptr = fopen(image_account_directory,"wb");
            fwrite(img_dst_buffer, strm.total_out,1, writeptr);
            fclose(writeptr);
        }
    }
    
    
    if (png_convert_on == 1) {
        
        if (success_code > - 1 && save_yes == 1 && (rgb ==1 || rgb == 2)) {
            
            if (debug_mode == 1) {
                //printf("update: WRITING PNG IMAGE TO DISK - %d - %d - %d \n", rgb, height, width);
            }
            
            fp = fopen(image_account_directory, "wb");
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
            
            for (y=0 ; y< height ; y++) {
                
                if (rgb==1) {
                    for (x=0 ; x < 3*width; x++) {
                        row[x] = img_dst_buffer[y*width*3 +x];
                    }
                }
                else {
                    for (x=0; x < width; x++) {
                        row[x] = img_dst_buffer[y*width + x];
                        
                        if (cmyk_flag_on == 1) {
                            row[x] = 255 - img_dst_buffer[y*width + x];
                        }
                    }
                    
                }
                
                png_write_row(png_ptr, row);
            }
            
            if (debug_mode == 1) {
                if (cmyk_flag_on == 1) {
                    printf("update: pdf_parser - in image_handler_flate - identified inverted cmyk images - %s \n", img_name);
                }
            }
            
            
            // End write
            png_write_end(png_ptr, NULL);
            
            fclose(fp);
            
            png_destroy_write_struct(&png_ptr,&info_ptr);
            
        }
    }
    
    
    
    //                      STARTING PNG LIB WRITE HERE
    //
    //      --not being used currently - commented out
    //      --leaving here as potential replacement in the future from post-processing
    //      --for large PDFs with many 'binary' images, this png conversion can be time-consuming
    //
    
    /*
     
    if (height > -1 && width > -1) {
        
        fp = fopen(new_fp, "wb");
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        info_ptr = png_create_info_struct(png_ptr);
    
        png_init_io(png_ptr, fp);
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        title = "dbo";
        */
        
        // Set title
        /*
        if (title != NULL) {
            png_text title_text;
            title_text.compression = PNG_TEXT_COMPRESSION_NONE;
            title_text.key = "Title";
            title_text.text = title;
            png_set_text(png_ptr, info_ptr, &title_text, 1);
            }

        png_write_info(png_ptr, info_ptr);
        
        */
        
        // Allocate memory for one row (3 bytes per pixel - RGB)
        //row = (png_bytep) malloc(3 * width * sizeof(png_byte));
        
        // Write image data
        
        /*
        png_bytepp row_pointers = (png_bytepp)png_malloc(png_ptr, sizeof(png_bytepp) * height);
        for (int y = 0; y < height; y++) {
            row_pointers[y] = (png_bytep)png_malloc(png_ptr, 3*width);
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < 3*width; x++) {
                // img_dst_buffer is source data that we convert to png
                row_pointers[y][x] = img_dst_buffer[y*3*width + x];
            }
        }
        
        png_write_info(png_ptr, info_ptr);
        png_write_image(png_ptr, row_pointers);
        png_write_end(png_ptr, info_ptr);
        
        */
        
        /*
        for (y=0 ; y< height ; y++) {
            for (x=0 ; x <width ; x++) {
                //setRGB (&(row[x*3]), img_dst_buffer[y*width + x]);
                row[x*3] = img_dst_buffer[y*width+x];
                row[1+(x*3)] = img_dst_buffer[y*width+x+1];
                row[2+(x*3)] = img_dst_buffer[y*width+x+2];
               }
            png_write_row(png_ptr, row);
            png_count ++;

            }
          */

        // End write
        //png_write_end(png_ptr, NULL);
        //free(row);
        //fclose(fp);
        //}
    
    // ENDING PNG LIB WRITE HERE!
        
    //free(flate_src);
    free(img_dst_buffer);
    
    // do not create as blok if save_yes != 1 !!!
    
    if (success_code > -1 && save_yes == 1 && (rgb == 1 || rgb == 2)) {
        master_new_images_added ++;
        strcpy(Bloks[global_blok_counter].relationship, img_name);
        strcpy(Bloks[global_blok_counter].content_type,"image");
        Bloks[global_blok_counter].position.cx = (int)height;
        Bloks[global_blok_counter].position.cy = (int)width;
        Bloks[global_blok_counter].position.x = Pages[page_number].image_x_coord[image_number];
        Bloks[global_blok_counter].position.y = Pages[page_number].image_y_coord[image_number];
    
        Bloks[global_blok_counter].slide_num = page_number;
        Bloks[global_blok_counter].shape_num = image_number;
    
        if (no_text_flag == 0) {
            strcpy(Bloks[global_blok_counter].table_text,"OCR_FLAG");
        }
        else {
            strcpy(Bloks[global_blok_counter].table_text,"");
        }
        
        strcpy(Bloks[global_blok_counter].linked_text,"");
        strcpy(Bloks[global_blok_counter].formatted_text,"");
        strcpy(Bloks[global_blok_counter].text_run,"");
    
        // look for nearby text in neighboring Blok from this Page
        
        image_text_len = nearby_text(master_page_blok_start, master_page_blok_stop, Pages[page_number].image_x_coord[image_number], Pages[page_number].image_y_coord[image_number], global_blok_counter);
    
        global_blok_counter ++;
        master_image_tracker ++;
        
        blok_added = 1;
        
    }
    

    return blok_added;

}


int image_handler_dct (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int cmyk_flag) {
    
    int blok_added = 0;
    
    int x,y;
    FILE *writeptr;
    unsigned char* local_buffer;
    unsigned char* rgb_tmp;
    
    int filelen;
    int m;
    int save_yes = 1;
    int slider = 0;
    int counter = 0;
    char image_account_directory[300];
    char img_name[100];
    int local_img_counter = 0;
    int image_text_len = 0;
    
    int i=0;
    int r,g,b;
    
    strcpy(image_account_directory,"");
    strcat(image_account_directory,global_image_fp);
    
    /* representative path:
    sprintf(image_account_directory, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
    */

    // important tracker
    //local_img_counter = master_image_tracker + master_new_images_added;
    local_img_counter = master_image_tracker;
    // end - tracker
    
    sprintf(img_name,"image%d_%d.jpg",master_doc_tracker, local_img_counter);
    strcat(image_account_directory,img_name);
    
    filelen = stream_stop - stream_start;
    local_buffer = (unsigned char *)malloc(filelen * sizeof(unsigned char)); // Enough memory for the file

    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        
                        break;
                    }}}}
    }
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    counter = 0;
    for (x=stream_start+slider; x < stream_stop; x++) {
        local_buffer[counter] = buffer[x];
        counter++;
    }
    
    if (counter < IMG_MIN_HxW) {
        save_yes = -1;
    }
        
    // now, need to write the file to the new location
    
    if (save_yes == 1) {
        
        if (cmyk_flag == 1) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - in image_handler_dct - found cmyk flag - will need to handle in post-processing \n");
            }
            
            rgb_tmp = (unsigned char *)malloc(3 * sizeof(unsigned char));

            writeptr = fopen(image_account_directory,"wb");
            
            // need to convert every 4 bytes 'CMYK' into 3 bytes RGB
            /*
            for (i=0; i < (counter / 4); i++) {
                rgb_tmp[0] = (int) (255 * (1 - (local_buffer[i*4]/100)) * (1 - (local_buffer[i*4+3]/100)));
                rgb_tmp[1] = (int) (255 * (1 - (local_buffer[i*4+1]/100)) * (1 - (local_buffer[i*4+3]/100)));
                rgb_tmp[2] = (int) (255 * (1 - (local_buffer[i*4+2]/100)) * (1 - (local_buffer[i*4+3]/100)));
                
                fwrite(rgb_tmp,1,3,writeptr);
            }
            */
            /*
            for (i=0; i < counter; i++) {
                local_buffer[i] = 255 - local_buffer[i];
            }
             */
            
            fwrite(local_buffer,counter,1,writeptr);
            fclose(writeptr);
            free(local_buffer);
        }
        
        else {
            
            writeptr = fopen(image_account_directory,"wb");
            fwrite(local_buffer, counter,1, writeptr);
            fclose(writeptr);
            free(local_buffer);
        }
        
        // only write to blok if save_yes = 1
    
        master_new_images_added ++;
        strcpy(Bloks[global_blok_counter].relationship, img_name);
        strcpy(Bloks[global_blok_counter].content_type,"image");
        Bloks[global_blok_counter].position.cx = Pages[page_number].image_cx_coord[image_number];
        Bloks[global_blok_counter].position.cy = Pages[page_number].image_cy_coord[image_number];
        Bloks[global_blok_counter].position.x = Pages[page_number].image_x_coord[image_number];
        Bloks[global_blok_counter].position.y = Pages[page_number].image_y_coord[image_number];

        Bloks[global_blok_counter].slide_num = page_number;
        
        if (cmyk_flag == 1) {
            Bloks[global_blok_counter].shape_num = -3;
        }
        else {
            Bloks[global_blok_counter].shape_num = image_number;
        }
        
        if (no_text_flag == 0) {
            strcpy(Bloks[global_blok_counter].table_text,"OCR_FLAG");
        }
        else {
            strcpy(Bloks[global_blok_counter].table_text,"");
        }
        
        //strcpy(Bloks[global_blok_counter].table_text,"");
        
        strcpy(Bloks[global_blok_counter].linked_text,"");
        strcpy(Bloks[global_blok_counter].formatted_text,"");
        strcpy(Bloks[global_blok_counter].text_run,"");
    
        // look for nearby text in neighboring Blok from this Page
        
        image_text_len = nearby_text(master_page_blok_start, master_page_blok_stop, Pages[page_number].image_x_coord[image_number], Pages[page_number].image_y_coord[image_number], global_blok_counter);
    
        global_blok_counter ++;
        master_image_tracker ++;
        
        blok_added = 1;
        
    }
    else {
        free(local_buffer);
    }
    
    return blok_added;
}



int image_handler_ccitt (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int height, int width) {
    
    int blok_added = 0;
    
    int x,y;
    FILE *writeptr;
    unsigned char* local_buffer;
    int filelen;
    int m;
    int save_yes = 1;
    int slider = 0;
    int counter = 0;
    char image_account_directory[300];
    char img_name[100];
    int local_img_counter = 0;
    int image_text_len = 0;
    
    // assumption - 1, 3, or 4 channel ?
    int sampleperpixel = 1;
    int row = 0;
    
    strcpy(image_account_directory,"");
    strcat(image_account_directory,global_image_fp);
    
    /* representative path:
    sprintf(image_account_directory, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
    */

    // important tracker
    //local_img_counter = master_image_tracker + master_new_images_added;
    local_img_counter = master_image_tracker;
    // end - tracker
    
    sprintf(img_name,"image%d_%d.tiff",master_doc_tracker, local_img_counter);
    strcat(image_account_directory,img_name);
    
    filelen = stream_stop - stream_start;
    local_buffer = (unsigned char *)malloc(filelen * sizeof(unsigned char)); // Enough memory for the file

    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        
                        break;
                    }}}}
    }
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    counter = 0;
    for (x=stream_start+slider; x < stream_stop; x++) {
        local_buffer[counter] = buffer[x];
        counter++;
    }
    
    if (counter < IMG_MIN_HxW) {
        save_yes = -1;
    }
        
    // now, need to write the file to the new location
    
    if (save_yes == 1) {
        
        // start here to write and save the TIFF file
        
        /*
        TIFF *out= TIFFOpen(image_account_directory, "w");
        
        // add TIFF header
        TIFFSetField (out, TIFFTAG_IMAGEWIDTH, width);  // set the width of the image
        TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);    // set the height of the image
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, sampleperpixel);   // set number of channels per pixel
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);    // set the size of the channels
        TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);    // set the origin of the image.
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        
        //tsize_t linebytes = sampleperpixel * width;

        TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(out, width*sampleperpixel));
        
        //TIFFWriteRawTile(out,1,local_buffer,counter);
        
        //TIFFWriteScanLine(out,local_buffer,1,0);
        
        printf("update: ccitt counter - %d-%d-%d \n", counter, height,width);
        
        TIFFWriteTile(out,local_buffer,1,1,1,0);
        
        
        TIFFClose(out);
         */
        
        
        
        writeptr = fopen(image_account_directory,"wb");
        fwrite(local_buffer, counter,1, writeptr);
        fclose(writeptr);
        
        free(local_buffer);
        
        // only write to blok if save_yes = 1
    
        master_new_images_added ++;
        strcpy(Bloks[global_blok_counter].relationship, img_name);
        strcpy(Bloks[global_blok_counter].content_type,"image");
        Bloks[global_blok_counter].position.cx = Pages[page_number].image_cx_coord[image_number];
        Bloks[global_blok_counter].position.cy = Pages[page_number].image_cy_coord[image_number];
        Bloks[global_blok_counter].position.x = Pages[page_number].image_x_coord[image_number];
        Bloks[global_blok_counter].position.y = Pages[page_number].image_y_coord[image_number];

        //printf("update- check in ccitt - height - width - %d-%d \n", Pages[page_number].image_cx_coord[image_number], Pages[page_number].image_cy_coord[image_number]);
        
        Bloks[global_blok_counter].slide_num = page_number;
        Bloks[global_blok_counter].shape_num = image_number;

        if (no_text_flag == 0) {
            strcpy(Bloks[global_blok_counter].table_text,"OCR_FLAG");
        }
        else {
            strcpy(Bloks[global_blok_counter].table_text,"");
        }
        
        //strcpy(Bloks[global_blok_counter].table_text,"");
        
        strcpy(Bloks[global_blok_counter].linked_text,"");
        strcpy(Bloks[global_blok_counter].formatted_text,"");
        strcpy(Bloks[global_blok_counter].text_run,"");
    
        // look for nearby text in neighboring Blok from this Page
        
        image_text_len = nearby_text(master_page_blok_start, master_page_blok_stop, Pages[page_number].image_x_coord[image_number], Pages[page_number].image_y_coord[image_number], global_blok_counter);
    
        global_blok_counter ++;
        master_image_tracker ++;
        
        blok_added = 1;
        
    }
    else {
        free(local_buffer);
    }
    
    return blok_added;
}




int image_handler_jpx (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag) {
    
    int blok_added = 0;
    
    int x,y;
    FILE *writeptr;
    unsigned char* local_buffer;
    int filelen;
    int m;
    int save_yes = 1;
    int slider = 0;
    int counter = 0;
    char image_account_directory[300];
    char img_name[100];
    int local_img_counter = 0;
    int image_text_len = 0;
    
    strcpy(image_account_directory,"");
    strcat(image_account_directory,global_image_fp);
    
    /* representative path:
    sprintf(image_account_directory, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
    */

    // important tracker
    //local_img_counter = master_image_tracker + master_new_images_added;
    local_img_counter = master_image_tracker;
    // end - tracker
    
    sprintf(img_name,"image%d_%d.jpx",master_doc_tracker,local_img_counter);
    strcat(image_account_directory,img_name);
    
    filelen = stream_stop - stream_start;
    local_buffer = (unsigned char *)malloc(filelen * sizeof(unsigned char)); // Enough memory for the file

    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        
                        break;
                    }}}}
    }
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    counter = 0;
    for (x=stream_start+slider; x < stream_stop; x++) {
        local_buffer[counter] = buffer[x];
        counter++;
    }
    
    if (counter < IMG_MIN_HxW) {
        save_yes = -1;
    }
        
    // now, need to write the file to the new location
    
    if (save_yes == 1) {
        
        writeptr = fopen(image_account_directory,"wb");
        fwrite(local_buffer, counter,1, writeptr);
        fclose(writeptr);
        free(local_buffer);
        
        // only write to blok if save_yes = 1
    
        master_new_images_added ++;
        strcpy(Bloks[global_blok_counter].relationship, img_name);
        strcpy(Bloks[global_blok_counter].content_type,"image");
        Bloks[global_blok_counter].position.cx = Pages[page_number].image_cx_coord[image_number];
        Bloks[global_blok_counter].position.cy = Pages[page_number].image_cy_coord[image_number];
        Bloks[global_blok_counter].position.x = Pages[page_number].image_x_coord[image_number];
        Bloks[global_blok_counter].position.y = Pages[page_number].image_y_coord[image_number];

        Bloks[global_blok_counter].slide_num = page_number;
        Bloks[global_blok_counter].shape_num = image_number;

        if (no_text_flag == 0) {
            strcpy(Bloks[global_blok_counter].table_text,"OCR_FLAG");
        }
        else {
            strcpy(Bloks[global_blok_counter].table_text,"");
        }
        
        //strcpy(Bloks[global_blok_counter].table_text,"");
        
        strcpy(Bloks[global_blok_counter].linked_text,"");
        strcpy(Bloks[global_blok_counter].formatted_text,"");
        strcpy(Bloks[global_blok_counter].text_run,"");
    
        // look for nearby text in neighboring Blok from this Page
        
        image_text_len = nearby_text(master_page_blok_start, master_page_blok_stop, Pages[page_number].image_x_coord[image_number], Pages[page_number].image_y_coord[image_number], global_blok_counter);
    
        global_blok_counter ++;
        master_image_tracker ++;
        
        blok_added = 1;
        
    }
    else {
        free(local_buffer);
    }
    
    return blok_added;
}



int image_handler_jbig2 (int stream_start, int stream_stop, int global_ref_num, char*account, char*corpus, int page_number, int image_number, int no_text_flag, int height, int width) {
        
    int blok_added = 0;
    
    int x,y;
    FILE *writeptr;
    unsigned char* local_buffer;
    int filelen;
    int m;
    int save_yes = 1;
    int slider = 0;
    int counter = 0;
    char image_account_directory[300];
    char img_name[100];
    int local_img_counter = 0;
    int image_text_len = 0;
    
    strcpy(image_account_directory,"");
    strcat(image_account_directory,global_image_fp);
    
    /* representative path:
    sprintf(image_account_directory, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
    */

    // important tracker
    //local_img_counter = master_image_tracker + master_new_images_added;
    local_img_counter = master_image_tracker;
    // end - tracker
    
    sprintf(img_name,"image%d_%d.jbig",master_doc_tracker,local_img_counter);
    strcat(image_account_directory,img_name);
    
    filelen = stream_stop - stream_start;
    local_buffer = (unsigned char *)malloc(filelen * sizeof(unsigned char)); // Enough memory for the file

    if ((stream_stop - stream_start) > 15) {
        m = 15;
    }
    else {
        m = stream_stop - stream_start;
    }
    
    for (y=0; y< m; y++) {
        if (buffer[stream_start+y] == 115) {
            if ((y+2) < m) {
                if (buffer[stream_start+y+1] == 116) {
                    if (buffer[stream_start+y+2] == 114) {
                        slider = y+6;
                        
                        break;
                    }}}}
    }
    
    // potential for 1 or 2 markers - 13/10 - skip past them
    
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    if (buffer[stream_start+slider] == 10 || buffer[stream_start+slider] == 13) {
        slider ++;
    }
    
    counter = 0;
    for (x=stream_start+slider; x < stream_stop; x++) {
        local_buffer[counter] = buffer[x];
        counter++;
    }
    
    if (counter < IMG_MIN_HxW) {
        save_yes = -1;
    }
        
    // now, need to write the file to the new location
    
    if (save_yes == 1) {
        
        writeptr = fopen(image_account_directory,"wb");
        fwrite(local_buffer, counter,1, writeptr);
        fclose(writeptr);
        free(local_buffer);
        
        // only write to blok if save_yes = 1
    
        master_new_images_added ++;
        strcpy(Bloks[global_blok_counter].relationship, img_name);
        strcpy(Bloks[global_blok_counter].content_type,"image");
        Bloks[global_blok_counter].position.cx = Pages[page_number].image_cx_coord[image_number];
        Bloks[global_blok_counter].position.cy = Pages[page_number].image_cy_coord[image_number];
        Bloks[global_blok_counter].position.x = Pages[page_number].image_x_coord[image_number];
        Bloks[global_blok_counter].position.y = Pages[page_number].image_y_coord[image_number];

        Bloks[global_blok_counter].slide_num = page_number;
        Bloks[global_blok_counter].shape_num = image_number;

        if (no_text_flag == 0) {
            strcpy(Bloks[global_blok_counter].table_text,"OCR_FLAG");
        }
        else {
            strcpy(Bloks[global_blok_counter].table_text,"");
        }
        
        //strcpy(Bloks[global_blok_counter].table_text,"");
        
        strcpy(Bloks[global_blok_counter].linked_text,"");
        strcpy(Bloks[global_blok_counter].formatted_text,"");
        strcpy(Bloks[global_blok_counter].text_run,"");
    
        // look for nearby text in neighboring Blok from this Page
        
        image_text_len = nearby_text(master_page_blok_start, master_page_blok_stop, Pages[page_number].image_x_coord[image_number], Pages[page_number].image_y_coord[image_number], global_blok_counter);
    
        global_blok_counter ++;
        master_image_tracker ++;
        
        blok_added = 1;
        
    }
    else {
        free(local_buffer);
    }
    
    return blok_added;
}


int metadata_handler (int dict_start, int dict_stop) {
    
    int x,f,g,h;
    
    int meta_start;
    int meta_stop;
    int s1,s2,s3,s4;
    int m1,m2;
    
    char* meta_str;
    char* xml_str = NULL;
    slice* m_slice;
    
    int l = 0;
    int author_match = 0;
    int text_on = 0;
    char author_text[1000];
    char modify_date_text[100];
    char create_date_text[100];
    char creator_tool_text[500];
    char tmp[100];
    
    int md_match = 0;
    int cd_match = 0;
    int ct_match = 0;
    int field_len_max = 50;
    
    int start_tag = 0;
    int end_tag = 0;
    
    strcpy(author_text,"");
    strcpy(modify_date_text,"");
    strcpy(create_date_text,"");
    strcpy(creator_tool_text,"");
    strcpy(tmp,"");
    
    // look for Metadata 10 0 R
    
    m_slice = dict_find_key_value_buffer(dict_start,dict_stop,metadata_seq,8);
    meta_start = m_slice->value_start;
    meta_stop = m_slice->value_stop;
    
    //  useful for debugging:  printf("update: slicing meta_start and meta_stop -%d -%d \n", meta_start,meta_stop);
    
    if (meta_start > -1 && meta_stop > -1) {
        
        x = extract_obj_from_buffer(meta_start,meta_stop);
        
        // updating - replacing old check:  x > -1
        // with correct check:   x > 0
        
        if (x > 0) {
            
            m1 = nn_global_tmp[0].num;
            
            if (m1 > -1) {
                
                m2 = get_obj(m1);
                
                if (m2 > -1) {
                    
                    s1 = global_tmp_obj.dict_start;
                    s2 = global_tmp_obj.dict_stop;
                    
                    if (debug_mode == 1) {
                        if (s1 > -1 && s2 > -1) {
                            //meta_str = get_string_from_buffer(s1,s2);
                            //printf("update: Meta Dict Str - %s \n", meta_str);
                        }}
                    
                    s3 = global_tmp_obj.stream_start;
                    s4 = global_tmp_obj.stop;
                    
                    // useful for debugging:   printf("update: stream start- %d - %d \n", s3,s4);
                    
                    if (s3 > -1 && s4 > -1) {
                        xml_str = get_string_from_buffer(s3,s4);
                        
                        //  note: not always, but often times the metadata is an XML stream
                        
                        if (debug_mode == 1) {
                            //printf("update: XML stream - %s \n", xml_str);
                        }
                        
                        l = strlen(xml_str);
                    }
                    else {
                        
                        if (debug_mode == 1) {
                            printf("update: pdf_parser - xml meta stream not found - may not be able to extract key file metadata. \n");
                        }
                        
                        l = 0;
                        
                    }
                    
                    for (f=0; f < l; f++) {
                        
                        // printf("%c", xml_str[f]);

                        if (xml_str[f] == 60 && ((f+16 < l))) {
                            
                            // found '<' -> marker to look for extractd selected xml keys
                            
                            md_match = 1;
                            cd_match = 1;
                            ct_match = 1;
                            author_match = 1;
                            
                            // first - look for "<xmp:ModifyDate>"
                            
                            for (g=1; g < 16; g++) {
                                
                                if (xml_str[f+g] == xmp_modify_date_seq[g]) {
                                    md_match ++;
                                }
                                else {
                                    md_match = 0;
                                    break;
                                }
                            }
                            
                            if (md_match == 16 && (16+f <l)) {
                                
                                //  printf("update: found modify_date @ %d \n", f);
                                
                                if (l > (f+16+50)) {
                                    field_len_max = 50 + f + 16;
                                }
                                else {
                                    field_len_max = l-1;
                                }
                                for (h=f+16; h < field_len_max; h++) {
                                    
                                    if (xml_str[h] == 60) {
                                        break;
                                    }
                                    else {
                                        
                                        if (xml_str[h] > 31 && xml_str[h] <= 128) {
                                            sprintf(tmp,"%c",xml_str[h]);
                                            strcat(modify_date_text,tmp);
                                        }
                                    }
                                }
                                
                                //  printf("update: modify_date = %s \n", modify_date_text);
                            }
                            
                            // second - look for "<xml:CreateDate>"
                            
                            for (g=1; g < 16; g++) {
                                
                                if (xml_str[f+g] == xmp_create_date_seq[g]) {
                                    cd_match ++;
                                }
                                else {
                                    cd_match = 0;
                                    break;
                                }
                            }
                            
                            if (cd_match == 16 && (16+f < l)) {
                                
                                // printf("update: found create_date @ %d \n", f);
                                
                                if (l > (f+16+50)) {
                                    field_len_max = 50 + f + 16;
                                }
                                else {
                                    field_len_max = l-1;
                                }
                                for (h=f+16; h < field_len_max; h++) {
                                    
                                    if (xml_str[h] == 60) {
                                        break;
                                    }
                                    else {
                                        
                                        if (xml_str[h] > 31 && xml_str[h] <= 128) {
                                            sprintf(tmp,"%c",xml_str[h]);
                                            strcat(create_date_text,tmp);
                                        }
                                    }
                                }
                                
                                // printf("update: create_date = %s \n", create_date_text);
                                
                            }
                            
                            // third - look for "<xmp:CreatorTool>"
                            
                            for (g=1; g < 17; g++) {
                                
                                if (xml_str[f+g] == xmp_creator_tool_seq[g]) {
                                    ct_match ++;
                                }
                                else {
                                    ct_match = 0;
                                    break;
                                }
                            }
                            
                            if (ct_match == 17 && (17+f < l)) {
                                
                                // printf("update: found creator_tool found @ %d \n", f);
                                
                                if (l > (f+17+100)) {
                                    field_len_max = 100 + f + 17;
                                }
                                else {
                                    field_len_max = l-1;
                                }
                                for (h=f+17; h < field_len_max; h++) {
                                    
                                    if (xml_str[h] == 60) {
                                        break;
                                    }
                                    else {
                                        
                                        if (xml_str[h] > 31 && xml_str[h] <= 128) {
                                            sprintf(tmp,"%c",xml_str[h]);
                                            strcat(creator_tool_text,tmp);
                                        }
                                    }
                                }
                                
                                // printf("update: creator tool = %s \n", creator_tool_text);
                                
                            }
                            
                            
                            // fourth - look for "<dc:creator>"
                            
                            for (g=1; g < 12; g++) {
                                
                                if (xml_str[f+g] == dccreator_seq[g]) {
                                    author_match ++;
                                }
                                else {
                                    author_match = 0;
                                    break;
                                }
                            }
                            
                            if (author_match == 12 && (12+f < l)) {
                                
                                // printf("update: author found @ %d \n", f);
                                
                                // look at wider look-ahead window
                                // may include a few other tags that will be ignored
                                
                                if (l > (f+12+100)) {
                                    field_len_max = 100 + f + 12;
                                }
                                else {
                                    field_len_max = l-1;
                                }
                                
                                text_on = 0;
                                start_tag = 0;
                                end_tag = 0;
                                
                                for (h=f+12; h < field_len_max; h++) {
                                    
                                    // look for end of > - and then get plain text value
                                    
                                    if (end_tag == 1 && xml_str[h] != 13 && xml_str[h] != 10 && xml_str[h] != 60 && xml_str[h] != 32) {
                                        
                                        // end_tag reached -> not inside a tag
                                        // if not start of new tag (e.g., < or 10/13)
                                        // avoid turning on text with a 32 space as well
                                        
                                        text_on = 1;
                                    }
                                    
                                    if (xml_str[h] == 60) {
                                        start_tag = 1;
                                        end_tag = 0;
                                        if (text_on == 1) {
                                            // reached the end of text field
                                            text_on = 0;
                                            break;
                                        }
                                    }
                                
                                    else {
                                        
                                        if (text_on == 1) {
                                            
                                            if (xml_str[h] > 31 && xml_str[h] <= 128 && xml_str[h] != 62) {
                                                sprintf(tmp, "%c",xml_str[h]);
                                                
                                                if (strlen(author_text) < 50) {
                                                    strcat(author_text,tmp);
                                                }
                                                else {
                                                    // likely error
                                                    // printf("error: metadata_handler - author name longer than 50 char - %s \n", author_text);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    
                                    if (xml_str[h] == 62) {
                                        start_tag = 0;
                                        end_tag = 1;
                                    }
                                    
 
                                }
                                
                                // printf("update: author = %s \n", author_text);
                                
                            }
                        
                            
                        }
                        
                        /*
                        if (xml_str[f] == dccreator_seq[0] && ((f+11) < l)) {
                            author_match = 1;
                            for (g=1 ; g < 11; g++) {
                                if (xml_str[f+g] == dccreator_seq[g]) {
                                    author_match ++;
                                }
                                else {
                                    author_match = 0;
                                    break;
                                }
                                if (author_match == 11 && ((11+f) <l)) {
                                    text_on = 1;
                                    for (h=11+f; h < l ; h++) {
                                        
                                        if (xml_str[h] == 60 && text_on < 99) {
                                            
                                            if (text_on == 2) {
                                                
                                                text_on = 99;
                                                break;
                                            }
                                            text_on = 0;
                                        }
                                        if (xml_str[h] == 62 && text_on < 99) {
                                            text_on = 1;
                                        }
                                        if (text_on > 0 & text_on < 99 && xml_str[h] != 62) {
                                            
                                            if ((xml_str[h] != 10 && xml_str[h] !=13 && xml_str[h] !=32) || (text_on==2 && xml_str[h] !=10 && xml_str[h] !=13)) {
                                                strncat(author_text,&xml_str[h],1);
                                                text_on = 2;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        */
                    }
                }
                
                
            }
        
        }}
    

    // finished processing metadata stream
    
    if (strlen(author_text) > 0) {
        strcpy(my_doc.author, author_text);
        
        if (debug_mode == 1) {
            //printf("update: pdf metadata_handler - author - %s \n", author_text);
        }
    }
    else {
        strcpy(my_doc.author,"");
    }
    
    if (strlen(create_date_text) > 0) {
        strcpy(my_doc.create_date, create_date_text);
        
        if (debug_mode == 1) {
            //printf("update: pdf metadata_handler - create_date - %s \n", create_date_text);
        }
    }
    else {
        strcpy(my_doc.create_date,"");
    }
    
    
    if (strlen(modify_date_text) > 0) {
        strcpy(my_doc.modify_date, modify_date_text);
        
        if (debug_mode == 1) {
            //printf("update: pdf metadata_handler - modify_date - %s \n", modify_date_text);
        }
    }
    else {
        strcpy(my_doc.modify_date,"");
    }
    
    
    if (strlen(creator_tool_text) > 0 ) {
        strcpy(my_doc.creator_tool, creator_tool_text);
        
        if (debug_mode == 1) {
            //printf("update: pdf metadata_handler - creator_tool - %s \n", creator_tool_text);
        }
    }
    else {
        strcpy(my_doc.creator_tool,"");
    }
    
    return 0;
}







int pdf_builder (char *fp, char *account, char *corpus, char *time_stamp) {
    
    // Section 1 variables - Create Buffer + Get Objects
    
    //int pi1;
    
    FILE *fileptr;
    
    int filelen;
    int obj_created = 0;
    int objstm_hidden_created = 0;
    int x,z;
    
    int start,stop, stream_start, stream_stop;
    char* img_dict_str;
    char* catalog_dict_str;
    
    // Section 2 variables - Special Handler for Objstrm
    
    int objstm_found = -1;
    int catalog_found = -1;
    
    // Section 3 variables - Page object
    
    int w1;
    
    int page_success= 0;
    int my_content_count = 0;
    int my_content = 0;
    int x2, s1,s2, f, x3,i, i1,i2,i3,i4,s3,s4;
    
    int blok_added_on_page = 0;
    
    int i5 = 0;
    
    int content_obj_found = 0;
    int stream = 0;
    int image_obj_found = 0;
    int img_type = 0;
    
    int encrypt_found = -1;
    char* encrypt_dict_str;
    
    int meta = -1;
    
    slice* height_slice;
    slice* width_slice;
    
    // added new here - to iterate on form xobjects
    slice* nested_xobject_slice;
    int nxs1,nxs2, nxs3, nxs4;
    int y1=0;
    int y2=0;
    int nested_image_obj_found = 0;
    int nested_form_img_running_total = 0;
    int image_local_count = 0;
    
    int n1,n2,n3,n4;
    
    char test_fp[200];
    
    // end - new here
    
    int img_stream_start, img_stream_stop;
    
    int img_cmyk_on = 0;
    int colorspace_on = 0;
    int color_start = -1;
    int color_stop = -1;
    char *color_str;
    int color_obj_found = 0;
    int cs1 = 0;
    int cs2 = 0;
    int cs3 = 0;
    int cs4 = 0;
    
    slice* colorspace_slice;
    slice* cs_obj_slice;
    
    int height;
    int width;
    
    int bloks_start_of_new_doc;
    int total_bloks_added;
    int images_start_of_new_doc;
    int post_page_confirm_dummy;
    
    // Note:  MAX safety check for hidden compressed buffers
    //  --capped at 30M -> very rare to find > 1-2M
    //  --may need additional checks for safety and size
    
    //int hidden_buffer_adder = 30000000;
    
    int hidden_buffer_adder = 40000000;
    
    
    //int test_var = 0;
    
    // insert new var - dummy for database write output
    int dummy = 0;
    // end - insert new
    
    // restart with each new pdf file
    global_obj_counter = 0;
    
    // start global_page_count at first page = Page 1, not Page 0
    //global_page_count = 0;
    global_page_count = 0;
    
    global_font_count = 0;
    global_blok_counter = 0;
    master_new_images_added = 0;
    master_new_bloks_added = 0;
    
    global_text_found = 0;
    
    global_blok_save_off = -1;
    
    // new variable to track unhandled images -> indicator of need to run OCR
    global_unhandled_img_counter = 0;
    
    // experiment starts here
    //memset(Pages,0,sizeof(Pages[0]) *2005);
    //memset(Font_CMAP,0,sizeof(Font_CMAP[0]) * 10000);
    
    // experiment ends here
    
    // reset global_headlines for each doc
    strcpy(global_headlines,"");
    
    if (debug_mode == 1) {
        printf("update: pdf_parser - START NEW PDF Processing - file path-%s \n", fp);
    }
    
    fileptr = fopen(fp, "rb");
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);             // Get the current byte offset in the file
    rewind(fileptr);                      // Jump back to the beginning of the file

    global_buffer_cursor = filelen; // initialize cursor at end of main buffer
    
    buffer = (unsigned char *)malloc((filelen + hidden_buffer_adder) * sizeof(char)); // Enough memory for the file

    fread(buffer, filelen, 1, fileptr); // Read in the entire file
    
    fclose(fileptr);
    
    // printf("update:  in open pdf handler- byte count size-%d \n", filelen+ hidden_buffer_adder);
    //printf("core file len - %d \n", filelen);
    
    // Step 1 - Get Objects - obj[] master list + global_obj_counter
    
    obj_created = build_obj_master_list(filelen);
        
    if (debug_mode == 1) {
        printf("update: pdf_parser - build_obj_master_list - obj created - %d \n", obj_created);
    }
    
    // Step 2 - look for ObjStm and handle
    
    catalog_found = - 1;
    
    for (x=0; x < obj_created; x++) {
        start = obj[x].dict_start;
        stop = obj[x].dict_stop;
        
        //printf("update: obj iter - %d - %d - %d \n", x, start, stop);
        
        objstm_found = dict_search_buffer(start,stop,objstm_seq,6);
        
        if (objstm_found == 1) {
            stream_start = obj[x].stream_start;
            stream_stop = obj[x].stop;
            
            //printf("update: going down objstm path - %d - %d \n", stream_start, stream_stop);
            
            // objstm are objects that need to be inflated
            // note: very rough check used below - unlikely to be triggered, but ...
            //  ... may need more sophisticated size cap checking
            
            //printf("update: objstm_found - %d-%s \n", x, get_string_from_buffer(start,stop));
            
            if (global_buffer_cursor < filelen + 39000000) {
                
                objstm_hidden_created += objstm_handler(stream_start,stream_stop);
                
                //printf("objstm created - %d - %d \n", x,objstm_hidden_created);

            }
            
            else {
                printf("warning: pdf_parser - stopped adding hidden obj for safety - too big - may lead to missing content or broken links - %d \n", global_buffer_cursor);
            }
        }
        
        if (catalog_found < 0) {
            catalog_found = dict_search_buffer(start,stop, catalog_seq,7);

            if (catalog_found == 1) {
                catalog.obj_num = obj[x].obj_num;
                catalog.gen_num = obj[x].gen_num;
                catalog.start = obj[x].start;
                catalog.stop = obj[x].stop;
                catalog.dict_start = obj[x].dict_start;
                catalog.dict_stop = obj[x].dict_stop;
                catalog.stream_start = obj[x].stream_start;
                catalog.hidden = 0;
                
                if (debug_mode == 1) {
                    catalog_dict_str = get_string_from_buffer(catalog.dict_start,catalog.dict_stop);
                    printf("update: pdf_parser - Catalog Dict - %s \n", catalog_dict_str);
                }
                
                meta = metadata_handler (catalog.dict_start, catalog.dict_stop);
                }
        }
        

        if (encrypt_found < 0) {
            
            encrypt_found = dict_search_buffer(start,stop,encrypt_seq,8);
            
            //  Note: if the PDF file is encrypted, we do not support parsing
            //      -- encrypted files are possible to parse, but need to develop special handling
            //      -- fairly rare case, but there are a small number of PDFs that are encrypted
            //
            
            if (encrypt_found == 1) {
                //printf("error:  pdf_parser - FOUND ENCRYPT! \n");
                encrypt_dict_str = get_string_from_buffer(obj[x].dict_start,obj[x].dict_stop);
                printf("error: pdf_parser - Encrypt Dict - %s \n", encrypt_dict_str);
                // will need to keep encrypt dict - and go to encrypt obj
                printf("error: pdf_parser - returning error code - can not process further since file encrypted. \n");
                total_bloks_added = -2;
                
                free(buffer);
                
                return total_bloks_added;
            }
            else {
                // look for /encrypt in trailer
                if (filelen > 300) {
                    start = filelen - 300;
                }
                else {
                    start = 0;
                }
                
                if (filelen > 5) {
                    stop = filelen-5;
                    encrypt_found = dict_search_buffer(start,stop,encrypt_seq,8);
                    //encrypt_found = -1;
                    
                    if (encrypt_found == 1) {
                        printf("error: pdf_parser - found Encrypt in Trailer! \n");
                        printf("error: pdf_parser - returning error code - can not process further since file encrpyted. \n");
                        total_bloks_added = -2;
                        free(buffer);
                        return total_bloks_added;
                    }}
                }
        }
        }
    
     
    // key overall updates on pdf file structure, objects found, catalog + objstm found
    

    if (debug_mode == 1) {
        
        printf("update: pdf_parser - filelen - %d \n", filelen);
        printf("update: pdf_parser - created additional hidden objstm objects - %d \n", objstm_hidden_created);
            
        //printf("update: pdf_parser - catalog found = %d \n", catalog_found);
        //printf("update: pdf_parser - global cursor - %d \n", global_buffer_cursor);
        //printf("update: pdf_parser - global cursor - %d - %d \n", (global_buffer_cursor - filelen), filelen);
    }
    
    // useful for debugging and getting a view of selection of identified objects + dictionaries
    
    // display list of obj dictionaries -> note can be very large
    // default debug_mode to show small sample only
    
    
    
    if (debug_mode == 1) {
        
        /*
        
        for (x=0 ; x < global_obj_counter; x++) {
        
            // display only up to this object
        
            if (x < 1400) {
                
                if (obj[x].obj_num >= 0 && obj[x].obj_num < 2000) {
                    printf("update:  Objs found - %d-%d-%d-%s-%d-%d-%d-%d \n",x, obj[x].obj_num,obj[x].hidden, get_string_from_buffer(obj[x].dict_start,obj[x].dict_stop),obj[x].dict_start,obj[x].dict_stop,obj[x].start,obj[x].stop);
                }
                
                }
            else {
                break;
            }
        
        }
        */
    }
    
    
    // Step 3 - Catalog to Page Mapping
    
    // do a final check for the catalog in objstm
    
    if (catalog_found < 1) {
        
        if (objstm_hidden_created > 0 ) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - unusual_case - searching in objstm for catalog: %d-%d \n", obj_created,global_obj_counter);
            }
            
            for (x=obj_created-1; x <global_obj_counter; x++) {
            
                start = obj[x].dict_start;
                stop = obj[x].dict_stop;
                catalog_found = dict_search_buffer(start,stop, catalog_seq,7);

                if (catalog_found == 1) {
                    catalog.obj_num = obj[x].obj_num;
                    catalog.gen_num = obj[x].gen_num;
                    catalog.start = obj[x].start;
                    catalog.stop = obj[x].stop;
                    catalog.dict_start = obj[x].dict_start;
                    catalog.dict_stop = obj[x].dict_stop;
                    catalog.stream_start = obj[x].stream_start;
                    catalog.hidden = 0;
                    catalog_dict_str = get_string_from_buffer(catalog.dict_start,catalog.dict_stop);
                    
                    if (debug_mode == 1) {
                        printf("update: pdf_parser - unusual_case - success OK - found Catalog Dict in ObjStm - %s \n", catalog_dict_str);
                    }
                    
                    meta = metadata_handler (catalog.dict_start, catalog.dict_stop);
                    
                    break;
                }
            }}}
    
    
    
    if (catalog_found < 1) {
        
        total_bloks_added = -1;
        
        printf("error: pdf_parser - unusual - CATALOG NOT FOUND - returning error code - can not process further. \n");
        
        free(buffer);
        
        return total_bloks_added;
    }
    
    
    // Step 4 - Build Page Objects
    
    w1 = catalog_to_page_handler(catalog.dict_start,catalog.dict_stop);
    
    if (w1 == -1) {
        // caught an error - no further processing
        total_bloks_added = -1;
        free(buffer);
        
        return total_bloks_added;
        
    }
    
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - completed catalog_to_page_handler \n");
    }
    
    x = page_resource_handler(0);
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - completed page_resource_handler \n");
    }
    

    x = font_encoding_cmap_handler(0);
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - completed font_encoding_cmap_handler \n");
    }
       
    
    // Step 5 - Process Text Page-by-Page
    
    //printf("\nPage Text Processing - pages-%d \n", global_page_count);
    
    total_bloks_added = 0;
    
    master_new_images_added = 0;
    bloks_start_of_new_doc = global_blok_counter;
    images_start_of_new_doc = master_image_tracker;
    
    
    for (x=0; x < global_page_count; x++) {
        
        // Main processing loop - goes page-by-page
        // Core text & image processing handled by text_processor_page
        
        // looping through global page count
        // may need to confirm if page content is added first
        
        blok_added_on_page = 0;
        // global_total_pages_added ++;

        // end increment global_total_pages
        
        // safety check for max bloks created
        // if exceed 4500, then flip global_blok_save_off = 1
        // this limit should not get triggered
        
        if (global_blok_counter > 4500) {
            
            printf("error:  pdf_parser - global_block_counter - %d \n", global_blok_counter);
            global_blok_save_off = 1;
        }
        
        // insert iterative db save
        
        if (global_blok_counter > 1000) {
            
            // push bloks from array into db and reset global_blok_counter
            // no need to hold in memory - no linkage between bloks
            
            // note: implemented this 'iterative' write recently
            //      --needs more testing to confirm no issues
            //
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - global_block_counter > 1000 - writing to disk now - %d \n", global_blok_counter);
            }
            
            if (GLOBAL_WRITE_TO_DB == 1) {
            
                dummy = write_to_db(0,global_blok_counter,account,corpus,fp,master_doc_tracker,master_blok_tracker, time_stamp);
                
            }
            
            else {
                // new option insert
                
                //strcpy(test_fp,"bloks_out.txt");
                
                dummy = write_to_file (0, global_blok_counter, account, corpus, fp, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename);
            }
            
            
            master_blok_tracker = master_blok_tracker + global_blok_counter;
            global_blok_counter = 0;
        }
        
        // end - insert iterative db save
        
        
        // set blok count @ start of page
        // default for stop = start (e.g., no bloks created)
        master_page_blok_start = global_blok_counter;
        master_page_blok_stop = global_blok_counter;
        
        
        my_content_count = Pages[x].content_entries;
        
        // default for each page - start with page_success (e.g., new bloks created) set to 0
        page_success = 0;
        
        // start at each page fresh with reset of GLOBAL_PAGE_CMAP
        GLOBAL_PAGE_CMAP = -1;
        
        if (debug_mode == 1) {
            if (x >= 0 && x < 10) {
                printf("update: pdf_parser - PAGE PROCESSING-MAIN-LOOP -%d-content entries-%d \n", x,my_content_count);
            }
        }
        
        if (Pages[x].image_entries > 0) {
            for (z=0; z < Pages[x].image_entries ; z++) {
                
                if (debug_mode == 1) {
                    //printf("update:  Images on Page - %d - %s \n", z,Pages[x].image_names[z]);
                }
            }
        }

        if (my_content_count > 0) {
            
            for (x2=0; x2 < my_content_count; x2++){
                my_content = Pages[x].content[x2];
                content_obj_found = get_obj(my_content);
                
                if (debug_mode == 1) {
                    //printf("update: my content- %d - %d \n", my_content, content_obj_found);
                }
                
                if (content_obj_found > -1) {
                    s1 = global_tmp_obj.dict_start;
                    s2 = global_tmp_obj.dict_stop;
                    f = dict_search_buffer(s1,s2,flatedecode_seq,11);
                    
                
                    if (x == 0) {
                        //printf("update: content object - %d-%d-%s \n", x2, f, get_string_from_buffer(s1,s2));
                        //printf("update: stream start / stop - %d - %d \n", global_tmp_obj.stream_start, global_tmp_obj.stop);
                    }
                
                    
                    if (f > -1) {
                        
                        // Confirmed that this is a flate obj
                        // Need to inflate first before passing to text processor
                        
                        //printf("f > -1 scenario - %d \n", f);
                        
                        //printf("update: confirmed - flate - stream start / stop - %d - %d \n", global_tmp_obj.stream_start, global_tmp_obj.stop);
                        
                        stream = flate_handler_buffer_v2(global_tmp_obj.stream_start,global_tmp_obj.stop);
                        
                        if (debug_mode == 1) {
                            //printf("update: pdf_parser - in global page loop-%d-stream size-%d \n", x,stream);
                        }
                        
                        page_success = text_processor_page(stream,x);
                        
                        if (debug_mode == 1) {
                            //printf("update: pdf_parser - page_success (bloks created) - %d \n", page_success);
                        }
                        
                        total_bloks_added += page_success;
                        
                        blok_added_on_page += page_success;
                        
                        global_text_found += page_success;
                        
                        master_page_blok_stop = master_page_blok_start + page_success;
                        free(flate_dst_tmp_buffer);
                        

                    }
                    else {
                        
                        if (debug_mode == 1) {
                            //printf("update: found object, but not flatedecode - %s - skipping! \n", get_string_from_buffer(s1,s2));
                        }
                        
                        /*
                        printf("update: found object, but not flatedecode - %d-%d-%s \n", global_tmp_obj.stream_start,global_tmp_obj.stop, get_string_from_buffer(global_tmp_obj.stream_start,global_tmp_obj.stop));
                        
                        
                        stream = create_new_buffer (global_tmp_obj.stream_start,global_tmp_obj.stop);
                        
                        page_success = text_processor_page(stream,x);
                        
                        if (debug_mode == 1) {
                            //printf("update: page_success (bloks_created) - %d \n", page_success);
                        }
                        
                        total_bloks_added += page_success;
                        
                        blok_added_on_page += page_success;
                        
                        global_text_found += page_success;
                        
                        master_page_blok_stop = master_page_blok_start + page_success;
                        
                        free(flate_dst_tmp_buffer);
                        
                        */
                        
                    }
                    
                }
            }
        }
        
                
        //printf("update: going to image processing! \n");
        // check for Images on Page
        
        // add global parameter config option to 'turn off' image saving
        // checks global_image_save_on parameter
        //      if global_image_save_on == 1 -> save image = default case
        //      if global_image_save_on == any other value -> no images saved
        
        // correct test:   > 0
        
        if (debug_mode == 1) {
            //printf("update: on page - %d - images found - %d \n", x, Pages[x].image_entries);
        }
        
        if (Pages[x].image_entries > 0 && global_image_save_on == 1) {
            
            // there are images found on the page
            
            nested_form_img_running_total = 0;
            image_local_count = Pages[x].image_entries;
            
            for (x3=0; x3 < Pages[x].image_entries; x3++) {
                
                if (debug_mode == 1) {
                    //printf("update:  Image Processing - %d - %d \n", x3,Pages[x].image_entries);
                }
                
                // only do further image processing if image found on page!
                
                // test == 1
                // evaluating whether to 'relax' this requirement
                // alternative: check if page_success == 0 -> relax requirement in that case
                // there is mode with scanned image content + no text box
                
                if (Pages[x].image_found_on_page[x3] == 1 || page_success == 0) {
                    Pages[x].image_cx_coord[x3] = 0;
                    Pages[x].image_cy_coord[x3] = 0;
                    
                    // printf("update:  image found on page - confirmed! \n");
                            
                    image_obj_found = get_obj(Pages[x].images[x3]);
                    
                    //printf("obj found? %d \n", image_obj_found);
                    
                    if (image_obj_found > -1) {
                        
                        s3 = global_tmp_obj.dict_start;
                        s4 = global_tmp_obj.dict_stop;
                        
                        img_stream_start = global_tmp_obj.stream_start;
                        img_stream_stop = global_tmp_obj.stop;
                        
                        // can this line below be removed -> duplicative???
                        //img_dict_str = get_string_from_buffer(s3,s3+1000);
                        //printf("img dict sample - %s \n", img_dict_str);
                        
                        img_dict_str = get_string_from_buffer(s3,s4);
                        
                        if (debug_mode == 1) {
                            
                            //printf("update:  Image Name + Dict - %s-%s \n", Pages[x].image_names[x3], img_dict_str);
                        }
                        
                        // look for CMYK marker
                        img_cmyk_on = dict_search_buffer(s3,s4,device_cmyk_seq,10);
                        
                        /*
                        colorspace_on = dict_search_buffer(s3,s4,color_space_seq,10);
                        
                        if (colorspace_on > 0) {
                            
                            if (debug_mode == 1) {
                                printf("update: pdf_parser - FOUND COLORSPACE - WIP - need to get details and look for CMYK flag \n");
                            }
                          */
                        
                            //colorspace_slice = dict_find_key_value_buffer(s3,s4,color_space_seq,10);
                            //color_start = colorspace_slice->value_start;
                            //color_stop = colorspace_slice->value_stop;
                            
                            // problematic code
                            /*
                            if (color_start > - 1 && color_stop > -1) {
                                
                                if (debug_mode == 1) {
                                    //printf("%s \n", get_string_from_buffer(color_start,color_stop));
                                }
                                
                                // this is the problematic part
                                // ColorSpace value box can take a lot of different forms
                                // need a different approach
                                color_obj_found = extract_obj_from_buffer(color_start,color_stop);
                                

                                if (color_obj_found >= 1) {
                                    
                                    cs1 = get_obj(nn_global_tmp[0].num);
                                    
                                    if (cs1 > 0) {
                                        cs3 = global_tmp_obj.dict_start;
                                        cs4 = global_tmp_obj.dict_stop;
                                        
                                        cs2 = dict_search_buffer(cs3,cs4,device_cmyk_seq,10);
                                        
                                        if (cs2 > 0) {
                                            printf("update: found CMYK in ColorSpace! \n");
                                            img_cmyk_on = 1;
                                        }
                                    }}
                            }
                            */
                        //}
                        
                        img_type = 0;
                        
                        if (s3 > - 1 && s4 > -1) {
                            
                            i = dict_search_buffer(s3,s4, form_seq,4);
                            
                            // first check for Form with embedded XObjects
                            
                            if (i > -1) {
                                
                                // Note: WIP - need to improve FORM handling
                                
                                if (debug_mode == 1) {
                                    //printf("update: FOUND FORM - need to handle - %s ! \n", img_dict_str);
                                }
                                
                                // insert new here
                                
                                // step 1 - get stream attached to Form, if any
                                
                                i1 = dict_search_buffer(s3,s4,flatedecode_seq,11);
                                
                                if (i1 > 0) {
                                    
                                    if (debug_mode == 1) {
                                        //printf("update: found Form/Flate stream - process text in stream \n");
                                    }
                                    
                                    stream = flate_handler_buffer_v2(img_stream_start, img_stream_stop);
                                    
                                    //printf("update: stream = %d \n", stream);
                                    
                                    page_success = text_processor_page(stream,x);
                                    
                                    total_bloks_added += page_success;
                                    
                                    blok_added_on_page += page_success;
                                    
                                    global_text_found += page_success;
                                
                                    master_page_blok_stop = master_page_blok_start + page_success;
                                    
                                    free(flate_dst_tmp_buffer);
                                    
                                }
                                else {
                                    //printf("update: skipping Form stream - not Flate - %s \n", img_dict_str);
                                }
                                
                                // step 2 - look for nested xobject referenced in form dict
                                
                                nested_xobject_slice = dict_find_key_value_buffer(s3,s4,xobject_seq,7);
                                
                                nxs1 = nested_xobject_slice->value_start;
                                nxs2 = nested_xobject_slice->value_stop;
                                
                                if (nxs1 > -1 && nxs2 > -1) {
                                    
                                    //printf("update: value start-%d-stop-%d \n", nxs1,nxs2);
                                    //printf("update: value str - %s \n", get_string_from_buffer(nxs1,nxs2));
                                    
                                    i = extract_obj_from_buffer(nxs1,nxs2);
                                    
                                    //printf("update: found embedded images: %d \n", i);
                                    
                                    for (y1=0; y1 < i; y1++) {
                                            
                                        //printf("update: embedded in form image num/name found - %d - %d - %s \n", y1, nn_global_tmp[y1].num, nn_global_tmp[y1].name);
                                        
                                        nested_image_obj_found = get_obj(nn_global_tmp[y1].num);
                                        
                                        if (nested_image_obj_found > -1) {
                                            
                                            nxs3 = global_tmp_obj.dict_start;
                                            nxs4 = global_tmp_obj.dict_stop;
                                            
                                            img_stream_start = global_tmp_obj.stream_start;
                                            img_stream_stop = global_tmp_obj.stop;
                                            
                                            img_dict_str = get_string_from_buffer(nxs3,nxs4);
                                            
                                            //printf("update: Form-Embedded Image Dict: %s \n", img_dict_str);
                                            
                                            i1 = dict_search_buffer(nxs3,nxs4,flatedecode_seq,11);
                                            i2 = dict_search_buffer(nxs3,nxs4,dctdecode_seq,9);
                                        
                                            if (i1 > -1) {
                                                img_type = 1;
                                                //printf("update: found embedded flate image! \n");
                                            }
                                            else {
                                                i2 = dict_search_buffer(nxs3,nxs4,dctdecode_seq,9);
                                                if (i2 > -1) {
                                                    img_type = 2;
                                                    //printf("update: found embedded dctdecode image! \n");
                                            }}
                                
                                            height_slice = dict_find_key_value_buffer(nxs3,nxs4,height_seq,6);
                                            i3 = height_slice->value_start;
                                            i4 = height_slice->value_stop;
                                            if (i3 > -1 && i4 > -1) {
                                                height = get_int_from_buffer(i3,i4);}
                                            else {
                                                height = 0;}
                                            
                                            width_slice = dict_find_key_value_buffer(nxs3,nxs4,width_seq,5);
                                            i3 = width_slice->value_start;
                                            i4 = width_slice->value_stop;
                                            if (i3 > -1 && i4 > -1) {
                                                width = get_int_from_buffer(i3,i4);}
                                            else {
                                                width = 0;}
                                            
                                            //printf("update:  Image-Height-Width - %d-%d \n", height,width);
                                            Pages[x].images[image_local_count] = nn_global_tmp[y1].num;
                                            Pages[x].image_cx_coord[image_local_count] = height;
                                            Pages[x].image_cy_coord[image_local_count] = width;
                                            Pages[x].image_x_coord[image_local_count] = 0;
                                            Pages[x].image_y_coord[image_local_count] = 0;
                                            
                                            if (img_type == 1) {
                                                
                                                if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width >= IMG_MIN_HxW)) {
                                            
                                                    // change on july 12 - added parameter - dct_flag
                                                    // if dct_flag > -1 -> save as jpg, not ras
                                                
                                                    i5 = image_handler_flate(height,width,img_stream_start, img_stream_stop, master_image_tracker,account,corpus,x,image_local_count, i2,page_success, global_png_convert_on, img_cmyk_on);
                                                    
                                                    blok_added_on_page += i5;
                                                    
                                                }
                                            
                                                else {
                                                    //printf("update: HEIGHT = WIDTH = 0 for Flate - DO NOT SAVE! \n");
                                                    i5 = -1;
                                                }
                                            }
                                        
                                            if (img_type == 2) {
                                                
                                                if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width >= IMG_MIN_HxW)) {
                                                    
                                                    i5 = image_handler_dct(img_stream_start,img_stream_stop, master_image_tracker,account,corpus,x,image_local_count, page_success, img_cmyk_on);
                                                    
                                                    blok_added_on_page += i5;
                                                    
                                                }
                                            }
                                            
                                            if (img_type !=1 && img_type != 2) {
                                                i5 = -1;
                                            
                                                if (debug_mode == 1) {
                                                    //printf("error:  image_not_processed -  Image Name + Dict - %s-%s \n", Pages[x].image_names[image_local_count], img_dict_str);
                                                }

                                                Pages[x].image_cx_coord[image_local_count] = 0;
                                                Pages[x].image_cy_coord[image_local_count] = 0;
                                            }
                                            
                                            if (i5 > -1) {
                                            
                                                Pages[x].image_global_ref[image_local_count] = master_image_tracker;
                                            
                                                total_bloks_added ++;
                                                image_local_count ++;
                                                
                                                //printf("found + saved image - master tracker - %d \n",master_image_tracker);
                                            }
                                    }
                                }
                                    
                            }}
                            
                            else {
                                
                                // not a Form - process as Image
                                
                                i = dict_search_buffer(s3,s4,flatedecode_seq,11);
                                
                                // Define img_type - Flate = 1 // DCTDecode = 2;
                            
                                // change - july 12, 2022 - possible for FLATE + DCT
                            
                                i2 = dict_search_buffer(s3,s4,dctdecode_seq,9);
                            
                                // end - change
                            
                            
                                if (i > -1) {
                                    img_type = 1;
                                    //printf("update: found flate image! \n");
                                }
                                else {
                                    i2 = dict_search_buffer(s3,s4,dctdecode_seq,9);
                                    if (i2 > -1) {
                                        img_type = 2;
                                    //printf("update: found dctdecode image! \n");
                                }}
                    
                                height_slice = dict_find_key_value_buffer(s3,s4,height_seq,6);
                                i3 = height_slice->value_start;
                                i4 = height_slice->value_stop;
                                if (i3 > -1 && i4 > -1) {
                                    height = get_int_from_buffer(i3,i4);}
                                else {
                                    height = 0;}
                                
                                width_slice = dict_find_key_value_buffer(s3,s4,width_seq,5);
                                i3 = width_slice->value_start;
                                i4 = width_slice->value_stop;
                                if (i3 > -1 && i4 > -1) {
                                    width = get_int_from_buffer(i3,i4);}
                                else {
                                    width = 0;}
                                
                                
                                //printf("update:  Image-Height-Width - %d-%d \n", height,width);
                                Pages[x].image_cx_coord[x3] = height;
                                Pages[x].image_cy_coord[x3] = width;
                                
                                if (img_type == 1) {
                                
                                    //printf("update: Flate image - going to process! \n");
                                    
                                    // save image + create blok in image_handler_flate
                                    // Note: minimum check on image size
                                    //  --may require further experimentation
                                
                                    if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width >= IMG_MIN_HxW)) {
                                
                                        // change on july 12 - added parameter - dct_flag
                                        // if dct_flag > -1 -> save as jpg, not ras
                                    
                                        //img_cmyk_on = dict_search_buffer(s3,s4,device_cmyk_seq,10);
                                        
                                        /*
                                        if (img_cmyk_on > 0) {
                                            printf("FOUND CMYK - need to handle! \n");
                                        }
                                        */
                                        
                                        i5 = image_handler_flate(height,width,img_stream_start, img_stream_stop, master_image_tracker,account,corpus,x,x3, i2,page_success, global_png_convert_on, img_cmyk_on);
                                        
                                        blok_added_on_page += i5;
                                        
                                    }
                                
                                    else {
                                        //printf("update: HEIGHT = WIDTH = 0 for Flate - DO NOT SAVE! \n");
                                        i5 = -1;
                                    }
                                }
                            
                                if (img_type == 2) {
                                    //printf("update: DCT image - going to process! \n");
                                
                                    // save image + create blok in image_handler_dct
                                    
                                    i5 = image_handler_dct(img_stream_start,img_stream_stop, master_image_tracker,account,corpus,x,x3, page_success, img_cmyk_on);
                                    
                                    blok_added_on_page += i5;
                                    
                                }
                                
                                if (img_type !=1 && img_type != 2) {
                                    
                                    i5 = -1;
                                
                                    if (debug_mode == 1) {
                                        //printf("error:  image_not_processed -  Image Name + Dict - %s-%s \n", Pages[x].image_names[x3], img_dict_str);
                                    }
                                    
                                    
                                    // look for CCITFaxDecode
                                    
                                    n1 = dict_search_buffer(s3,s4,ccittfaxdecode_seq,14);
                                    
                                    if (n1 > -1) {
                                        
                                        global_unhandled_img_counter ++;
                                        
                                        if (debug_mode == 1) {
                                            printf("update: pdf_parser - found CCITTFAXDECODE - %s - registering as 'unhandled' - note: can be converted to TIFF in post-processing. \n", img_dict_str);
                                        }
                                        
                                        //for (n2=img_stream_start; n2 < img_stream_start+20; n2++) {
                                            //printf("%d - ", buffer[n2]);
                                        
                                    
                                            if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width > IMG_MIN_HxW)) {
                                                
                                                if (global_ccitt_image_save_on == 1) {
                                                    
                                                    i5 = image_handler_ccitt(img_stream_start,img_stream_stop, master_image_tracker,account,corpus,x,x3, page_success, height, width);
                                                    
                                                    blok_added_on_page += i5;
                                    
                                                //printf("update: saved ccitfax image as .tiff \n");
                                                }}
                                        }
                                    
                                    
                                    // look for JBIG2Decode
                                    
                                    n1 = dict_search_buffer(s3,s4,jbig2decode_seq,11);
                                    
                                    if (n1 > -1) {
                                        
                                        global_unhandled_img_counter ++;
                                        
                                        if (debug_mode == 1) {
                                            printf("update: pdf_parser - image handler- JBIG2DECODE image type found - flagging as unhandled-  %s \n", img_dict_str);
                                        }
                                        
                                        /*
                                         for (n2=img_stream_start; n2 < img_stream_start+20; n2++) {
                                             printf("%d - ", buffer[n2]);
                                         }
                                          */
                                         
                                         if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width > IMG_MIN_HxW)) {
                                             
                                             if (debug_mode == 1) {
                                                 //printf("update: pdf_parser - not saving JBIG2. \n");
                                             }
                                             
                                             //i5 = image_handler_jbig2(img_stream_start,img_stream_stop, master_image_tracker,account,corpus,x,x3, page_success, height, width);
                                             
                                             //blok_added_on_page += i5;
                                
                                        }
                                        
                                        
                                    }
                                    
                                    // look for JPXDecode
                                    
                                    n1 = dict_search_buffer(s3,s4,jpxdecode_seq,9);
                                    
                                    if (n1 > -1) {
                                        
                                        //printf("update: FOUND JPXDECODE - %s \n", img_dict_str);
                                        
                                        /*
                                        for (n2=img_stream_start; n2 < img_stream_start+20; n2++) {
                                            printf("%d - ", buffer[n2]);
                                        }
                                         */
                                        
                                        if (height > IMG_MIN_HEIGHT && width > IMG_MIN_WIDTH && (height * width > IMG_MIN_HxW)) {
                                    
                                            i5 = image_handler_jpx(img_stream_start,img_stream_stop, master_image_tracker,account,corpus,x,image_local_count, page_success);
                                            
                                            blok_added_on_page += i5;
                                
                                            //printf("update: saved jpx image as .jpx \n");
                                        }
                                    }
                                    
                                    // look for list of 'nested' images
                                    // new experiment starts here
                                    
                                    /*
                                    n1 = extract_obj_from_buffer(s3,s4);
                                    
                                    if (n1 > -1) {
                                        
                                        for (n2=0; n2 < n1; n1++) {
                                            n3 = get_obj(nn_global_tmp[n2].num);
                                            if (n3 > 0) {
                                                printf("update: img found - %s \n", get_string_from_buffer(global_tmp_obj.dict_start, global_tmp_obj.dict_stop));
                                            }
                                        }
                                    }
                                    */
                                    
                                    // experiment ends here
                                    
                                    // printf("error:  could not identify image type - %d ! \n", img_type);
                                    
                                    //Pages[x].image_cx_coord[x3] = 0;
                                    //Pages[x].image_cy_coord[x3] = 0;
                                    
                                }
                                
                                if (i5 > -1) {
                                
                                    Pages[x].image_global_ref[x3] = master_image_tracker;
                                
                                    total_bloks_added ++;
                                
                                    // *** updating master image trackers in Flate + DCT handlers
                                    //master_new_images_added ++;
                                    //master_image_tracker ++;
                                    
                                    //printf("found + saved image - master tracker - %d \n",master_image_tracker);
                                    
                                    //printf("global page count - %d \n", global_page_count);
                                    //printf("x - %d \n", x);
                                    
                                }
                            }
                                
                        }}}}}
        
        if (blok_added_on_page > 0) {
            if (debug_mode == 1) {
                //printf("update:  bloks added on page - %d - %d \n", x, blok_added_on_page);
            }
            
            global_total_pages_added ++;
        }
        else {
            if (debug_mode == 1) {
                //printf("update: no content found on page - %d \n", x);
            }
        }
        
        //printf("at bottom of loop \n");
                
    }
    
    //printf("finishing up doc here ! \n");
    
    free(buffer);
    
    //printf("survived free buffer \n");
    
    // loop useful to check Images found
    
    /*
    for (pi1=0; pi1 < global_page_count; pi1++) {
        for (pi2=0; pi2 < Pages[pi1].image_entries; pi2++) {
            
            if (Pages[pi1].image_found_on_page[pi2] == 1) {
                
                //printf("Page - %d - Image - %s - %f - %f - %f - %f \n", pi1, Pages[pi1].image_names[pi2], Pages[pi1].image_cx_coord[pi2], Pages[pi1].image_cy_coord[pi2], Pages[pi1].image_x_coord[pi2], Pages[pi1].image_y_coord[pi2]);
                
        }
    }
    }
    */
    
    
    //printf("master page blok start - %d \n", bloks_start_of_new_doc);
    //printf("master page blok stop - %d \n", total_bloks_added);
    //printf("global blok counter - %d \n", global_blok_counter);
    
    
    // insert new - pick up any bloks not yet written to disk
    
    if (global_blok_counter > 0) {
        
        // push bloks from array into db and reset global_blok_counter
        // no need to hold in memory - no linkage between bloks
        
        // printf("writing bloks to db - %d-%d-%d \n", global_blok_counter, master_doc_tracker, master_blok_tracker);
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - read all of the pdf pages - writing to disk - %d \n", global_blok_counter);
        }
         
        if (GLOBAL_WRITE_TO_DB == 1) {
        
            dummy = write_to_db(0,global_blok_counter,account,corpus,fp,master_doc_tracker,master_blok_tracker, time_stamp);
        }
        
        else {
            // new option insert
            
            //strcpy(test_fp,"bloks_out.txt");
            
            dummy = write_to_file (0, global_blok_counter, account, corpus, fp, master_doc_tracker, master_blok_tracker, time_stamp, global_write_to_filename);
        }
        
        
        master_blok_tracker = master_blok_tracker + global_blok_counter;
        global_blok_counter = 0;
    }
    
    // end - new insert
    
    /*
    for (pi1=bloks_start_of_new_doc; pi1 < global_blok_counter; pi1++){
        
        if (strcmp(Bloks[pi1].content_type,"image") == 0) {
            //printf("Image bloks found - %d - %s - %s \n", pi1,Bloks[pi1].relationship,Bloks[pi1].text_run);
        }
    }
     */
    
    if (debug_mode == 1) {
        //printf("update: pdf_parser - total blocks created - %d \n", total_bloks_added);
    }
    
    return total_bloks_added;
}



int update_counters (char *account, char *corpus, int blok_count, int doc_count, int image_count) {
 
    char fp_path_to_master_counter[200];
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter, global_master_fp);
    
    /*  representative path:   /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",account,corpus);
    */
    
    
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

/*
int save_images (int start_blok, int stop_blok, int image_count, char *account,char*corpus,int wf ) {
    
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
        if (strcmp(Bloks[i].content_type,"image") == 0) {
            strcpy(file_type,get_file_type(Bloks[i].relationship));
            sprintf(new_image,"image%d.",new_img_counter);
            strcat(new_image,file_type);
            
            // current file path to find image
            strcpy(current_img, WORKING_FOLDER);
            sprintf(tmp_str,"%d/%s", wf, Bloks[i].relationship);
            strcat(current_img,tmp_str);
                        
            sprintf(new_fp, "/Users/darrenoberst/Documents/bloks/accounts/main_accounts/%s/%s/images/",account,corpus);
            
            strcpy(Bloks[i].relationship,new_image);
            
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
            
            new_img_counter ++;
            
            strcpy(Bloks[i].relationship,new_image);
        }
    }
    
    return new_img_counter;
}
 */

// register_ae_add_pdf_event is WIP
//  ... can be developed further to register key processing events from PDF parsing
//  ... can also be a valuable debugging tool to figure out what went wrong
//  ... most useful application is likely to identify a file that was not saved / processed correctly

int register_ae_add_pdf_event(char *ae_type, char *event, char *account,char *corpus,char *fp, char *time_stamp) {
    
    // const char *ae_type = "ADD_PDF_LOG";
    // const char *ae_type = "REJECTED_FILE_PDF";
    
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
                       "library_name", BCON_UTF8(corpus),
                       "block", BCON_UTF8(""),
                       "search_query", BCON_UTF8(event),
                       "permission_scope", BCON_UTF8(""),
                       "user_IP_address", BCON_UTF8(""),
                       "file_name", BCON_UTF8(my_doc.file_name),
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
    
    //char *str;
    
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
                
                if (debug_mode == 1) {
                    //printf("update: pdf_parser - pulled new unique doc_id value - %d \n", bson_iter_int64(&sub_iter));
                }
                
                new_doc_id = bson_iter_int64(&sub_iter);
            }
        }
    

    //str = bson_as_canonical_extended_json(&reply, NULL);
    //printf("update: doc_id output - %s \n", str);
    //bson_free(str);
    
    
    bson_destroy (&reply);
    bson_destroy (update);
    bson_destroy (filter);
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
                            "tables", BCON_INT64(inc_tables),
                            "pages", BCON_INT64(inc_pages),
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

int write_to_file (int start_blok, int stop_blok, char *account, char *library, char *file_source, int doc_number, int blok_number, char *time_stamp, char *filename) {
    
    if (debug_mode == 1) {
        printf("update: pdf_parser - writing parsing output to file. \n");
    }
    
    int i=0;
    char text_search[100000];
    char cmyk_flag [100];
    
    char fp[500];
    
    FILE* bloks_out;
    
    strcpy(fp,"");
    strcat(fp,global_image_fp);
    strcat(fp,filename);
    strcpy(cmyk_flag,"");
    
    if ((bloks_out = fopen(fp,"r")) != NULL) {
        
        // file already exists
        fclose(bloks_out);
        
        bloks_out = fopen(fp,"a");
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - output file already started -> opening in 'a' mode to append \n");
        }
    }
    
    else {
        bloks_out = fopen(fp,"w");
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - creating new parsing output file -> opening in 'w' write \n");
        }
    }
        
    
    // create text_search in loop - derived from text_run + formatted_text + linked_text
    
    for (i=start_blok; i < stop_blok; i++) {
        
        // create text_search
        
        strcpy(text_search,Bloks[i].text_run);
        
        if (strcmp(Bloks[i].content_type,"image") == 0) {
            
            // only add global headline to search index if content_type == "image"
            
            if (strlen(text_search) < 10) {
                
                // no text linked to image - need to add global headline
                
                if (strlen(global_headlines) > 0) {
                    
                    strcat(text_search, global_headlines);
                
                    if (strlen(Bloks[i].formatted_text) < 20) {
                        strcat(Bloks[i].formatted_text, " ");
                        strcat(Bloks[i].formatted_text, global_headlines);
                        }
                    }
                    
                }
            
        }
                
        
        if (strcmp(Bloks[i].content_type,"image")==0 && Bloks[i].shape_num == -3) {
            strcpy(cmyk_flag,"CMYK_FLAG");
        }
        else {
            strcpy(cmyk_flag,"");
        }
        
        //printf("update: writing blok file source - %s \n", my_doc.file_name);
        
        
        fprintf(bloks_out,"\n<block_ID>: %d,",blok_number);
        fprintf(bloks_out, "\n<doc_ID>: %d,", doc_number);
        fprintf(bloks_out, "\n<content_type>: %s,", Bloks[i].content_type);
        fprintf(bloks_out, "\n<file_type>: %s,", "pdf");
        fprintf(bloks_out, "\n<master_index>: %d,", Bloks[i].slide_num + 1);
        fprintf(bloks_out, "\n<master_index2>: %d,", 0);
        fprintf(bloks_out, "\n<coords_x>: %d,", Bloks[i].position.x);
        fprintf(bloks_out, "\n<coords_y>: %d,", Bloks[i].position.y);
        fprintf(bloks_out, "\n<coords_cx>: %d,", Bloks[i].position.cx);
        fprintf(bloks_out, "\n<coords_cy>: %d,", Bloks[i].position.cy);
        fprintf(bloks_out, "\n<author_or_speaker>: %s,", my_doc.author);
        fprintf(bloks_out, "\n<added_to_collection>: %s,", time_stamp);
        fprintf(bloks_out, "\n<file_source>: %s,", my_doc.file_short_name);
        fprintf(bloks_out, "\n<table>: %s,", Bloks[i].table_text);
        fprintf(bloks_out, "\n<modified_date>: %s,", my_doc.modify_date);
        fprintf(bloks_out, "\n<created_date>: %s,", my_doc.create_date);
        fprintf(bloks_out, "\n<creator_tool>: %s,", my_doc.creator_tool);
        fprintf(bloks_out, "\n<external_files>: %s,", Bloks[i].relationship);
        fprintf(bloks_out, "\n<text>: %s,", Bloks[i].text_run);
        fprintf(bloks_out, "\n<header_text>: %s,", Bloks[i].formatted_text);
        fprintf(bloks_out, "\n<text_search>: %s,", text_search);
        fprintf(bloks_out, "\n<user_tags>: %s,", "");
        fprintf(bloks_out, "\n<special_field1>: %s,", "");
        fprintf(bloks_out, "\n<special_field2>: %s,", "");
        fprintf(bloks_out, "\n<special_field3>: %s,", cmyk_flag);
        fprintf(bloks_out, "\n<graph_status>: false");
        fprintf(bloks_out, "\n<dialog>: false");
        fprintf(bloks_out, "%s\n", "<END_BLOCK>");
        
        blok_number ++;
        
    }
    
    fclose(bloks_out);
       
    //printf ("created mongo collection successfully \n");
    
   return blok_number;
}


int write_to_db (int start_blok,int stop_blok, char *account, char *library, char *file_source, int doc_number,int blok_number, char *time_stamp)
{
    //const char *uri_string = "mongodb://localhost:27017";
    
    const char *uri_string = global_mongo_db_path;
    
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *coll;
    bson_t *insert = NULL;
    bson_error_t error;
    
    int i=0;
    int j=0;
    char text_search[100000];
    char formatted_text[10000];
    char tmp[10];
    
    char cmyk_flag[100];
    
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

    // create text_search in loop - derived from text_run + formatted_text + linked_text
    
    for (i=start_blok; i < stop_blok; i++) {
        
        strcpy(text_search,"");
        strcpy(formatted_text,"");
        strcpy(cmyk_flag,"");
        
        if (debug_mode == 1) {

            if (strcmp(Bloks[i].content_type,"table") == 0) {
                if (debug_mode == 1) {
                    printf("update: pdf_parser - capturing table and writing to db. \n");
                }
            }
        }
        
        // Clean text_run if needed to ensure BSON validation -> write "text_search" to DB
        
        if (!bson_utf8_validate (Bloks[i].text_run, strlen(Bloks[i].text_run), true)) {
           
            if (debug_mode == 1) {
                printf ("warning: pdf_parser - write_to_db - bson_utf8 validation failed - will remediate before adding to db. \n");
            }
            
            for (j=0; j < strlen(Bloks[i].text_run); j++) {

                if (Bloks[i].text_run[j] >= 32 && Bloks[i].text_run[j] <= 128) {
                    sprintf(tmp,"%c",Bloks[i].text_run[j]);
                    strcat(text_search,tmp);
                }
                else
                {
                    if (debug_mode == 1) {
                        if (Bloks[i].text_run[j] < 0) {
                            if (debug_mode == 1) {
                                //printf("error: pdf_parser- unusual - found negative char num. %d \n", Bloks[i].text_run[j]);
                            }
                        }
                        //printf("error:  BAD CHAR-%d-%d - ", j,Bloks[i].text_run[j]);
                    }
                    
                }
            }
            }
            
        else
        {
            strcpy(text_search,Bloks[i].text_run);
        }
        
        // text_search is now a clean copy of Bloks[i].text_run and can be inserted into db safely
        
        
        if (strcmp(Bloks[i].content_type,"image") == 0) {
            
            // only add global headline to search index if content_type == "image"
            
            //printf("update: in write_to_db - image - text - %d - %s \n", strlen(text_search), text_search);
            
            if (strlen(text_search) < 10) {
                
                // no text linked to image - need to add global headline
                
                if (strlen(global_headlines) > 0) {
                    
                    if (!bson_utf8_validate (global_headlines, strlen(global_headlines),true)) {
                        
                        // strcat(Bloks[i].formatted_text, " ");
                        
                        for (j=0; j < strlen(global_headlines); j++) {
                            if (global_headlines[j] >= 32 && global_headlines[j] <= 128) {
                                sprintf(tmp,"%c",global_headlines[j]);
                                strcat(text_search,tmp);
                                
                                strcat(Bloks[i].formatted_text, tmp);
                            }
                        }
                    }
                    
                    else {
                        strcat(text_search, " ");
                        strcat(text_search, global_headlines);
                        
                        if (strlen(Bloks[i].formatted_text) < 20) {
                            strcat(Bloks[i].formatted_text, " ");
                            strcat(Bloks[i].formatted_text, global_headlines);
                        }
                    }
                    
                }}
                
        }
        
        if (strcmp(Bloks[i].content_type,"image")==0 && Bloks[i].shape_num == -3) {
            strcpy(cmyk_flag,"CMYK_FLAG");
        }
        else {
            strcpy(cmyk_flag,"");
        }
        
        
        //printf("update: writing to db: %s \n", Bloks[i].text_run);
        
        insert = BCON_NEW (
                           "block_ID", BCON_INT64(blok_number),
                           "doc_ID", BCON_INT64(doc_number),
                           "content_type",BCON_UTF8(Bloks[i].content_type),
                           "file_type", BCON_UTF8("pdf"),
                           "master_index", BCON_INT64((Bloks[i].slide_num + 1)),
                           "master_index2", BCON_INT64(0),
                           "coords_x", BCON_INT64(Bloks[i].position.x),
                           "coords_y", BCON_INT64(Bloks[i].position.y),
                           "coords_cx", BCON_INT64(Bloks[i].position.cx),
                           "coords_cy", BCON_INT64(Bloks[i].position.cy),
                           "author_or_speaker", BCON_UTF8(my_doc.author),
                           "added_to_collection", BCON_UTF8(time_stamp),
                           "file_source", BCON_UTF8(my_doc.file_short_name), // alt:  file_source
                           "table", BCON_UTF8(Bloks[i].table_text),
                           "modified_date", BCON_UTF8(my_doc.modify_date),
                           "created_date", BCON_UTF8(my_doc.create_date),
                           "creator_tool", BCON_UTF8(my_doc.creator_tool),
                           "external_files", BCON_UTF8(Bloks[i].relationship),
                           "text", BCON_UTF8(text_search),
                           "header_text", BCON_UTF8(Bloks[i].formatted_text),
                           "text_search", BCON_UTF8(text_search),
                           "user_tags", BCON_UTF8(""),
                           "special_field1", BCON_UTF8(""),
                           "special_field2", BCON_UTF8(""),
                           "special_field3", BCON_UTF8(cmyk_flag),
                           "graph_status", BCON_UTF8("false"),
                           "dialog", BCON_UTF8("false")
                           );
        
        blok_number ++;
        
        if (!mongoc_collection_insert_one (coll, insert, NULL, NULL, &error)) {
          fprintf (stderr, "%s\n", error.message);
        }
        
        bson_destroy(insert);
        
    }
    
    //bson_destroy (insert);
    
    // only non-freed item is error bson_error_t -> could be potential memory leak?

    mongoc_collection_destroy (coll);
    mongoc_database_destroy (database);
    mongoc_uri_destroy (uri);
    mongoc_client_destroy (client);
    mongoc_cleanup ();
    
    //printf ("created mongo collection successfully \n");
    
   return blok_number;
}




int add_pdf_main  (char *input_account_name,
                   char *input_library_name,
                   char *input_fp,
                   char *input_mongodb_path,
                   char *input_master_fp,
                   char *input_images_fp,
                   int input_debug_mode,
                   int input_image_save_mode)

{
    
    //  Main overall dynamic library handler for "add_pdf_files_to_directory"
    //
    //  Inputs:
    //      1.  account_name
    //      2.  library name
    //      3.  input_fp = local file path to the directory that contains the files to be added
    //      4.  input mongodb path = path to the mongodb server
    //      5.  input master_fp = path to the {{account/library}} master_files counter
    //      6.  input images fp = path in {{account/library}} to save images
    //      7.  debug_mode - 1 = verbose output to console
    //


    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    // add new variable - july 15, 2022
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    //start_t = clock();
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    // *** KEY INPUTS for .DYLIB ***

    //printf ("input account name-%s \n", input_account_name);
    //printf ("input corpus name-%s \n", input_corpus_name);
    //printf ("input file path-%s \n", input_fp);
    
    char fp_path_to_master_counter[200];
    
    /* example path: /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_corpus_name);
    */
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter, input_master_fp);

    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int found = 0;
    int i = 0;
    
    char files[5000][300];
    
    global_mongo_db_path = input_mongodb_path;
    global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    GLOBAL_BLOK_SIZE = 400;
    
    IMG_MIN_HEIGHT = 5;
    IMG_MIN_WIDTH = 5;
    IMG_MIN_HxW = 250;
    
    GLOBAL_WRITE_TO_DB = 1;
    
    global_write_to_filename = "pdf_blocks_out.txt";
    
    // default - ccitt_image_save_on SET TO OFF
    global_ccitt_image_save_on = -1;
    
    // NOTE: assumes the 'old' default -> saves image_handler_flate as raw binary, e.g., ".ras"
    global_png_convert_on = 0;
    
    // takes input_debug_mode and assigns to global var
    // used across all functions to guide verbosity of log read out to screen
    // for debugging pdf encoding errors, it requires tremendous amount of log detail
    //  ... may need to look for better alternative with log captured to database
    //
    // with debug_mode == 1 -> verbose
    // all other values -> limited read-out
    
    debug_mode = input_debug_mode;
    
    strcpy(f_in, fp_path_to_master_counter);
    
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
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    // input image save mode
    
    // printf("update: received input_image_save_mode = %d \n", input_image_save_mode);
    // printf("update: received input_debug_mode = %d \n", input_debug_mode);
    
    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // Main Loop through upload folder

    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                //  iterating through folder looking for PDF files
                //  printf("iterating thru folder-%s-%d \n", ent->d_name,upload_files_count);
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
                    
                    // found pdf file -> will skip any other file extensions in the folder
                    //  printf ("found pdf file-%s-%d \n", ent->d_name,pdf_counter);
                    
                    pdf_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    
                    //  adding to files[] list the found matching pdf file for further processing
                    //  printf("processing fp-%s \n",tmp_fp_iter);
                    
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    
                    upload_files_count ++;
                    
                }
                
            }}}
    
    closedir (web_dir);
    
    //free(ent);
    
    // start clock with main loop processing
    
    start_t = clock();
    
    for (i=0; i < upload_files_count ; i++) {
        
        //printf("update:  pdf initiating file processing fp-%d-%s \n", i,files[i]);
        
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, files[i]);

        bloks_created = pdf_builder (files[i], input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            //printf("down this path - bloks_created > 0 \n");
            //printf("my_doc - %s - %s - %d - %d - %d \n", my_doc.account_name,my_doc.corpus_name,doc_count,blok_count,thread_number);
            
            // dummy = write_to_db(0,bloks_created,my_doc.account_name,my_doc.corpus_name,files[i], doc_count,blok_count, time_stamp);
            
            if (debug_mode == 1) {
                printf("update: pdf_parser-  finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            doc_count ++;
            master_doc_tracker ++;
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser- blocks_created = \n",bloks_created);
            }
            
            // account events:  capture exception output if no bloks not created
            //      --opportunity to build out further to capture more detailed parsing info
            
            if (bloks_created == -1) {
                event = "NO_CATALOG";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == -2) {
                event = "ENCRYPTED";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == 0) {
                event = "NO_CONTENT_FOUND";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
        }
                
    }
    
    // last step - update the counters
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count);

    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
    }
    
    return global_total_pages_added;
}


int add_pdf_main_customize  (char *input_account_name,
                             char *input_library_name,
                             char *input_fp,
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
                             int png_convert_on)

{
    
    //  Main overall dynamic library handler for "add_pdf_files_to_directory"
    //
    //  Inputs:
    //      1.  account_name
    //      2.  library name
    //      3.  input_fp = local file path to the directory that contains the files to be added
    //      4.  input mongodb path = path to the mongodb server
    //      5.  input master_fp = path to the {{account/library}} master_files counter
    //      6.  input images fp = path in {{account/library}} to save images
    //      7.  debug_mode - 1 = verbose output to console
    //


    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    // add new variable - july 15, 2022
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    //start_t = clock();
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    // *** KEY INPUTS for .DYLIB ***

    //printf ("input account name-%s \n", input_account_name);
    //printf ("input corpus name-%s \n", input_corpus_name);
    //printf ("input file path-%s \n", input_fp);
    
    char fp_path_to_master_counter[200];
    
    /* example path: /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_corpus_name);
    */
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter, input_master_fp);

    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int found = 0;
    int i = 0;
    
    char files[5000][300];
    
    global_mongo_db_path = input_mongodb_path;
    global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    // NEW - set global 'customize' parameter configurations
    
    //GLOBAL_BLOK_SIZE = 400;
    GLOBAL_BLOK_SIZE = user_blok_size;
    
    //IMG_MIN_HEIGHT = 5;
    IMG_MIN_HEIGHT = user_img_height;
    
    //IMG_MIN_WIDTH = 5;
    IMG_MIN_WIDTH = user_img_width;
    
    //IMG_MIN_HxW = 250;
    IMG_MIN_HxW = user_img_hxw;
    
    
    //GLOBAL_WRITE_TO_DB = 1;
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    //global_write_to_filename = "bloks_out.txt";
    global_write_to_filename = write_to_filename;
    
    global_ccitt_image_save_on = ccitt_img_on;

    // new - assumes that png_conversion set to "on"
    // default = 1
    global_png_convert_on = png_convert_on;
    
    
    // end - new - set global 'customize' parameter configurations
    
    
    // takes input_debug_mode and assigns to global var
    // used across all functions to guide verbosity of log read out to screen
    // for debugging pdf encoding errors, it requires tremendous amount of log detail
    //  ... may need to look for better alternative with log captured to database
    //
    // with debug_mode == 1 -> verbose
    // all other values -> limited read-out
    
    debug_mode = input_debug_mode;
    
    strcpy(f_in, fp_path_to_master_counter);
    
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
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    global_text_found = 0;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    // input image save mode
    
    // printf("update: received input_image_save_mode = %d \n", input_image_save_mode);
    // printf("update: received input_debug_mode = %d \n", input_debug_mode);
    
    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // Main Loop through upload folder

    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                //  iterating through folder looking for PDF files
                //  printf("iterating thru folder-%s-%d \n", ent->d_name,upload_files_count);
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
                    
                    // found pdf file -> will skip any other file extensions in the folder
                    //  printf ("found pdf file-%s-%d \n", ent->d_name,pdf_counter);
                    
                    pdf_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    
                    //  adding to files[] list the found matching pdf file for further processing
                    //  printf("processing fp-%s \n",tmp_fp_iter);
                    
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    
                    upload_files_count ++;
                    
                }
                
            }}}
    
    closedir (web_dir);
    
    //free(ent);
    
    // start clock with main loop processing
    
    start_t = clock();
    
    // insert experiment starts here
    master_doc_tracker = 3000;
    // end experiment here
    
    for (i=0; i < upload_files_count ; i++) {
        
        //printf("update:  pdf initiating file processing fp-%d-%s \n", i,files[i]);
        
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, files[i]);
        
        bloks_created = pdf_builder (files[i], input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            //printf("down this path - bloks_created > 0 \n");
            //printf("my_doc - %s - %s - %d - %d - %d \n", my_doc.account_name,my_doc.corpus_name,doc_count,blok_count,thread_number);
            
            // dummy = write_to_db(0,bloks_created,my_doc.account_name,my_doc.corpus_name,files[i], doc_count,blok_count, time_stamp);
            
            if (debug_mode == 1) {
                printf("update: pdf_parser- finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            // printf("pdf_builder: master_doc_tracker - %d \n", master_doc_tracker);
            
            doc_count ++;
            
            // insert experiment here
            //master_doc_tracker ++;
            master_doc_tracker --;
            master_doc_tracker --;
            master_doc_tracker --;
            
            // end - experiment
            
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser- no content captured - blocks_created = %d \n",bloks_created);
            }
            
            // account events:  capture exception output if no bloks not created
            //      --opportunity to build out further to capture more detailed parsing info
            
            if (bloks_created == -1) {
                event = "NO_CATALOG";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == -2) {
                event = "ENCRYPTED";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == 0) {
                event = "NO_CONTENT_FOUND";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
        }
        
        if (global_text_found == 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no text content found - even though images found - %d \n", bloks_created);
            }
            
            event = "NO_TEXT_FOUND";
            ae_type = "REJECTED_FILE_PDF";
            dummy = register_ae_add_pdf_event(ae_type, event, my_doc.account_name,my_doc.corpus_name, files[i], time_stamp);
            
        }
        
        if (global_unhandled_img_counter > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - global unhandled img counter > 0 - flag for Triage processing - %d \n", global_unhandled_img_counter);
            }
            
            event = "UNHANDLED_IMAGES";
            ae_type = "REJECTED_FILE_PDF";
            dummy = register_ae_add_pdf_event(ae_type, event, my_doc.account_name,my_doc.corpus_name, files[i], time_stamp);
        }
                
    }
    
    // last step - update the counters
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count);

    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
        
        if (global_text_found == 0) {
            printf("Summary PDF: ***important note*** - NO TEXT FOUND IN PDF ! \n");
        }
    }
    
    return global_total_pages_added;
}


int add_pdf_main_customize_parallel  (char *input_account_name,
                                      char *input_library_name,
                                      char *input_fp,
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
                                      int unique_doc_num)

{
    
    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    // *** KEY INPUTS for .DYLIB ***

    //char fp_path_to_master_counter[200];
    //strcpy(fp_path_to_master_counter,"");
    //strcat(fp_path_to_master_counter, input_master_fp);

    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int found = 0;
    int i = 0;
    int j = 0;
    char files[5000][300];
    
    int new_doc_id = -1;
    
    global_mongo_db_path = input_mongodb_path;
    global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    global_table_count = 0;
    
    // NEW - set global 'customize' parameter configurations
    
    //GLOBAL_BLOK_SIZE = 400;
    GLOBAL_BLOK_SIZE = user_blok_size;
    
    //IMG_MIN_HEIGHT = 5;
    IMG_MIN_HEIGHT = user_img_height;
    
    //IMG_MIN_WIDTH = 5;
    IMG_MIN_WIDTH = user_img_width;
    
    //IMG_MIN_HxW = 250;
    IMG_MIN_HxW = user_img_hxw;
    
    //GLOBAL_WRITE_TO_DB = 1;
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    //global_write_to_filename = "bloks_out.txt";
    global_write_to_filename = write_to_filename;
    
    global_ccitt_image_save_on = ccitt_img_on;

    // new - assumes that png_conversion set to "on"
    // default = 1
    global_png_convert_on = png_convert_on;
    
    // with debug_mode == 1 -> verbose
    // all other values -> limited read-out
    
    debug_mode = input_debug_mode;
    
    master_blok_tracker = 0;
    
    //master_doc_tracker = unique_doc_num;
    master_doc_tracker = 0;
    
    master_image_tracker = 0;
    blok_count = 0;
    doc_count = 0;
    image_count = 0;
    
    global_total_pages_added = 0;
    
    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // Main Loop through upload folder

    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                //  iterating through folder looking for PDF files
                //  printf("iterating thru folder-%s-%d \n", ent->d_name,upload_files_count);
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
                    
                    // found pdf file -> will skip any other file extensions in the folder
                    //  printf ("found pdf file-%s-%d \n", ent->d_name,pdf_counter);
                    
                    pdf_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    
                    //  adding to files[] list the found matching pdf file for further processing
                    //  printf("processing fp-%s \n",tmp_fp_iter);
                    
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    
                    upload_files_count ++;
                    
                }
                
            }}}
    
    closedir (web_dir);
    
    //free(ent);
    
    // start clock with main loop processing
    
    start_t = clock();
    
    for (i=0; i < upload_files_count ; i++) {
        
        //printf("update:  pdf initiating file processing fp-%d-%s \n", i,files[i]);
        
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, files[i]);
        
        master_doc_tracker = pull_new_doc_id(input_account_name,input_library_name);
        
        // very simple triage - will need to enhance over time
        
        if (master_doc_tracker < 1) {
            // something went wrong - should return +1 at a minimum as "after" response
            // setting a large, arbitrary integer so we can identify a problem post-parsing...
            master_doc_tracker = 1000000;
        }
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - pull_new_doc_id call - master_doc_tracker - new doc id = %d \n", master_doc_tracker);
        }
        
        bloks_created = pdf_builder (files[i], input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - finished reading document - added total new blocks - %d \n", bloks_created);
            }
            
            doc_count ++;
            //master_doc_tracker ++;
            
            // change - re-start blok_count and image_count at 0 with each new doc
            master_blok_tracker = 0;
            master_image_tracker = 0;
            
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no content captured - blocks_created = %d \n",bloks_created);
            }
            
            // account events:  capture exception output if no bloks not created
            //      --opportunity to build out further to capture more detailed parsing info
            
            if (bloks_created == -1) {
                event = "NO_CATALOG";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == -2) {
                event = "ENCRYPTED";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == 0) {
                event = "NO_CONTENT_FOUND";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
        }
        
        if (global_text_found == 0 || bloks_created == 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no text content found - even though images found - %d \n", bloks_created);
            }
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - note: no text found in document parse. \n");
            }
            
            char myevent[10];
            sprintf(myevent, "%d", master_doc_tracker);
            event = &myevent;
            ae_type = "NO_TEXT_FOUND";
            dummy = register_ae_add_pdf_event(ae_type, event, my_doc.account_name,my_doc.corpus_name, files[i], time_stamp);
            master_doc_tracker ++;
            //global_total_pages_added = starting_global_total_pages_added; //Reset any pages that may have been added while parsing this invalid doc
            
        }
        
        if (global_unhandled_img_counter > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - global unhandled img counter > 0 - flag for Triage processing - %d \n", global_unhandled_img_counter);
            }
            
            if (global_text_found == 0 || bloks_created == 0) {
                //printf("update: UNHANDLED_IMAGES <-> NO TEXT \n");
            }
            else
            
            {
                //printf("update: UNHANDLED_IMAGES with TEXT \n");
                
                // note: only registering a separate UNHANDLED_IMAGES event if text is found
                //  e.g., "UNHANDLED_IMAGES" = text was found, but likely missing some scanned content
                
                event = "UNHANDLED_IMAGES";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event, my_doc.account_name,my_doc.corpus_name, files[i], time_stamp);
            }
            
        }
           
    }
    
    // last step - update the counters
    
    dummy = update_library_inc_totals(my_doc.account_name, my_doc.corpus_name, doc_count, blok_count, image_count, global_total_pages_added, global_table_count);
    
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
        printf("Summary PDF:  TABLE COUNT CREATED - %d \n", global_table_count);
    }
    
    return global_total_pages_added;
}






// alternative entry point into the parser -> single file
// added on sept 29, 2022

int add_one_pdf_main  (char *input_account_name,
                       char *input_library_name,
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
                       int png_convert_on)

{
    
    //  Main overall dynamic library handler for "add_pdf_files_to_directory"
    //
    //  Inputs:
    //      1.  account_name
    //      2.  library name
    //      3.  input_fp = local file path to the directory that contains the files to be added
    //      4.  input mongodb path = path to the mongodb server
    //      5.  input master_fp = path to the {{account/library}} master_files counter
    //      6.  input images fp = path in {{account/library}} to save images
    //      7.  debug_mode - 1 = verbose output to console
    //


    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    // add new variable - july 15, 2022
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    char account[50],collection[50];
    int blok_count, doc_count, image_count;
    
    FILE* bloks_out;
    
    //start_t = clock();
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    // *** KEY INPUTS for .DYLIB ***

    //printf ("input account name-%s \n", input_account_name);
    //printf ("input corpus name-%s \n", input_corpus_name);
    //printf ("input file path-%s \n", input_fp);
    
    char fp_path_to_master_counter[200];
    
    /* example path: /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_corpus_name);
    */
    
    strcpy(fp_path_to_master_counter,"");
    strcat(fp_path_to_master_counter, input_master_fp);

    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int i = 0;
    
    int go_ahead_check = -1;
    
    char files[5000][300];
    
    global_mongo_db_path = input_mongodb_path;
    global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    global_ccitt_image_save_on = ccitt_img_on;
    
    // new - assumes png_convert_on = "on" as default
    // default = 1
    global_png_convert_on = png_convert_on;
    
    // GLOBAL_BLOK_SIZE = 400;
    GLOBAL_BLOK_SIZE = user_blok_size;
    
    //IMG_MIN_HEIGHT = 5;
    IMG_MIN_HEIGHT = user_img_height;
    
    //IMG_MIN_WIDTH = 5;
    IMG_MIN_WIDTH = user_img_width;
    
    //IMG_MIN_HxW = 250;
    IMG_MIN_HxW = user_img_hxw;
    
    //GLOBAL_WRITE_TO_DB = 1;
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    //global_write_to_filename = "bloks_out.txt";
    global_write_to_filename = write_to_filename;
    
    // takes input_debug_mode and assigns to global var
    // used across all functions to guide verbosity of log read out to screen
    // for debugging pdf encoding errors, it requires tremendous amount of log detail
    //  ... may need to look for better alternative with log captured to database
    //
    // with debug_mode == 1 -> verbose
    // all other values -> limited read-out
    
    debug_mode = input_debug_mode;
    
    strcpy(f_in, fp_path_to_master_counter);
    
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
    
    master_blok_tracker = blok_count;
    master_doc_tracker = doc_count;
    master_image_tracker = image_count;
    
    global_total_pages_added = 0;
    
    // *** Key variables:   blok_count, doc_count, image_count ***
    // need to track and update these three counters & save file at the end
    
    // input image save mode
    
    // printf("update: received input_image_save_mode = %d \n", input_image_save_mode);
    // printf("update: received input_debug_mode = %d \n", input_debug_mode);
    
    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_filename));
                    
    if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_filename);
        
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
    
    
    // start processing here
    
    if (go_ahead_check == 1) {
    
        // start clock with main loop processing
    
        start_t = clock();
    
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, tmp_fp_iter);

        bloks_created = pdf_builder (tmp_fp_iter, input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            doc_count ++;
            master_doc_tracker ++;
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            pdf_counter ++;
            upload_files_count ++;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - blocks_created = \n",bloks_created);
            }
            
            // account events:  capture exception output if no bloks not created
            //      --opportunity to build out further to capture more detailed parsing info
            
            if (bloks_created == -1) {
                event = "NO_CATALOG";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == -2) {
                event = "ENCRYPTED";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == 0) {
                event = "NO_CONTENT_FOUND";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
        }}
    
    else {
        
        if (go_ahead_check == -1) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file is not PDF. \n");
            }
        }
        
        if (go_ahead_check == -2) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file not found at this path. \n");
            }
        }
        
    }
                
    
    // last step - update the counters
    
    dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count);

    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
    }
    
    return global_total_pages_added;
}



// alternative entry point into the parser -> single file
// added on Oct 24, 2022
// needs to remove master_counters and change internal indexing
// adds last input:   unique_doc_num

int add_one_pdf_main_parallel  (char *input_account_name,
                                char *input_library_name,
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
                                int unique_doc_num)

{
    
    //  Main overall dynamic library handler for "add_pdf_files_to_directory"
    //
    //  Inputs:
    //      1.  account_name
    //      2.  library name
    //      3.  input_fp = local file path to the directory that contains the files to be added
    //      4.  input mongodb path = path to the mongodb server
    //      5.  input master_fp = path to the {{account/library}} master_files counter
    //      6.  input images fp = path in {{account/library}} to save images
    //      7.  debug_mode - 1 = verbose output to console
    //


    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    // add new variable - july 15, 2022
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    
    //char header_account[50],header_collection[50], header_blok_count[50], header_doc_count[50],header_image_count[50];
    //char account[50],collection[50];
    
    int blok_count, doc_count, image_count;
    
    FILE* bloks_out;
    
    //start_t = clock();
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    // *** KEY INPUTS for .DYLIB ***

    //printf ("input account name-%s \n", input_account_name);
    //printf ("input corpus name-%s \n", input_corpus_name);
    //printf ("input file path-%s \n", input_fp);
    
    // char fp_path_to_master_counter[200];
    
    /* example path: /bloks/accounts/main_accounts/%s/%s/master_files/master_counters.csv",input_account_name,input_corpus_name);
    */
    
    //strcpy(fp_path_to_master_counter,"");
    //strcat(fp_path_to_master_counter, input_master_fp);

    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int i = 0;
    
    int go_ahead_check = -1;
    
    char files[5000][300];
    
    global_mongo_db_path = input_mongodb_path;
    global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    global_ccitt_image_save_on = ccitt_img_on;
    
    // new - assumes png_convert_on = "on" as default
    // default = 1
    global_png_convert_on = png_convert_on;
    
    // GLOBAL_BLOK_SIZE = 400;
    GLOBAL_BLOK_SIZE = user_blok_size;
    
    //IMG_MIN_HEIGHT = 5;
    IMG_MIN_HEIGHT = user_img_height;
    
    //IMG_MIN_WIDTH = 5;
    IMG_MIN_WIDTH = user_img_width;
    
    //IMG_MIN_HxW = 250;
    IMG_MIN_HxW = user_img_hxw;
    
    //GLOBAL_WRITE_TO_DB = 1;
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    
    //global_write_to_filename = "bloks_out.txt";
    global_write_to_filename = write_to_filename;
    
    // takes input_debug_mode and assigns to global var
    // used across all functions to guide verbosity of log read out to screen
    // for debugging pdf encoding errors, it requires tremendous amount of log detail
    //  ... may need to look for better alternative with log captured to database
    //
    // with debug_mode == 1 -> verbose
    // all other values -> limited read-out
    
    debug_mode = input_debug_mode;
    
    /*
    strcpy(f_in, fp_path_to_master_counter);
    
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
    */
    
    //                                  * key update *
    //  these are global variables used internally within the parser to track IDs
    //          -- unique_doc_num -> passed as input to parser call
    //          -- master_blok_tracker -> increment from 0 throughout the document
    //          -- master_image_tracker -> increment from 0 throughout the document
    //                  -- new image id name = image{doc_number}_{img_number}.abc
    
    master_blok_tracker = 0;
    master_doc_tracker = unique_doc_num;
    master_image_tracker = 0;
    blok_count = 0;
    doc_count = 0;
    image_count = 0;
    
    //                      * end - key update - master tracker replacement *
    
    
    global_total_pages_added = 0;
    

    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    
    strcpy(file_type, get_file_type(input_filename));
                    
    if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_filename);
        
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
    
    
    // start processing here
    
    if (go_ahead_check == 1) {
    
        // start clock with main loop processing
    
        start_t = clock();
    
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, tmp_fp_iter);

        bloks_created = pdf_builder (tmp_fp_iter, input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            // doc_count ++;
            // master_doc_tracker ++;
            
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            pdf_counter ++;
            upload_files_count ++;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - blocks_created = \n",bloks_created);
            }
            
            // account events:  capture exception output if no bloks not created
            //      --opportunity to build out further to capture more detailed parsing info
            
            if (bloks_created == -1) {
                event = "NO_CATALOG";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == -2) {
                event = "ENCRYPTED";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
            if (bloks_created == 0) {
                event = "NO_CONTENT_FOUND";
                ae_type = "REJECTED_FILE_PDF";
                dummy = register_ae_add_pdf_event(ae_type, event,my_doc.account_name,my_doc.corpus_name,files[i], time_stamp);
            }
            
        }}
    
    else {
        
        if (go_ahead_check == -1) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file is not PDF. \n");
            }
        }
        
        if (go_ahead_check == -2) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file not found at this path. \n");
            }
        }
        
    }
                
    
    // last step - update the counters
    
    //dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count);

    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
    }
    
    return global_total_pages_added;
}


// new entry point - assumes passed as text file

int add_one_pdf  (char *account_name,
                  char *library_name,
                  char *input_fp,
                  char *input_filename,
                  char *input_images_fp,
                  char *write_to_filename,
                  int user_blok_size)
{
    
    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    char* event;
    char* ae_type;
    
    char doc_file_fp[300];
    char f_in[200];
    
    int blok_count, doc_count, image_count;
    
    FILE* bloks_out;
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int i = 0;
    
    int go_ahead_check = -1;
    
    char files[5000][300];
    
    // create new tracker for file_names only - no path attached
    char files_copies[500][30];
    
    // do these need to be assigned?
    // global_mongo_db_path = input_mongodb_path;
    // global_master_fp = input_master_fp;
    
    global_image_fp = input_images_fp;
    GLOBAL_BLOK_SIZE = user_blok_size;

    global_ccitt_image_save_on = 1;
    global_png_convert_on = 1;
    IMG_MIN_HEIGHT = 5;
    IMG_MIN_WIDTH = 5;
    IMG_MIN_HxW = 250;
    
    // by default - will try to file
    GLOBAL_WRITE_TO_DB = 0;
    global_write_to_filename = write_to_filename;
    
    debug_mode = 0;
    
    master_blok_tracker = 0;
    master_doc_tracker = 0;
    master_image_tracker = 0;
    blok_count = 0;
    doc_count = 0;
    image_count = 0;
    global_total_pages_added = 0;
    
    // do not save images - flip to 1 to save
    global_image_save_on = -1;
    
    // *** Processing Starts here ***

    // safety check #1 - confirm that file extension is .pdf
    strcpy(file_type, get_file_type(input_filename));
                    
    if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
        
        strcpy(tmp_fp_iter,doc_file_fp);
        strcat(tmp_fp_iter,input_filename);
        
        // new insert starts here
        // printf("update: pdf parser - input_filename - %s \n", input_filename);
        // strcpy(files_copies[0],input_filename);
        // new insert ends here
        
        go_ahead_check = 1;
    }
    else {
        go_ahead_check = -1;
    }
    // safety check #2 - confirm that file exists in file path
    if ((bloks_out = fopen(tmp_fp_iter,"r")) == NULL)
    {
        go_ahead_check = -2;
    }
    fclose(bloks_out);
    
    
    // start processing here
    
    if (go_ahead_check == 1) {
    
        // start clock with main loop processing
    
        start_t = clock();
    
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,library_name);
        strcpy(my_doc.account_name,account_name);
        
        // new insert starts here
        strcpy(my_doc.file_name, tmp_fp_iter);
        //printf("update: files_copies[0]- %s \n", files_copies[0]);
        //strcpy(my_doc.file_name, files_copies[0]);
        //printf("update: survived assignment - %s \n", my_doc.file_name);
        //printf("update: tmp_fp_iter - %s \n", tmp_fp_iter);
        // new insert ends here
        
        bloks_created = pdf_builder (tmp_fp_iter, account_name,library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            // doc_count ++;
            // master_doc_tracker ++;
            
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            pdf_counter ++;
            upload_files_count ++;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - blocks_created = \n",bloks_created);
            }
            
        }}
    
    else {
        
        if (go_ahead_check == -1) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file is not PDF. \n");
            }
        }
        
        if (go_ahead_check == -2) {
            
            if (debug_mode == 1) {
                printf("error: pdf_parser - file not found at this path. \n");
            }
        }
        
    }
                
    
    // last step - update the counters
    
    //dummy = update_counters(input_account_name,input_library_name,blok_count,doc_count,image_count);

    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("\nSummary PDF:  pdf files-%d \n", pdf_counter);
        printf("Summary PDF:  total processed upload files-%d \n", upload_files_count);
        printf("Summary PDF:  total blocks created - %d \n", blok_count);
        printf("Summary PDF:  total pages added - %d \n", global_total_pages_added);
        printf("Summary PDF:  PDF Processing - Finished - time elapsed - %f \n",total_t);
    }
    
    return global_total_pages_added;
}




// new path designed for entry point for llmware specifically


int add_pdf_main_llmware (char *input_account_name,
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
                          char *db_pw)

{
    
    clock_t start_t, end_t;
    
    time_t ti = time(NULL);
    struct tm *tm = localtime(&ti);
    char time_stamp[64];
    
    int dummy;
    
    int bloks_created = 0;
    char file_type[100];
    char web_dir_upload_folder[200];
    char tmp_fp_iter[1000];
    
    // start change here
    char local_short_name_copy[300];
    // end change here
    
    char doc_file_fp[300];
    char f_in[200];
    
    int blok_count, doc_count, image_count;
    
    strftime(time_stamp, sizeof(time_stamp), "%c", tm);
    
    // *** INPUT START ***
    
    strcpy(web_dir_upload_folder,input_fp);
    strcpy(doc_file_fp,input_fp);
    
    // start change here
    strcpy(local_short_name_copy,"");
    // end change here
    
    DIR *web_dir;
    struct dirent *ent;
    int upload_files_count=0;
    
    int pdf_counter = 0;
    int found = 0;
    int i = 0;
    int j = 0;
    char files[5000][300];
    
    int new_doc_id = -1;
    
    global_mongo_db_path = input_mongodb_path;
    //global_master_fp = input_master_fp;
    global_image_fp = input_images_fp;
    
    global_table_count = 0;
    
    // NEW - set global 'customize' parameter configurations
    
    // GLOBAL_BLOK_SIZE = 400;
    GLOBAL_BLOK_SIZE = user_blok_size;
    
    IMG_MIN_HEIGHT = 5;
    IMG_MIN_WIDTH = 5;
    IMG_MIN_HxW = 250;
    global_ccitt_image_save_on = 1;
    global_png_convert_on = 1;
    
    GLOBAL_WRITE_TO_DB = write_to_db_on;
    global_write_to_filename = write_to_filename;
    
    debug_mode = input_debug_mode;
    
    master_blok_tracker = 0;
    
    //master_doc_tracker = unique_doc_num;
    master_doc_tracker = 0;
    
    master_image_tracker = 0;
    blok_count = 0;
    doc_count = 0;
    image_count = 0;
    
    global_total_pages_added = 0;
    
    if (input_image_save_mode == 1) {
        // default case
        global_image_save_on = 1;
    }
    else
    {
        // option to "turn off" image gathering
        global_image_save_on = -1;
    }
    
    
    // *** Processing Starts here ***

    // Main Loop through upload folder

    if ((web_dir = opendir (web_dir_upload_folder)) != NULL) {
      
        while ((ent = readdir (web_dir)) != NULL) {
            
            found = 0;
            
            if ((strcmp(ent->d_name,".") > 0) && ((strcmp(ent->d_name,"..") >0)) && (strcmp(ent->d_name,".DS_Store"))) {
                
                //  iterating through folder looking for PDF files
                //  printf("iterating thru folder-%s-%d \n", ent->d_name,upload_files_count);
                
                strcpy(file_type,get_file_type(ent->d_name));
                
                if ((strcmp(file_type,"pdf")==0) || (strcmp(file_type,"PDF")==0)) {
                    
                    // found pdf file -> will skip any other file extensions in the folder
                    //  printf ("found pdf file-%s-%d \n", ent->d_name,pdf_counter);
                    
                    pdf_counter ++;
                    found = 1;}
            
                if (found == 1) {
                    
                    strcpy(tmp_fp_iter,doc_file_fp);
                    strcat(tmp_fp_iter,ent->d_name);
                    
                    //  adding to files[] list the found matching pdf file for further processing
                    //  printf("processing fp-%s \n",tmp_fp_iter);
                    
                    strcpy(files[upload_files_count],tmp_fp_iter);
                    
                    upload_files_count ++;
                    
                }
                
            }}}
    
    closedir (web_dir);
    
    //free(ent);
    
    // start clock with main loop processing
    
    start_t = clock();
    
    for (i=0; i < upload_files_count ; i++) {
        
        //printf("update:  pdf initiating file processing fp-%d-%s \n", i,files[i]);
        
        // step 1- pdf_builder - output will be blocks_created
        
        strcpy(my_doc.corpus_name,input_library_name);
        strcpy(my_doc.account_name,input_account_name);
        strcpy(my_doc.file_name, files[i]);
        
        // insert change here
        strcpy(local_short_name_copy, files[i]);
        strcpy(my_doc.file_short_name, get_file_name(local_short_name_copy));
        // ends change here
        
        // note: key config option - if unique_doc_num >=0, then use as default and increment directly
        //  -- if unique_doc_num < 0, then regard as "OFF" and pull_new_doc_id directly from library db
        
        if (unique_doc_num < 0) {
            master_doc_tracker = pull_new_doc_id(input_account_name,input_library_name);
        }
        else {
            master_doc_tracker = unique_doc_num + doc_count;
        }
        
        // very simple triage - will need to enhance over time
        
        if (master_doc_tracker < 1) {
            // something went wrong - should return +1 at a minimum as "after" response
            // setting a large, arbitrary integer so we can identify a problem post-parsing...
            master_doc_tracker = 1000000;
        }
        
        if (debug_mode == 1) {
            //printf("update: pdf_parser - pull_new_doc_id call - master_doc_tracker - new doc id = %d \n", master_doc_tracker);
        }
        
        bloks_created = pdf_builder (files[i], input_account_name,input_library_name, time_stamp);
        

        if (bloks_created > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - finished reading document- added total new blocks - %d \n", bloks_created);
            }
            
            doc_count ++;
            //master_doc_tracker ++;
            
            // change - re-start blok_count and image_count at 0 with each new doc
            master_blok_tracker = 0;
            master_image_tracker = 0;
            
            blok_count = blok_count + bloks_created;
            image_count = image_count +  master_new_images_added;
            
            }
        
        else {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no content captured - blocks_created = %d \n",bloks_created);
            }
            
        }
        
        if (global_text_found == 0 || bloks_created == 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no text content found - even though images found - %d \n", bloks_created);
            }
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - no text found in parsing. \n");
            }
            
            //  char myevent[10];
            //  sprintf(myevent, "%d", master_doc_tracker);
            
            //master_doc_tracker ++;
            //global_total_pages_added = starting_global_total_pages_added; //Reset any pages that may have been added while parsing this invalid doc
            
        }
        
        if (global_unhandled_img_counter > 0) {
            
            if (debug_mode == 1) {
                printf("update: pdf_parser - global unhandled img counter > 0 - flag for Triage processing - %d \n", global_unhandled_img_counter);
            }
            
            if (global_text_found == 0 || bloks_created == 0) {
                //printf("update: UNHANDLED_IMAGES <-> NO TEXT \n");
            }
            
            else
            
            {
                //printf("update: UNHANDLED_IMAGES with TEXT \n");
                
                // note: only registering a separate UNHANDLED_IMAGES event if text is found
                //  e.g., "UNHANDLED_IMAGES" = text was found, but likely missing some scanned content
                
            }
            
        }
           
    }
    
    // last step - update the counters
    
    if (GLOBAL_WRITE_TO_DB == 1) {
        dummy = update_library_inc_totals(my_doc.account_name, my_doc.corpus_name, doc_count, blok_count, image_count, global_total_pages_added, global_table_count);
    }
    
    end_t = clock();
    double total_t;
    total_t = ((double) (end_t - start_t) / CLOCKS_PER_SEC);
    
    if (debug_mode == 1) {
        printf("summary: pdf_parser - total pdf files processed - %d \n", pdf_counter);
        printf("summary: pdf_parser - total input files received - %d \n", upload_files_count);
        printf("summary: pdf_parser - total blocks created - %d \n", blok_count);
        printf("summary: pdf_parser - total images created - %d \n", image_count);
        printf("summary: pdf_parser - total tables created - %d \n", global_table_count);
        printf("summary: pdf_parser - total pages added - %d \n", global_total_pages_added);
        printf("summary: pdf_parser - PDF Processing - Finished - time elapsed - %f \n",total_t);
    }
    
    return global_total_pages_added;
}

