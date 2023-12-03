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

/**
 * @brief コマンドライン引数をパースする
 *
 * @param[in]       argc コマンドライン引数の数
 * @param[in]       argv コマンドライン引数の内容
 * @param[in, out]  server_host_str サーバーのホスト名のデフォルト値・パース結果
 * @param[in, out]  port_num_str サーバーのポート番号のデフォルト値・パース結果
 * @param[in, out]  filename 保存するファイル名のデフォルト値・パース結果
 * @return 正常にパースできたなら0、ヘルプメッセージを表示したなら1
 */
int ParseArgs(int argc, char **argv, char **server_host_str,
              char **port_num_str, char **filename) {
  if (argc == 2 && strncmp(argv[1], "-h", 2) == 0) {
    printf("Usage: %s [dst_host] [port] [o_filename]\n", argv[0]);
    return 1;
  }
  if (argc > 1)
    *server_host_str = argv[1];
  if (argc > 2)
    *port_num_str = argv[2];
  if (argc > 3) { /* 出力ファイルの指定 */
    printf("set outputfile: %s", argv[3]);
    *filename = argv[3];
  }

  return 0;
}

/**
 * @brief サーバー名とポート名を解決する
 *
 * @param[in]  server_host_str サーバーのホスト名
 * @param[in]  port_num_str ポート番号
 * @param[out] res 解決結果
 * @return 正常に解決できたなら0
 */
int ResolveAddress(char *server_host_str, char *port_num_str,
                   struct addrinfo **res) {
  struct addrinfo hints;
  int err;

  /* ホスト名からIPアドレスを取得する */
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  err = getaddrinfo(server_host_str, port_num_str, &hints, res);
  if (err != 0) {
    perror("getaddrinfo");
    return 1;
  }

  return 0;
}

/**
 * @brief struct addrinfoを標準出力に表示する
 *
 * @param[in] addrinfo
 */
void ShowAddress(struct addrinfo *addrinfo) {
  struct in_addr addr;
  addr.s_addr = ((struct sockaddr_in *)(addrinfo->ai_addr))->sin_addr.s_addr;
  printf("ip address: %s\n", inet_ntoa(addr));
  printf("port#: %d\n",
         ntohs(((struct sockaddr_in *)(addrinfo->ai_addr))->sin_port));
}

int main(int argc, char **argv) {
  char *server_host_str = "127.0.0.1"; /* サーバ名（文字列） */
  char *port_num_str = "10000";        /* ポート番号（文字列） */
  char *filename = NULL; /* 出力ファイル名。NULLなら標準出力 */
  int fd = 1;            /* 受信した内容の出力先 */
  char *dummy_file = "HELLO.txt"; /* ダミーのリクエストメッセージ */

  int sock;          /* ソケットディスクリプタ */
  char buf[BUF_LEN]; /* 受信バッファ */
  int n;             /* 読み込み／受信バイト数 */

  struct addrinfo *res; /* アドレス情報の構造体 */

  /* コマンドライン引数の処理 */
  if (ParseArgs(argc, argv, &server_host_str, &port_num_str, &filename) != 0) {
    return 1;
  }

  if (filename != NULL) {
    fd = open(filename, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
      perror("open");
      return 1;
    }
  }

  if (ResolveAddress(server_host_str, port_num_str, &res) != 0) {
    fprintf(stderr, "ResolveAddress failed\n");
    return 1;
  }
  ShowAddress(res);

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

  /* STEP 4: ダミーのファイル要求メッセージの作成と送信  \r\nをデリミタにする
   */
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
