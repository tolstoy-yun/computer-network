#include<iostream>
#include<WinSock2.h>
#include<string.h>
#include<bitset>
#include<fstream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

SOCKET s_recieve;

//校验和
unsigned long sum = 0;

//最大序号
int maxseq;

//初始化socket
void initialize() {
	//初始化socket，协商使用的socket版本
	WORD wVersionRequested = MAKEWORD(2, 2);//版本号
	WSADATA wsadata;
	int err;
	err = WSAStartup(wVersionRequested, &wsadata);
	if (err != 0) {
		cout << "初始化套接字库失败！" << endl;
	}
	else {
		cout << "初始化套接字库成功!" << endl;
	}
	//检测版本号是否正确
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		cout << "套接字库版本错误！" << endl;
		WSACleanup();
	}
	else {
		cout << "套接字库版本正确！" << endl;
	}
}
//从字符串中截取string
string words(char* data, int start, int end) {
	string result = "";
	for (int i = start; i <= end; i++) {
		result.push_back(data[i]);
	}
	return result;
}

//校验和，传入要加和的buf，与全局变量sum进行加和
u_short checksum(u_short buf) {
	sum += buf;
	if (sum & 0xffff0000) {
		sum &= 0xffff;
		sum++;
	}
	return sum;
}
//字符串截取
char* char_words(char*data, int start, int end) {
	char* result = new char[end - start + 1];
	int j = 0;
	for (int i = start; i <= end; i++) {
		result[j] = data[i];
		j++;
	}
	return result;
}

