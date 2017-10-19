/**
 *
 * Taliesin - Media server
 * 
 * Webservices (endpoints) implementations
 *
 * Copyright 2017 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU GENERAL PUBLIC LICENSE
 * License as published by the Free Software Foundation;
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "taliesin.h"

int file_list_init(struct _t_file_list * file_list) {
  pthread_mutexattr_t mutexattr;
  
  if (file_list != NULL) {
    file_list->nb_files = 0;
    file_list->start = NULL;
    file_list->end = NULL;
    pthread_mutexattr_init ( &mutexattr );
    pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE_NP );
    if (pthread_mutex_init(&file_list->file_lock, &mutexattr)) {
      fprintf(stderr, "Error setting file_lock\n");
      return T_ERROR;
    }
    return T_OK;
  } else {
    return T_ERROR_PARAM;
  }
}

int file_list_enqueue_file(struct _t_file_list * file_list, struct _t_file * file) {
  if (file_list != NULL && file != NULL) {
    if (pthread_mutex_lock(&file_list->file_lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error lock mutex file_list");
      return T_ERROR;
    } else {
      if (file_list->start == NULL) {
        file_list->start = file;
        file_list->end = file;
      } else {
        file_list->end->next = file;
        file_list->end = file;
      }
      file_list->nb_files++;
      pthread_mutex_unlock(&file_list->file_lock);
      return T_OK;
    }
  } else {
    return T_ERROR_PARAM;
  }
}

int file_list_enqueue_new_file(struct _t_file_list * file_list, const char * path, json_int_t tm_id) {
  struct _t_file * file;
  if (path != NULL) {
    file = o_malloc(sizeof(struct _t_file));
    if (file != NULL) {
      file->path = o_strdup(path);
      file->next = NULL;
      file->tm_id = tm_id;
      if (file_list_enqueue_file(file_list, file) == T_OK) {
        return T_OK;
      } else {
        y_log_message(Y_LOG_LEVEL_ERROR, "file_list_enqueue_new_file - Error file_list_enqueue_file");
        return T_ERROR;
      }
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "file_list_enqueue_new_file - Error allocating resources for file");
      return T_ERROR_MEMORY;
    }
  } else {
    return T_ERROR_PARAM;
  }
}

int file_list_insert_file_at(struct _t_file_list * file_list, struct _t_file * file, unsigned long index) {
  int i;
  struct _t_file * cur_file;
  
  if (file_list != NULL && file != NULL && index >= 0) {
    if (pthread_mutex_lock(&file_list->file_lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error lock mutex file_list");
      return T_ERROR;
    } else {
      cur_file = file_list->start;
      if (index >= (file_list->nb_files)) {
        file_list->end->next = file;
        file_list->end = file;
      } else if (index) {
        // Get the previous file so we can insert the new one
        for (i=0; i<(index - 1) && cur_file != NULL; i++) {
          cur_file = cur_file->next;
        }
        
        if (cur_file != NULL) {
          file->next = cur_file->next;
          cur_file->next = file;
        }
      } else {
        file->next = file_list->start;
        file_list->start = file;
        
      }
      file_list->nb_files++;
      pthread_mutex_unlock(&file_list->file_lock);
      return T_OK;
    }
  } else {
    return T_ERROR_PARAM;
  }
}

struct _t_file * file_list_dequeue_file(struct _t_file_list * file_list, unsigned long index) {
  struct _t_file * file, * previous;
  unsigned long i;
  
  if (file_list != NULL && index < file_list->nb_files) {
    if (pthread_mutex_lock(&file_list->file_lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error lock mutex file_list");
      return NULL;
    } else {
      if (index > 0) {
        file = file_list->start->next;
        previous = file_list->start;
        for (i=1; i<index && file != NULL; i++) {
          file = file->next;
          previous = previous->next;
        }
        previous->next = file->next;
        file->next = NULL;
        file_list->nb_files--;
      } else {
        file = file_list->start;
        if (file->next != NULL) {
          file_list->start = file->next;
        } else {
          file_list->start = NULL;
          file_list->end = NULL;
        }
        file->next = NULL;
        file_list->nb_files--;
      }
      pthread_mutex_unlock(&file_list->file_lock);
      return file;
    }
  } else {
    return NULL;
  }
}

struct _t_file * file_list_get_file(struct _t_file_list * file_list, unsigned long index) {
  struct _t_file * file;
  unsigned long i;
  
  if (file_list != NULL && index < file_list->nb_files) {
    if (pthread_mutex_lock(&file_list->file_lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error lock mutex file_list");
      return NULL;
    } else {
      file = file_list->start;
      if (index > 0) {
        for (i=0; i<index && file != NULL; i++) {
          file = file->next;
        }
      }
      pthread_mutex_unlock(&file_list->file_lock);
      return file;
    }
  } else {
    return NULL;
  }
}

void file_list_clean_file(struct _t_file * file) {
  if (file != NULL) {
    o_free(file->path);
    file->path = NULL;
    if (file->next != NULL) {
      file_list_clean_file(file->next);
      file->next = NULL;
    }
    o_free(file);
  }
}

void file_list_clean(struct _t_file_list * file_list) {
  if (file_list != NULL) {
    if (pthread_mutex_lock(&file_list->file_lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error lock mutex file_list");
    } else {
      if (file_list->start != NULL) {
        file_list_clean_file(file_list->start);
      }
      pthread_mutex_unlock(&file_list->file_lock);
    }
    pthread_mutex_destroy(&file_list->file_lock);
    o_free(file_list);
  }
}

int file_list_add_media_list(struct config_elements * config, struct _t_file_list * file_list, json_t * media_list) {
  json_t * j_media;
  size_t index;
  int ret = T_OK;
  char * full_path;
  
  json_array_foreach(media_list, index, j_media) {
    full_path = msprintf("%s/%s", json_string_value(json_object_get(j_media, "data_source_path")), json_string_value(json_object_get(j_media, "path")));
    //y_log_message(Y_LOG_LEVEL_DEBUG, "Add media to list %s",json_dumps(j_media, JSON_ENCODE_ANY));
    if (file_list_enqueue_new_file(file_list, full_path, json_integer_value(json_object_get(j_media, "id"))) != T_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "file_list_add_media_list - Error file_list_enqueue_new_file for %s", full_path);
      ret = T_ERROR;
      break;
    }
    o_free(full_path);
  }
  return ret;
}
