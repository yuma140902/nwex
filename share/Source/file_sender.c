/**
 * @file file_sender.c
 * @brief ファイルを送信するプログラム
 */

#include "icslab2_net.h"
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

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

/**
 * @brief ファイルの一部を送信する
 *
 * @param filename ファイル名
 * @param start_pos 送信する部分の開始地点
 * @param length 送信する部分の長さ
 * @param dst 送信先
 * @return 送信に成功したら0
 */
int SendFilePortion(char *filename, long start_pos, long length,
                    struct addrinfo *dst);

int main(int argc, char **argv) {
  struct Args args = {0}; /* コマンドライン引数 */

  int result;
  int pid;
  int num_threads;
  int i;

  struct addrinfo *res[MAX_NUM_CONNECTIONS]; /* アドレス情報の構造体 */

  long positions[MAX_NUM_CONNECTIONS]; /* ファイル分割の各部分の開始地点 */
  long lengths[MAX_NUM_CONNECTIONS]; /* ファイル分割の各部分の長さ */

  /* コマンドライン引数の処理 */
  result = ParseArgs(argc, argv, &args);
  if (result == 1) {
    return 0;
  } else if (result == 2) {
    return 1;
  }
  printf("=== args ===\n");
  printf("input file: %s\n", args.filename);
  printf("send to:\n");
  for (i = 0; i < args.num_connections; ++i) {
    if (ResolveAddress(args.hosts[i], args.ports[i], &res[i]) != 0) {
      fprintf(stderr, "ResolveAddress failed\n");
      return 1;
    }
    printf("  [%d] ratio %d : ", i, args.ratios[i]);
    ShowAddress(res[i]);
    printf("\n");
  }

  DivideFile(args.filename, BUF_LEN, args.num_connections, args.ratios,
             positions, lengths);
  for (i = 0; i < args.num_connections; ++i) {
    printf("positions[%d] = %ld\n", i, positions[i]);
    printf("lengths[%d] = %ld\n", i, lengths[i]);
  }

  num_threads = 0;
  while (num_threads + 1 < args.num_connections) {
    pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }
    if (pid == 0) {
      break;
    } else {
      ++num_threads;
    }
  }

  printf("child %d\n", num_threads);
  if (SendFilePortion(args.filename, positions[num_threads],
                      lengths[num_threads], res[num_threads]) != 0) {
    printf("SendFilePortion failed\n");
    return 1;
  }
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
  printf("%s:%d", inet_ntoa(addr),
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

int SendFilePortion(char *filename, long start_pos, long length,
                    struct addrinfo *dst) {

  FILE *fp;            /* 送信するファイルのファイルポインタ */
  int sock;            /* ソケットディスクリプタ */
  char buf[BUF_LEN];   /* 送信バッファ */
  long n;              /* 送信バイト数 */
  long total_sent = 0; /* 送信済みのバイト数 */

  printf("start sending %ld bytes to ", length);
  ShowAddress(dst);
  printf("\n");

  sock = PrepareSocket(dst);
  if (sock < 0) {
    return 1;
  }
  fp = fopen(filename, "rb");
  if (fseek(fp, start_pos, SEEK_SET) != 0) {
    perror("fseek");
    return 1;
  }

  /* ファイルの内容を送信する */
  while ((n = fread(buf, sizeof(char), BUF_LEN, fp)) > 0) {
    total_sent += n;
    if (total_sent > length) {
      return 0;
    }
    write(sock, buf, n);
  }

  /* 出力ファイルのクローズ */
  fclose(fp);

  /* ソケットのクローズ */
  close(sock);

  return 0;
}
/* vim: set ff=unix fenc=utf-8 : */
