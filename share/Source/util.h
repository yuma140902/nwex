
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
 * @brief 待ち受け用のソケットを準備する
 *
 * @param[in] port 待ち受けるポート番号
 * @return ソケットディスクリプタ。途中でエラーが発生した場合は負の値
 */
int PrepareSockWait(unsigned short port);

/**
 * @brief struct sockaddr_inを標準出力に表示する
 */
void ShowSockAddr(struct sockaddr_in *addr);
