/**
 * @file file_receiver.c
 * @brief ファイルを受け取るプログラム
 */

#include "icslab2_net.h"

#define MAX_NUM_CONNECTIONS 10

struct Args {
  /** 受信した内容を保存するファイル名 */
  char *filename;
  /** 待機するポート番号 */
  int ports[MAX_NUM_CONNECTIONS];
  /** 待機するポートの数 */
  int num_ports;
  /** senderのホスト名 */
  char *sender_host;
  /** senderのポート番号 */
  char *sender_port;
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
 * @brief ソケットをオープンし、送信先に接続する
 *
 * @param[in] addrinfo 送信先のアドレス
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
int PrepareSocket(struct addrinfo *addrinfo);

/**
 * @brief 待ち受け用のソケットを準備する
 *
 * @param[in] port 待ち受けるポート番号
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
int PrepareSockWait(unsigned short port);

/**
 * @brief ファイルの一部を受信する
 *
 * @param[in] filename   ファイル名
 * @param[in] portion_id 受信するファイルの部分のID
 * @param[in] port       待ち受けるポート番号
 * @return 受信に成功したら0
 */
int ReceiveFilePortion(char *filename, int portion_id, unsigned short port);
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

int main(int argc, char **argv) {
  struct Args args = {0}; /* コマンドライン引数 */

  int result;
  int num_threads;
  int pid;
  int i;
  struct addrinfo *sender_addr;
  int sock;

  args.filename = "output%d.dat"; /* デフォルトのファイル名 */

  /* コマンドライン引数の処理 */
  result = ParseArgs(argc, argv, &args);
  if (result == 1) {
    return 0;
  } else if (result == 2) {
    return 1;
  }
  /* コマンドライン引数の内容を表示 */
  printf("=== args ===\n");
  printf("output file: %s\n", args.filename);
  for (i = 0; i < args.num_ports; ++i) {
    printf("listen on 0.0.0.0:%d\n", args.ports[i]);
  }
  if (ResolveAddress(args.sender_host, args.sender_port, &sender_addr) != 0) {
    printf("resolve address failed\n");
    return 1;
  }
  printf("sender: ");
  ShowAddress(sender_addr);
  printf("\n");

  sock = PrepareSocket(sender_addr);
  write(sock, "GET", 3);

  num_threads = 0;
  while (num_threads + 1 < args.num_ports) {
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
  if (ReceiveFilePortion(args.filename, num_threads, args.ports[num_threads]) !=
      0) {
    printf("ReceiveFilePortion failed\n");
    return 1;
  }

  return 0;
}

int ParseArgs(int argc, char **argv, struct Args *args) {
  static char help[] = "Usage: %s OPTIONS\n"
                       "OPTIONS:\n"
                       "  -h                     show this help\n"
                       "  -o FILE                output file name\n"
                       "  -p PORT [-p PORT ...]  port number to listen\n"
                       "  -s HOST:PORT           sender's host and port\n";
  int i;
  args->num_ports = 0;
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
      case 'p':
        if (i + 1 < argc) {
          args->ports[args->num_ports] = atoi(argv[++i]);
          ++args->num_ports;
        } else {
          printf("missing port number\n");
          return 2;
        }
        break;
      case 's':
        if (i + 1 < argc) {
          args->sender_host = strtok(argv[++i], ":");
          args->sender_port = strtok(NULL, ":");
        } else {
          printf("missing sender host\n");
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

  if (args->num_ports > MAX_NUM_CONNECTIONS) {
    printf("too many ports\n");
    return 2;
  }
  return 0;
}

void ShowSockAddr(struct sockaddr_in *addr) {
  char buf[80];
  struct in_addr *addr_in_p = (struct in_addr *)&addr->sin_addr;
  inet_ntop(AF_INET, addr_in_p, buf, sizeof(buf));
  printf("%s:%d", buf, ntohs(addr->sin_port));
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

int ReceiveFilePortion(char *filename, int portion_id, unsigned short port) {
  struct sockaddr_in senderAddr; /* 送信元のアドレス構造体 */
  int addrLen;                   /* clientAddrのサイズ */
  char buf[BUF_LEN];             /* 受信バッファ */

  char numbered_filename[256];
  int sock0; /* 待ち受け用ソケットディスクリプタ */
  int sock;  /* ソケットディスクリプタ */
  FILE *fp;  /* 出力先のファイルポインタ */

  int n;         /* 受信バイト数 */
  int isEnd = 0; /* 終了フラグ，0でなければ終了 */

  sprintf(numbered_filename, filename, portion_id);

  sock0 = PrepareSockWait(port);
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
    printf("connection from ");
    ShowSockAddr(&senderAddr);
    printf("\n");

    /* 出力先のファイルをオープン */
    fp = fopen(numbered_filename, "wb");
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

/* vim: set ff=unix fenc=utf-8 : */
