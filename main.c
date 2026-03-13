#include <stdio.h>
#include <curl/curl.h>
int main(const int argc, char *argv[]) {
    printf("argc: %d\n", argc);
    if (argc != 3) {
        printf("Supply client ID & secret");
        return -1;
    }
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
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL,
            "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, token_opts);

        result = curl_easy_perform(curl);
        if(result != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(result));

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return 0;
}