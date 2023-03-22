troughput_sta.py文件作为数据处理工具，当throughput_sta_tool目录和lab_results处于同一级文件目录时，在这一级目录执行命令：
`python ./throughput_sta_tool/throughput_sta.py s`
来实现对./lab_results/pkt_send_mul_auto_sta目录下的发包吞吐量的曲线图绘制

或执行
`python ./throughput_sta_tool/throughput_sta.py sr`
来实现对./lab_results/pkt_send_mul_auto_sta和./lab_results/pkt_rcv_mul_auto_sta目录下的收发包吞吐量的曲线图绘制，依照文件名字典序排序，分别一一对应，每一组send和rcv输出在同一张图片中。

输出图片保存在./lab_results/fig目录下。
