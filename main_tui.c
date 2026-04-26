#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <cJSON.h>
#include <notcurses/notcurses.h>
#include "main_functions.h"

#define MAX_SONGS  100
#define IMAGE_ROWS 14
#define IMAGE_COLS 28
#define NUM_W       4
#define YEAR_W      6

typedef struct {
    char name[128];
    char artist[128];
    char year[5];
    char image_url[512];
} Song;

int download_image(const char *filename, const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return 1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) { curl_easy_cleanup(curl); return 1; }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
    return res == CURLE_OK ? 0 : 1;
}

static void slice(const char *str, char *result, size_t start, size_t end) {
    strncpy(result, str + start, end - start);
    result[end - start] = '\0';
}

static void append_repeat(char *buf, size_t bufsz, const char *ch, int n) {
    size_t ch_len = strlen(ch);
    size_t cur = strlen(buf);
    for (int i = 0; i < n; i++) {
        if (cur + ch_len >= bufsz - 1) break;
        memcpy(buf + cur, ch, ch_len);
        cur += ch_len;
    }
    buf[cur] = '\0';
}

/* Build a box-drawing horizontal rule into buf. */
static void build_hline(char *buf, size_t bufsz, const char *L, const char *fill,
                        const char *M, const char *R, int text_w) {
    buf[0] = '\0';
#define CAT(s) strncat(buf, (s), bufsz - strlen(buf) - 1)
    CAT(L);
    append_repeat(buf, bufsz, fill, NUM_W);
    CAT(M);
    append_repeat(buf, bufsz, fill, YEAR_W);
    CAT(M);
    append_repeat(buf, bufsz, fill, text_w);
    CAT(M);
    append_repeat(buf, bufsz, fill, IMAGE_COLS);
    CAT(R);
#undef CAT
}

static int collect_songs(cJSON *batch, Song *songs, int count) {
    cJSON *items = cJSON_GetObjectItemCaseSensitive(batch, "items");
    if (!items) return count;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    cJSON *item;
    cJSON_ArrayForEach(item, items) {
        if (count >= MAX_SONGS || !item) continue;
        cJSON *added_at   = cJSON_GetObjectItemCaseSensitive(item, "added_at");
        cJSON *track      = cJSON_GetObjectItemCaseSensitive(item, "track");
        if (!track || !added_at || !added_at->valuestring) continue;
        cJSON *artists    = cJSON_GetObjectItemCaseSensitive(track, "artists");
        cJSON *track_name = cJSON_GetObjectItemCaseSensitive(track, "name");
        if (!artists || !track_name || !track_name->valuestring) continue;
        char month_s[3] = "", day_s[3] = "";
        slice(added_at->valuestring, month_s, 5, 7);
        slice(added_at->valuestring, day_s,   8, 10);
        if ((tm.tm_mon + 1) != atoi(month_s) || tm.tm_mday != atoi(day_s)) continue;
        Song *s = &songs[count++];
        strncpy(s->name, track_name->valuestring, 127);
        s->name[127] = '\0';
        cJSON *first_artist = artists->child;
        cJSON *aname = first_artist
            ? cJSON_GetObjectItemCaseSensitive(first_artist, "name") : NULL;
        strncpy(s->artist, (aname && aname->valuestring) ? aname->valuestring : "Unknown Artist", 127);
        s->artist[127] = '\0';
        strncpy(s->year, added_at->valuestring, 4);
        s->year[4] = '\0';
        s->image_url[0] = '\0';
        cJSON *album  = cJSON_GetObjectItemCaseSensitive(track, "album");
        cJSON *images = album ? cJSON_GetObjectItemCaseSensitive(album, "images") : NULL;
        if (images && cJSON_GetArraySize(images) > 0) {
            cJSON *img      = cJSON_GetArrayItem(images, 0);
            cJSON *url_item = cJSON_GetObjectItemCaseSensitive(img, "url");
            if (url_item && url_item->valuestring) {
                strncpy(s->image_url, url_item->valuestring, 511);
                s->image_url[511] = '\0';
            }
        }
    }
    return count;
}

