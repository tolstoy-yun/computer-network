#include<iostream>
#include<fstream>
#include<winsock.h>
#include<string.h>
#include <windows.h>
#include<bitset>
#include<ctime>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

//定义发送端socket
SOCKET s_send;
//接收端地址和发送端地址
struct sockaddr_in  recieve_addr;
struct sockaddr_in send_addr;

//校验和
unsigned long sum = 0;
unsigned long s_sum = 0;

//计时
clock_t file_begin;
clock_t file_end;

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

//校验和，传入要加和的buf，与全局变量sum进行加和
u_short checksum(u_short buf) {
	sum += buf;
	if (sum & 0xffff0000) {
		sum &= 0xffff;
		sum++;
	}
	return sum;
}

//从字符串中截取string
string words(char* data, int start, int end) {
	string result = "";
	for (int i = start; i <= end; i++) {
		result.push_back(data[i]);
	}
	return result;
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

//字符串拼接
char* char_strcat(char* a, char* b, int start, int length,int allLength) {
	int j = 0;
	char* c = new char[allLength+1];
	for (int i = 0; i < start; i++) {
		c[i] = a[i];
	}
	for (int i = start; i < start + length; i++) {
		if (j >= length) break;
		c[i] = b[j];
		j++;
	}
	c[allLength] = '\0';
	return c;
}

int main() {
	int recv_len = 0;
	int send_len = 0;
	int socketaddr_len = sizeof(SOCKADDR);
	//初始化socket
	initialize();
	//接收端IP地址和端口号
	char recieve_IP[16];
	USHORT recieve_port=5000;
	char connect_ack [20];//用于存放从接收端收到的连接ack

	//输入接收端IP地址
	cout << "请输入接收端的IP地址：" << endl;
	cin>> recieve_IP;
	//填充接收端信息
	recieve_addr.sin_family = AF_INET;
	recieve_addr.sin_addr.S_un.S_addr = inet_addr(recieve_IP);
	recieve_addr.sin_port = htons(recieve_port);

	//创建套接字
	s_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_send == INVALID_SOCKET) {
		cout << "socket创建失败！错误信息为：" << WSAGetLastError() << endl;
		return 0;
	}
	cout << "套接字创建成功！" << endl;

	//发送连接信息
	string connect_str= "connect";
	int connect_len;
	connect_len = sendto(s_send, connect_str.data(), strlen(connect_str.data()), 0, (SOCKADDR*)&recieve_addr, sizeof(SOCKADDR));
	if (connect_len == SOCKET_ERROR) {
		cout << "连接失败！" << endl;
		return 0;
	}

	//收到接收端发过来的分配的端口信息
	char send_port_char[17];
	recv_len = recvfrom(s_send,send_port_char, 16, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
	if (recv_len == SOCKET_ERROR) {
		cout << "接收端口信息失败！" << endl;
		return 0;
	}
	send_port_char[16] = '\0';
	string send_port_str = send_port_char;
	bitset<16> send_port_b = bitset<16>(send_port_str);//16位2进制
	string send_port_bstr = send_port_b.to_string();//16位2进制字符串
	unsigned short send_port_us = strtol(send_port_bstr.data(), NULL, 2);//10进制，unsigned short类型

	//发送文件
	while (1) {
		//定义长度变量
		int send_len = 0;
		int length;//数据长度
		//定义发送缓冲区
		char* filecontent;
		string send_str = "";//用于拼接数据报
		char recv_buf[20];//用于存放从接收端收到的ack

		//输入要发送的文件的路径
		char* filepath = new char[50];
		cout << endl;
		cout << "请输入要传输的文件的路径：" << endl;
		cin >> filepath;
		//关闭程序
		if (strcmp(filepath, "close") == 0) {
			cout << "关闭程序" << endl;
			break;
		}
		//读取文件
		ifstream file(filepath, ios::in | ios::binary);
		if (!(file.is_open()))
		{
			return 0;
		}

		file.seekg(0, ios::end); //定位输入流结束位置
		ios::pos_type endPos = file.tellg(); //获取输入流结尾指针
		unsigned long fileLen = static_cast<unsigned long>(endPos); //获取输入数据大小

		file.seekg(0, ios::beg);//定位输入流开始位置
		filecontent = new char[fileLen];
		file.read(filecontent, fileLen);
		int seq = 0;//序列号

		//分片发送
		/* 数据报格式：
		* 序列号:send_buf[0:15]
		* 源端口号： send_buf[16:31]
		* 目的端口号：send_buf[32:47]
		* 长度：send_buf[48:63]
		* 校验和：send_buf[64:79]
		* 数据：send_buf[80:]
		*/
		int last = fileLen % 1024;
		file_begin = clock();
		for (int i = 0; i < fileLen;i+=1024) {
			char* send_content;//用于存放从文件中读到的数据
			char* send_buf=NULL;//发送缓冲区

			if (i + 1023 >= fileLen) {
				send_content = new char[last + 1];
				send_content=char_words(filecontent, i, i + last - 1);
				length = last;
			}
			else {
				send_content = new char[1025];
				send_content=char_words(filecontent, i, i + 1023);
				length = 1024;
			}
			sum = 0;//初始化校验和为0
			
			send_content[length] = '\0';
			
			//设置序列号
			bitset<16> seq_b = bitset<16>(seq);//16位2进制
			string seq_bstr =seq_b.to_string();//序列号
			unsigned short seq_us = strtol(seq_bstr.data(), NULL, 2);//10进制，unsigned short类型
			//设置目的端口号
			bitset<16> recieve_port_b = bitset<16>(recieve_port);//目的端口号，16位二进制
			string recieve_port_bstr = recieve_port_b.to_string();//目的端口号，16位二进制字符串
			unsigned short recieve_port_us = strtol(recieve_port_bstr.data(), NULL, 2);//10进制，unsigned short类型

			//设置数据报的长度（整个数据报的长度）=序列号（16）+目的and源端口号(32)+校验和(16)+长度(16)+数据
			int len = 16 +32 + 32 + length;
			bitset<16> len_b = bitset<16>(len);
			string len_bstr = len_b.to_string();
			unsigned short len_us = strtol(len_bstr.data(), NULL, 2);//10进制，unsigned short类型

			//校验和
			checksum(seq_us);//序列号
			checksum(send_port_us);//源端口号
			checksum(recieve_port_us);//目的端口号
			checksum(len_us);//长度

			//数据，2个字节2个字节地算
			for (int i = 0; i < length; i += 2) {
				//如果要找的下一个字节超过了长度范围，则将当前字节+0
				unsigned short send_content_us = 0;
				if (i + 1 >= length) {
					send_content_us = ((unsigned char)send_content[i])<< 8;
				}
				else {
					send_content_us = (((unsigned char)send_content[i])<< 8) + (unsigned char)send_content[i + 1];
				}
				checksum(send_content_us);
			}
			//校验和取反
			sum = ~(sum & 0xffff);
			bitset<16> sum_b = bitset<16>(sum);
			string sum_str = sum_b.to_string();

			//数据报内容拼接
			send_str = seq_bstr;//序列号
			send_str.append(send_port_bstr);//源端口号
			send_str.append(recieve_port_bstr);//目的端口号
			send_str.append(len_bstr);//长度
			send_str.append(sum_str);//校验和
			char* send_buf_head = (char*)send_str.data();
			send_buf = new char[len + 1];
			send_buf = char_strcat(send_buf_head, send_content, 80, length, 80 + length);//数据

			send_len=sendto(s_send,send_buf,len,0,(SOCKADDR*)&recieve_addr,sizeof(recieve_addr));
			if (send_len == SOCKET_ERROR) {
				cout << "发送失败！" << endl;
				break;
			}
			cout << "已发送第" << seq << "个数据包" << endl;

			//确认重传
			while (1) {
				recv_len = recvfrom(s_send, recv_buf, 19, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
				if (recv_len == SOCKET_ERROR) {
					cout << "接收ack失败！" << endl;
					return 0;
				}
				recv_buf[19] = '\0';
				string ack_seq_str = words(recv_buf, 3, 18);
				int ack_seq = strtol(ack_seq_str.data(), NULL, 2) ;

				//收到ack，跳出循环，继续发送下一个分组
				string recv_buf_content = words(recv_buf, 0, 2);
				if ((!recv_buf_content.compare("ack")) && ack_seq==seq) {
					cout << "分组" << seq << "已发送成功" << endl;
					break;
				}
				//收到nak，重传分组
				else {
					cout << "重传分组" << seq << endl;
					send_len = sendto(s_send, send_buf, len, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
					if (send_len == SOCKET_ERROR) {
						cout << "重传失败！" << endl;
						continue;
					}
				}
			}
			seq++;
		}
		file_end = clock();
		cout << "当前文件已发送成功" << endl;
		int trans_time = (file_end - file_begin) / CLOCKS_PER_SEC;
		cout << "传输时间为" << trans_time << "秒" << endl;
		cout << "平均吞吐率为" << (fileLen / 1024) / trans_time << "Kbps" << endl;
	}
	//关闭socket
	closesocket(s_send);
	//释放资源
	WSACleanup();
	return 0;
}
