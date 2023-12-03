/**
 * @file file_sender.c
 * @brief ファイルを送信するプログラム
 */

#include "icslab2_net.h"
#include <netdb.h>
#include <stdio.h>

#define MAX_NUM_CONNECTIONS 10

struct Args {
  /** 送信するファイル名 */
  char *filename;
  /** ファイル分割の比 */
  int ratios[MAX_NUM_CONNECTIONS];
  /** 送信先のホスト名 */
  char *hosts[MAX_NUM_CONNECTIONS];
  /** 送信先のポート番号 */
  char *ports[MAX_NUM_CONNECTIONS];
  /** 分割数 */
  int num_connections;
};

/**
 * @brief コマンドライン引数をパースする
 *
 * @param[in]       argc            コマンドライン引数の数
 * @param[in]       argv            コマンドライン引数の内容
 * @param[out]      args            パース結果
 * @return
 * 正常にパースできたなら0、ヘルプを表示したなら1、引数が足りなかったなら2
 */
int ParseArgs(int argc, char **argv, struct Args *args);

/**
 * @brief サーバー名とポート名を解決する
 *
 * @param[in]  host_str ホスト名
 * @param[in]  port_str ポート番号
 * @param[out] res      解決結果
 * @return 正常に解決できたなら0
 */
int ResolveAddress(char *host_str, char *port_str, struct addrinfo **res);

/**
 * @brief struct addrinfoを標準出力に表示する
 */
void ShowAddress(struct addrinfo *addrinfo);

/**
 * @brief ソケットをオープンし、送信先に接続する
 *
 * @param[in] addrinfo 送信先のアドレス
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
int PrepareSocket(struct addrinfo *addrinfo);

/**
 * @brief n以上の値でmの倍数である最小の値を返す
 */
long NextMultipleOf(long n, long m);

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
 *
 * ファイルサイズがblock_sizeの倍数ではなかった場合、lengths[num-1]だけはblock_sizeの倍数にならない
 */
int DivideFile(char *filename, int block_size, int num, int *ratios,
               long *positions, long *lengths);

int main(int argc, char **argv) {
  struct Args args = {0};
  FILE *fp; /* 送信するファイルのファイルポインタ */

  int sock;          /* ソケットディスクリプタ */
  char buf[BUF_LEN]; /* 受信バッファ */
  int n;             /* 読み込み／受信バイト数 */
  int result;

  struct addrinfo *res; /* アドレス情報の構造体 */

  long positions[2];
  long lengths[2];

  /* コマンドライン引数の処理 */
  result = ParseArgs(argc, argv, &args);
  if (result == 1) {
    return 0;
  } else if (result == 2) {
    return 1;
  }

  /* アドレスを解決して表示する */
  if (ResolveAddress(args.hosts[0], args.ports[0], &res) != 0) {
    fprintf(stderr, "ResolveAddress failed\n");
    return 1;
  }
  ShowAddress(res);

  sock = PrepareSocket(res);
  if (sock < 0) {
    return 1;
  }

  DivideFile(args.filename, BUF_LEN, 2, (int[]){1, 2}, positions, lengths);
  printf("positions: %ld, %ld\n", positions[0], positions[1]);
  printf("lengths: %ld, %ld\n", lengths[0], lengths[1]);

  fp = fopen(args.filename, "rb");

  /* ファイルの内容を送信する */
  while ((n = fread(buf, 1, BUF_LEN, fp)) > 0) {
    write(sock, buf, n);
  }

  /* 出力ファイルのクローズ */
  fclose(fp);

  /* ソケットのクローズ */
  close(sock);

  return 0;
}

int ParseArgs(int argc, char **argv, struct Args *args) {
  static char help[] = "Usage: %s OPTIONS\n"
                       "OPTIONS:\n"
                       "  -h        show this help\n"
                       "  -f FILE   file to send\n"
                       "  -s RATIO:HOST:PORT [-s RATIO:HOST:PORT ...]\n";
  int i;
  args->num_connections = 0;
  if (argc == 1) {
    printf(help, argv[0]);
    return 2;
  }
  for (i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'h':
        printf(help, argv[0]);
        return 1;
      case 'f':
        if (i + 1 < argc) {
          args->filename = argv[++i];
        } else {
          printf("missing filename\n");
          return 2;
        }
        break;
      case 's':
        if (i + 1 < argc) {
          char *ratio_str = strtok(argv[++i], ":");
          char *host_str = strtok(NULL, ":");
          char *port_str = strtok(NULL, ":");
          if (ratio_str == NULL || host_str == NULL || port_str == NULL) {
            printf("invalid argument: %s\n", argv[i]);
            return 2;
          }
          args->ratios[args->num_connections] = atoi(ratio_str);
          args->hosts[args->num_connections] = host_str;
          args->ports[args->num_connections] = port_str;
          ++args->num_connections;
        } else {
          printf("missing ratio:host:port\n");
          return 2;
        }
        break;
      default:
        printf("invalid argument: %s\n", argv[i]);
        return 2;
      }
    } else {
      printf("invalid argument: %s\n", argv[i]);
      return 2;
    }
  }

  if (args->num_connections > MAX_NUM_CONNECTIONS) {
    printf("too many connections\n");
    return 2;
  }
  return 0;
}

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
  printf("ip address: %s\n", inet_ntoa(addr));
  printf("port#: %d\n",
         ntohs(((struct sockaddr_in *)(addrinfo->ai_addr))->sin_port));
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

long NextMultipleOf(long n, long m) {
  long r = n % m;
  if (r == 0) {
    return n;
  } else {
    return n + (m - r);
  }
}

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
    if (current_pos + lengths[i] > file_size) {
      lengths[i] = file_size - current_pos;
    }
    positions[i] = current_pos;
    current_pos += lengths[i];
  }

  if (fclose(fp) != 0) {
    perror("fclose");
    return -1;
  }

  return 0;
}

/* vim: set ff=unix fenc=utf-8 : */
