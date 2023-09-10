from scapy.all import Ether, IP, send

eth = Ether(dst="0c:42:a1:d8:10:84", src="b8:83:03:82:a2:10")
ip = IP(src="192.168.200.1", dst="192.168.200.2")
packet = eth/ip
send(packet, count=5, iface="enp216s0f0")
