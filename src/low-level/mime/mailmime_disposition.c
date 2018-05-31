/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mailmime_disposition.c,v 1.17 2011/05/03 16:30:22 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_disposition.h"
#include "mailmime.h"
#include "charconv.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int
mailmime_disposition_parm_parse(const char * message, size_t length,
				size_t * indx,
				struct mailmime_disposition_parm **
				result);

static int
mailmime_creation_date_parm_parse(const char * message, size_t length,
				  size_t * indx, char ** result);

static int
mailmime_filename_parm_parse(const char * message, size_t length,
			     size_t * indx, char ** result);

static int
mailmime_modification_date_parm_parse(const char * message, size_t length,
				      size_t * indx, char ** result);

static int
mailmime_read_date_parm_parse(const char * message, size_t length,
			      size_t * indx, char ** result);

static int
mailmime_size_parm_parse(const char * message, size_t length,
			 size_t * indx, size_t * result);

static int
mailmime_quoted_date_time_parse(const char * message, size_t length,
				size_t * indx, char ** result);

/*
     disposition := "Content-Disposition" ":"
                    disposition-type
                    *(";" disposition-parm)

*/


int mailmime_disposition_parse(const char * message, size_t length,
			       size_t * indx,
			       struct mailmime_disposition ** result)
{
  size_t final_token;
  size_t cur_token;
  struct mailmime_disposition_type * dsp_type;
  clist * list;
  struct mailmime_disposition * dsp;
  int r;
  int res;

  cur_token = * indx;

  r = mailmime_disposition_type_parse(message, length, &cur_token,
				      &dsp_type);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_type;
  }

  while (1) {
    struct mailmime_disposition_parm * param;

    final_token = cur_token;
    r = mailimf_unstrict_char_parse(message, length, &cur_token, ';');
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      break;
    }
    else {
      res = r;
      goto free_list;
    }

    param = NULL;
    r = mailmime_disposition_parm_parse(message, length, &cur_token, &param);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      cur_token = final_token;
      break;
    }
    else {
      res = r;
      goto free_list;
    }

    r = clist_append(list, param);
    if (r < 0) {
      res = MAILIMF_ERROR_MEMORY;
      goto free_list;
    }
  }

  dsp = mailmime_disposition_new(dsp_type, list);
  if (dsp == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = dsp;
  * indx = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_disposition_parm_free, NULL);
  clist_free(list);
 free_type:
  mailmime_disposition_type_free(dsp_type);
 err:
  return res;
}

/*		    
     disposition-type := "inline"
                       / "attachment"
                       / extension-token
                       ; values are not case-sensitive

*/



int
mailmime_disposition_type_parse(const char * message, size_t length,
				size_t * indx,
				struct mailmime_disposition_type ** result)
{
  size_t cur_token;
  int type;
  char * extension;
  struct mailmime_disposition_type * dsp_type;
  int r;
  int res;

  cur_token = * indx;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  type = MAILMIME_DISPOSITION_TYPE_ERROR; /* XXX - removes a gcc warning */

  extension = NULL;
  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "inline");
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_DISPOSITION_TYPE_INLINE;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "attachment");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISPOSITION_TYPE_ATTACHMENT;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailmime_extension_token_parse(message, length, &cur_token,
				       &extension);
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISPOSITION_TYPE_EXTENSION;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  dsp_type = mailmime_disposition_type_new(type, extension);
  if (dsp_type == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = dsp_type;
  * indx = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (extension != NULL)
    free(extension);
 err:
  return res;
}

/*
     disposition-parm := filename-parm
                       / creation-date-parm
                       / modification-date-parm
                       / read-date-parm
                       / size-parm
                       / parameter
*/


