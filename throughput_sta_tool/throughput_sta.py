# -*- coding: utf-8 -*-
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy.interpolate import make_interp_spline
import sys
import os

def send_plot_pps(file,ticks_interval=1):
    reader = pd.read_csv(file)
    datalist = np.array(reader).tolist()
    core_num = reader['core'][0]
    flow_num = reader['flow_num'][0]
    pkt_len = reader['pkt_len'][0]
    timestamp = reader['timestamp'][0]
    # print(datalist)
    length = len(datalist)
    x = list()
    y = list()
    for i in range(length):
        x.append(0.5*datalist[i][4]+timestamp)
        y.append(0.5*datalist[i][5])
    x = np.array(x)
    y = np.divide(np.array(y),1000000)# 每一项都除以10^6

    x_smooth = np.linspace(x.min(), x.max(), 1000)
    y_smooth = make_interp_spline(x, y)(x_smooth)#插值法平滑曲线图
    plt.clf()
    #设置曲线图样式
    plt.plot(x_smooth, y_smooth)  # 绘制x,y的曲线图
    # print(x_smooth, y_smooth)
    #设置坐标轴范围
    plt.xlim((timestamp, x[-1]))
    plt.ylim((0, 30))
    #设置坐标轴名称
    plt.xlabel('Timestamp')
    plt.ylabel('Troughput/Mpps')
    #设置坐标轴刻度
    my_x_ticks = np.arange(timestamp, x[-1], ticks_interval)
    my_y_ticks = np.arange(0, 30, 5)
    plt.xticks(my_x_ticks)
    plt.yticks(my_y_ticks)
    plt.title(str(core_num)+" core, flow_num = "+str(flow_num)+", pkt_len = "+str(pkt_len)+"B")
    plt.ticklabel_format(style='plain', useOffset=False, axis='x')
    # plt.legend()
    # plt.show()  # 显示图
    if not os.path.exists("./lab_results/fig"):
        os.makedirs("./lab_results/fig")
    plt.savefig("./lab_results/fig/"+str(timestamp)+"_s_pps.png")
    


def send_plot_bps(file,ticks_interval=1):
    reader = pd.read_csv(file)
    datalist = np.array(reader).tolist()
    core_num = reader['core'][0]
    flow_num = reader['flow_num'][0]
    pkt_len = reader['pkt_len'][0]
    timestamp = reader['timestamp'][0]
    # print(datalist)
    length = len(datalist)
    x = list()
    y = list()
    for i in range(length):
        x.append(0.5*datalist[i][4]+timestamp)
        y.append(0.5*datalist[i][5])
    x = np.array(x)
    y = np.divide(np.array(y),1000000000)# 每一项都除以10^6

    x_smooth = np.linspace(x.min(), x.max(), 1000)
    y_smooth = make_interp_spline(x, y)(x_smooth)#插值法平滑曲线图
    plt.clf()
    #设置曲线图样式
    plt.plot(x_smooth, y_smooth)  # 绘制x,y的曲线图
    # print(x_smooth, y_smooth)
    #设置坐标轴范围
    plt.xlim((timestamp, x[-1]))
    plt.ylim((0, 15))
    #设置坐标轴名称
    plt.xlabel('Timestamp')
    plt.ylabel('Troughput/Gbps')
    #设置坐标轴刻度
    my_x_ticks = np.arange(timestamp, x[-1], ticks_interval)
    my_y_ticks = np.arange(0, 15, 5)
    plt.xticks(my_x_ticks)
    plt.yticks(my_y_ticks)
    plt.title(str(core_num)+" core, flow_num = "+str(flow_num)+", pkt_len = "+str(pkt_len)+"B")
    plt.ticklabel_format(style='plain', useOffset=False, axis='x')
    # plt.legend()
    # plt.show()  # 显示图
    if not os.path.exists("./lab_results/fig"):
        os.makedirs("./lab_results/fig")
    plt.savefig("./lab_results/fig/"+str(timestamp)+"_s_bps.png")



