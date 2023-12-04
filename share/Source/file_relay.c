/*  -*- coding: utf-8-unix; -*-                                     */
/*  FILENAME     :  tcp_file_server.c                                 */
/*  DESCRIPTION  :  Simple TCP file Server                          */
/*  USAGE:          tcp_file_server.out filename                    */
/*  VERSION      :                                                  */
/*  DATE         :  Sep 01, 2020                                    */
/*  UPDATE       :                                                  */
/*                                                                  */

#include "icslab2_net.h"

int
main(int argc, char** argv)
{
    int     sock0;                  /* 待ち受け用ソケットディスクリプタ */
    int     sock;                   /* ソケットディスクリプタ */
    struct sockaddr_in  serverAddr; /* サーバ＝中継用アドレス構造体 */
    struct sockaddr_in  clientAddr; /* クライアント＝送信元アドレス構造体 */
    struct sockaddr_in  sendAddr; /* センド＝送信先アドレス構造体 */
    int     addrLen;                /* clientAddrのサイズ */

    char    buf[BUF_LEN];           /* 受信バッファ */
    int     n;                      /* 受信バイト数 */
    int     isEnd = 0;              /* 終了フラグ，0でなければ終了 */

    char *filename;                 /* 返送するファイルの名前 */
    int fd;                         /* ファイルデスクリプタ */

    int     yes = 1;                /* setsockopt()用 */
	
    int	sock1;  /*送信用ソケットディスクリプタ*/
    struct in_addr addr;            /* アドレス表示用 */
    unsigned int sendToPort = TCP_SERVER_PORT; 
    unsigned int openPort = TCP_SERVER_PORT; 

    char *sendToHost = "127.0.0.1";  /* サーバIPアドレス（文字列） */
    

    /* コマンドライン引数の処理 */
    if(argc != 4) {
        printf("can't accept!!!");
        return 0;
    }

    /* 送信先IPアドレス */
    sendToHost = argv[1];
    printf("send host: %s\n", sendToHost);
    
    sendToPort = (unsigned int)atoi(argv[2]);
    printf("send port: %d\n", sendToPort);

    openPort = (unsigned int)atoi(argv[3]);
    printf("open port: %d\n", openPort);

    /* STEP 0: 出力ファイルのオープン */
    fd = open("buffer.txt", O_CREAT | O_RDWR, 0644);
    if(fd < 0) {
        perror("open");
        return 1;
    }

    /* STEP 1: センドのIPアドレスとポートを設定する */
    memset(&sendAddr, 0, sizeof(sendAddr));     /* ゼロクリア */
    sendAddr.sin_family = AF_INET;                /* Internetプロトコル */
    sendAddr.sin_port = htons(sendToPort);   /* 待ち受けるポート */
    inet_pton(AF_INET, sendToHost, &sendAddr.sin_addr.s_addr);  /* IPアドレス（文字列）から変換 */
    
    /* 確認用：センドのIPアドレスを文字列に変換して表示 */
    addr.s_addr = sendAddr.sin_addr.s_addr;
    printf("ip address: %s\n", inet_ntoa(addr));
    printf("port#: %d\n", ntohs(sendAddr.sin_port));

    /* STEP 1: TCPソケットをオープンする */
    if((sock0 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return  1;
    }
    
    /* sock0のコネクションがTIME_WAIT状態でもbind()できるように設定 */
    setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&yes, sizeof(yes));    

    /* STEP 2: 受け付けるIPアドレスとポートを指定する */
    memset(&serverAddr, 0, sizeof(serverAddr));     /* ゼロクリア */
    serverAddr.sin_family = AF_INET;                /* Internetプロトコル */
    serverAddr.sin_port = htons(openPort);   /* 待ち受けるポート */
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* どのIPアドレス宛でも */

    /* STEP 3: ソケットとアドレスをbindする */
    printf("%d",sock0);
    /* 確認用：センドのIPアドレスを文字列に変換して表示 */
    addr.s_addr = serverAddr.sin_addr.s_addr;
    printf("ip address: %s\n", inet_ntoa(addr));
    printf("port#: %d\n", ntohs(serverAddr.sin_port));

    if(bind(sock0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        return  1;
    }

    /* STEP 4: コネクションの最大同時受け入れ数を指定する */
    if(listen(sock0, 5) != 0) {
        perror("listen");
        return  1;
    }

    while(!isEnd) {     /* 終了フラグが0の間は繰り返す */

        /* STEP 5: クライアントからの接続要求を受け付ける */        
        printf("waiting connection...\n");
        addrLen = sizeof(clientAddr);
        sock = accept(sock0, (struct sockaddr *)&clientAddr, (socklen_t *)&addrLen);
        if(sock < 0) {
            perror("accept");
            return  1;
        }

        // センドとのconnect処理開始
        
        /* STEP 2: TCPソケットをオープンする */
        if((sock1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            return  1;
        }

        /* STEP 3: サーバに接続（bind相当も実行） */
        if(connect(sock1, (struct sockaddr *)&sendAddr, sizeof(sendAddr)) < 0) {
            perror("connect");
            return  1;
        }

        // センドとのconnect処理完了

        /* STEP 1: 受信するたびにファイルに出力 */
        while((n = read(sock, buf, BUF_LEN)) > 0) {
            write(fd, buf, n);  /* ファイルに出力 */
        }

        // クライアントのread完了

        /* STEP 1: クライアントからのファイルをそのままセンドに転送 */
        buf[n] = '\0';
        while((n = read(fd, buf, BUF_LEN)) > 0) {
            write(sock1, buf, n);
        }

        // センドへのwrite完了

        close(sock);
        close(sock1);


    }

    close(fd);
    close(sock0);

    return  0;
}

/* Local Variables: */
/* compile-command: "gcc tcp_file_server.c -o tcp_file_server.out" */
/* End: */
