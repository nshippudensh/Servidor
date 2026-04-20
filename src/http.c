#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdbool.h>

#include "http.h"
#include "route.h"

extern Route routes[];
extern int route_count;

http_parse_e read_http_request(int socket_fd, http_request *request) {
  if (socket_fd < 0 || request == NULL)
    return HTTP_PARSE_INVALID;

  ssize_t bytes_read = read(socket_fd, request->buffer, sizeof(request->buffer) - 1);

  if (bytes_read < 0)
    return HTTP_PARSE_INVALID;

  request->buffer[bytes_read] = '\0';

  if (sscanf(request->buffer, "%7s %2047s %15s", request->method, request->path, request->protocol) != 3)
    return HTTP_PARSE_INVALID;

  if (strcmp(request->method, "GET") == 0)
    request->methode = HTTP_METHOD_GET;
  else if (strcmp(request->method, "POST") == 0)
    request->methode = HTTP_METHOD_POST;

  return HTTP_OK;
}

http_parse_e parse_http_headers(const char *raw_request, http_request *request) {
  if (raw_request == NULL || request == NULL)
    return HTTP_PARSE_INVALID;

  request->header_count = 0;

  char *line_start = strstr(raw_request, "\r\n");
  if (line_start == NULL)
    return HTTP_PARSE_INVALID;

  // Se aumenta la direccion de la string para que no apunte a "\r\n"
  line_start += 2;

  /* El bucle se ejecuta mientras line_start tenga contenido, no sea NULL
     y no sea el caracter \r o \n */
  while (line_start && *line_start && *line_start != '\r' && *line_start != '\n') {
    char *line_end = strstr(line_start, "\r\n");
    if(line_end == NULL)
      return HTTP_PARSE_INVALID;

    // Longitud de la string
    size_t line_length = line_end - line_start;

    // Copiar la string en un nuevo buffer
    char line[1024] = { 0 };
    strncpy(line, line_start, line_length);

    // Encontrar la string a partir del ':'
    char *colon_position = strchr(line, ':');
    if (colon_position) {
      *colon_position = '\0';
      const char *key = line;
      const char *value = colon_position + 1;

      // Quitar los espacios
      while(*value == ' ')
        value++;

      request->headers = realloc(request->headers, sizeof(http_header_t) * (request->header_count + 1));
      if(request->headers == NULL) {
        perror("Failed to allocate memory for headers");
        exit(EXIT_FAILURE);
      }
      strncpy(request->headers[request->header_count].key, key, sizeof(request->headers[request->header_count].key));
      strncpy(request->headers[request->header_count].value, value, sizeof(request->headers[request->header_count].value));

      request->header_count++;
     }

    line_start = line_end + 2;
  }
  return HTTP_OK;
}


void free_http_headers(http_request *request) {
  for(size_t i  = 0; i < request->header_count; i++) {
    memset(request->headers[i].value, 0, sizeof(request->headers[i].value));
    memset(request->headers[i].value, 0, sizeof(request->headers[i].value));
  }
  request->header_count = 0;
  free(request->headers);
  request->headers = NULL;
}

void init_http_response(http_response *response) {
  response->status_code = 200;        
  strncpy(response->reason_phrase, "OK", sizeof(response->reason_phrase) - 1); 
  response->headers = NULL; 
  response->header_count = 0;    
  response->body = NULL;             
  response->body_length = 0;
}
void add_http_header(http_response *response, const char *key, const char *value) {
  if (response == NULL || key == NULL || value == NULL)
    return;
  
  response->headers = realloc(response->headers, sizeof(http_header_t) * (response->header_count + 1));
  if (response->headers == NULL) {
    perror("Failed t o allocate memory for headers");
    exit(EXIT_FAILURE);
  }

  strncpy(response->headers[response->header_count].key, key, sizeof(response->headers[response->header_count].key) - 1);
  strncpy(response->headers[response->header_count].value, value, sizeof(response->headers[response->header_count].value) - 1);

  response->header_count++;
}

void free_http_response(http_response *response) {
  free(response->headers);
  response->headers = NULL;
  response->header_count = 0;
}

