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
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#define PORT 8080

// Include the function declarations
#include "main_functions.h"

// Function to download an image using libcurl
size_t write_image_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

int download_image(const char *url, const char *filename) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    
    curl = curl_easy_init();
    if (!curl) {
        return 1;
    }
    
    fp = fopen(filename, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return 1;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_image_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    return (res == CURLE_OK) ? 0 : 1;
}

// Enhanced TUI function to display songs in a table with inline album art
void display_songs_with_inline_art(cJSON *batch, struct notcurses *nc) {
  struct ncplane *n = notcurses_stdplane(nc);
  if (!n) return;
  
  cJSON *items = cJSON_GetObjectItemCaseSensitive(batch, "items");
  cJSON *item = NULL;
  int row = 0;
  
  // Draw table header
  ncplane_putstr_yx(n, row++, 0, "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗");
  ncplane_putstr_yx(n, row++, 0, "║                                                                                              ║");
  ncplane_putstr_yx(n, row++, 0, "║   Date Added       │ Song Name                          │ Artist               │ Album       ║");
  ncplane_putstr_yx(n, row++, 0, "║                                                                                              ║");
  
  int songs_displayed = 0;
  
  cJSON_ArrayForEach(item, items) {
    if (item != NULL && row < 10) { // Limit to 10 songs for better display
      cJSON *added_at = cJSON_GetObjectItemCaseSensitive(item, "added_at");
      cJSON *track = cJSON_GetObjectItemCaseSensitive(item, "track");
      if (track != NULL) {
        cJSON *artist = cJSON_GetObjectItemCaseSensitive(track, "artists")->child;
        cJSON *track_name = cJSON_GetObjectItemCaseSensitive(track, "name");
        cJSON *album = cJSON_GetObjectItemCaseSensitive(track, "album");
        
        if (artist != NULL && track_name != NULL) {
          cJSON *artist_name = cJSON_GetObjectItemCaseSensitive(artist, "name");
          cJSON *album_name = cJSON_GetObjectItemCaseSensitive(album, "name");
          
          // Format the date (first 10 characters of ISO date)
          char date_buf[11] = {0};
          if (added_at->valuestring && strlen(added_at->valuestring) >= 10) {
            strncpy(date_buf, added_at->valuestring, 10);
          }
          
          // Truncate long names
          char song_buf[30] = {0};
          char artist_buf[20] = {0};
          char album_buf[15] = {0};
          
          strncpy(song_buf, track_name->valuestring, 29);
          strncpy(artist_buf, artist_name->valuestring, 19);
          strncpy(album_buf, album_name->valuestring, 14);
          
          // Draw table row
          char row_str[150];
          snprintf(row_str, sizeof(row_str), "║ %-18s │ %-32s │ %-20s │ %-15s ║", 
                  date_buf, song_buf, artist_buf, album_buf);
          ncplane_putstr_yx(n, row++, 0, row_str);
          
          // Check for album images and download/display them inline
          cJSON *images = cJSON_GetObjectItemCaseSensitive(album, "images");
          if (images && cJSON_GetArraySize(images) > 0) {
            cJSON *first_image = cJSON_GetArrayItem(images, 0);
            cJSON *url = cJSON_GetObjectItemCaseSensitive(first_image, "url");
            if (url && url->valuestring) {
              // Create a unique filename for the image
              char filename[100];
              snprintf(filename, sizeof(filename), "/tmp/album_art_%d_%ld.jpg", songs_displayed++, time(NULL));
              
              // Download the image
              if (download_image(url->valuestring, filename) == 0) {
                // Try to display the image using notcurses
                struct ncvisual *ncv = ncvisual_from_file(filename);
                if (ncv) {
                  // Create a new plane for the image below the song row
                  // Create a new plane for the image below the song row
                  // Create a smaller image plane that fits better in Ghostty
                  struct ncplane_options nopts = {
                    .y = row,
                    .x = 70,  // Position further right to avoid table overlap
                    .rows = 4,   // Smaller height for better fit
                    .cols = 12,  // Narrower width for better terminal fit
                    .userptr = NULL,
                    .name = "imageplane",
                    .resizecb = NULL,
                    .flags = 0,
                    .margin_b = 0,
                    .margin_r = 0,
                  };
                  
                  struct ncplane *image_plane = ncplane_create(n, &nopts);
                  if (image_plane) {
                    // Scale the image to fit Ghostty terminal better
                    ncvisual_resize(ncv, 4, 12);
                    struct ncvisual_options vopts = {
                      .n = image_plane,
                      .flags = 0,
                      .scaling = NCSCALE_SCALE,  // Better proportional scaling
                      .blitter = NCBLIT_PIXEL,
                      .y = 0,
                      .x = 0,
                    };
                    ncvisual_blit(nc, ncv, &vopts);
                    row += 1; // Only skip 1 row since image is small and to the side
                  }
                  ncvisual_destroy(ncv);
                }
                // Remove the temporary file
                unlink(filename);
              } else {
                // If download fails, just show an indicator
                ncplane_putstr_yx(n, row++, 2, "[Album art failed to load]");
                row++;
              }
            }
          } else {
            ncplane_putstr_yx(n, row, 60, "[No album art]");
            row += 2;
          }
        }
      }
    }
  }
  
  // Draw table footer
  ncplane_putstr_yx(n, row++, 0, "║                                                                                              ║");
  ncplane_putstr_yx(n, row++, 0, "╚═══════════════════════════════════════════════════════════════════════════════════════════════════╝");
  ncplane_putstr_yx(n, row + 1, 0, "Press 'q' to quit. Album art is being downloaded and displayed...");
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
  
  // Initialize curl for image downloads
  curl_global_init(CURL_GLOBAL_ALL);
  
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
    curl_global_cleanup();
    return -1;
  } else {
    char *access_token = parsed_json_response->child->valuestring;
    
    struct curl_slist *user_headers = NULL;
    char access_token_header[300] = {0};
    sprintf(access_token_header, "Authorization: Bearer %s", access_token);
    user_headers = curl_slist_append(user_headers, access_token_header);
    
    struct MemoryStruct chunk3 = {};
    curl_request_result = perform_curl_request("https://api.spotify.com/v1/me/tracks?offset=0&limit=10&market=IE", NULL, user_headers, &chunk, errbuf, 0L);
    cJSON *liked_songs = parse_json_response(curl_request_result, &chunk, errbuf);
    
    if (liked_songs) {
      // Display songs using enhanced TUI with inline album art
      display_songs_with_inline_art(liked_songs, nc);
      
      // Wait for user to press 'q'
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
  curl_global_cleanup();
  return 0;
}