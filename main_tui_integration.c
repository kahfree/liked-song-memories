#include "cJSON.h"
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 8080

// Include the function declarations
#include "main_functions.h"

// TUI function to display songs
void display_songs_tui(cJSON *batch, struct notcurses *nc) {
  struct ncplane *n = notcurses_stdplane(nc);
  if (!n) return;
  
  cJSON *items = cJSON_GetObjectItemCaseSensitive(batch, "items");
  cJSON *item = NULL;
  int row = 0;
  
  ncplane_putstr(n, "Your Liked Spotify Songs:");
  row += 2;
  
  cJSON_ArrayForEach(item, items) {
    if (item != NULL && row < 20) { // Limit to 20 rows for demo
      cJSON *added_at = cJSON_GetObjectItemCaseSensitive(item, "added_at");
      cJSON *track = cJSON_GetObjectItemCaseSensitive(item, "track");
      if (track != NULL) {
        cJSON *artist = cJSON_GetObjectItemCaseSensitive(track, "artists")->child;
        cJSON *track_name = cJSON_GetObjectItemCaseSensitive(track, "name");
        if (artist != NULL) {
          cJSON *artist_name = cJSON_GetObjectItemCaseSensitive(artist, "name");
          char song_info[256];
          sprintf(song_info, "%s - %s (%s)", artist_name->valuestring, 
                  track_name->valuestring, added_at->valuestring);
          ncplane_putstr_yx(n, row++, 0, song_info);
        }
      }
    }
  }
  
  ncplane_putstr_yx(n, row + 1, 0, "Press 'q' to quit");
  notcurses_render(nc);
}

int main(const int argc, char *argv[]) {
  // Check arg amount
  if (argc != 3) {
    printf("Supply client ID & client secret");
    return -1;
  }
  
  // Initialize notcurses
  struct notcurses *nc = notcurses_init(NULL, stdout);
  if (!nc) {
    fprintf(stderr, "Failed to initialize notcurses\n");
    return -1;
  }
  
  // parse our args
  char *client_id = argv[1];
  char *client_secret = argv[2];
  
  char token_opts[150] = {0};
  int token_opts_result = sprintf(
      token_opts,
      "client_id=%s&response_type=code&scope=user-library-read&redirect_uri=http://127.0.0.1:8080",
      client_id);
  
  perform_initial_auth_request(token_opts);
  
  char buffer[1024] = {0};
  open_socket_for_auth_code(buffer);
  
  char auth_code[300] = {0};
  parse_auth_code(buffer, auth_code);
  
  char args_to_base64_encode[66] = {0};
  sprintf(args_to_base64_encode, "%s:%s", client_id, client_secret);
  
  struct curl_slist *hs = NULL;
  hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
  struct curl_slist *temp22 = NULL;
  char authorization_header[300] = {0};
  sprintf(authorization_header, "Authorization: Basic %s",
          base64Encoder(args_to_base64_encode, 65));
  temp22 = curl_slist_append(hs, authorization_header);
  
  char new_opts[300] = {0};
  sprintf(new_opts,
          "grant_type=authorization_code&code=%s&redirect_uri=http://"
          "127.0.0.1:8080",
          auth_code);
  struct MemoryStruct chunk = {};
  char errbuf[CURL_ERROR_SIZE];
  CURLcode curl_request_result =
      perform_curl_request("https://accounts.spotify.com/api/token", new_opts,
                          hs, &chunk, errbuf, 1L);
  cJSON *parsed_json_response =
      parse_json_response(curl_request_result, &chunk, errbuf);
  
  if (parsed_json_response == NULL) {
    printf("Empty response\n");
    notcurses_stop(nc);
    return -1;
  } else {
    char *access_token = parsed_json_response->child->valuestring;
    
    struct curl_slist *user_headers = NULL;
    char access_token_header[300] = {0};
    sprintf(access_token_header, "Authorization: Bearer %s", access_token);
    user_headers = curl_slist_append(user_headers, access_token_header);
    
    struct MemoryStruct chunk3 = {};
    curl_request_result = perform_curl_request("https://api.spotify.com/v1/me/tracks?offset=0&limit=50&market=IE", NULL, user_headers, &chunk, errbuf, 0L);
    cJSON *liked_songs = parse_json_response(curl_request_result, &chunk, errbuf);
    
    if (liked_songs) {
      // Display songs using TUI
      display_songs_tui(liked_songs, nc);
      
      // Wait for user to press 'q' using notcurses input handling
      bool running = true;
      while (running) {
        // Get input from notcurses
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 50000000 }; // 50ms timeout
        ncinput ni;
        int ret = notcurses_get(nc, &ts, &ni);
        if (ret > 0) {
          if (ni.id == 'q' || ni.id == 'Q') {
            running = false;
          } else if (ni.id == 3) { // Ctrl+C
            running = false;
          }
        }
      }
      
      cJSON_Delete(liked_songs);
    }
  }
  
  notcurses_stop(nc);
  return 0;
}