def send_and_rcv_plot_pps(SendRecordFile,RcvRecordFile,ticks_interval=1):
    SendReader = pd.read_csv(SendRecordFile)
    RcvReader = pd.read_csv(RcvRecordFile)
    datalist_send = np.array(SendReader).tolist()
    datalist_rcv = np.array(RcvReader).tolist()
    send_core_num = SendReader['core'][0]
    rcv_core_num = RcvReader['core'][0]
    flow_num = SendReader['flow_num'][0]
    pkt_len = SendReader['pkt_len'][0]
    send_timestamp = SendReader['timestamp'][0]
    rcv_timestamp = RcvReader['timestamp'][0]

    send_length = len(datalist_send)
    rcv_length = len(datalist_rcv)
    x_send = list()
    y_send = list()
    x_rcv = list()
    y_rcv = list()
    for i in range(send_length):
        x_send.append(0.5*datalist_send[i][4]+send_timestamp)
        y_send.append(datalist_send[i][5])
    for i in range(rcv_length):
        x_rcv.append(0.5*datalist_rcv[i][2]+rcv_timestamp)
        y_rcv.append(datalist_rcv[i][3])
    x_send = np.array(x_send)
    y_send = np.divide(np.array(y_send),1000000)# 每一项都除以10^6
    x_rcv = np.array(x_rcv)
    y_rcv = np.divide(np.array(y_rcv),1000000)

    # 插值法平滑曲线图
    x_send_smooth = np.linspace(x_send.min(), x_send.max(), 1000)
    y_send_smooth = make_interp_spline(x_send, y_send)(x_send_smooth)
    x_rcv_smooth = np.linspace(x_rcv.min(), x_rcv.max(), 1000)
    y_rcv_smooth = make_interp_spline(x_rcv, y_rcv)(x_rcv_smooth)

    #设置曲线图样式
    plt.clf()
    plt.plot(x_send_smooth, y_send_smooth,label='send')  # 绘制x,y的曲线图
    plt.plot(x_rcv_smooth, y_rcv_smooth,label='rcv') 
    # print(x_smooth, y_smooth)
    #设置坐标轴范围
    plt.xlim((min(send_timestamp,rcv_timestamp), max(x_send[-1],x_rcv[-1])))
    plt.ylim((0, 30))
    #设置坐标轴名称
    plt.xlabel('Timestamp')
    plt.ylabel('Troughput/Mpps')
    #设置坐标轴刻度
    my_x_ticks = np.arange(min(send_timestamp,rcv_timestamp), max(x_send[-1],x_rcv[-1]), ticks_interval)
    my_y_ticks = np.arange(0, 30, 5)
    plt.xticks(my_x_ticks)
    plt.yticks(my_y_ticks)
    plt.title(str(send_core_num)+" Send Core, "+str(rcv_core_num)+" Rcv Core, flow_num = "+str(flow_num)+", pkt_len = "+str(pkt_len)+"B")
    plt.ticklabel_format(style='plain', useOffset=False, axis='x')
    plt.legend()
    # plt.show()  # 显示图
    if not os.path.exists("./lab_results/fig"):
        os.makedirs("./lab_results/fig")
    plt.savefig("./lab_results/fig/"+str(send_timestamp)+"_rs_pps.png")



def send_and_rcv_plot_bps(SendRecordFile,RcvRecordFile,ticks_interval=1):
    SendReader = pd.read_csv(SendRecordFile)
    RcvReader = pd.read_csv(RcvRecordFile)
    datalist_send = np.array(SendReader).tolist()
    datalist_rcv = np.array(RcvReader).tolist()
    send_core_num = SendReader['core'][0]
    rcv_core_num = RcvReader['core'][0]
    flow_num = SendReader['flow_num'][0]
    pkt_len = SendReader['pkt_len'][0]
    send_timestamp = SendReader['timestamp'][0]
    rcv_timestamp = RcvReader['timestamp'][0]

    send_length = len(datalist_send)
    rcv_length = len(datalist_rcv)
    x_send = list()
    y_send = list()
    x_rcv = list()
    y_rcv = list()
    for i in range(send_length):
        x_send.append(0.5*datalist_send[i][4]+send_timestamp)
        y_send.append(datalist_send[i][5])
    for i in range(rcv_length):
        x_rcv.append(0.5*datalist_rcv[i][2]+rcv_timestamp)
        y_rcv.append(datalist_rcv[i][3])
    x_send = np.array(x_send)
    y_send = np.divide(np.array(y_send),1000000000)# 每一项都除以10^6
    x_rcv = np.array(x_rcv)
    y_rcv = np.divide(np.array(y_rcv),1000000000)

    # 插值法平滑曲线图
    x_send_smooth = np.linspace(x_send.min(), x_send.max(), 1000)
    y_send_smooth = make_interp_spline(x_send, y_send)(x_send_smooth)
    x_rcv_smooth = np.linspace(x_rcv.min(), x_rcv.max(), 1000)
    y_rcv_smooth = make_interp_spline(x_rcv, y_rcv)(x_rcv_smooth)

    #设置曲线图样式
    plt.clf()
    plt.plot(x_send_smooth, y_send_smooth,label='send')  # 绘制x,y的曲线图
    plt.plot(x_rcv_smooth, y_rcv_smooth,label='rcv') 
    # print(x_smooth, y_smooth)
    #设置坐标轴范围
    plt.xlim((min(send_timestamp,rcv_timestamp), max(x_send[-1],x_rcv[-1])))
    plt.ylim((0, 20))
    #设置坐标轴名称
    plt.xlabel('Timestamp')
    plt.ylabel('Troughput/Gbps')
    #设置坐标轴刻度
    my_x_ticks = np.arange(min(send_timestamp,rcv_timestamp), max(x_send[-1],x_rcv[-1]), ticks_interval)
    my_y_ticks = np.arange(0, 20, 4)
    plt.xticks(my_x_ticks)
    plt.yticks(my_y_ticks)
    plt.title(str(send_core_num)+" Send Core, "+str(rcv_core_num)+" Rcv Core, flow_num = "+str(flow_num)+", pkt_len = "+str(pkt_len)+"B")
    plt.ticklabel_format(style='plain', useOffset=False, axis='x')
    plt.legend()
    # plt.show()  # 显示图
    if not os.path.exists("./lab_results/fig"):
        os.makedirs("./lab_results/fig")
    plt.savefig("./lab_results/fig/"+str(send_timestamp)+"_rs_bps.png")

