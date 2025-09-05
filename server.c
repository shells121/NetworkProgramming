#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#define SOCKET int
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define GETSOCKETERRNO() (errno)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
  int status;
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Find local address that our web server should bind to
  printf("%s", "Configuring local address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_addr;
  if ((status = getaddrinfo(0, "8080", &hints, &bind_addr)) != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
    return 1;
  }

  // Create socket
  printf("%s", "Creating socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_addr->ai_family,
          bind_addr->ai_socktype, bind_addr->ai_protocol);
  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 2;
  }

  // Bind socket to local address
  printf("%s", "Binding socket to local address..\n");
  if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen)) {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    return 3;
  }
  freeaddrinfo(bind_addr);

  // Listen for incoming connections
  printf("%s", "Listening...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 4;
  }

  // Waiting for a connection
  printf("%s", "Waiting for connection...\n");
  struct sockaddr_storage client_address;
  socklen_t client_len = sizeof(client_address);
  SOCKET socket_client = accept(socket_listen,
          (struct sockaddr*) &client_address, &client_len);
  if (!ISVALIDSOCKET(socket_client)) {
    fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
    return 5;
  }
  
  // Print client's address to console
  printf("%s", "Client connected: ");
  char address_buffer[100];
  getnameinfo((struct sockaddr*)&client_address,
          client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
  printf("%s\n", address_buffer);

  // Read request info
  printf("%s", "Reading request...\n");
  char request[1024]; // Max of 1024 bytes of info from the request
  int bytes_received = recv(socket_client, request, 1024, 0);
  if (bytes_received <= 0) {
    printf("Connection terminated...\n");
    return 0;
  }
  printf("Received %d bytes.\n", bytes_received);
  printf("%.*s", bytes_received, request); // Note, no garuntee it will be null terminated,
                                           // so you must give total bytes of stuff
  
  // Send response back
  printf("Sending response...\n");
  const char *response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n\r\n"
      "Local time is: ";
  int bytes_sent = send(socket_client, response, strlen(response), 0);
  printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(response));

  // Send current time
  time_t timer;
  time(&timer);
  char *time_msg = ctime(&timer);
  bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
  printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

  // Close connection
  printf("%s", "Closing connection...\n");
  CLOSESOCKET(socket_client);

  // Close listening socket
  printf("%s", "Closing listening socket...\n");
  CLOSESOCKET(socket_listen);

#if defined(_WIN32)
  WSACleanup();
#endif
  printf("%s", "Fininshed.\n");
  return 0;
}