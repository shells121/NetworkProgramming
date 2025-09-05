#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#pragma comment(lib, "ws2_32.lib")
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main() {

#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif
  // Configure local address
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  
  struct addrinfo *bind_addr;
  int status;
  if ((status = getaddrinfo(0, "8080", &hints, &bind_addr)) != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
    return 1;
  }

  printf("%s", "Creating socket...\n");
  SOCKET sk_listen;
  sk_listen = socket(bind_addr->ai_family, bind_addr->ai_socktype, bind_addr->ai_protocol);
  if (!ISVALIDSOCKET(sk_listen)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  freeaddrinfo(bind_addr);

  printf("%s", "Listening...\n");
  if (listen(sk_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  fd_set master;
  FD_ZERO(&master);
  FD_SET(sk_listen, &master);
  SOCKET max_socket = sk_listen;

  printf("%s", "Waiting for connections...\n");

  while (1) {
    fd_set reads;
    reads = master;
    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
      fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      return 1;
    }

    SOCKET i;
    for (i = 1; i <= max_socket; i++) {
      if (FD_ISSET(i, &reads)) {
        // Determine which socket has a connection
        if (i == sk_listen) {
          struct sockaddr_storage client_address;
          socklen_t client_len = sizeof(client_address);
          SOCKET socket_client = accept(sk_listen, (struct sockaddr*) &client_address,
                    &client_len);
          if (!ISVALIDSOCKET(socket_client)) {
            fprintf(stderr, "accetp() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
          }

          FD_SET(socket_client, &master);
          if (socket_client > max_socket) max_socket = socket_client;

          char address_buffer[100];
          getnameinfo((struct sockaddr*) &client_address, client_len,
                    address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
          printf("New connection from %s\n", address_buffer);
        } else {
          char read[1024];
          int bytes_received = recv(i, read, 1024, 0);
          if (bytes_received < 1) {
            FD_CLR(i, &master);
            CLOSESOCKET(i);
            continue;
          }

          SOCKET j;
          for (j = 1; j <= max_socket; j++) {
            if (FD_ISSET(j, &master)) {
              if (j == sk_listen || j == i) {
                continue;
              } else {
                send(j, read, bytes_received, 0);
              }
            }
          }
        }
      }
    }
  }
  printf("Closing listening socket...\n");
  CLOSESOCKET(sk_listen);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("%s", "Finished.\n");
  return 0;
}