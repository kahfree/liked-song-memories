#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <cJSON.h>
#include <notcurses/notcurses.h>
#include "main_functions.h"

// Add download_image function to download images using CURL
int download_image(const char *filename, const char *url) {
  CURL *curl;
  FILE *fp;
  CURLcode res;
  
  curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "CURL init failed\n");
    return 1;
  }
  
  fp = fopen(filename, "wb");
  if (!fp) {
    curl_easy_cleanup(curl);
    fprintf(stderr, "Failed to open file %s\n", filename);
    return 1;
  }
  
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL); // Default write function
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  
  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  fclose(fp);
  
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    return 1;
  }
  
  return 0;
}

// Function declarations
void display_songs_with_inline_art(cJSON *liked_songs, struct notcurses *nc);

// Main function for the TUI application
int main(int argc, char *argv[]) {
  // Initialize notcurses
  struct notcurses_options opts = {
    .flags = NCOPTION_INHIBIT_SETLOCALE,
  };
  
  struct notcurses* nc = notcurses_init(&opts, stdout);
  if (nc == NULL) {
    fprintf(stderr, "Failed to initialize notcurses\n");
    return 1;
  }
  
  // TODO: Implement Spotify API auth flow and fetch liked songs
  // For now, we'll use a simple placeholder JSON
  const char* sample_json = 
    "{"
    "  \"items\": ["
    "    {"
    "      \"track\": {"
    "        \"name\": \"Sample Track 1\","
    "        \"artists\": [{ \"name\": \"Artist 1\" }],"
    "        \"album\": {"
    "          \"name\": \"Album 1\","
    "          \"images\": [{ \"url\": \"https://via.placeholder.com/300\" }]"
    "        }"
    "      }"
    "    },"
    "    {"
    "      \"track\": {"
    "        \"name\": \"Sample Track 2\","
    "        \"artists\": [{ \"name\": \"Artist 2\" }],"
    "        \"album\": {"
    "          \"name\": \"Album 2\","
    "          \"images\": [{ \"url\": \"https://via.placeholder.com/300\" }]"
    "        }"
    "      }"
    "    }"
    "  ]"
    "}";
  
  cJSON* liked_songs = cJSON_Parse(sample_json);
  if (liked_songs == NULL) {
    fprintf(stderr, "Failed to parse sample JSON\n");
    notcurses_stop(nc);
    return 1;
  }
  
  // Display the songs
  display_songs_with_inline_art(liked_songs, nc);
  
  // Wait for user input
  struct ncplane* std = notcurses_stdplane(nc);
  ncinput in;
  while (notcurses_get_nblock(nc, &in)) {
    if (in.id == 'q') {
      break;
    }
  }
  
  // Cleanup
  cJSON_Delete(liked_songs);
  notcurses_stop(nc);
  
  return 0;
}

