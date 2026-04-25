#ifndef MAIN_FUNCTIONS_H
#define MAIN_FUNCTIONS_H

#include "cJSON.h"
#include <curl/curl.h>

struct MemoryStruct {
  char *data;
  size_t size;
};

size_t write_callback(char *data, size_t size, size_t num_bytes, void *userdata);
char *base64Encoder(char input_str[], int len_str);
CURLcode perform_curl_request(char *url, char *opts, struct curl_slist *headers,
                              struct MemoryStruct *chunk, char *errbuf,
                              long isPost);
void perform_initial_auth_request(char *opts);
void parse_auth_code(char *buffer, char *code);
void open_socket_for_auth_code(char *buffer);
cJSON *parse_json_response(CURLcode response_code,
                           struct MemoryStruct *raw_response, char *errbuf);

#endif