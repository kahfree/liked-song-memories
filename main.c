#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "cmake-build-debug/_deps/cjson-src/cJSON.h"

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

int main(const int argc, char *argv[]) {
    printf("argc: %d\n", argc);
    if (argc != 3) {
        printf("Supply client ID & secret");
        return -1;
    }
    struct MemoryStruct chunk;
    char *client_id = argv[1];
    char *client_secret = argv[2];
    char token_opts[150] = {0};
    int token_opts_result = sprintf(token_opts,
        "grant_type=client_credentials&client_id=%s&client_secret=%s"
        ,client_id,client_secret);

    if (token_opts_result < 1) {
        printf("Something went wrong processing the opts\n");
        return -1;
    }
    // The actual operating object
    CURL *curl;
    // The enum response type for curl operations
    CURLcode result;
    curl = curl_easy_init();
    chunk.data = malloc(1);
    chunk.size = 0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL,
            "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, token_opts);
        char errbuf[CURL_ERROR_SIZE];
        errbuf[0] = 0;
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        void *data = {0};

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        struct curl_slist *hs = NULL;
        hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
        result = curl_easy_perform(curl);
        if(result != CURLE_OK) {
            size_t len = strlen(errbuf);
            fprintf(stderr, "\nlibcurl: (%d) ", result);
            if(len)
                fprintf(stderr, "%s%s", errbuf,
                        ((errbuf[len - 1] != '\n') ? "\n" : ""));
            else
                fprintf(stderr, "%s\n", curl_easy_strerror(result));
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

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return 0;
}