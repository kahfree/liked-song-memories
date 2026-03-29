#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "cJSON.h"

struct MemoryStruct {
    char *data;
    size_t size;
};

size_t write_callback(char *data, size_t size, size_t num_bytes, void *userdata) {
    /* size always 1 according to docs, will avoid using */
    // size_t realsize = size * num_bytes;
    /* casting generic ptr to our struct to reuse memory*/
    struct MemoryStruct *mem = userdata;

    char *parsed_data = realloc(mem->data, mem->size + num_bytes + 1);
    if(!parsed_data) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->data = parsed_data;
    memcpy(&(mem->data[mem->size]), data, num_bytes);
    mem->size += num_bytes;
    mem->data[mem->size] = 0;

    return num_bytes;
}

CURLcode perform_curl_request(char *url, char *opts, struct curl_slist *headers, struct MemoryStruct chunk, char *errbuf) {
    CURL *curl;
    // The enum response type for curl operations
    CURLcode result;
    curl = curl_easy_init();
    chunk.data = malloc(1);
    chunk.size = 0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL,
            "https://accounts.spotify.com/authorize");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts);
        errbuf[0] = 0;
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        void *data = {0};

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        result = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return result;
    }
    curl_global_cleanup();
    return CURLE_FAILED_INIT;
}
int main(const int argc, char *argv[]) {
    printf("argc: %d\n", argc);
    if (argc != 2) {
        printf("Supply client ID");
        return -1;
    }
    struct MemoryStruct chunk;
    char errbuf[CURL_ERROR_SIZE];
    char *client_id = argv[1];
    char token_opts[150] = {0};
    char *url = "https://accounts.spotify.com/authorize";
    struct curl_slist *hs = NULL;
    hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
    int token_opts_result = sprintf(token_opts,
        "client_id=%s&response_type=code&redirect_uri=http://127.0.0.1:80"
        ,client_id);
    if (token_opts_result < 1) {
        printf("Something went wrong processing the opts\n");
        return -1;
    }

    char get_request[300] = {0};
    int get_request_result = sprintf(get_request,"firefox \"https://accounts.spotify.com/authorize?%s\"", token_opts);
    // Launch the GET request in firefox
    system(get_request);
    CURLcode curl_request_result = perform_curl_request(url, token_opts, hs, chunk, errbuf);
    if(curl_request_result != CURLE_OK) {
        size_t len = strlen(errbuf);
        fprintf(stderr, "\nlibcurl: (%d) ", curl_request_result);
        if(len)
            fprintf(stderr, "%s%s", errbuf,
                    ((errbuf[len - 1] != '\n') ? "\n" : ""));
        else
            fprintf(stderr, "%s\n", curl_easy_strerror(curl_request_result));
    } else {
        cJSON *json = cJSON_Parse(chunk.data);
        if (json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                printf("Error: %s\n", error_ptr);
            }
            cJSON_Delete(json);
            return 1;
        }
        char *access_token = json->child->valuestring;
        printf("access_token: %s\n", access_token);
    }

    return 0;
}
