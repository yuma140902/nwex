/**
 * @file file_sender.c
 * @brief ファイルを送信するプログラム
 */

#include "icslab2_net.h"
#include <netdb.h>
#include <stdio.h>

/**
 * @brief コマンドライン引数をパースする
 *
 * @param[in]       argc コマンドライン引数の数
 * @param[in]       argv コマンドライン引数の内容
 * @param[in, out]  server_host_str 送信先のホスト名のデフォルト値・パース結果
 * @param[in, out]  port_num_str 送信先のポート番号のデフォルト値・パース結果
 * @param[out]      filename 送信するファイル名のパース結果
 * @return
 * 正常にパースできたなら0、ヘルプメッセージを表示したなら1、引数が足りなかったなら2
 */
int ParseArgs(int argc, char **argv, char **server_host_str,
              char **port_num_str, char **filename) {
  if (argc == 1 || (argc == 2 && strncmp(argv[1], "-h", 2) == 0)) {
    printf("Usage: %s filename [dst_host] [port]\n", argv[0]);
    return argc == 1 ? 2 : 1;
  }
  if (argc > 1)
    *filename = argv[1];
  if (argc > 2)
    *server_host_str = argv[2];
  if (argc > 3)
    *port_num_str = argv[3];

  return 0;
}

/**
 * @brief サーバー名とポート名を解決する
 *
 * @param[in]  host_str ホスト名
 * @param[in]  port_str ポート番号
 * @param[out] res 解決結果
 * @return 正常に解決できたなら0
 */
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

/**
 * @brief ソケットをオープンし、送信先に接続する
 *
 * @param addrinfo 送信先のアドレス
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
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

/**
 * @brief n以上の値でmの倍数である最小の値を返す
 */
long NextMultipleOf(long n, long m) {
  long r = n % m;
  if (r == 0) {
    return n;
  } else {
    return n + (m - r);
  }
}

/**
 * @brief ファイルを分割する
 *
 * @param[in]  filename   ファイル名
 * @param[in]  block_size 各分割の大きさははblock_sizeの倍数バイト数になる
 * @param[in]  num        分割数
 * @param[in]  ratios     分割の比
 * @param[out] positions  分割の各開始地点
 * @param[out] lengths    各分割の長さ
 * @return 正常に分割できたなら0
 */
int DivideFile(char *filename, int block_size, int num, int *ratios,
               long *positions, long *lengths) {
  FILE *fp;
  long file_size;
  int sum;
  int i;
  long current_pos;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    perror("fseek to end");
    return -1;
  }
  file_size = ftell(fp);
  if (fseek(fp, 0, SEEK_SET) != 0) {
    perror("fseek to head");
    return -1;
  }

  sum = 0;
  for (i = 0; i < num; ++i) {
    sum += ratios[i];
  }

  current_pos = 0;
  for (i = 0; i < num; ++i) {
    lengths[i] = (long)((double)file_size * (double)ratios[i] / (double)sum);
    lengths[i] = NextMultipleOf(lengths[i], block_size);
    positions[i] = current_pos;
    current_pos += lengths[i];
  }

  if (fclose(fp) != 0) {
    perror("fclose");
    return -1;
  }

  return 0;
}

int main(int argc, char **argv) {
  char *dst_host_str = "127.0.0.1"; /* 送信先のホスト名（文字列） */
  char *port_str = "10000";         /* ポート番号（文字列） */
  char *filename = "test.dat"; /* 送信するファイル名。NULLなら標準出力 */
  FILE *fp; /* 送信するファイルのファイルポインタ */

  int sock;          /* ソケットディスクリプタ */
  char buf[BUF_LEN]; /* 受信バッファ */
  int n;             /* 読み込み／受信バイト数 */
  int result;

  struct addrinfo *res; /* アドレス情報の構造体 */

  long positions[2];
  long lengths[2];

  /* コマンドライン引数の処理 */
  result = ParseArgs(argc, argv, &dst_host_str, &port_str, &filename);
  if (result == 1) {
    return 0;
  } else if (result == 2) {
    return 1;
  }

  /* 送信するファイルを開く */
  fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  /* アドレスを解決して表示する */
  if (ResolveAddress(dst_host_str, port_str, &res) != 0) {
    fprintf(stderr, "ResolveAddress failed\n");
    return 1;
  }
  ShowAddress(res);

  sock = PrepareSocket(res);
  if (sock < 0) {
    return 1;
  }

  DivideFile(filename, BUF_LEN, 2, (int[]){1, 2}, positions, lengths);
  printf("positions: %ld, %ld\n", positions[0], positions[1]);
  printf("lengths: %ld, %ld\n", lengths[0], lengths[1]);

  int fd = open(filename, O_RDONLY);

  /* ファイルの内容を送信する */
  while ((n = read(fd, buf, BUF_LEN)) > 0) {
    write(sock, buf, n);
  }

  /* 出力ファイルのクローズ */
  close(fd);

  /* ソケットのクローズ */
  close(sock);

  return 0;
}

/* vim: set ff=unix fenc=utf-8 : */
