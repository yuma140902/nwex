/**
 * @file file_receiver.c
 * @brief ファイルを受け取るプログラム
 */

#include "icslab2_net.h"

struct Args {
  /** 受信した内容を保存するファイル名 */
  char *filename;
};

/**
 * @brief コマンドライン引数をパースする
 *
 * @param[in]  argc     コマンドライン引数の数
 * @param[in]  argv     コマンドライン引数の内容
 * @param[out] args     パース結果
 * @return
 * 正常にパースできたなら0、ヘルプを表示したなら1、引数が足りなかったなら2
 */
int ParseArgs(int argc, char **argv, struct Args *args);

/**
 * @brief struct sockaddr_inを標準出力に表示する
 */
void ShowSockAddr(struct sockaddr_in *addr);

/**
 * @brief 待ち受け用のソケットを準備する
 *
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
int PrepareSockWait();

int main(int argc, char **argv) {
  int sock0; /* 待ち受け用ソケットディスクリプタ */
  int sock;  /* ソケットディスクリプタ */
  struct sockaddr_in senderAddr; /* 送信元のアドレス構造体 */
  int addrLen;                   /* clientAddrのサイズ */

  char buf[BUF_LEN]; /* 受信バッファ */
  int n;             /* 受信バイト数 */
  int isEnd = 0;     /* 終了フラグ，0でなければ終了 */

  struct Args args = {0}; /* コマンドライン引数 */
  FILE *fp;               /* 出力先のファイルポインタ */

  int result;

  /* コマンドライン引数の処理 */
  result = ParseArgs(argc, argv, &args);
  if (result == 1) {
    return 0;
  } else if (result == 2) {
    return 1;
  }
  printf("filename: %s\n", args.filename);

  sock0 = PrepareSockWait();
  if (sock0 < 0) {
    return 1;
  }

  while (!isEnd) { /* 終了フラグが0の間は繰り返す */

    /* 送信元からの接続要求を受け付ける */
    printf("waiting connection...\n");
    addrLen = sizeof(senderAddr);
    sock = accept(sock0, (struct sockaddr *)&senderAddr, (socklen_t *)&addrLen);
    if (sock < 0) {
      perror("accept");
      return 1;
    }
    printf("accept: ");
    ShowSockAddr(&senderAddr);

    /* 出力先のファイルをオープン */
    fp = fopen(args.filename, "wb");
    if (fp == NULL) {
      perror("fopen");
      return 1;
    }

    /* 受信した内容をファイルに書き込む */
    while ((n = read(sock, buf, BUF_LEN)) > 0) {
      fwrite(buf, sizeof(char), n, fp);
    }

    fclose(fp);

    /* 通信用のソケットのクローズ */
    close(sock);
    printf("closed\n");
  }

  /* 待ち受け用ソケットのクローズ */
  close(sock0);

  return 0;
}

int ParseArgs(int argc, char **argv, struct Args *args) {
  static char help[] = "Usage: %s OPTIONS\n"
                       "OPTIONS:\n"
                       "  -h        show this help\n"
                       "  -o FILE   output file name\n";
  int i;
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
      case 'o':
        if (i + 1 < argc) {
          args->filename = argv[++i];
        } else {
          printf("missing filename\n");
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
  return 0;
}

void ShowSockAddr(struct sockaddr_in *addr) {
  char buf[80];
  struct in_addr *addr_in_p = (struct in_addr *)&addr->sin_addr;
  inet_ntop(AF_INET, addr_in_p, buf, sizeof(buf));
  printf("addr: %s, port#: %d\n", buf, ntohs(addr->sin_port));
}

int PrepareSockWait() {
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
  memset(&selfAddr, 0, sizeof(selfAddr));     /* ゼロクリア */
  selfAddr.sin_family = AF_INET;              /* Internetプロトコル */
  selfAddr.sin_port = htons(TCP_SERVER_PORT); /* 待ち受けるポート */
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

/* vim: set ff=unix fenc=utf-8 : */