# send_and_rcv_plot_pps("1679127683.csv","1679127680.csv",ticks_interval=3)

def send_main(typename):
    if not os.path.exists(os.path.join("./lab_results/send/",typename)):
        os.makedirs(os.path.join("./lab_results/send/",typename))
    data = pd.read_csv("lab_results/pkt_send_mul_auto_sta/throughput_"+typename+".csv")
    #print(data['core'][24])#显示'core'列第24项的值（是数值）
    department_list = list(data['timestamp'].drop_duplicates())  # 获取数据 timestamp 列，去重并放入列表
    # print(department_list)
    for i in department_list:
        department = data[data['timestamp'] == i]
        department.to_csv(os.path.join("./lab_results/send/",typename, str(i) + typename + '.csv'),index=0)

def rcv_main(typename):
    if not os.path.exists(os.path.join("./lab_results/rcv/",typename)):
        os.makedirs(os.path.join("./lab_results/rcv/",typename))
    data = pd.read_csv("lab_results/pkt_rcv_mul_auto_sta/throughput_"+typename+".csv")
    #print(data['core'][24])#显示'core'列第24项的值（是数值）
    department_list = list(data['timestamp'].drop_duplicates())  # 获取数据 timestamp 列，去重并放入列表
    # print(department_list)
    for i in department_list:
        department = data[data['timestamp'] == i]
        department.to_csv(os.path.join("./lab_results/rcv/", typename, str(i)+typename+'.csv'),index=0)

def send_and_rcv_main():
    send_main("pps")
    send_main("bps")
    rcv_main("pps")
    rcv_main("bps")


if __name__ == "__main__":
    cmdtype = sys.argv[1]
    if cmdtype == 's':
        send_main("pps")
        send_main("bps")
        items_pps = os.listdir("./lab_results/send/pps/")
        for item in items_pps:
            if 'pps.csv' in item:
                send_plot_pps("./lab_results/send/pps/"+item, ticks_interval=2)
        items_bps = os.listdir("./lab_results/send/bps/")
        for item in items_bps:
            if 'bps.csv' in item:
                send_plot_bps("./lab_results/send/bps/"+item, ticks_interval=2)
    if cmdtype == 'sr':
        send_and_rcv_main()
        items_pps_send = os.listdir("./lab_results/send/pps/")
        items_pps_rcv = os.listdir("./lab_results/rcv/pps/")
        for i in range(len(items_pps_send)):
            send_and_rcv_plot_pps("./lab_results/send/pps/"+items_pps_send[i],"./lab_results/rcv/pps/"+items_pps_rcv[i], ticks_interval=2)
        items_bps_send = os.listdir("./lab_results/send/bps/")
        items_bps_rcv = os.listdir("./lab_results/rcv/bps/")
        for i in range(len(items_bps_send)):
            send_and_rcv_plot_bps("./lab_results/send/bps/"+items_bps_send[i],"./lab_results/rcv/bps/"+items_bps_rcv[i], ticks_interval=2)
