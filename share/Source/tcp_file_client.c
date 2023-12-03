/*  -*- coding: utf-8-unix; -*-                                     */
/*  FILENAME     :  tcp_file_client.c                               */
/*  DESCRIPTION  :  TCP file Client                                 */
/*                                                                  */
/*  VERSION      :                                                  */
/*  DATE         :  Sep 01, 2020                                    */
/*  UPDATE       :                                                  */
/*                                                                  */

#include "icslab2_net.h"
#include <netdb.h>
#include <stdio.h>

int main(int argc, char **argv) {
  char *server_host_str = "127.0.0.1"; /* サーバ名（文字列） */
  char *port_num_str = "10000";        /* ポート番号（文字列） */
  char *filename = NULL;
  int fd = 1;                     /* 標準出力 */
  char *dummy_file = "HELLO.txt"; /* ダミーのリクエストメッセージ */

  int sock;          /* ソケットディスクリプタ */
  char buf[BUF_LEN]; /* 受信バッファ */
  int n;             /* 読み込み／受信バイト数 */

  struct addrinfo hints, *res; /* アドレス情報の構造体 */
  struct in_addr addr;         /* アドレス表示用 */
  int err;

  /* コマンドライン引数の処理 */
  if (argc == 2 && strncmp(argv[1], "-h", 2) == 0) {
    printf("Usage: %s [dst_host] [port] [o_filename]\n", argv[0]);
    return 0;
  }
  if (argc > 1) /* 宛先を指定のIPアドレスにする。 portはデフォルト */
    server_host_str = argv[1];
  if (argc > 2) /* 宛先を指定のIPアドレス、portにする */
    port_num_str = argv[2];
  if (argc > 3) { /* 出力ファイルの指定 */
    printf("set outputfile: %s", argv[3]);
    filename = argv[3];
    /* STEP 0: 出力ファイルのオープン */
    fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
      perror("open");
      return 1;
    }
  }

  /* ホスト名からIPアドレスを取得する */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  err = getaddrinfo(server_host_str, port_num_str, &hints, &res);
  if (err != 0) {
    perror("getaddrinfo");
    return 1;
  }

  /* IPアドレスを標準出力に表示 */
  addr.s_addr = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
  printf("ip address: %s\n", inet_ntoa(addr));
  printf("port#: %d\n",
         ntohs(((struct sockaddr_in *)(res->ai_addr))->sin_port));

  /* STEP 2: TCPソケットをオープン */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return 1;
  }

  /* STEP 3: サーバに接続（bind相当も実行） */
  if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
    perror("connect");
    return 1;
  }

  /* STEP 4: ダミーのファイル要求メッセージの作成と送信  \r\nをデリミタにする */
  sprintf(buf, "GET %s\r\n", dummy_file);
  write(sock, buf, strlen(buf)); /* 要求の送信 */

  /* STEP 5: 受信するたびにファイルに出力 */
  while ((n = read(sock, buf, BUF_LEN)) > 0) {
    write(fd, buf, n); /* ファイルに出力 */
  }

  /* STEP 6: 出力ファイルのクローズ */
  close(fd);

  /* STEP 7: ソケットのクローズ */
  close(sock);

  return 0;
}
/*--------------------------- <end> --------------------------------*/
