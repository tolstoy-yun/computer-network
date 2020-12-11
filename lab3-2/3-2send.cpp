#include<iostream>
#include<fstream>
#include<winsock.h>
#include<string.h>
#include <windows.h>
#include<bitset>
#include<ctime>
#include<windows.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

//定义发送端socket
SOCKET s_send;
//接收端地址和发送端地址
struct sockaddr_in  recieve_addr;
struct sockaddr_in send_addr;
int socketaddr_len = sizeof(SOCKADDR);
//接收端端口号
USHORT recieve_port = 5000;
bitset<16> recieve_port_b = bitset<16>(recieve_port);//目的端口号，16位二进制
string recieve_port_bstr = recieve_port_b.to_string();//目的端口号，16位二进制字符串
unsigned short recieve_port_us = strtol(recieve_port_bstr.data(), NULL, 2);//10进制，unsigned short类型
//发送端端口号
string send_port_bstr = "";//2进制，16个字节
unsigned short send_port_us;//10进制

//计时
clock_t file_begin;
clock_t file_end;
clock_t time_start;
clock_t time_now;
int timer_flag = 0;//开始计时的标志位，开始为1，没开始或结束为0
//开始计时
void start_timer() {
	time_start = clock();
	timer_flag = 1;
}
//结束计时
void stop_timer() {
	timer_flag = 0;
}

int base = 0;//基序号
int seq = 0;//下一个序号
int maxseq;//最大序号
int close_resend = 0;

//文件相关
char* filecontent;//文件内容
unsigned long fileLen;//文件长度
int f_flag = 0;//文件内容指针
int baseflags[201] = { -1 };//当前seq所对应的文件内容指针

//校验和
unsigned long sum = 0;
unsigned long resum = 0;//重发时的校验和

//窗口大小
int windowSize = 0;

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
//重传的校验和
u_short checkresum(u_short buf) {
	resum += buf;
	if (resum & 0xffff0000) {
		resum &= 0xffff;
		resum++;
	}
	return resum;
}

