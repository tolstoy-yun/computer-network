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

//���巢�Ͷ�socket
SOCKET s_send;
//���ն˵�ַ�ͷ��Ͷ˵�ַ
struct sockaddr_in  recieve_addr;
struct sockaddr_in send_addr;
int socketaddr_len = sizeof(SOCKADDR);
//���ն˶˿ں�
USHORT recieve_port = 4001;
bitset<16> recieve_port_b = bitset<16>(recieve_port);//Ŀ�Ķ˿ںţ�16λ������
string recieve_port_bstr = recieve_port_b.to_string();//Ŀ�Ķ˿ںţ�16λ�������ַ���
unsigned short recieve_port_us = strtol(recieve_port_bstr.data(), NULL, 2);//10���ƣ�unsigned short����
//���Ͷ˶˿ں�
USHORT send_port = 6002;
bitset<16> send_port_b = bitset<16>(send_port);//Դ�˿ںţ�16λ������
string send_port_bstr = send_port_b.to_string();//Դ�˿ںţ�16λ�������ַ���
unsigned short send_port_us = strtol(send_port_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

//��ʱ
clock_t file_begin;
clock_t file_end;
clock_t time_start;
clock_t time_now;
int timer_flag = 0;//��ʼ��ʱ�ı�־λ����ʼΪ1��û��ʼ�����Ϊ0
//��ʼ��ʱ
void start_timer() {
	time_start = clock();
	timer_flag = 1;
}
//������ʱ
void stop_timer() {
	timer_flag = 0;
}

int base = 0;//�����
int seq = 0;//��һ�����
int maxseq;//������
int close_resend = 0;
int dupsend_flag = 0;//�����ش�״̬��־��1Ϊ�����ش�״̬��0����
int resend_flag = 0;//�ش�״̬��־��1Ϊ��ʱ�ش���0����

//�ļ����
char* filecontent;//�ļ�����
unsigned long fileLen;//�ļ�����
int f_flag = 0;//�ļ�����ָ��
int baseflags[501] = { -1 };//��ǰseq����Ӧ���ļ�����ָ��

//У���
unsigned long sum = 0;
unsigned long resum = 0;//�ط�ʱ��У���
unsigned long dsum = 0;//�����ش�ʱ��У���

//���ڴ�С
double cwnd = 0;
//��ֵ
int ssthresh = 20;
//�ظ�ack����
int dupackcount=0;

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

//У��ͣ�����Ҫ�Ӻ͵�buf����ȫ�ֱ���sum���мӺ�
u_short checksum(u_short buf) {
	sum += buf;
	if (sum & 0xffff0000) {
		sum &= 0xffff;
		sum++;
	}
	return sum;
}
//�ش���У���
u_short checkresum(u_short buf) {
	resum += buf;
	if (resum & 0xffff0000) {
		resum &= 0xffff;
		resum++;
	}
	return resum;
}
//�����ش���У���
u_short checkdsum(u_short buf) {
	dsum += buf;
	if (dsum & 0xffff0000) {
		dsum &= 0xffff;
		dsum++;
	}
	return dsum;
}

//���ַ����н�ȡstring
string words(char* sdata, int s_start, int s_end) {
	string sresult = "";
	for (int i = s_start; i <= s_end; i++) {
		sresult.push_back(sdata[i]);
	}
	return sresult;
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

//�ַ���ƴ��
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

//�жϳ�ʱ���߳�
DWORD WINAPI timeout(LPVOID lpParameter) {
	while (1) {
		if (close_resend == 1) {
			cout << "������ʱ�߳�" << endl;
			stop_timer();
			return 0;
		}
		time_now = clock();
		//�����ǰʱ���ȥ��ʼʱ����ڵ���0.5�룬���ش���base����ǰseq-1�����з���
		if ((time_now - time_start) / CLOCKS_PER_SEC >= 0.5 && timer_flag == 1) {
			cout << "��ʱ��" << endl;
			ssthresh = cwnd / 2;
			//��ֵ�����Ϊ0��������1
			if (ssthresh <= 0) {
				ssthresh = 1;
			}
			cwnd = 1;//�����������׶�
			dupackcount = 0;
			//�ش�
			resend_flag = 1;//�����ش�
			int reseq = (base % (maxseq + 1));
			int reseq_all = base;//��ģ��seq
			int reend_nextseq;//Ҫ�ط������һ����ŵ���һ�����
			if ((seq % (maxseq + 1)) == maxseq) {
				reend_nextseq = 0;
			}
			else {
				reend_nextseq = (seq % (maxseq + 1)) + 1;
			}
			int resend_len = 0;
			int last = fileLen % 1024;
			int relength;//���ݳ���
			string resend_str = "";//����ƴ�����ݱ�
			for (int k = baseflags[reseq]; reseq != reend_nextseq && k < fileLen; k += 1024) {
				if (reseq_all < base + cwnd) {
					char* resend_content;//���ڴ�Ŵ��ļ��ж���������
					char* resend_buf = NULL;//�ط�������
					//��ȡ�ļ�����
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
					resum = 0;//��ʼ��У���Ϊ0
					resend_content[relength] = '\0';

					//�������к�
					bitset<16> reseq_b = bitset<16>(reseq);//16λ2����
					string reseq_bstr = reseq_b.to_string();//���к�
					unsigned short reseq_us = strtol(reseq_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

					//�������ݱ��ĳ��ȣ��������ݱ��ĳ��ȣ�=���кţ�16��+Ŀ��andԴ�˿ں�(32)+У���(16)+����(16)+����
					int relen = 16 + 32 + 32 + relength + 1;
					bitset<16> relen_b = bitset<16>(relen);
					string relen_bstr = relen_b.to_string();
					unsigned short relen_us = strtol(relen_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

					//У���
					checkresum(reseq_us);//���к�
					checkresum(send_port_us);//Դ�˿ں�
					checkresum(recieve_port_us);//Ŀ�Ķ˿ں�
					checkresum(relen_us);//����

					u_short refinalflag;
					//���Ѿ������һ�����ݰ���
					if (k + 1024 >= fileLen) {
						refinalflag = 0;
					}
					//֮�������ݰ�
					else {
						refinalflag = 1;
					}
					checkresum(refinalflag);

					//���ݣ�2���ֽ�2���ֽڵ���
					for (int p = 0; p < relength; p += 2) {
						//���Ҫ�ҵ���һ���ֽڳ����˳��ȷ�Χ���򽫵�ǰ�ֽ�+0
						unsigned short resend_content_us = 0;
						if (p + 1 >= relength) {
							resend_content_us = ((unsigned char)resend_content[p]) << 8;
						}
						else {
							resend_content_us = (((unsigned char)resend_content[p]) << 8) + (unsigned char)resend_content[p + 1];
						}
						checkresum(resend_content_us);
					}
					//У���ȡ��
					resum = ~(resum & 0xffff);
					bitset<16> resum_b = bitset<16>(resum);
					string resum_str = resum_b.to_string();

					resend_str = reseq_bstr;//���к�
					resend_str.append(send_port_bstr);//Դ�˿ں�
					resend_str.append(recieve_port_bstr);//Ŀ�Ķ˿ں�
					resend_str.append(relen_bstr);//����
					resend_str.append(resum_str);//У���
					resend_str.push_back(refinalflag + '0');//���һ���ĵ���־λ
					char* resend_buf_head = (char*)resend_str.data();
					resend_buf = new char[relen + 1];
					resend_buf = char_strcat(resend_buf_head, resend_content, 81, relength, 81 + relength);//����

					resend_len = sendto(s_send, resend_buf, relen, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
					if (resend_len == SOCKET_ERROR) {
						cout << "����ʧ�ܣ� " << endl;
						break;
					}
					cout << "���ط�" << reseq << "���� " << endl;
					if (reseq == maxseq) {
						reseq = 0;
					}
					else {
						reseq++;
					}
					reseq_all++;
				}
				else {
					/*cout << "��ʱ�ش�����" << endl;*/
					k -= 1024;
				}
			}
			resend_flag = 0;
		}
	}
	return 0;
}

//�����ش��߳�
DWORD WINAPI quick_resend(LPVOID lpParameter) {
	resend_flag = 1;
	int dsend_len = 0;
	int last = fileLen % 1024;
	string dsend_str = "";//����ƴ�����ݱ�
	int dsendseq = (base % (maxseq + 1));//Ҫ�ش������(�ظ�ack�����+1)
	int df_flag = baseflags[dsendseq];//��ö�Ӧ���ݵ��ĵ��±�
	int dend_nextseq;//Ҫ�ط������һ����ŵ���һ�����
	if ((seq % (maxseq + 1)) == maxseq) {
		dend_nextseq = 0;
	}
	else {
		dend_nextseq = (seq % (maxseq + 1)) + 1;
	}
	int dlength;//���ݳ���
	//�ĵ�������±�
	int g;
	if (base == 0) {
		g = 0;
	}
	else {
		g = baseflags[dsendseq];
	}
	//�����ļ�
	for (; dsendseq != dend_nextseq && g < fileLen; g += 1024) {
		if (dsendseq < base + cwnd) {
			char* dsend_content;//���ڴ�Ŵ��ļ��ж���������
			char* dsend_buf = NULL;//�ط�������
			//��ȡ�ļ�����
			if (g + 1023 >= fileLen) {
				dsend_content = new char[last + 1];
				dsend_content = char_words(filecontent, g, g + last - 1);
				dlength = last;
			}
			else {
				dsend_content = new char[1025];
				dsend_content = char_words(filecontent, g, g + 1023);
				dlength = 1024;
			}
			dsum = 0;//��ʼ��У���Ϊ0
			dsend_content[dlength] = '\0';

			//�������к�
			bitset<16> dseq_b = bitset<16>(dsendseq);//16λ2����
			string dseq_bstr = dseq_b.to_string();//���к�
			unsigned short dseq_us = strtol(dseq_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

			//�������ݱ��ĳ��ȣ��������ݱ��ĳ��ȣ�=���кţ�16��+Ŀ��andԴ�˿ں�(32)+У���(16)+����(16)+����
			int dlen = 16 + 32 + 32 + dlength + 1;
			bitset<16> dlen_b = bitset<16>(dlen);
			string dlen_bstr = dlen_b.to_string();
			unsigned short dlen_us = strtol(dlen_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

			//У���
			checkdsum(dseq_us);//���к�
			checkdsum(send_port_us);//Դ�˿ں�
			checkdsum(recieve_port_us);//Ŀ�Ķ˿ں�
			checkdsum(dlen_us);//����

			u_short dfinalflag;
			//���Ѿ������һ�����ݰ���
			if (g + 1024 >= fileLen) {
				dfinalflag = 0;
			}
			//֮�������ݰ�
			else {
				dfinalflag = 1;
			}
			checkdsum(dfinalflag);

			//���ݣ�2���ֽ�2���ֽڵ���
			for (int p = 0; p < dlength; p += 2) {
				//���Ҫ�ҵ���һ���ֽڳ����˳��ȷ�Χ���򽫵�ǰ�ֽ�+0
				unsigned short dsend_content_us = 0;
				if (p + 1 >= dlength) {
					dsend_content_us = ((unsigned char)dsend_content[p]) << 8;
				}
				else {
					dsend_content_us = (((unsigned char)dsend_content[p]) << 8) + (unsigned char)dsend_content[p + 1];
				}
				checkdsum(dsend_content_us);
			}
			//У���ȡ��
			dsum = ~(dsum & 0xffff);
			bitset<16> dsum_b = bitset<16>(dsum);
			string dsum_str = dsum_b.to_string();

			dsend_str = dseq_bstr;//���к�
			dsend_str.append(send_port_bstr);//Դ�˿ں�
			dsend_str.append(recieve_port_bstr);//Ŀ�Ķ˿ں�
			dsend_str.append(dlen_bstr);//����
			dsend_str.append(dsum_str);//У���
			dsend_str.push_back(dfinalflag + '0');//���һ���ĵ���־λ
			char* dsend_buf_head = (char*)dsend_str.data();
			dsend_buf = new char[dlen + 1];
			dsend_buf = char_strcat(dsend_buf_head, dsend_content, 81, dlength, 81 + dlength);//����

			dsend_len = sendto(s_send, dsend_buf, dlen, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
			if (dsend_len == SOCKET_ERROR) {
				cout << "����ʧ�ܣ� " << endl;
				break;
			}
			cout << "�����ش�" << dsendseq << "���� " << endl;
			if (dsendseq == maxseq) {
				dsendseq = 0;
			}
			else {
				dsendseq++;
			}
		}
		else {
			g -= 1024;
		}
	}
	return 0;
}

//����ack�߳�
DWORD WINAPI recvack(LPVOID lpParameter) {
	int recvack_len = 0;
	char recvack_buf[21];//���ڴ�Ŵӽ��ն��յ���ack
	while (1) {
		//����ack
		recvack_len = recvfrom(s_send, recvack_buf, 20, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
		if (recvack_len == SOCKET_ERROR) {
			cout << "����ackʧ�ܣ� " << endl;
			continue;
		}
		recvack_buf[20] = '\0';
		string ack_seq_str = words(recvack_buf, 4, 19);
		int ack_seq = strtol(ack_seq_str.data(), NULL, 2);
		int ack_flag = recvack_buf[3] - '0';
		//�յ�ack
		string recvack_buf_content = words(recvack_buf, 0, 2);
		if ((!recvack_buf_content.compare("ack"))) {
			cout << "����" << ack_seq << "�յ�ack " << endl;
			//�ѷ��ͳɹ��������ݰ�
			if (ack_flag == 0) {
				close_resend = 1;//�رճ�ʱ�߳�
				stop_timer();
				cout << "����ack�߳�" << endl;
				return 0;
			}
			cout << "��ǰbaseΪ��" << (base % (maxseq+1)) << endl;
			//����յ�����ŵ���(base % 501) - 1�����ظ�ack����+1
			int temp = 0;
			if ((base % (maxseq + 1)) == 0) {
				temp = maxseq;
			}
			else {
				temp = (base % (maxseq + 1)) - 1;
			}
			//�յ��ظ�ack
			if (ack_seq ==temp) {
				cout << "�յ��ظ�ack" <<ack_seq<< endl;
				//�������ڿ����ش�״̬
				if (dupsend_flag == 1) {
					cwnd = cwnd + 1;
					cout << "�������ڿ����ش�״̬" << endl;
					continue;
				}
				else {
					dupackcount++;
					//����ظ�ack����Ϊ3�������������ش�
					if (dupackcount == 3) {
						//��������ش�״̬
						dupsend_flag = 1;
						ssthresh = cwnd / 2;
						if (ssthresh <= 0) {
							ssthresh = 1;
						}
						cwnd = ssthresh + 3;
						//�ر���һ�׶μ�ʱ
						stop_timer();
						//���������ش��߳�
						HANDLE hthread;
						hthread = CreateThread(NULL, 0, quick_resend, NULL, 0, NULL);
						//�ر��߳̾��
						CloseHandle(hthread);
						Sleep(5);
					}
					continue;
				}
			}
			//����յ������µ�ack
			else {
				//�����ش��׶�
				if (dupsend_flag == 1) {
					cwnd = ssthresh;
					dupackcount = 0;
					dupsend_flag = 0;//�˳������ش�״̬
					resend_flag = 0;//�˳��ش�״̬
					cout << "�˳������ش�" << endl;
				}
				else {
					//ӵ������׶�
					if (cwnd >= ssthresh) {
						cwnd = cwnd + (1 / cwnd);
						dupackcount = 0;
					}					
					//�������׶�
					else {
						cwnd = cwnd + 1;
						dupackcount = 0;
					}
				}
			}
			//�������һ���
			if ((base % (maxseq + 1)) > ack_seq) {
				base = base + (ack_seq + (maxseq + 1) - (base % (maxseq + 1))) + 1;
			}
			else {
				base = base + (ack_seq - (base % (maxseq + 1))) + 1;
			}
			if ((base % (maxseq + 1)) == (seq % (maxseq + 1))) {
				stop_timer();//������һ����ʱ
			}
			else {
				start_timer();//��ʼ��һ����ʱ
			}
		}
	}
	return 0;
}

int main() {
	int send_len = 0;
	int recv_len = 0;
	//�ļ�����ʱ���͵�����

	//��ʼ��socket
	initialize();
	//���ն�IP��ַ
	char recieve_IP[16];
	//���Ͷ�IP��ַ
	char send_IP[16];
	//���ڴ�Ŵӽ��ն��յ�������ack
	char connect_ack[20];

	//������ն�IP��ַ
	cout << "��������ն˵�IP��ַ��" << endl;
	cin >> recieve_IP;
	//���뷢�Ͷ�IP��ַ
	cout << "�����뷢�Ͷ˵�IP��ַ��" << endl;
	cin >> send_IP;
	//�����ն���Ϣ
	recieve_addr.sin_family = AF_INET;
	recieve_addr.sin_addr.S_un.S_addr = inet_addr(recieve_IP);
	recieve_addr.sin_port = htons(recieve_port);
	//��䷢�Ͷ���Ϣ
	send_addr.sin_family = AF_INET;
	send_addr.sin_addr.S_un.S_addr = inet_addr(send_IP);
	send_addr.sin_port = htons(send_port);

	//�����׽���
	s_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s_send == INVALID_SOCKET) {
		cout << "socket����ʧ�ܣ�������ϢΪ��" << WSAGetLastError() << endl;
		return 0;
	}
	//bind����һ�����ص�ַ�󶨵�ָ����socket
	if (bind(s_send, (SOCKADDR*)&send_addr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "�׽��ְ�ʧ�ܣ�������ϢΪ��" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else {
		cout << "�׽��ְ󶨳ɹ���" << endl;
	}

	//����������Ϣ
	string connect_str = "connect";
	int connect_len;
	connect_len = sendto(s_send, connect_str.data(), strlen(connect_str.data()), 0, (SOCKADDR*)&recieve_addr, sizeof(SOCKADDR));
	if (connect_len == SOCKET_ERROR) {
		cout << "����ʧ�ܣ�" << endl;
		return 0;
	}

	//�յ����ն˷�����������ȷ����Ϣ
	char send_ok[3];
	recv_len = recvfrom(s_send, send_ok, 3, 0, (SOCKADDR*)&recieve_addr, &socketaddr_len);
	if (recv_len == SOCKET_ERROR) {
		cout << "����ok��Ϣʧ�ܣ�" << endl;
		return 0;
	}
	send_ok[2] = '\0';
	if (strcmp(send_ok, "ok") == 0) {
		cout<<"���ӳɹ���"<<endl;
	}

	//�����ļ�
	while (1) {
		//���ڴ�С�������š���ֵ���ظ�ack����
		cwnd = 1;
		maxseq = 500;
		ssthresh = 20;
		dupackcount = 0;

		//�����ش�״̬��ʼ��Ϊ0
		dupsend_flag = 0;

		//���峤�ȱ���
		int send_len = 0;
		int length;//���ݳ���
		//���巢�ͻ�����
		string send_str = "";//����ƴ�����ݱ�

		//����Ҫ���͵��ļ���·��
		char* filepath = new char[50];
		cout << endl;
		cout << "������Ҫ������ļ���·����" << endl;
		cin >> filepath;
		//�رճ���
		if (strcmp(filepath, "close") == 0) {
			break;
		}
		//��ȡ�ļ�
		ifstream file(filepath, ios::in | ios::binary);
		if (!(file.is_open()))
		{
			return 0;
		}

		file.seekg(0, ios::end); //��λ����������λ��
		ios::pos_type endPos = file.tellg(); //��ȡ��������βָ��
		fileLen = static_cast<unsigned long>(endPos); //��ȡ�������ݴ�С

		file.seekg(0, ios::beg);//��λ��������ʼλ��
		filecontent = new char[fileLen];
		file.read(filecontent, fileLen);

		//base��seq
		base = 0;
		seq = 0;//��һ��Ҫ���͵����ݰ������к�
		f_flag = 0;

		//��Ƭ��������
		/* ���ݱ���ʽ��
		* ���к�:send_buf[0:15]
		* Դ�˿ںţ� send_buf[16:31]
		* Ŀ�Ķ˿ںţ�send_buf[32:47]
		* ���ȣ�send_buf[48:63]
		* У��ͣ�send_buf[64:79]
		* ���һ���ĵ���־λ��send_buf[80]
		* ���ݣ�send_buf[81:]
		*/
		int last = fileLen % 1024;
		//������ʱ�̺߳ͽ���ack�߳�
		close_resend = 0;
		HANDLE hThread[2];
		hThread[0] = CreateThread(NULL, 0, timeout, NULL, 0, NULL);
		hThread[1] = CreateThread(NULL, 0, recvack, NULL, 0, NULL);
		//�ر��߳̾��
		CloseHandle(hThread[0]);
		CloseHandle(hThread[1]);
		Sleep(5);
		file_begin = clock();

		for (f_flag = 0; f_flag < fileLen; f_flag += 1024) {
			//�����<base+���ڴ�С�Ҳ������ش�״̬ʱ��ƴ�����ݱ������ͣ���ʱ
			if (seq < base + cwnd && resend_flag==0) {
				cout << "��ǰcwndΪ��" << cwnd << endl;
				cout << "seqΪ��" << seq << endl;
				baseflags[seq % (maxseq + 1)] = f_flag;
				char* send_content;//���ڴ�Ŵ��ļ��ж���������
				char* send_buf = NULL;//���ͻ�����

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
				sum = 0;//��ʼ��У���Ϊ0

				send_content[length] = '\0';

				//�������к�
				bitset<16> seq_b = bitset<16>(seq % (maxseq + 1));//16λ2����
				string seq_bstr = seq_b.to_string();//���к�
				unsigned short seq_us = strtol(seq_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

				//�������ݱ��ĳ��ȣ��������ݱ��ĳ��ȣ�=���кţ�16��+Ŀ��andԴ�˿ں�(32)+У���(16)+����(16)+����+���һ�����ݰ���־λ
				int len = 16 + 32 + 32 + length + 1;
				bitset<16> len_b = bitset<16>(len);
				string len_bstr = len_b.to_string();
				unsigned short len_us = strtol(len_bstr.data(), NULL, 2);//10���ƣ�unsigned short����

				//У���
				checksum(seq_us);//���к�
				checksum(send_port_us);//Դ�˿ں�
				checksum(recieve_port_us);//Ŀ�Ķ˿ں�
				checksum(len_us);//����

				u_short finalflag;
				//���Ѿ������һ�����ݰ���
				if (f_flag + 1024 >= fileLen) {
					finalflag = 0;
				}
				//֮�������ݰ�
				else {
					finalflag = 1;
				}
				checksum(finalflag);

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

				//���ݱ�����ƴ��
				send_str = seq_bstr;//���к�
				send_str.append(send_port_bstr);//Դ�˿ں�
				send_str.append(recieve_port_bstr);//Ŀ�Ķ˿ں�
				send_str.append(len_bstr);//����
				send_str.append(sum_str);//У���
				send_str.push_back(finalflag + '0');//���һ���ĵ���־λ
				char* send_buf_head = (char*)send_str.data();
				send_buf = new char[len + 1];
				send_buf = char_strcat(send_buf_head, send_content, 81, length, 81 + length);//����

				send_len = sendto(s_send, send_buf, len, 0, (SOCKADDR*)&recieve_addr, sizeof(recieve_addr));
				if (send_len == SOCKET_ERROR) {
					cout << "����ʧ�ܣ�" << endl;
					break;
				}
				cout << "�ѷ���" << (seq % (maxseq + 1)) << "���� " << endl;
				cout << "���͵���" << seq << "������" << endl;
				if ((base % (maxseq + 1)) == (seq % (maxseq + 1))) {
					start_timer();
				}
				//�ĵ��Ѷ���ĩβ������յ������һ��ack�����ļ�������ϣ���������ʱ�ڴ�ͣ�����Ա��ط�
				while(f_flag + 1024 >= fileLen) {
					cout << "�ļ�������" << endl;
					if (close_resend == 1) {
						break;
					}
				}
				if (close_resend == 1) {
					break;
				}
				seq++;
			}
			//ָ�������һ�����ݶ�λ��
			else {
				f_flag -= 1024;
			}
		}
		while (close_resend != 1) {}
		file_end = clock();
		timer_flag = 0;
		cout << "��ǰ�ļ��ѷ��ͳɹ�" << endl;
		int trans_time = (file_end - file_begin) / CLOCKS_PER_SEC;
		cout << "����ʱ��Ϊ" << trans_time << "��" << endl;
		cout << "ƽ��������Ϊ" << (fileLen / 1000) / trans_time << "Kbps" << endl;

	}
	//�ر�socket
	closesocket(s_send);
	//�ͷ���Դ
	WSACleanup();
	return 0;
}