char *construct_http_response(const http_response *response, size_t *response_length) {
  if (response == NULL || response_length == NULL)
    exit(EXIT_FAILURE);
  
  size_t buffer_size = 1024;
  char *buffer = malloc(buffer_size);

  if (buffer == NULL) {
    perror("Failed to allocate memory for response buffer");
    exit(EXIT_FAILURE);
  }

  size_t offset = snprintf(buffer, buffer_size, "HTTP/1.1 %d %s\r\n", response->status_code, response->reason_phrase);

  for (size_t i = 0; i < response->header_count; i++) {
    size_t header_lenght = snprintf(NULL, 0, "%s: %s", response->headers[i].key, response->headers[i].value);

    while (offset + header_lenght + 1 > buffer_size) {
      buffer_size *= 2;
      buffer = realloc(buffer, buffer_size);

      if (buffer == NULL) {
        perror("Failed to reallocate memory for response buffer");
        exit(EXIT_FAILURE);
      }
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "%s: %s\r\n", response->headers[i].key, response->headers[i].value);
    
  }

  offset += snprintf(buffer + offset, buffer_size - offset, "\r\n");

  if (response->body) {
    while (offset + response->body_length + 1 > buffer_size) {
      buffer_size *= 2;
      buffer = realloc(buffer, buffer_size);

      if (buffer == NULL) {
        perror("Failed to reallocate memory for response buffer");
        exit(EXIT_FAILURE);
      }
    }

    memcpy(buffer + offset, response->body, response->body_length);
    offset += response->body_length;
  }
  *response_length = offset;
  return buffer;
}

void send_http_response(int client_fd, const http_response *response) {
  if (client_fd < 0 || response == NULL)
    return;

  size_t response_length = 0;
  char *response_data = construct_http_response(response, &response_length);

  size_t total_sent = 0;
  while(total_sent < response_length) {
    ssize_t bytes_sent = send(client_fd, response_data + total_sent, response_length - total_sent, 0);
    if(bytes_sent < 0) {
      perror("Failed to send response");
      break;
    }
    total_sent += bytes_sent;
  }
  free(response_data);
}

void sanitize_path(const char *requested_path, char *sanitized_path, size_t buffer_size) {
  const char *web_root = "./www";
  snprintf(sanitized_path, buffer_size, "%s%s", web_root, requested_path);

  // Prevent directory traversal by normalizing the path
  if (strstr(sanitized_path, ".."))
    strncpy(sanitized_path, "./www/404.html", buffer_size - 1); // Server a 404 page
}

void serve_file(const char *path, http_response *response) {
  FILE *file = fopen(path, "rb+");
  if(!file) {
    response->status_code = 404;
    strncpy(response->reason_phrase, "Not found", sizeof(response->reason_phrase) - 1);
    file = fopen("./www/404.html", "rb");
  }

  // Determine the file size
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Allocate memory for the file content
  char *file_content = malloc(file_size + 1);
  if(!file_content) {
    perror("Failed to allocate memory for file content");
    fclose(file);
    exit(EXIT_FAILURE);
  }

  fread(file_content, 1, file_size, file);
  fclose(file);
  file_content[file_size] = '\0';

  // Set the response body
  response->body = file_content;
  response->body_length = file_size;

  // Determine contennt type (basic implementation)
  if (strstr(path, ".html")) {
    add_http_header(response, "Content-Type", "text/html");
  } else if (strstr(path, ".css")) {
    add_http_header(response, "Content-Type", "text/css");
  } else if (strstr(path, ".js")) {
    add_http_header(response, "Content-Type", "application/javascript");
  } else if (strstr(path, ".png")) {
    add_http_header(response, "Content-Type", "image/png");
  } else {
    add_http_header(response, "Content-Type", "application/octet-stream");
  }

  // Add content length header
  char content_length[32];
  snprintf(content_length, sizeof(content_length), "%zu", file_size);
  add_http_header(response, "Content-Length", content_length);

  //free(file_content);
}

bool handle_request(http_request *req, http_response *res) {
  printf("req->methode = %d\n", req->methode);
  for (int i = 0; i < route_count; i++) {
    if (strcmp(routes[i].path, req->path) == 0 && routes[i].method == req->methode) {
      routes[i].handler(req, res);
      return true;
    }
  }
  return false;
}