//从字符串中截取string
string words(char* sdata, int s_start, int s_end) {
	string sresult = "";
	for (int i = s_start; i <= s_end; i++) {
		sresult.push_back(sdata[i]);
	}
	return sresult;
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
char* char_strcat(char* a, char* b, int start, int length, int allLength) {
	int j = 0;
	char* c = new char[allLength + 1];
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

//判断超时的线程
DWORD WINAPI timeout(LPVOID lpParameter) {
	while (1) {
		if (close_resend == 1) {
			return 0;
		}
		time_now = clock();
		//如果当前时间减去开始时间大于等于0.5秒，则重传从base到当前seq-1的所有分组
		if ((time_now - time_start) / CLOCKS_PER_SEC>=0.5 && timer_flag==1){
			int reseq =(base%201);
			int reend_nextseq  ;//要重发的最后一个序号的下一个序号
			if ((seq%201) == maxseq) {
				reend_nextseq = 0;
			}
			else {
				reend_nextseq = (seq%201) + 1;
			}
			int resend_len = 0;
			int last = fileLen % 1024;
			int relength;//数据长度
			string resend_str = "";//用于拼接数据报
			
			for (int k = baseflags[reseq]; reseq!=reend_nextseq && k < fileLen; k += 1024) {
				char* resend_content;//用于存放从文件中读到的数据
				char* resend_buf = NULL;//重发缓冲区
				//截取文件内容
				if (k + 1023 >= fileLen) {
					resend_content = new char[last + 1];
					resend_content = char_words(filecontent, k, k + last - 1);
					relength = last;
				}
				else {
					resend_content = new char[1025];
					resend_content = char_words(filecontent, k, k + 1023);
					relength = 1024;
				}
				resum = 0;//初始化校验和为0
				resend_content[relength] = '\0';

				//设置序列号
				bitset<16> reseq_b = bitset<16>(reseq);//16位2进制
				string reseq_bstr = reseq_b.to_string();//序列号
				unsigned short reseq_us = strtol(reseq_bstr.data(), NULL, 2);//10进制，unsigned short类型

				//设置数据报的长度（整个数据报的长度）=序列号（16）+目的and源端口号(32)+校验和(16)+长度(16)+数据
				int relen = 16 + 32 + 32 + relength+1;
				bitset<16> relen_b = bitset<16>(relen);
				string relen_bstr = relen_b.to_string();
				unsigned short relen_us = strtol(relen_bstr.data(), NULL, 2);//10进制，unsigned short类型

				//校验和
				checkresum(reseq_us);//序列号
				checkresum(send_port_us);//源端口号
				checkresum(recieve_port_us);//目的端口号
				checkresum(relen_us);//长度

				u_short refinalflag;
				//这已经是最后一个数据包了
				if (k + 1024 >= fileLen) {
					refinalflag = 0;
				}
				//之后还有数据包
				else {
					refinalflag = 1;
				}
				checkresum(refinalflag);

				//数据，2个字节2个字节地算
				for (int p = 0; p < relength; p += 2) {
					//如果要找的下一个字节超过了长度范围，则将当前字节+0
					unsigned short resend_content_us = 0;
					if (p + 1 >= relength) {
						resend_content_us = ((unsigned char)resend_content[p]) << 8;
					}
					else {
						resend_content_us = (((unsigned char)resend_content[p]) << 8) + (unsigned char)resend_content[p + 1];
					}
					checkresum(resend_content_us);
				}
				//校验和取反
				resum = ~(resum & 0xffff);
				bitset<16> resum_b = bitset<16>(resum);
				string resum_str = resum_b.to_string();

				resend_str = reseq_bstr;//序列号
				resend_str.append(send_port_bstr);//源端口号
				resend_str.append(recieve_port_bstr);//目的端口号
				resend_str.append(relen_bstr);//长度
				resend_str.append(resum_str);//校验和
				resend_str.push_back(refinalflag + '0');//最后一个文档标志位
				char* resend_buf_head = (char*)resend_str.data();
				resend_buf = new char[relen + 1];
				resend_buf = char_strcat(resend_buf_head, resend_content, 81, relength, 81 + relength);//数据

				resend_len = sendto(s_send, resend_buf, relen, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
				if (resend_len == SOCKET_ERROR) {
					cout << "发送失败！ " << endl;
					break;
				}
				cout << "已重发" << reseq << "分组 " << endl;
				if (reseq == maxseq) {
					reseq = 0;
				}
				else {
					reseq++;
				}
			}
		}
	}
	return 0;
}

//接收ack线程
DWORD WINAPI recvack(LPVOID lpParameter) {
	int recvack_len = 0;
	char recvack_buf[21];//用于存放从接收端收到的ack
	while (1) {
		//接收ack
		recvack_len = recvfrom(s_send, recvack_buf, 20, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
		if (recvack_len == SOCKET_ERROR) {
			cout << "接收ack失败！ " << endl;
			continue;
		}
		recvack_buf[20] = '\0';
		string ack_seq_str = words(recvack_buf, 4, 19);
		int ack_seq = strtol(ack_seq_str.data(), NULL, 2);
		int ack_flag = recvack_buf[3] - '0';
		//收到ack
		string recvack_buf_content = words(recvack_buf, 0, 2);
		if ((!recvack_buf_content.compare("ack"))) {
			cout << "分组" << ack_seq << "收到ack " << endl;
			//已发送成功所有数据包
			if (ack_flag == 0) {
				close_resend = 1;//关闭超时线程
				stop_timer();
				return 0;
			}
			//窗口向右滑动
			if ((base % 201) > ack_seq) {
				base = base + (ack_seq + 201 - (base % 201))+1;
			}
			else {
				base = base + (ack_seq - (base % 201))+1;
			}
			if ((base%201) == (seq%201)) {
				stop_timer();//结束上一个计时
			}
			else {
				start_timer();//开始下一个计时
			}
			/*cout << "当前base为：" <<base<< endl;*/
		}
	}
	return 0;
}

int main() {
	int send_len = 0;
	int recv_len = 0;
	//文件结束时发送的内容
	char end[4] = "end";
	
	//初始化socket
	initialize();
	//接收端IP地址
	char recieve_IP[16];
	//用于存放从接收端收到的连接ack
	char connect_ack[20];

	//输入接收端IP地址
	cout << "请输入接收端的IP地址：" << endl;
	cin >> recieve_IP;
	//填充接收端信息
	recieve_addr.sin_family = AF_INET;
	recieve_addr.sin_addr.S_un.S_addr = inet_addr(recieve_IP);
	recieve_addr.sin_port = htons(recieve_port);

	//窗口大小
	windowSize = 20;
	//设置最大序号为窗口大小的5倍
	maxseq = windowSize * 10;

	//创建套接字
	s_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_send == INVALID_SOCKET) {
		cout << "socket创建失败！错误信息为：" << WSAGetLastError() << endl;
		return 0;
	}

	//发送连接信息
	string connect_str = "connect";
	int connect_len;
	connect_len = sendto(s_send,connect_str.data() ,strlen(connect_str.data()), 0, (SOCKADDR*)&recieve_addr, sizeof(SOCKADDR));
	if (connect_len == SOCKET_ERROR) {
		cout << "连接失败！" << endl;
		return 0;
	}

	//收到接收端发过来的分配的端口信息
	char send_port_char[17];
	recv_len = recvfrom(s_send, send_port_char, 16, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
	if (recv_len == SOCKET_ERROR) {
		cout << "接收端口信息失败！" << endl;
		return 0;
	}
	send_port_char[16] = '\0';
	string send_port_str = send_port_char;
	bitset<16> send_port_b = bitset<16>(send_port_str);//16位2进制
	send_port_bstr = send_port_b.to_string();//16位2进制字符串
	send_port_us = strtol(send_port_bstr.data(), NULL, 2);//10进制，unsigned short类型

	//发送文件
	while (1) {
		//定义长度变量
		int send_len = 0;
		int length;//数据长度
		//定义发送缓冲区
		string send_str = "";//用于拼接数据报

		//输入要发送的文件的路径
		char* filepath = new char[50];
		cout << endl;
		cout << "请输入要传输的文件的路径：" << endl;
		cin >> filepath;
		//关闭程序
		if (strcmp(filepath, "close") == 0) {
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
		fileLen = static_cast<unsigned long>(endPos); //获取输入数据大小

		file.seekg(0, ios::beg);//定位输入流开始位置
		filecontent = new char[fileLen];
		file.read(filecontent, fileLen);
		base = 0;
		seq = 0;//下一个要发送的数据包的序列号
		f_flag = 0;
		//分片持续发送
		/* 数据报格式：
		* 序列号:send_buf[0:15]
		* 源端口号： send_buf[16:31]
		* 目的端口号：send_buf[32:47]
		* 长度：send_buf[48:63]
		* 校验和：send_buf[64:79]
		* 最后一个文档标志位：send_buf[80]
		* 数据：send_buf[81:]
		*/
		int last = fileLen % 1024;
		//开启超时线程和接收ack线程
		close_resend = 0;
		HANDLE hThread[2];
		hThread[0] = CreateThread(NULL, 0, timeout, NULL, 0, NULL);
		hThread[1] = CreateThread(NULL, 0, recvack, NULL, 0, NULL);
		//关闭线程句柄
		CloseHandle(hThread[0]);
		CloseHandle(hThread[1]);
		Sleep(5);
		file_begin = clock();
		for (f_flag = 0; f_flag < fileLen; f_flag += 1024) {
			//当序号<base+窗口大小时，拼接数据报，发送，计时
			if (seq < base + windowSize) {
				baseflags[seq%201] = f_flag;
				char* send_content;//用于存放从文件中读到的数据
				char* send_buf = NULL;//发送缓冲区

				if (f_flag + 1023 >= fileLen) {
					send_content = new char[last + 1];
					send_content = char_words(filecontent, f_flag, f_flag + last - 1);
					length = last;
				}
				else {
					send_content = new char[1025];
					send_content = char_words(filecontent, f_flag, f_flag + 1023);
					length = 1024;
				}
				sum = 0;//初始化校验和为0

				send_content[length] = '\0';

				//设置序列号
				bitset<16> seq_b = bitset<16>(seq%201);//16位2进制
				string seq_bstr = seq_b.to_string();//序列号
				unsigned short seq_us = strtol(seq_bstr.data(), NULL, 2);//10进制，unsigned short类型

				//设置数据报的长度（整个数据报的长度）=序列号（16）+目的and源端口号(32)+校验和(16)+长度(16)+数据+最后一个数据包标志位
				int len = 16 + 32 + 32 + length+1;
				bitset<16> len_b = bitset<16>(len);
				string len_bstr = len_b.to_string();
				unsigned short len_us = strtol(len_bstr.data(), NULL, 2);//10进制，unsigned short类型

				//校验和
				checksum(seq_us);//序列号
				checksum(send_port_us);//源端口号
				checksum(recieve_port_us);//目的端口号
				checksum(len_us);//长度

				u_short finalflag;
				//这已经是最后一个数据包了
				if (f_flag + 1024 >= fileLen) {
					finalflag = 0;
				}
				//之后还有数据包
				else {
					finalflag = 1;
				}
				checksum(finalflag);

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

				//数据报内容拼接
				send_str = seq_bstr;//序列号
				send_str.append(send_port_bstr);//源端口号
				send_str.append(recieve_port_bstr);//目的端口号
				send_str.append(len_bstr);//长度
				send_str.append(sum_str);//校验和
				send_str.push_back(finalflag + '0');//最后一个文档标志位
				char* send_buf_head = (char*)send_str.data();
				send_buf = new char[len + 1];
				send_buf = char_strcat(send_buf_head, send_content, 81, length, 81 + length);//数据

				send_len = sendto(s_send, send_buf, len, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
				if (send_len == SOCKET_ERROR) {
					cout << "发送失败！" << endl;
					break;
				}
				cout << "已发送" << (seq%201) << "分组 " << endl;
				if ((base%201) == (seq%201)) {
					start_timer();
				}
				if (f_flag + 1024 >= fileLen) {
					break;
				}
				seq++;
			}
			//指针减到上一个数据段位置
			else {
				f_flag -= 1024;
			}	
		}
		while (close_resend != 1) {}
		file_end = clock();
		timer_flag == 0;
		cout << "当前文件已发送成功" << endl;
		int trans_time = (file_end - file_begin) / CLOCKS_PER_SEC;
		cout << "传输时间为" << trans_time <<"秒"<< endl;
		cout << "平均吞吐率为"<<(fileLen / 1000) / trans_time << "Kbps" << endl;
		
	}
	//关闭socket
	closesocket(s_send);
	//释放资源
	WSACleanup();
	return 0;
}
