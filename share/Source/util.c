#include "icslab2_net.h"
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"

int ResolveAddress(char *host_str, char *port_str, struct addrinfo **res) {
  struct addrinfo hints;
  int err;

  /* ホスト名からIPアドレスを取得する */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  err = getaddrinfo(host_str, port_str, &hints, res);
  if (err != 0) {
    perror("getaddrinfo");
    return 1;
  }

  return 0;
}

void ShowAddress(struct addrinfo *addrinfo) {
  struct in_addr addr;
  addr.s_addr = ((struct sockaddr_in *)(addrinfo->ai_addr))->sin_addr.s_addr;
  printf("%s:%d", inet_ntoa(addr),
         ntohs(((struct sockaddr_in *)(addrinfo->ai_addr))->sin_port));
}

void ShowSockAddr(struct sockaddr_in *addr) {
  char buf[80];
  struct in_addr *addr_in_p = (struct in_addr *)&addr->sin_addr;
  inet_ntop(AF_INET, addr_in_p, buf, sizeof(buf));
  printf("%s:%d", buf, ntohs(addr->sin_port));
}

int PrepareSocket(struct addrinfo *addrinfo) {
  int sock;

  /* TCPソケットをオープン */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  /* 送信先に接続（bind相当も実行） */
  if (connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
    perror("connect");
    return -1;
  }

  return sock;
}

int PrepareSockWait(unsigned short port) {
  int sock0;   /* 待ち受け用ソケットディスクリプタ */
  int yes = 1; /* setsockopt()用 */
  struct sockaddr_in selfAddr;

  /* TCPソケットをオープンする */
  if ((sock0 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  /* sock0のコネクションがTIME_WAIT状態でもbind()できるように設定 */
  setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

  /* 送信元からの要求を受け付けるIPアドレスとポートを設定する */
  memset(&selfAddr, 0, sizeof(selfAddr));       /* ゼロクリア */
  selfAddr.sin_family = AF_INET;                /* Internetプロトコル */
  selfAddr.sin_port = htons(port);              /* 待ち受けるポート */
  selfAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* どのIPアドレス宛でも */

  /* ソケットとアドレスをbindする */
  if (bind(sock0, (struct sockaddr *)&selfAddr, sizeof(selfAddr)) < 0) {
    perror("bind");
    return -1;
  }

  /* コネクションの最大同時受け入れ数を指定する */
  if (listen(sock0, 5) != 0) {
    perror("listen");
    return -1;
  }

  return sock0;
}

