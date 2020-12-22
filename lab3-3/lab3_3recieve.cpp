#include<iostream>
#include<WinSock2.h>
#include<string.h>
#include<bitset>
#include<fstream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

SOCKET s_recieve;

//У���
unsigned long sum = 0;

//������
int maxseq;

//��ʼ��socket
void initialize() {
	//��ʼ��socket��Э��ʹ�õ�socket�汾
	WORD wVersionRequested = MAKEWORD(2, 2);//�汾��
	WSADATA wsadata;
	int err;
	err = WSAStartup(wVersionRequested, &wsadata);
	if (err != 0) {
		cout << "��ʼ���׽��ֿ�ʧ�ܣ�" << endl;
	}
	else {
		cout << "��ʼ���׽��ֿ�ɹ�!" << endl;
	}
	//���汾���Ƿ���ȷ
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		cout << "�׽��ֿ�汾����" << endl;
		WSACleanup();
	}
	else {
		cout << "�׽��ֿ�汾��ȷ��" << endl;
	}
}
//���ַ����н�ȡstring
string words(char* data, int start, int end) {
	string result = "";
	for (int i = start; i <= end; i++) {
		result.push_back(data[i]);
	}
	return result;
}

//У��ͣ�����Ҫ�Ӻ͵�buf����ȫ�ֱ���sum���мӺ�
u_short checksum(u_short buf) {
	sum += buf;
	if (sum & 0xffff0000) {
		sum &= 0xffff;
		sum++;
	}
	return sum;
}
//�ַ�����ȡ
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
	int filenum = 1;//�ļ����

	//���յ�ַ
	struct sockaddr_in recieve_addr;
	//���ն˶˿ں�IP��ַ
	USHORT recieve_port = 5000;
	char* recieve_IP = new char(16);

	//��ʼ��socket
	initialize();

	//���÷������Ϣ
	memset(&recieve_addr, 0, sizeof(recieve_addr));
	recieve_addr.sin_family = AF_INET;  //Э��أ�ѡ��TCP/IP iPv4
	recieve_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//��ʾIP��ַ�ķ�ʽ���������޷��ų�������������ʾIP��ַ
	recieve_addr.sin_port = htons(recieve_port);  //���ն˶˿�

	//�����׽���
	s_recieve = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_recieve == INVALID_SOCKET) {
		cout << "�׽��ִ������󣡴�����ϢΪ��" << WSAGetLastError() << endl;
		return 0;
	}

	//bind����һ�����ص�ַ�󶨵�ָ����socket
	if (bind(s_recieve, (SOCKADDR*)&recieve_addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "�׽��ְ�ʧ�ܣ�������ϢΪ��" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else {
		cout << "�׽��ְ󶨳ɹ���" << endl;
	}
	//�ڴ�����һ�����
	int expectseq = 0;

	//����
	while (1) {
		//��ʼ��У���Ϊ0
		sum = 0;
		//������к�
		maxseq = 500;

		int send_len = 0;
		int recv_len = 0;

		//���巢�ͻ������ͽ��ܻ�����
		const char* send_ack;
		const char* send_nak;
		char recv_buf[1106] = { 0 };
		char recv_connect[8] = { 0 };
		//�����µ�socket
		SOCKET s_send;
		//���Ͷ˵�ַ
		SOCKADDR_IN send_addr;

		//recvfrom��������������
		int len = 0;//��ַ����
		len = sizeof(SOCKADDR);

		recv_len = recvfrom(s_recieve, recv_buf, 1105, 0, (SOCKADDR*)&send_addr, &len);
		if (recv_len == SOCKET_ERROR) {
			cout << "����ʧ�ܣ�" << endl;
			continue;
		}

		//���յ�������Ϊ���ӣ���send_addr�ж˿ڵ����ݷ��͸����Ͷ�
		if (strcmp(recv_buf, "connect") == 0) {
			cout << "���ӳɹ���" << endl;
			//��send_addr�ж˿ڵ����ݷ��͸����Ͷ�
			cout << "���Ͷ˶˿ں�Ϊ��" << htons(send_addr.sin_port) << endl;
			cout << "���Ͷ�IP��ַΪ��" << inet_ntoa(send_addr.sin_addr) << endl;
			char send_ok[] = "ok";
			int sendPort_len;
			sendPort_len = sendto(s_recieve,send_ok, strlen(send_ok), 0, (SOCKADDR*)&send_addr, sizeof(SOCKADDR));
			if (sendPort_len == SOCKET_ERROR) {
				cout << "����ok��Ϣʧ�ܣ�" << endl;
				return 0;
			}
			continue;
		}

		recv_buf[1105] = '\0';

		//У���
		/* ���ݱ���ʽ��
		* ���к�:send_buf[0:15]
		* Դ�˿ںţ� send_buf[16:31]
		* Ŀ�Ķ˿ںţ�send_buf[32:47]
		* ���ȣ�send_buf[48:63]
		* У��ͣ�send_buf[64:79]
		* ���һ�����ݰ���־λ�����滹�����ݰ���Ϊ1���Ѿ������һ�����ݰ���Ϊ0):send_buf[80]
		* ���ݣ�send_buf[81:]
		*/
		string seq_bstr = words(recv_buf, 0, 15);//���к�
		unsigned short seq_us = strtol(seq_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

		checksum(seq_us);

		string send_port_bstr = words(recv_buf, 16, 31);//Դ�˿ں�
		unsigned short send_port_us = strtol(send_port_bstr.data(), NULL, 2);//10���ƣ�unsigned short����
		checksum(send_port_us);

		string recieve_port_bstr = words(recv_buf, 32, 47);//Ŀ�Ķ˿ں�
		unsigned short recieve_port_us = strtol(recieve_port_bstr.data(), NULL, 2);//10���ƣ�unsigned short����
		checksum(recieve_port_us);

		string len_bstr = words(recv_buf, 48, 63);//����
		unsigned short len_us = strtol(len_bstr.data(), NULL, 2);//10���ƣ�unsigned short����
		checksum(len_us);

		string send_sum_bstr = words(recv_buf, 64, 79);//У���
		unsigned short send_sum_us = strtol(send_sum_bstr.data(), NULL, 2);//10���ƣ�unsigned short����
		checksum(send_sum_us);

		unsigned short finalflag = recv_buf[80] - '0';//���һ�����ݰ���־λ
		checksum(finalflag);

		//����
		char * send_content = char_words(recv_buf, 81, len_us - 1);

		int length = len_us - 81;
		//���ݣ�2���ֽ�2���ֽڵ���
		for (int i = 0; i < length; i += 2) {
			//���Ҫ�ҵ���һ���ֽڳ����˳��ȷ�Χ���򽫵�ǰ�ֽ�+0
			unsigned short send_content_us = 0;
			if (i + 1 >= length) {
				send_content_us = ((unsigned char)send_content[i]) << 8;
			}
			else {
				send_content_us = (((unsigned char)send_content[i]) << 8) + (unsigned char)send_content[i + 1];
			}
			checksum(send_content_us);
		}

		//У���ȡ��
		sum = ~(sum & 0xffff);
		bitset<16> sum_b = bitset<16>(sum);
		string sum_str = sum_b.to_string();
		/*cout << "У���Ϊ��" << strtol(sum_str.data(), NULL, 2) << endl;*/

		//У��͵�16λΪ0������ack
		/* ack��ʽ��
		* ack��send_ack[0:2]
		* ���һ�����ݰ���־λ��send_ack[3]
		* ���кţ�send_ack[4:19]
		*/
		if (strtol(sum_str.data(), NULL, 2) == 0) {
			//�ж��Ƿ�С���ڴ�����ţ�����ǾͶ���(continue)
			if (seq_us < expectseq) {
				cout << "���ݰ�" << seq_us << "�����ڴ��ķ���" << endl;
				continue;
			}
			string ack = "";
			ack.append("ack");
		
			//�յ������кŵ����ڴ������кţ���������ack���к�
			if (seq_us == expectseq) {
				cout << "�ѽ��յ���" << seq_us << "�����ݰ�" << endl;
				ack.push_back(finalflag + '0');
				ack.append(seq_bstr);
				send_ack = ack.data();
				send_len = sendto(s_recieve, send_ack, strlen(send_ack), 0, (SOCKADDR*)&send_addr, sizeof(SOCKADDR));
				if (send_len == SOCKET_ERROR) {
					cout << "����ackʧ�ܣ�" << endl;
					return 0;
				}
			}
			//����յ������кŴ����ڴ������кţ���ACK�ڴ������к�-1
			else {
				cout << "�յ������ڴ����кŵİ�" << seq_us << endl;
				finalflag = 1;
				ack.push_back(finalflag + '0');
				int dseq;
				if (expectseq == 0) {
					dseq = maxseq;
				}
				else {
					dseq = expectseq - 1;
				}
				bitset<16> dseq_b = bitset<16>(dseq);//16λ2����
				string dseq_bstr = dseq_b.to_string();//���к�
				ack.append(dseq_bstr);
				send_ack = ack.data();
				send_len = sendto(s_recieve, send_ack, strlen(send_ack), 0, (SOCKADDR*)&send_addr, sizeof(SOCKADDR));
				if (send_len == SOCKET_ERROR) {
					cout << "����ackʧ�ܣ�" << endl;
					return 0;
				}
				continue;
			}
			//�����ǰ����������ŵ��������ţ�����ڴ������ֵ��Ϊ0
			if (seq_us == maxseq) {
				expectseq = 0;
			}
			else {
				expectseq++;
			}
		}
		else {
			cout << "�������ݴ���" << endl;
			continue;
		}

		string filepath = "F:/netlab3/result3/";
		filepath.append(to_string(filenum));//�ļ�����Ϊ�ļ����
		ofstream savefile(filepath, ios::binary | ios::app);
		savefile.write(send_content, length);
		savefile.close();
		//�Ѿ������һ�����ݰ��ˣ��ļ����+1
		if (finalflag == 0) {
			cout << "�ļ�" << filenum << "�������" << endl;
			filenum++;
			expectseq = 0;//�ڴ�����һ����Ÿ�Ϊ0
		}
	}
	//�ر�socket
	closesocket(s_recieve);
	//�ͷ���Դ
	WSACleanup();
	return 0;
}