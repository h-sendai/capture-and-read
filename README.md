tcpdumpでキャプチャしながらSiTCP機器からデータを読むプログラム。
1秒間に指定したバイト数読めなかった場合はtcpdumpをとめ、プログラムも
終了する。

tcpdumpには-W MB_sizeというオプションがあり、このバイト数まで
バイト数まで-wで指定したファイルに書き、上限まで達したら次のファイルに
書き始める。さらに-C filecountというオプションがあり、-Wとともに指定すると
作るファイル数に上限を設定することができる。上限まで達すると、一番古い
ファイルに上書きしていく。

-W 10 -C 5とすると、5個の10MBのファイルに循環的にデータを書くようになる。
ファイル名は-w filenameと指定するとfilename.0, filename.1, ... filename.9
となる。
これで際限なくファイルを作るということは回避できる。

## 使い方

コンパイルしてcap-and-read -hとするとヘルプが出る:
    % ./cap-read -h
    Usage: cap-read [-i interface] [-C cap_file_size(MB)] [-W filecount]
           [-w cap_filename] [-s snaplen] [-t threshold (Bytes)]
           [-I read_count_interval ] [-b socket_read_bufsize] ip_address port

