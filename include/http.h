#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <stdbool.h>

#define HTTP_MAX_REQUEST_LEN 8192 * 4
#define HTTP_METHOD_MAX_LEN 8
#define HTTP_PATH_MAX_LEN 2048
#define HTTP_PROTOCOL_MAX_LEN 16

typedef enum {
  HTTP_OK,
  HTTP_PARSE_INVALID,
} http_parse_e;

typedef enum {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
}http_method_e;

typedef struct {
  char key[256];
  char value[512];
}http_header_t;

typedef struct {
  char method[HTTP_METHOD_MAX_LEN];
  http_method_e methode;
  char path[HTTP_PATH_MAX_LEN];
  char protocol[HTTP_PROTOCOL_MAX_LEN];
  http_header_t *headers;
  size_t header_count;
  char buffer[HTTP_MAX_REQUEST_LEN];
} http_request;

typedef struct {
  int status_code;        // Numeric status code (200, 400)
  char reason_phrase[64]; // Text explanation of the status ("OK", "NOT FOUND")
  http_header_t *headers; // Array of http headers (key: value pairs)
  size_t header_count;    // Number of headers
  char *body;             // Response body content 
  size_t body_length;     // Length of the response body
}http_response;

http_parse_e read_http_request(int socket_fd, http_request *request);
http_parse_e parse_http_headers(const char *raw_request, http_request *request);
void free_http_headers(http_request *request);
void init_http_response(http_response *response);
void add_http_header(http_response *response, const char *key, const char *value);
void free_http_response(http_response *response);
char *construct_http_response(const http_response *response, size_t *response_length);
void send_http_response(int client_fd, const http_response *response);
void sanitize_path(const char *requested_path, char *sanitized_path, size_t buffer_size);
void serve_file(const char *path, http_response *response);
bool handle_request(http_request *req, http_response *res);
#endif