static const char *day_ordinal(int day) {
    if (day >= 11 && day <= 13) return "th";
    switch (day % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

static void display_songs(Song *songs, int count, struct notcurses *nc) {
    struct ncplane *n = notcurses_stdplane(nc);
    ncplane_erase(n);

    unsigned int rows_u, cols_u;
    ncplane_dim_yx(n, &rows_u, &cols_u);
    int term_cols = (int)cols_u;

    /* text_w = inner width of track/artist column */
    int text_w = term_cols - NUM_W - YEAR_W - IMAGE_COLS - 5;
    if (text_w < 10) text_w = 10;
    /* x offset where image planes start */
    int art_x = 1 + NUM_W + 1 + YEAR_W + 1 + text_w + 1;

    char hline[2048];

    /* Title */
    time_t t = time(NULL);
    struct tm tm_now = *localtime(&t);
    char month_name[16];
    strftime(month_name, sizeof(month_name), "%B", &tm_now);
    char title[64];
    snprintf(title, sizeof(title), "LIKED SONGS - %d%s %s",
             tm_now.tm_mday, day_ordinal(tm_now.tm_mday), month_name);
    int inner  = term_cols - 2;
    int ct_len = (int)strlen(title);
    int lpad   = (inner - ct_len) / 2;
    int rpad   = inner - ct_len - lpad;
    if (lpad < 0) lpad = 0;
    if (rpad < 0) rpad = 0;

    int row = 0;
    build_hline(hline, sizeof(hline), "╔", "═", "╦", "╗", text_w);
    ncplane_putstr_yx(n, row++, 0, hline);
    ncplane_printf_yx(n, row++, 0, "║%*s%s%*s║", lpad, "", title, rpad, "");
    build_hline(hline, sizeof(hline), "╠", "═", "╬", "╣", text_w);
    ncplane_putstr_yx(n, row++, 0, hline);
    ncplane_printf_yx(n, row++, 0, "║ %-*s║ %-*s║ %-*s║ %-*s║",
                      NUM_W - 1,    "#",
                      YEAR_W - 1,   "Year",
                      text_w - 1,   "Track / Artist",
                      IMAGE_COLS - 1, "Art");
    build_hline(hline, sizeof(hline), "╠", "═", "╬", "╣", text_w);
    ncplane_putstr_yx(n, row++, 0, hline);

    for (int i = 0; i < count; i++) {
        int song_start = row;

        for (int r = 0; r < IMAGE_ROWS; r++) {
            if (r == 0) {
                ncplane_printf_yx(n, row, 0, "║ %2d ║ %-4s ║ %-*.*s ║%*s║",
                                  i + 1, songs[i].year,
                                  text_w - 2, text_w - 2, songs[i].name,
                                  IMAGE_COLS, "");
            } else if (r == 1) {
                ncplane_printf_yx(n, row, 0, "║    ║      ║ %-*.*s ║%*s║",
                                  text_w - 2, text_w - 2, songs[i].artist,
                                  IMAGE_COLS, "");
            } else {
                ncplane_printf_yx(n, row, 0, "║    ║      ║%*s║%*s║",
                                  text_w, "", IMAGE_COLS, "");
            }
            row++;
        }

        build_hline(hline, sizeof(hline), "╠", "═", "╬", "╣", text_w);
        ncplane_putstr_yx(n, row++, 0, hline);

        /* Download and blit album art */
        if (songs[i].image_url[0] != '\0') {
            char tmp[] = "/tmp/spotify_art_XXXXXX";
            int fd = mkstemp(tmp);
            if (fd >= 0) {
                close(fd);
                if (download_image(tmp, songs[i].image_url) == 0) {
                    struct ncvisual *ncv = ncvisual_from_file(tmp);
                    if (ncv) {
                        struct ncplane_options nopts = {
                            .y    = song_start,
                            .x    = art_x,
                            .rows = IMAGE_ROWS,
                            .cols = IMAGE_COLS,
                        };
                        struct ncplane *img_plane = ncplane_create(n, &nopts);
                        if (img_plane) {
                            struct ncvisual_options vopts = {
                                .n       = img_plane,
                                .scaling = NCSCALE_SCALE,
                                .blitter = NCBLIT_PIXEL,
                            };
                            ncvisual_blit(nc, ncv, &vopts);
                        }
                        ncvisual_destroy(ncv);
                    }
                }
                unlink(tmp);
            }
        }
    }

    /* Replace last ╠═╬═╣ with ╚═╩═╝, or draw fresh bottom border */
    if (count > 0) {
        row--;
        build_hline(hline, sizeof(hline), "╚", "═", "╩", "╝", text_w);
        ncplane_putstr_yx(n, row++, 0, hline);
    } else {
        build_hline(hline, sizeof(hline), "╚", "═", "╩", "╝", text_w);
        ncplane_putstr_yx(n, row++, 0, hline);
        ncplane_putstr_yx(n, row++, 1, "No songs liked on this day in previous years.");
    }

    ncplane_putstr_yx(n, row, 1, "Press 'q' to quit");
    notcurses_render(nc);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client_id> <client_secret>\n", argv[0]);
        return 1;
    }
    char *client_id     = argv[1];
    char *client_secret = argv[2];

    struct notcurses_options opts = { .flags = 0 };
    struct notcurses *nc = notcurses_init(&opts, NULL);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return 1;
    }

    struct ncplane *std = notcurses_stdplane(nc);
    ncplane_putstr_yx(std, 0, 0, "Authenticating with Spotify...");
    notcurses_render(nc);

    /* OAuth: open browser, wait for redirect */
    char token_opts[256] = {0};
    snprintf(token_opts, sizeof(token_opts),
             "client_id=%s&response_type=code&scope=user-library-read"
             "&redirect_uri=http://127.0.0.1:8080", client_id);
    perform_initial_auth_request(token_opts);

    char buffer[1024] = {0};
    open_socket_for_auth_code(buffer);

    char auth_code[300] = {0};
    parse_auth_code(buffer, auth_code);

    /* OAuth: exchange code for access token */
    char b64_input[256] = {0};
    snprintf(b64_input, sizeof(b64_input), "%s:%s", client_id, client_secret);
    char *b64 = base64Encoder(b64_input, strlen(b64_input));

    char auth_hdr[600] = {0};
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Basic %s", b64);
    free(b64);

    struct curl_slist *hs = NULL;
    hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
    hs = curl_slist_append(hs, auth_hdr);

    char exchange_opts[512] = {0};
    snprintf(exchange_opts, sizeof(exchange_opts),
             "grant_type=authorization_code&code=%s"
             "&redirect_uri=http://127.0.0.1:8080", auth_code);

    struct MemoryStruct chunk = {0};
    char errbuf[CURL_ERROR_SIZE] = {0};
    CURLcode res = perform_curl_request(
        "https://accounts.spotify.com/api/token",
        exchange_opts, hs, &chunk, errbuf, 1L);
    curl_slist_free_all(hs);

    cJSON *token_json = parse_json_response(res, &chunk, errbuf);
    free(chunk.data);
    if (!token_json) {
        notcurses_stop(nc);
        fprintf(stderr, "Auth failed\n");
        return 1;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(token_json, "access_token");
    if (!token_item || !token_item->valuestring) {
        cJSON_Delete(token_json);
        notcurses_stop(nc);
        fprintf(stderr, "No access_token in response\n");
        return 1;
    }

    char bearer_hdr[600] = {0};
    snprintf(bearer_hdr, sizeof(bearer_hdr), "Authorization: Bearer %s",
             token_item->valuestring);
    cJSON_Delete(token_json);

    /* Fetch liked songs with pagination */
    ncplane_erase(std);
    ncplane_putstr_yx(std, 0, 0, "Fetching your liked songs...");
    notcurses_render(nc);

    struct curl_slist *user_hs = curl_slist_append(NULL, bearer_hdr);

    Song songs[MAX_SONGS];
    int song_count = 0;

    memset(&chunk, 0, sizeof(chunk));
    res = perform_curl_request(
        "https://api.spotify.com/v1/me/tracks?offset=0&limit=50&market=IE",
        NULL, user_hs, &chunk, errbuf, 0L);
    cJSON *first_batch = parse_json_response(res, &chunk, errbuf);
    free(chunk.data);
    if (!first_batch) {
        curl_slist_free_all(user_hs);
        notcurses_stop(nc);
        fprintf(stderr, "Failed to fetch liked songs\n");
        return 1;
    }

    cJSON *total_item = cJSON_GetObjectItemCaseSensitive(first_batch, "total");
    int total = total_item ? total_item->valueint : 0;
    song_count = collect_songs(first_batch, songs, song_count);
    cJSON_Delete(first_batch);

    for (int offset = 50; offset < total && song_count < MAX_SONGS; offset += 50) {
        char url[256] = {0};
        snprintf(url, sizeof(url),
                 "https://api.spotify.com/v1/me/tracks?offset=%d&limit=50&market=IE", offset);
        memset(&chunk, 0, sizeof(chunk));
        res = perform_curl_request(url, NULL, user_hs, &chunk, errbuf, 0L);
        cJSON *batch = parse_json_response(res, &chunk, errbuf);
        free(chunk.data);
        if (batch) {
            song_count = collect_songs(batch, songs, song_count);
            cJSON_Delete(batch);
        }
    }
    curl_slist_free_all(user_hs);

    display_songs(songs, song_count, nc);

    /* Block until 'q' — fixing the non-blocking loop that caused nothing to show */
    ncinput in;
    uint32_t ch;
    while ((ch = notcurses_get_blocking(nc, &in)) != (uint32_t)-1) {
        if (ch == 'q' || ch == 'Q') break;
    }

    notcurses_stop(nc);
    return 0;
}