// NEW: Improved display function with larger images and better layout
void display_songs_with_inline_art(cJSON *liked_songs, struct notcurses *nc) {
  struct ncplane *n = notcurses_stdplane(nc);
  
  // Clear the screen
  notcurses_drop_planes(nc);
  
  // Get terminal dimensions
  int term_rows, term_cols;
  ncplane_dim_yx(n, &term_rows, &term_cols);
  
  // Print header
  int row = 0;
  ncplane_putstr_yx(n, row++, 0, "╔═══════════════════════════════════════════════════════════════════════════════════════════════════╗");
  ncplane_putstr_yx(n, row++, 0, "║                              SPOTIFY LIKED SONGS                                            ║");
  ncplane_putstr_yx(n, row++, 0, "╠════╦════════════════════════════════════════════════════════════════════╦═════════════════════════╣");
  ncplane_putstr_yx(n, row++, 0, "║ #  ║ Track / Artist                                        ║ Album Art                 ║");
  ncplane_putstr_yx(n, row++, 0, "╠════╬════════════════════════════════════════════════════════════════════╬═════════════════════════╣");
  
  cJSON *items = cJSON_GetObjectItem(liked_songs, "items");
  if (!items) {
    ncplane_putstr_yx(n, row++, 1, "No items found");
    notcurses_render(nc);
    return;
  }
  
  int display_count = (term_rows > 15) ? 10 : 5;
  
  // Display each song
  for (int i = 0; i < display_count && i < cJSON_GetArraySize(items); i++) {
    cJSON *track_item = cJSON_GetArrayItem(items, i);
    cJSON *track = cJSON_GetObjectItem(track_item, "track");
    
    if (!track) continue;
    
    // Get track info
    cJSON *name = cJSON_GetObjectItem(track, "name");
    cJSON *artists = cJSON_GetObjectItem(track, "artists");
    cJSON *album = cJSON_GetObjectItem(track, "album");
    cJSON *images = cJSON_GetObjectItem(album, "images");
    
    // Format track number and name
    char track_line[60] = {0};
    snprintf(track_line, sizeof(track_line), "║ %2d ║ %-55s ║", 
             i + 1, name ? name->valuestring : "Unknown");
    ncplane_putstr_yx(n, row, 0, track_line);
    
    // Artist name on next line
    char artist_line[60] = {0};
    if (artists && cJSON_GetArraySize(artists) > 0) {
      cJSON *first_artist = cJSON_GetArrayItem(artists, 0);
      cJSON *artist_name = cJSON_GetObjectItem(first_artist, "name");
      if (artist_name && artist_name->valuestring) {
        snprintf(artist_line, sizeof(artist_line), "║    ║ %-55s ║", artist_name->valuestring);
      } else {
        snprintf(artist_line, sizeof(artist_line), "║    ║ %-55s ║", "Unknown Artist");
      }
    } else {
      snprintf(artist_line, sizeof(artist_line), "║    ║ %-55s ║", "Unknown Artist");
    }
    ncplane_putstr_yx(n, row + 1, 0, artist_line);
    
    // Download and display album art
    if (images && cJSON_GetArraySize(images) > 0) {
      // Get the first (largest) image for better quality
      cJSON *img = cJSON_GetArrayItem(images, 0);
      cJSON *url = cJSON_GetObjectItem(img, "url");
      
      if (url && url->valuestring) {
        // Download to temp file
        char temp_file[] = "/tmp/spotify_art_XXXXXX.jpg";
        int fd = mkstemp(temp_file);
        if (fd >= 0) {
          close(fd);
          
          if (download_image(temp_file, url->valuestring) == 0) {
            // Load the image
            struct ncvisual *ncv = ncvisual_from_file(temp_file);
            if (ncv) {
              // Create a LARGER image plane for better quality
              struct ncplane_options nopts = {
                .y = row,
                .x = 78,  // Position in the album art column
                .rows = 8,   // Larger height
                .cols = 20,  // Larger width
                .userptr = NULL,
                .name = "imageplane",
                .resizecb = NULL,
                .flags = 0,
                .margin_b = 0,
                .margin_r = 0,
              };
              
              struct ncplane *image_plane = ncplane_create(n, &nopts);
              if (image_plane) {
                // Resize to match the plane size for better quality
                ncvisual_resize(ncv, 8, 20);
                struct ncvisual_options vopts = {
                  .n = image_plane,
                  .flags = 0,
                  .scaling = NCSCALE_STRETCH,  // Stretch to fill the plane
                  .blitter = NCBLIT_PIXEL,
                  .y = 0,
                  .x = 0,
                };
                ncvisual_blit(nc, ncv, &vopts);
              }
              ncvisual_destroy(ncv);
            }
            unlink(temp_file);
          }
        }
      }
    }
    
    // Draw separator line
    ncplane_putstr_yx(n, row + 2, 0, "╠════╬════════════════════════════════════════════════════════════════════╬═════════════════════════╣");
    
    row += 3;  // Move to next song (3 rows per song: name, artist, separator)
  }
  
  // Draw table footer
  ncplane_putstr_yx(n, row, 0, "╚════╩════════════════════════════════════════════════════════════════════╩═════════════════════════╝");
  ncplane_putstr_yx(n, row + 1, 0, "Press 'q' to quit. Album art is being downloaded and displayed...");
  
  notcurses_render(nc);
}