int mailmime_disposition_guess_type(const char * message, size_t length,
				    size_t indx)
{
  if (indx >= length)
    return MAILMIME_DISPOSITION_PARM_PARAMETER;

  switch ((char) toupper((unsigned char) message[indx])) {
  case 'F':
    return MAILMIME_DISPOSITION_PARM_FILENAME;
  case 'C':
    return MAILMIME_DISPOSITION_PARM_CREATION_DATE;
  case 'M':
    return MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE;
  case 'R':
    return MAILMIME_DISPOSITION_PARM_READ_DATE;
  case 'S':
    return MAILMIME_DISPOSITION_PARM_SIZE;
  default:
    return MAILMIME_DISPOSITION_PARM_PARAMETER;
  }
}

static int
mailmime_disposition_parm_parse(const char * message, size_t length,
				size_t * indx,
				struct mailmime_disposition_parm **
				result)
{
  char * filename;
  char * creation_date;
  char * modification_date;
  char * read_date;
  size_t size;
  struct mailmime_parameter * parameter;
  size_t cur_token;
  struct mailmime_disposition_parm * dsp_parm;
  int type;
  int guessed_type;
  int r;
  int res;

  cur_token = * indx;

  filename = NULL;
  creation_date = NULL;
  modification_date = NULL;
  read_date = NULL;
  size = 0;
  parameter = NULL;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  guessed_type = mailmime_disposition_guess_type(message, length, cur_token);

  type = MAILMIME_DISPOSITION_PARM_PARAMETER;

  switch (guessed_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    r = mailmime_filename_parm_parse(message, length, &cur_token,
				     &filename);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    r = mailmime_creation_date_parm_parse(message, length, &cur_token,
					  &creation_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    r = mailmime_modification_date_parm_parse(message, length, &cur_token,
					      &modification_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
    
  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    r = mailmime_read_date_parm_parse(message, length, &cur_token,
				      &read_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_SIZE:
    r = mailmime_size_parm_parse(message, length, &cur_token,
				 &size);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  }

  if (type == MAILMIME_DISPOSITION_PARM_PARAMETER) {
    r = mailmime_parameter_parse(message, length, &cur_token,
				 &parameter);
    if (r != MAILIMF_NO_ERROR) {
      type = guessed_type;
      res = r;
      goto err;
    }
  }

  dsp_parm = mailmime_disposition_parm_new(type, filename, creation_date,
					   modification_date, read_date,
					   size, parameter);

  if (dsp_parm == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = dsp_parm;
  * indx = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (filename != NULL)
    mailmime_filename_parm_free(filename);
  if (creation_date != NULL)
    mailmime_creation_date_parm_free(creation_date);
  if (modification_date != NULL)
    mailmime_modification_date_parm_free(modification_date);
  if (read_date != NULL)
    mailmime_read_date_parm_free(read_date);
  if (parameter != NULL)
    mailmime_parameter_free(parameter);
 err:
  return res;
}

/*
//     filename-parm := "filename" "=" value
*/

static int
mailmime_extended_parm_parse(const char * message, size_t length,
			                 char * key, size_t * indx, char ** result)
{
    int r;
    size_t cur_token;
    char* built_str = NULL;
    size_t built_len = 0;
    
    cur_token = * indx;

    r = mailimf_token_case_insensitive_parse(message, length,
    				                         &cur_token, key);
    if (r != MAILIMF_NO_ERROR)
        return r;

    // Ok, we know it's of this type.
    // So let's see if it's encoded or extended or both

    int encoded = 0;
    int extended = 0;

    // Find out if message is extended, encoded, or both
    r = mailimf_char_parse(message, length, &cur_token, '*');

    if (r == MAILIMF_NO_ERROR) {
        r = mailimf_char_parse(message, length, &cur_token, '0');

        if (r == MAILIMF_NO_ERROR) {
            extended = 1;
            r = mailimf_char_parse(message, length, &cur_token, '*');
            if (r == MAILIMF_NO_ERROR)
                encoded = 1;
            else if (r != MAILIMF_ERROR_PARSE)
                return r;
        }    
        else if (r != MAILIMF_ERROR_PARSE)
            return r;
        else
            encoded = 1;
    }
    else //if (r != MAILIMF_ERROR_PARSE)
        return r;
  
    r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
    if (r != MAILIMF_NO_ERROR)
        return r;
  
    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
        return r;

    // Ok, let's go.
    if (encoded || extended) {
        char* _charset = NULL;
        char* _lang = NULL;
        
        
        // Get the first of either
        if (encoded) {
            r = mailmime_extended_initial_value_parse(message, length, &cur_token, &built_str, &_charset,
                                                      &_lang);
            if (r != MAILIMF_NO_ERROR)
                return r;            
        } 
        else if (extended) {
            r = mailmime_value_parse(message, length, &cur_token, &built_str);
            if (r != MAILIMF_NO_ERROR)
                return r;            
        }                
        // Ok, we have an initial string and know it's extended, so let's roll.
        if (extended && built_str) {
            built_len = strlen(built_str);

            while (1) {

                r = mailimf_unstrict_char_parse(message, length, &cur_token, ';');
                if (r != MAILIMF_NO_ERROR && r != MAILIMF_ERROR_PARSE)
                    return r;

                r = mailimf_cfws_parse(message, length, &cur_token);
                if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
                    return r;
                    
                // FIXME: this is where we have to check and see what really happens...
                r = mailimf_token_case_insensitive_parse(message, length,
                				                         &cur_token, key);
                if (r == MAILIMF_ERROR_PARSE)
                    break;
                    
                if (r != MAILIMF_NO_ERROR)
                    return r;

                // Ok, we know it's of this type.
                // So let's see if it's encoded or extended or both
                
                // int part_encoded = 0;
                // int part_extended = 0;

                // Find out if message part is extended, encoded, or both
                r = mailimf_char_parse(message, length, &cur_token, '*');

                if (r == MAILIMF_NO_ERROR) {
                    uint32_t part_num = 0;
                    r = mailimf_number_parse(message, length, &cur_token, &part_num);

                    if (r == MAILIMF_NO_ERROR) {
//                        part_extended = 1;
                        r = mailimf_char_parse(message, length, &cur_token, '*');
                        // See RFC2231, Section 4.1. FIXME - it's possible to have unencoded parts interspersed 
                        // with encoded post per RFC, so this may not be smart. Depends on if decoding is an issue with
                        // interspersed ASCII segments.
                        // However, at this point, we know that the first part of the parameter either contained encoding information,
                        // or it shouldn't be encoded. Also, it seems very doubtful most clients would go to the trouble of mixing encoded
                        // and non-encoded information when splitting the string.
                        // The fix right now is to ignore the encoding flag at this point, as we will either decode the whole string,
                        // or not at all. 
                    }    
                    else if (r != MAILIMF_ERROR_PARSE)
                        return r;
                }
                else if (r != MAILIMF_ERROR_PARSE)
                    return r;
              
                r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
                if (r != MAILIMF_NO_ERROR)
                    return r;
              
                r = mailimf_cfws_parse(message, length, &cur_token);
                if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
                    return r;

                // Ok, let's go.
//                if (part_encoded || part_extended) {                    
                    
                char* part_str = NULL;
                
                // See RFC2231, Section 4.1. FIXME - it's possible to have unencoded parts interspersed 
                // with encoded post per RFC, so this may not be smart. Depends on if decoding is an issue with
                // interspersed ASCII segments.                
                r = mailmime_value_parse(message, length, &cur_token, &part_str);
                if (r != MAILIMF_NO_ERROR)
                    return r;        
                                                 
                size_t part_size = strlen(part_str);
                size_t new_size = built_len + part_size + 1;
                
                char* new_str = NULL;
                new_str = realloc((void*)built_str, new_size);
                if (new_str) {
                    strncat(new_str, part_str, part_size);
                    built_str = new_str;
                    free(part_str);
                    part_str = NULL;
                }
                else {
                    free(built_str);
                    return MAILIMF_ERROR_MEMORY;
                }                                               

//                }
                built_len = strlen(built_str);    
            }
        }
        
        if (encoded && built_str && _charset && _charset[0] != '\0') {
            char* replace_str = NULL;
            mailmime_parm_value_unescape(&replace_str, built_str);

            if (replace_str) {
                free(built_str);
                built_str = replace_str;
                replace_str = NULL;
            }
                
            if (strcasecmp(_charset, "utf-8") != 0 && 
                strcasecmp(_charset, "utf8") != 0) {
                
                // best effort
                r = charconv("utf-8", _charset, built_str,
                             strlen(built_str), &replace_str);

                switch(r) {
                    case MAIL_CHARCONV_ERROR_UNKNOWN_CHARSET:
                        r = charconv("utf-8", "iso-8859-1", built_str,
                                      strlen(built_str), &replace_str);
                        break;
                    case MAIL_CHARCONV_ERROR_MEMORY:
                        return MAILIMF_ERROR_MEMORY;
                    case MAIL_CHARCONV_ERROR_CONV:
                        return MAILIMF_ERROR_PARSE;
                }                                                                      
                switch (r) {
                    case MAIL_CHARCONV_ERROR_MEMORY:
                        return MAILIMF_ERROR_MEMORY;
                    case MAIL_CHARCONV_ERROR_CONV:
                        return MAILIMF_ERROR_PARSE;
                }
            }
                                        
            if (replace_str) {                         
                built_str = replace_str;
                replace_str = NULL;
            }
        }        
    }
    
    *indx = cur_token;
    *result = built_str;

    return MAILIMF_NO_ERROR;
}



/*
     filename-parm := "filename" "=" value
*/

static int
mailmime_filename_parm_parse(const char * message, size_t length,
			     size_t * indx, char ** result)
{
  char * value;
  int r;
  size_t cur_token;

  cur_token = * indx;
  
  r = mailmime_extended_parm_parse(message, length, "filename", &cur_token, &value);
  
  if (r != MAILIMF_NO_ERROR) {

    r = mailimf_token_case_insensitive_parse(message, length,
    				   &cur_token, "filename");
    if (r != MAILIMF_NO_ERROR)
        return r;

    r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
    if (r != MAILIMF_NO_ERROR)
        return r;

    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
        return r;

    r = mailmime_value_parse(message, length, &cur_token, &value);
    if (r != MAILIMF_NO_ERROR)
        return r;

  }

  * indx = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     creation-date-parm := "creation-date" "=" quoted-date-time
*/

static int
mailmime_creation_date_parm_parse(const char * message, size_t length,
				  size_t * indx, char ** result)
{
  char * value;
  int r;
  size_t cur_token;

  cur_token = * indx;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "creation-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * indx = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     modification-date-parm := "modification-date" "=" quoted-date-time
*/

static int
mailmime_modification_date_parm_parse(const char * message, size_t length,
				      size_t * indx, char ** result)
{
  char * value;
  size_t cur_token;
  int r;

  cur_token = * indx;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "modification-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * indx = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     read-date-parm := "read-date" "=" quoted-date-time
*/

static int
mailmime_read_date_parm_parse(const char * message, size_t length,
			      size_t * indx, char ** result)
{
  char * value;
  size_t cur_token;
  int r;

  cur_token = * indx;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "read-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * indx = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     size-parm := "size" "=" 1*DIGIT
*/

static int
mailmime_size_parm_parse(const char * message, size_t length,
			 size_t * indx, size_t * result)
{
  uint32_t value;
  size_t cur_token;
  int r;

  cur_token = * indx;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "size");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;
  
  r = mailimf_number_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * indx = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     quoted-date-time := quoted-string
                      ; contents MUST be an RFC 822 `date-time'
                      ; numeric timezones (+HHMM or -HHMM) MUST be used
*/

static int
mailmime_quoted_date_time_parse(const char * message, size_t length,
				size_t * indx, char ** result)
{
  return mailimf_quoted_string_parse(message, length, indx, result);
}