int main() {
	int filenum = 1;//文件序号

	//接收地址
	struct sockaddr_in recieve_addr;
	//接收端端口和IP地址
	USHORT recieve_port = 5000;
	char* recieve_IP = new char(16);

	//初始化socket
	initialize();

	//设置服务端信息
	memset(&recieve_addr, 0, sizeof(recieve_addr));
	recieve_addr.sin_family = AF_INET;  //协议簇，选择TCP/IP iPv4
	recieve_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//表示IP地址的方式，这里用无符号长整型数据来表示IP地址
	recieve_addr.sin_port = htons(recieve_port);  //接收端端口

	//创建套接字
	s_recieve = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_recieve == INVALID_SOCKET) {
		cout << "套接字创建错误！错误信息为：" << WSAGetLastError() << endl;
		return 0;
	}

	//bind，将一个本地地址绑定到指定的socket
	if (bind(s_recieve, (SOCKADDR*)&recieve_addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "套接字绑定失败！错误信息为：" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else {
		cout << "套接字绑定成功！" << endl;
	}
	//期待的下一个序号
	int expectseq = 0;

	//接收
	while (1) {
		//初始化校验和为0
		sum = 0;
		//发送端窗口大小
		int windowSize = 20;
		maxseq = windowSize * 10;

		int send_len = 0;
		int recv_len = 0;

		//定义发送缓冲区和接受缓冲区
		const char* send_ack;
		const char* send_nak;
		char recv_buf[1106] = { 0 };
		char recv_connect[8] = { 0 };
		//定义新的socket
		SOCKET s_send;
		//发送端地址
		SOCKADDR_IN send_addr;

		//recvfrom，接受连接请求
		int len = 0;//地址长度
		len = sizeof(SOCKADDR);

		recv_len = recvfrom(s_recieve, recv_buf, 1105, 0, (SOCKADDR*)&send_addr, &len);
		if (recv_len == SOCKET_ERROR) {
			cout << "接收失败！" << endl;
			continue;
		}

		//接收到的内容为连接，则将send_addr中端口的内容发送给发送端
		if (strcmp(recv_buf, "connect") == 0) {
			cout << "连接成功！" << endl;
			//将send_addr中端口的内容发送给发送端
			cout << "发送端端口号为：" << send_addr.sin_port << endl;
			bitset<16> send_port = bitset<16>(send_addr.sin_port);//2进制16位
			string send_port_str = send_port.to_string();
			const char* send_port_char = send_port_str.data();
			int sendPort_len;
			sendPort_len = sendto(s_recieve, send_port_char, strlen(send_port_char), 0, (SOCKADDR*)&send_addr, sizeof(SOCKADDR));
			if (sendPort_len == SOCKET_ERROR) {
				cout << "发送端口信息失败！" << endl;
				return 0;
			}
			continue;
		}

		recv_buf[1105] = '\0';

		//校验和
		/* 数据报格式：
		* 序列号:send_buf[0:15]
		* 源端口号： send_buf[16:31]
		* 目的端口号：send_buf[32:47]
		* 长度：send_buf[48:63]
		* 校验和：send_buf[64:79]
		* 最后一个数据包标志位（后面还有数据包则为1，已经是最后一个数据包则为0):send_buf[80]
		* 数据：send_buf[81:]
		*/
		string seq_bstr = words(recv_buf, 0, 15);//序列号
		unsigned short seq_us = strtol(seq_bstr.data(), NULL, 2);//10进制，unsigned short类型

		checksum(seq_us);

		string send_port_bstr = words(recv_buf, 16, 31);//源端口号
		unsigned short send_port_us = strtol(send_port_bstr.data(), NULL, 2);//10进制，unsigned short类型
		checksum(send_port_us);

		string recieve_port_bstr = words(recv_buf, 32, 47);//目的端口号
		unsigned short recieve_port_us = strtol(recieve_port_bstr.data(), NULL, 2);//10进制，unsigned short类型
		checksum(recieve_port_us);

		string len_bstr = words(recv_buf, 48, 63);//长度
		unsigned short len_us = strtol(len_bstr.data(), NULL, 2);//10进制，unsigned short类型
		checksum(len_us);

		string send_sum_bstr = words(recv_buf, 64, 79);//校验和
		unsigned short send_sum_us = strtol(send_sum_bstr.data(), NULL, 2);//10进制，unsigned short类型
		checksum(send_sum_us);

		unsigned short finalflag = recv_buf[80]-'0';//最后一个数据包标志位
		checksum(finalflag);

		//数据
		char * send_content = char_words(recv_buf, 81, len_us - 1);

		int length = len_us - 81;
		//数据，2个字节2个字节地算
		for (int i = 0; i < length; i += 2) {
			//如果要找的下一个字节超过了长度范围，则将当前字节+0
			unsigned short send_content_us = 0;
			if (i + 1 >= length) {
				send_content_us = ((unsigned char)send_content[i]) << 8;
			}
			else {
				send_content_us = (((unsigned char)send_content[i]) << 8) + (unsigned char)send_content[i + 1];
			}
			checksum(send_content_us);
		}

		//校验和取反
		sum = ~(sum & 0xffff);
		bitset<16> sum_b = bitset<16>(sum);
		string sum_str = sum_b.to_string();
		/*cout << "校验和为：" << strtol(sum_str.data(), NULL, 2) << endl;*/
		
		//校验和低16位为0，发送ack
		/* ack格式：
		* ack：send_ack[0:2]
		* 最后一个数据包标志位：send_ack[3]
		* 序列号：send_ack[4:19]
		*/
		if (strtol(sum_str.data(), NULL, 2) == 0) {
			//判断是否为期待的序号，如果不是就丢掉(continue)，是就继续往下
			if (seq_us != expectseq) {
				cout << "数据包" << seq_us << "不是期待的分组" << endl;
				continue;
			}
			cout << "已接收到第" << seq_us << "个数据包" << endl;
			string ack = "";
			ack.append("ack");
			ack.push_back(finalflag + '0');
			ack.append(seq_bstr);
			send_ack = ack.data();
			send_len = sendto(s_recieve, send_ack, strlen(send_ack), 0, (SOCKADDR*)&send_addr, sizeof(SOCKADDR));
			if (send_len == SOCKET_ERROR) {
				cout << "发送ack失败！" << endl;
				return 0;
			}
			//如果当前传过来的序号等于最大序号，则把期待的序号值改为1
			if (seq_us == maxseq) {
				expectseq = 0;
			}
			else {
				expectseq++;
			}
		}
		else {
			cout << "传输内容错误！" << endl;
			continue;
		}

		string filepath = "F:/netlab3/result2/";
		filepath.append(to_string(filenum));//文件名即为文件序号
		ofstream savefile(filepath, ios::binary | ios::app);
		savefile.write(send_content, length);
		savefile.close();
		//已经是最后一个数据包了，文件序号+1
		if (finalflag == 0) {
			cout << "文件"<<filenum<<"接收完成" << endl;
			filenum++;
			expectseq = 0;//期待的下一个序号改为0
		}
	}
	//关闭socket
	closesocket(s_recieve);
	//释放资源
	WSACleanup();
	return 0;
}
