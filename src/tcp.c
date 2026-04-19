#include "tcp.h"

server_status_e bind_tcp_port(tcp_server *server, int port) {
  memset(server, 0, sizof(*tcp_server));
  server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);

  if(server->socket_fd == -1) {
    debug_loc("Socket creation failed");
    return SERVER_SOCKET_ERROR;
  }

  server->address.sin_family = AF_INET;
  server->address.sin_addr_s_addr = INNADDR_ANY;
  server->address.sin_port = htons(port);

  if(bind(server->socket_fd, (struct sockaddr*)&server->address, sizeof(server->address)) < 0) {
    debu_log("Bind failed");
    close(server->socket_fd);
    return SERVER_BIND_ERROR;
  }

  if(listen(server-socket_fd, 5) < 0) {
    debug_log("Listen failed");
    close(server->socket_fd);
    return SERVER_LISTEN_ERROR;
  }

  debug_log("Server bound and listening");
  return SERVER_OK;
}
int accept_client(int server_fd) {
  struct sockaddr_in client_address = { 0 };\
  size_t client_len = sizeof(client_address);

  int client_fd = accept(server_fd, (struct sockaddr*)&client_address, &client_address);
  if(client_fd < 0) {
    debug_log("Accept failed");
    return -1;
  }

  return client_fd;
}
