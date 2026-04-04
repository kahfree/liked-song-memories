#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "cJSON.h"
#define PORT 8080

struct MemoryStruct {
    char *data;
    size_t size;
};

size_t write_callback(char *data, size_t size, size_t num_bytes, void *userdata);
char* base64Encoder(char input_str[], int len_str);
CURLcode perform_curl_request(char *url, char *opts, struct curl_slist *headers, struct MemoryStruct *chunk, char *errbuf, long isPost);
void perform_initial_auth_request(char *opts);
void parse_auth_code(char *buffer, char *code);
void open_socket_for_auth_code(char *buffer);

int main(const int argc, char *argv[]) {
    // Check arg amount
    if (argc != 3) {
        printf("Supply client ID & client secret");
        return -1;
    }
    // parse our args
    char *client_id = argv[1];
    char *client_secret = argv[2];

    char token_opts[150] = {0};
    int token_opts_result = 
      sprintf(token_opts,"client_id=%s&response_type=code&redirect_uri=http://127.0.0.1:8080",client_id);

    perform_initial_auth_request(token_opts);

    char buffer[1024] = { 0 };
    open_socket_for_auth_code(buffer);

    char auth_code[300] = {0};
    parse_auth_code(buffer, auth_code);
    printf("%s\n", auth_code);

    char args_to_base64_encode[66] = {0};
    sprintf(args_to_base64_encode, "%s:%s", client_id, client_secret);

    struct curl_slist *hs = NULL;
    hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
    struct curl_slist *temp22= NULL;
    char authorization_header[300] = {0}; 
    sprintf(authorization_header, "Authorization: Basic %s", base64Encoder(args_to_base64_encode, 65));
    temp22 = curl_slist_append(hs, authorization_header);

    char new_opts[300] = {0};
    sprintf(new_opts,
        "grant_type=authorization_code&code=%s&redirect_uri=http://127.0.0.1:8080"
        ,auth_code);
    struct MemoryStruct chunk = {};
    char errbuf[CURL_ERROR_SIZE];
    CURLcode curl_request_result = perform_curl_request("https://accounts.spotify.com/api/token", new_opts, hs, &chunk, errbuf, 1L);
    if(curl_request_result != CURLE_OK) {
      printf("requst wasn't ok\n");
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
    // TODO: Make a basic query on user information and print to console
    return 0;
}

void perform_initial_auth_request(char *opts) {
    char initial_auth_request[300] = {0};
    // Choose command based on OS
    #ifdef __APPLE__
        sprintf(initial_auth_request,"open \"https://accounts.spotify.com/authorize?%s\"", opts);
    #elif __linux__
        sprintf(initial_auth_request,"firefox \"https://accounts.spotify.com/authorize?%s\"", opts);
    #elif __unix__
        sprintf(initial_auth_request,"firefox \"https://accounts.spotify.com/authorize?%s\"", opts);
    #else
        printf("Operating System: Unknown\n");
    #endif
    // Send the GET request via CLI command
    system(initial_auth_request);
}

void parse_auth_code(char *buffer, char *code) {
    // Known character length of auth code
    const int AUTH_CODE_SIZE = 162 + 11;
    // Offset as we expect 'GET /?code='
    const int CODE_OFFSET = 11;
    for(int i = CODE_OFFSET; i < 300; i++) {
      if(buffer[i] == ' ')
        break;
      code[i-CODE_OFFSET] = buffer[i];
    }

    printf("auth code now: %s\n", code);
}

void open_socket_for_auth_code(char *buffer) {
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char* hello = "Hello from server";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("weve created the socket\n");

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);
    printf("set the socket options\n");

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("set the socket to the port\n");
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("done listening to the socket\n");
    if ((new_socket
         = accept(server_fd, (struct sockaddr*)&address,
                  &addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // subtract 1 for the null
    // terminator at the end
    valread = read(new_socket, buffer,
                   1024 - 1); 
    printf("I've read the buffer %d\n", valread);
    close(new_socket);
    close(server_fd);
}
CURLcode perform_curl_request(char *url, char *opts, struct curl_slist *headers, struct MemoryStruct *chunk, char *errbuf, long isPost) {
    CURL *curl;
    // The enum response type for curl operations
    CURLcode result;
    curl = curl_easy_init();
    chunk->data = malloc(1);
    chunk->size = 0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, isPost);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, opts);
        errbuf[0] = 0;
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        void *data = {0};

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        result = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return result;
    }
    curl_global_cleanup();
    return CURLE_FAILED_INIT;
}

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

    printf("parsed data %s\n", parsed_data);
    printf("returning num bytes %d\n", num_bytes);
    return num_bytes;
}

char* base64Encoder(char input_str[], int len_str)
{
    // Character set of base64 encoding scheme
    char char_set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    // Resultant string
    char *res_str = (char *) malloc(1000 * sizeof(char));
    
    int index, no_of_bits = 0, padding = 0, val = 0, count = 0, temp;
    int i, j, k = 0;
    
    // Loop takes 3 characters at a time from 
    // input_str and stores it in val
    for (i = 0; i < len_str; i += 3) {
        val = 0, count = 0, no_of_bits = 0;

        for (j = i; j < len_str && j <= i + 2; j++) {
          // binary data of input_str is stored in val
          val = val << 8; 
          
          // (A + 0 = A) stores character in val
          val = val | input_str[j]; 
          
          // calculates how many time loop 
          // ran if "MEN" -> 3 otherwise "ON" -> 2
          count++;
        }

        no_of_bits = count * 8; 

        // calculates how many "=" to append after res_str.
        padding = no_of_bits % 3; 

        // extracts all bits from val (6 at a time) 
        // and find the value of each block
        while (no_of_bits != 0) {
          // retrieve the value of each block
          if (no_of_bits >= 6)
          {
              temp = no_of_bits - 6;
              
              // binary of 63 is (111111) f
              index = (val >> temp) & 63; 
              no_of_bits -= 6;         
          }
          else
          {
              temp = 6 - no_of_bits;
              // append zeros to right if bits are less than 6
              index = (val << temp) & 63; 
              no_of_bits = 0;
          }
          res_str[k++] = char_set[index];
        }
    }

    // padding is done here
    for (i = 1; i <= padding; i++) {
        res_str[k++] = '=';
    }

    res_str[k] = '\0';
    return res_str;
}
