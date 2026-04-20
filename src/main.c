#include <stdio.h>
#include <stdlib.h>

#include "tcp.h"
#include "http.h"
#include "route.h"
#include <cjson/cJSON.h>

typedef struct {
  short port;
}server_config;

char* loadfile(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}

int load_config(server_config *config) {
  int status = 0;

  char *config_data = loadfile("config.json");
  if (!config_data) {
    printf("Failed to load config, what happend?");
  }
  
  // printf("config_data->%s\n", config_data);
  
  cJSON *config_json = cJSON_Parse(config_data);
  
  if (config_json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    
    if (error_ptr != NULL)
      fprintf(stderr, "Error before: %s\n", error_ptr);

    status = 0;
    goto end;
  }

  cJSON *port = cJSON_GetObjectItemCaseSensitive(config_json, "portnumber");
  if (!cJSON_IsNumber(port)) {
    status = 0;
    goto end;
  }

  if (port->valueint > 65535 || port->valueint < 0) {
    printf("Invalid port number specified in config.");
    status = 0;
    goto end;
  }

  config->port = (short)port->valueint;
  
  end:
    cJSON_Delete(config_json);
    return status;
}

void hello_handler(http_request *req, http_response *res) {
  res->status_code = 200;
  
  if (!res->body)
    res->body = malloc(64);

  strcpy(res->body, "Hello World!\n");
  res->body_length = 13;

  //  add_http_header(res, "Content-Type", "text/plain");
  add_http_header(res, "Content-Length", "13");
}

int main() {
  tcp_server server = { 0 };

  server_config config = {
    .port = 8080,
  };

  if (load_config(&config) != 0)
    printf("Failed to load config, using default values.\n");

  printf("port->%d\n", config.port);
  
  server_status_e status = bind_tcp_port(&server, config.port);

  if (status != SERVER_OK) {
    printf("Server initialization failed\n");
    exit(EXIT_FAILURE);
  }

  while(1) {
    int client_fd = accept_client(server.socket_fd);

    if (client_fd == -1) {
      printf("Failed to accept client connection\n");
      close(server.socket_fd);
      exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    http_request req = { 0 };
    http_response res = { 0 };
    
    init_http_response(&res);
    install_route(HTTP_METHOD_GET, "/hello", hello_handler);
  
    if (read_http_request(client_fd, &req) != HTTP_OK) {
      printf("Failed to read or parse HTTP request\n");
      close(server.socket_fd);
      exit(EXIT_FAILURE);
    }

    printf("Method->%s\n", req.method);

    if (parse_http_headers(req.buffer, &req) != HTTP_OK) {
      printf("Failed to read or parse HTTP request");
      close(client_fd);
      exit(EXIT_FAILURE);
    }

    char sanitized_path[1024] = { 0 };
    sanitize_path(req.path, sanitized_path, sizeof(sanitized_path));

    if (!handle_request(&req, &res))
      serve_file(sanitized_path, &res);

    send_http_response(client_fd, &res);
  
    free_http_headers(&req);
    close(client_fd);
    
  }
  
  close(server.socket_fd);
  return 0;
}
