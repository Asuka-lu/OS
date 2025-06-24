#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<algorithm>
#include<conio.h>
#include<fstream>
#include<sstream>
#include<iomanip>
#include<windows.h>
#include<map>
#include<vector>
using namespace std;
#pragma warning(disable: 4996)

#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define PORT 65432
#define totalsize 1024*1024//�ܴ��̿ռ�Ϊ1M
#define disksize 1024//���̿��С1K
#define disknum 1024//���̿�1K
#define max_direct_num 7//�����Ŀ¼��max subdirect
#define fatsize disknum*sizeof(fatItem)//FAT���С
#define user_root_block fatsize/disksize+1//�û���Ŀ¼��ʼ�̿��
#define user_root_size sizeof(direct)//�û���Ŀ¼��С
#define max_filesize 100//����ļ�����

typedef struct FCB {
	char fileName[20];//�ļ���Ŀ¼��
	int type;// 0--�ļ�  1--Ŀ¼ 
	char* content = NULL;//�ļ�����
	int size;//��С
	int firstDisk;//��ʼ�̿�
	int sign;//0--��ͨĿ¼  1--��Ŀ¼
	int oflag;//0--δ��״̬  1--��״̬
	int lock;//0--δ����   1--�Ѽ���
	int fseek;//�ļ���дָ����ƶ������ļ�ָ�뵱ǰλ�ô�����ƶ� offset������ʱ��ǰ�ƶ�offset
}FCB;
/*Ŀ¼*/
struct direct {
	FCB directItem[max_direct_num + 2];
};
/*�ļ������*/
typedef struct fatItem {
	int item;//����ļ���һ�����̵�ָ��
	int state;//0-- ����  1--��ռ��
}fatItem, * FAT;
map<SOCKET, string> sockets;
void Cleanup(SOCKET socket)
{
	closesocket(socket);
	WSACleanup();
}

void sepStr(string& command, string& info, string receiveinfo)
{
	int i;
	int len = receiveinfo.length();
	for (i = 0; i < len; i++)
		if (receiveinfo[i] == '-')break;
	command = receiveinfo.substr(0, i);
	if (i != len - 1)
		info = receiveinfo.substr(i + 1, len);
	else
		info = "";
}
void Sendtoclient(SOCKET clientSocket, string str)
{
	string message = str;
	int size = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
}

void Readwait(SOCKET clientSocket)
{
	char buffer[1000];
	int receivedBytes = recv(clientSocket, buffer, sizeof(buffer), 0);
}

/*�����������*/
int save(string userDat, char*& fdisk)
{
	ofstream fp;
	fp.open(userDat, ios::out | ios::binary);
	if (fp) {
		fp.write(fdisk, totalsize * sizeof(char));
		fp.close();
		return 1;
	}
	else {
		fp.close();
		return 0;

	}
}

void init(SOCKET clientSocket, string& userDat, char*& fdisk, FAT& fat, direct*& root, int flag)
{
	fdisk = new char[totalsize * sizeof(char)];//������̿ռ�
	//��ʼ��FAT��
	fat = (FAT)(fdisk + disksize);//fat��ַ
	fat[0].item = -1; fat[0].state = 1;//��ʼ��FAT��
	for (int i = 1; i < user_root_block - 1; i++) {
		fat[i].item = i + 1;
		fat[i].state = 1;
	}
	fat[user_root_block].item = -1;
	fat[user_root_block].state = 1;//��Ŀ¼���̿��
	//fat�������ʼ��
	for (int i = user_root_block + 1; i < disknum; i++) {
		fat[i].item = -1;
		fat[i].state = 0;
	}
	//��Ŀ¼��ʼ��
	root = (struct direct*)(fdisk + disksize + fatsize);//��Ŀ¼��ַ
	root->directItem[0].sign = 1;
	root->directItem[0].firstDisk = user_root_block;
	strcpy(root->directItem[0].fileName, ".");
	root->directItem[0].type = 1;
	root->directItem[0].size = user_root_size;
	root->directItem[0].oflag = 0;
	root->directItem[0].lock = 0;
	root->directItem[0].fseek = 0;
	//��Ŀ¼��ʼ��
	root->directItem[1].sign = 1;
	root->directItem[1].firstDisk = user_root_block;
	strcpy(root->directItem[1].fileName, "..");
	root->directItem[1].type = 1;
	root->directItem[1].size = user_root_size;
	root->directItem[1].oflag = 0;
	root->directItem[1].lock = 0;
	root->directItem[1].fseek = 0;
	//��Ŀ¼��ʼ��
	for (int i = 2; i < max_direct_num + 2; i++) {
		root->directItem[i].sign = 0;
		root->directItem[i].firstDisk = -1;
		strcpy(root->directItem[i].fileName, "");
		root->directItem[i].type = 0;
		root->directItem[i].size = 0;
		root->directItem[i].oflag = 0;
		root->directItem[i].lock = 0;
		root->directItem[i].fseek = 0;
	}

	if (!save(userDat, fdisk))
	{
		if (flag)
		{
			string message = "��ʼ��ʧ�ܣ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}
	else
	{
		if (flag)
		{
			string message = "��ʼ���ɹ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}

}

/*��ȡ�û�������������*/
void readUserDisk(SOCKET clientSocket, string userDat, char*& fdisk, FAT& fat, direct*& root, direct*& curDir, string& curPath, string userName)
{
	fdisk = new char[totalsize];//������̴�С������
	ifstream fp;
	fp.open(userDat, ios::in | ios::binary);
	if (fp)
		fp.read(fdisk, totalsize);
	else
	{
		string message = userDat + "��ʧ�ܣ�";
		Sendtoclient(clientSocket, message);
	}
	fp.close();
	fat = (FAT)(fdisk + disksize);//fat��ַ
	root = (struct direct*)(fdisk + disksize + fatsize);//��Ŀ¼��ַ
	curDir = root;
	curPath = userName + ":\\";
}
int create(SOCKET clientSocket, string str, direct* curDir, FAT fat)
{
	char fName[20];
	strcpy(fName, str.c_str());
	//����ļ����Ƿ�Ϸ�
	if (!strcmp(fName, ""))return 0;
	if (str.length() > 20) {
		string message = "�ļ���̫�������������룡";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ����ļ�����
	for (int pos = 2; pos < max_direct_num + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			string message = "��ǰĿ¼���Ѵ����ļ�����!";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < max_direct_num + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= max_direct_num + 2) {
		string message = "��ǰĿ¼�¿ռ�����";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = user_root_block + 1; j < disknum; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= disknum) {
		string message = "�޿����̿�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//�ļ���ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	curDir->directItem[i].oflag = 0;
	curDir->directItem[i].lock = 0;
	curDir->directItem[i].fseek = 0;
	return 1;
}

int mkdir(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char*& fdisk)
{
	char fName[20];
	strcpy(fName, str.c_str());
	//����ļ����Ƿ�Ϸ�
	if (!strcmp(fName, ""))return 0;
	if (str.length() > 20) {
		string message = "�ļ���̫�������������룡";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//�ҵ�ǰĿ¼���Ƿ����ļ�����
	for (int pos = 2; pos < max_direct_num + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			string message = "��ǰĿ¼���Ѵ����ļ�����!";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//��鵱ǰĿ¼�¿ռ��Ƿ�����
	int i;
	for (i = 2; i < max_direct_num + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= max_direct_num + 2) {
		string message = "��ǰĿ¼�¿ռ�����";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//����Ƿ��п��д��̿�
	int j;
	for (j = user_root_block + 1; j < disknum; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= disknum) {
		string message = "�޿����̿�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//�ļ���ʼ��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].type = 1;
	curDir->directItem[i].size = user_root_size;


	direct* cur_mkdir = (direct*)(fdisk + curDir->directItem[i].firstDisk * disksize);//����Ŀ¼�������ַ
	//ָ��ǰĿ¼��Ŀ¼��
	cur_mkdir->directItem[0].sign = 0;
	cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
	strcpy(cur_mkdir->directItem[0].fileName, ".");
	cur_mkdir->directItem[0].type = 1;
	cur_mkdir->directItem[0].size = user_root_size;
	//ָ����һ��Ŀ¼��Ŀ¼��
	cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
	cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
	strcpy(cur_mkdir->directItem[1].fileName, "..");
	cur_mkdir->directItem[1].type = 1;
	cur_mkdir->directItem[1].size = user_root_size;
	//��Ŀ¼��ʼ��
	for (int i = 2; i < max_direct_num + 2; i++) {
		cur_mkdir->directItem[i].sign = 0;
		cur_mkdir->directItem[i].firstDisk = -1;
		strcpy(cur_mkdir->directItem[i].fileName, "");
		cur_mkdir->directItem[i].type = 0;
		cur_mkdir->directItem[i].size = 0;
	}
	return 1;
}
int rmdir(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* fdisk)
{
	int j, i;
	char fName[20];
	strcpy(fName, str.c_str());
	for (i = 2; i < max_direct_num + 2; i++)
	{
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 1)
			break;
	}
	if (i >= max_direct_num + 2)
	{
		string message = "�Ҳ�����Ŀ¼";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}

	direct* temp = (direct*)(fdisk + curDir->directItem[i].firstDisk * disksize);
	for (j = 2; j < max_direct_num + 2; j++)
	{
		//��Ŀ¼������Ŀ¼���ļ���ɾ�����ļ�����Ŀ¼
		if (temp->directItem[j].firstDisk != -1)
		{
			int firstblock = temp->directItem[j].firstDisk;
			while (firstblock != -1)
			{
				int p = fat[firstblock].item;
				fat[firstblock].item = -1;
				fat[firstblock].state = 0;
				firstblock = p;
			}
			//�ͷ�Ŀ¼��
			temp->directItem[j].sign = 0;
			temp->directItem[j].firstDisk = -1;
			strcpy(temp->directItem[j].fileName, "");
			temp->directItem[j].type = 0;
			temp->directItem[j].size = 0;
			temp->directItem[j].oflag = 0;
			temp->directItem[j].lock = 0;
		}
	}
	fat[curDir->directItem[i].firstDisk].state = 0;
	fat[curDir->directItem[i].firstDisk].item = -1;

	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	curDir->directItem[j].oflag = 0;
	curDir->directItem[j].lock = 0;
	return 1;
}
void cd(SOCKET clientSocket, string str, string& curPath, direct*& curDir, direct*& root, string userName, char*& fdisk)
{
	char fName[20]; strcpy(fName, str.c_str());
	string objPath = str;//Ŀ��·��
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1; int end = 0;//��\Ϊ�ָÿ����ʼ�±�ͽ����±�
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//�����л�ǰ��·��
	string temp;
	for (i = 0; i < len; i++) {
		//���Ŀ��·����\��ͷ����Ӹ�Ŀ¼��ʼ��ѯ
		if (objPath[0] == '\\') {
			tempDir = root;
			curPath = userName + ":\\";
			continue;
		}
		if (start == -1)
			start = i;
		if (objPath[i] == '\\') {
			end = i;
			temp = objPath.substr(start, end - start);
			//���Ŀ¼�Ƿ����
			for (j = 0; j < max_direct_num + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= max_direct_num + 2) {
				curPath = oldPath;
				string message = "�Ҳ�����Ŀ¼";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
				return;
			}
			//���Ŀ��·��Ϊ".."���򷵻���һ��
			if (temp == "..") {
				//�����ڸ�Ŀ¼����cd ..�����˳��û�Ŀ¼
				if (tempDir->directItem[0].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//���Ŀ��·��Ϊ"."����ָ��ǰĿ¼
			else {
				if (temp != ".")
					curPath = curPath + objPath.substr(start, end - start) + "\\";
			}
			start = -1;
			tempDir = (direct*)(fdisk + tempDir->directItem[j].firstDisk * disksize);
		}
	}
	string message = "���ҳɹ�";
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
	curDir = tempDir;
}
int cd_copy(SOCKET clientSocket, string str, string& curPath, direct*& curDir, direct*& root, string userName, char*& fdisk)
{
	char fName[20]; strcpy(fName, str.c_str());
	string objPath = str;//Ŀ��·��
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1; int end = 0;//��\Ϊ�ָÿ����ʼ�±�ͽ����±�
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//�����л�ǰ��·��
	string temp;
	for (i = 0; i < len; i++) {
		//���Ŀ��·����\��ͷ����Ӹ�Ŀ¼��ʼ��ѯ
		if (objPath[0] == '\\') {
			tempDir = root;
			curPath = userName + ":\\";
			continue;
		}
		if (start == -1)
			start = i;
		if (objPath[i] == '\\') {
			end = i;
			temp = objPath.substr(start, end - start);
			//���Ŀ¼�Ƿ����
			for (j = 0; j < max_direct_num + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= max_direct_num + 2) {
				curPath = oldPath;
				string message = "�Ҳ�����Ŀ¼";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
				return 0;
			}
			//���Ŀ��·��Ϊ".."���򷵻���һ��
			if (temp == "..") {
				//�����ڸ�Ŀ¼����cd ..�����˳��û�Ŀ¼
				if (tempDir->directItem[j - 1].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//���Ŀ��·��Ϊ"."����ָ��ǰĿ¼
			else {
				if (temp != ".")
					curPath = curPath + objPath.substr(start, end - start) + "\\";
			}
			start = -1;
			tempDir = (direct*)(fdisk + tempDir->directItem[j].firstDisk * disksize);
		}
	}
	curDir = tempDir;
	return 1;
}
int deletes(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int move)
{

	char fName[20];
	strcpy(fName, str.c_str());
	//�ڸ�Ŀ¼����Ŀ���ļ�
	int i, temp;
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!move)
	{
		if (i >= max_direct_num + 2) {
			string message = "�Ҳ������ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].oflag == 1)
		{
			string message = "�ļ��Ѵ򿪣����ȹرո��ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "�ļ��Ѽ��������Ƚ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	int item = curDir->directItem[i].firstDisk;//Ŀ���ļ�����ʼ���̿��
	//�����ļ����������ɾ���ļ���ռ�ݵĴ��̿�
	while (item != -1)
	{
		temp = fat[item].item;
		fat[item].item = -1;
		fat[item].state = 0;
		item = temp;
	}
	//�ͷ�Ŀ¼��
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = -1;
	strcpy(curDir->directItem[i].fileName, "");
	curDir->directItem[i].type = 0;
	curDir->directItem[i].size = 0;
	curDir->directItem[i].oflag = 0;
	curDir->directItem[i].lock = 0;
	curDir->directItem[i].fseek = 0;
	return 1;
}

int open(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//����ļ����Ƿ�Ϸ�
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "�Ҳ������ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "�ļ��Ѽ��������Ƚ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}

	//��״̬��ֵ1
	curDir->directItem[i].oflag = 1;
	return 1;
}
int close(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//����ļ����Ƿ�Ϸ�
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "�Ҳ������ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "�ļ��Ѽ��������Ƚ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//��״̬��ֵ0
	curDir->directItem[i].oflag = 0;
	return 1;
}
int lock(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//����ļ����Ƿ�Ϸ�
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "�Ҳ������ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].oflag == 1)
		{
			string message = "�ļ��Ѵ򿪣����ȹر�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}

	//ͨ��һ������������/����
	if (curDir->directItem[i].lock == 0)
	{
		curDir->directItem[i].lock = 1;
		return 1;
	}

	else
	{
		curDir->directItem[i].lock = 0;
		return 2;
	}

}
int lseek(SOCKET clientSocket, string str, int offset, direct*& curDir, FAT& fat)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//����ļ����Ƿ�Ϸ�
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= max_direct_num + 2) {
		string message = "�Ҳ������ļ�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "�ļ��Ѽ��������Ƚ���";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//offset�����ƶ�����
	curDir->directItem[i].fseek += offset;
	if (curDir->directItem[i].fseek > curDir->directItem[i].size)
		curDir->directItem[i].fseek = curDir->directItem[i].size;
	if (curDir->directItem[i].fseek < 0)
		curDir->directItem[i].fseek = 0;
	string message = str + "��fseek�ƶ��ɹ�����ǰfseekֵΪ��" + to_string(curDir->directItem[i].fseek);
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
	return 1;
}
void read(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char*& content, char* fdisk, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	//�ڵ�ǰĿ¼�²���Ŀ���ļ�
	int i, j;
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "�Ҳ������ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}

		if (curDir->directItem[i].oflag == 0)
		{
			string message = "���ȴ򿪸��ļ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "�ļ��Ѽ��������Ƚ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}
	}


	int curflag = i;
	int fSize = curDir->directItem[curflag].size;//Ŀ���ļ���С
	if (fSize <= 0)
	{
		if (!flag)
		{
			string message = "-@not";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}
		else {
			content[0] = '\0';
			return;
		}
	}
	int firstblock = curDir->directItem[curflag].firstDisk;//Ѱ��Ŀ���ļ�����ʼ���̿��
	char* start = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//������ļ�����ʼ��ַ
	int curread = 0;
	int needDisk = fSize / disksize;//ռ�ݵĴ��̿�����
	int needRes = fSize % disksize;//ռ�����һ����̿�Ĵ�С
	if (needRes > 0)needDisk += 1;
	if (curDir->directItem[curflag].fseek != curDir->directItem[curflag].size)
	{
		for (j = 0; j < needDisk; j++) {
			if (j == needDisk - 1) {
				for (int k = 0; k < needRes; k++)
					content[curread++] = start[k];
			}
			else {
				for (int k = 0; k < disksize; k++)
					content[curread++] = start[k];
				firstblock = fat[firstblock].item;
				start = fdisk + firstblock * disksize;
			}
		}

	}
	else
	{
		string message = "-@notseek";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	content[curread] = '\0';
	if (!flag)//������ǽ�ʹ�ù��ܾ͸�content����ȥ
	{
		Sendtoclient(clientSocket, content);
		Readwait(clientSocket);
	}

}
void write(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* content, int size, char* fdisk, int curflag)
{
	int i, j;
	int fSize = curDir->directItem[curflag].size;//Ŀ���ļ���С
	int firstblock = curDir->directItem[curflag].firstDisk;
	int lastblock = firstblock;//���Ѱ��Ŀ���ļ��������̿��
	while (fat[lastblock].item != -1)
		lastblock = fat[lastblock].item;//���㱣����ļ������һ���̿��
	char* last = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//������ļ���ָ���ַ
	char* temp = fdisk + firstblock * disksize + fSize % disksize;//�����ļ�ĩ��ַ
	//����̿�ʣ�ಿ�ֹ�д����ֱ��д��ʣ�ಿ��
	if (disksize - fSize % disksize > size) {
		char* tempcontent = new char[100];
		if (curDir->directItem[curflag].fseek != curDir->directItem[curflag].size)
		{
			read(clientSocket, str, curDir, fat, tempcontent, fdisk, 1);
			strcpy(last, content);
			last += size;
			strcpy(last, tempcontent);
		}
		else
			strcpy(last, content);
		curDir->directItem[curflag].size += size;
		delete[] tempcontent;
	}
	//����̿�ʣ�ಿ�ֲ���д�����ҵ����д��̿�д��
	else {
		char totalcontent[2000];
		int tempsize = 0;
		if (curDir->directItem[curflag].fseek != curDir->directItem[curflag].size)
		{
			char* tempcontent = new char[1000];
			read(clientSocket, str, curDir, fat, tempcontent, fdisk, 1);
			tempsize = strlen(tempcontent);
			strcpy(totalcontent, content);
			strcat(totalcontent, tempcontent);
			delete[] tempcontent;
		}
		else
			strcpy(totalcontent, content);


		//�Ƚ���ʼ����ʣ�ಿ��д��
		for (j = 0; j < disksize - fSize % disksize + tempsize; j++) {
			last[j] = totalcontent[j];
		}

		int curwrite = j + 1;
		int last_size = strlen(totalcontent) - (disksize - fSize % disksize + tempsize);//ʣ��Ҫд�����ݴ�С

		int needDisk = last_size / disksize;//ռ�ݵĴ��̿�����

		int needRes = last_size % disksize;//ռ�����һ����̿�Ĵ�С
		if (needRes > 0)needDisk += 1;
		for (j = 0; j < needDisk; j++) {

			if (fat[curDir->directItem[curflag].firstDisk].item != -1)
			{
				i = fat[curDir->directItem[curflag].firstDisk].item;
				last = fdisk + i * disksize;//���д�����ʼ�������ַ
				//��д�����һ����̣���ֻдʣ�ಿ������
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
			}
			else {
				for (i = user_root_block + 1; i < disknum; i++)
					if (fat[i].state == 0) break;
				if (i >= disknum) {
					string message = "�����ѱ�������";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return;
				}
				last = fdisk + i * disksize;//���д�����ʼ�������ַ
				//��д�����һ����̣���ֻдʣ�ಿ������
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				//�޸��ļ����������
				fat[lastblock].item = i;
				fat[i].state = 1;
				fat[i].item = -1;
			}
		}
		curDir->directItem[curflag].size += size;
	}
	string message = "д����ɣ�";
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
}
int write_copy(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* content, int size, char* fdisk, int curflag)
{
	int i, j;
	int fSize = curDir->directItem[curflag].size;//Ŀ���ļ���С
	int firstblock = curDir->directItem[curflag].firstDisk;
	int lastblock = firstblock;//���Ѱ��Ŀ���ļ��������̿��
	while (fat[lastblock].item != -1)
		lastblock = fat[lastblock].item;//���㱣����ļ������һ���̿��
	char* last = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//������ļ���ָ���ַ
	char* temp = fdisk + firstblock * disksize + fSize % disksize;//�����ļ�ĩ��ַ
	//����̿�ʣ�ಿ�ֹ�д����ֱ��д��ʣ�ಿ��
	if (disksize - fSize % disksize > size) {
		char* tempcontent = new char[100];
		if (curDir->directItem[curflag].fseek != curDir->directItem[curflag].size)
		{
			read(clientSocket, str, curDir, fat, tempcontent, fdisk, 1);
			strcpy(last, content);
			last += size;
			strcpy(last, tempcontent);
		}
		else
			strcpy(last, content);
		curDir->directItem[curflag].size += size;
		delete[] tempcontent;
		return 1;
	}
	//����̿�ʣ�ಿ�ֲ���д�����ҵ����д��̿�д��
	else {
		char totalcontent[2000];
		int tempsize = 0;
		if (curDir->directItem[curflag].fseek != curDir->directItem[curflag].size)
		{
			char* tempcontent = new char[1000];
			read(clientSocket, str, curDir, fat, tempcontent, fdisk, 1);
			tempsize = strlen(tempcontent);
			strcpy(totalcontent, content);
			strcat(totalcontent, tempcontent);
			delete[] tempcontent;
		}
		else
			strcpy(totalcontent, content);


		//�Ƚ���ʼ����ʣ�ಿ��д��
		for (j = 0; j < disksize - fSize % disksize + tempsize; j++) {
			last[j] = totalcontent[j];
		}

		int curwrite = j + 1;
		int last_size = strlen(totalcontent) - (disksize - fSize % disksize + tempsize);//ʣ��Ҫд�����ݴ�С

		int needDisk = last_size / disksize;//ռ�ݵĴ��̿�����

		int needRes = last_size % disksize;//ռ�����һ����̿�Ĵ�С
		if (needRes > 0)needDisk += 1;
		for (j = 0; j < needDisk; j++) {

			if (fat[curDir->directItem[curflag].firstDisk].item != -1)
			{
				i = fat[curDir->directItem[curflag].firstDisk].item;
				last = fdisk + i * disksize;//���д�����ʼ�������ַ
				//��д�����һ����̣���ֻдʣ�ಿ������
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
			}
			else {
				for (i = user_root_block + 1; i < disknum; i++)
					if (fat[i].state == 0) break;
				if (i >= disknum) {
					string message = "�����ѱ�������";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return 0;
				}
				last = fdisk + i * disksize;//���д�����ʼ�������ַ
				//��д�����һ����̣���ֻдʣ�ಿ������
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				//�޸��ļ����������
				fat[lastblock].item = i;
				fat[i].state = 1;
				fat[i].item = -1;
			}
		}
		curDir->directItem[curflag].size += size;
		return 1;
	}
}
void dir(SOCKET clientSocket, direct* curDir)
{
	for (int i = 2; i < max_direct_num + 2; i++) {
		if (curDir->directItem[i].firstDisk != -1) {
			string message = curDir->directItem[i].fileName;
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			if (curDir->directItem[i].type == 0) {//��ʾ�ļ�
				message = "<FILE>";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else {//��ʾĿ¼
				message = "<DIR>";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			message = to_string(curDir->directItem[i].firstDisk);
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);

			if (curDir->directItem[i].type == 1) {//��ʾ�ļ�
				message = "file";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else {//��ʾĿ¼
				message = to_string(curDir->directItem[i].size);
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			if (curDir->directItem[i].oflag)
			{
				message = "�Ѵ�";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else
			{
				message = "δ��";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			if (curDir->directItem[i].lock)
			{
				message = "�Ѽ���";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else
			{
				message = "δ����";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			message = to_string(curDir->directItem[i].fseek);
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
		else
		{
			string message = "not";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}

	}
}

void sendTree(SOCKET clientSocket, direct* dir, const string& prefix, char* fdisk)
{
	// �ռ���ǰĿ¼��������Ч��Ŀ
	vector<FCB*> items;
	for (int i = 2; i < max_direct_num + 2; ++i)
	{
		auto& f = dir->directItem[i];
		if (f.firstDisk != -1)
		{
			items.push_back(&f);
		}
	}

	// ����������״�ṹ����ÿһ��
	for (size_t k = 0; k < items.size(); ++k)
	{
		bool last = (k + 1 == items.size());
		string branch = prefix + (last ? "������ " : "������ ");
		FCB* f = items[k];

		// ������ʾ��
		string line = branch + f->fileName + (f->type == 1 ? "/" : "");
		line += (f->type == 0) ? " (FILE, Size: " + to_string(f->size) + " bytes)" : " (DIR)";

		// ���͸��ͻ��˲��ȴ� ack
		Sendtoclient(clientSocket, line);
		Readwait(clientSocket);

		// Ŀ¼��ݹ�
		if (f->type == 1)
		{
			direct* sub = (direct*)(fdisk + f->firstDisk * disksize);
			string childPrefix = prefix + (last ? "    " : "��   ");
			sendTree(clientSocket, sub, childPrefix, fdisk);
		}
	}
}

void copy(SOCKET clientSocket, string sourcefile, string destination, direct* curDir, string curPath, FAT& fat, char* fdisk, direct*& root, string userName)
{

	string source = sourcefile;//ԭ�ļ�
	string des = destination;//Ŀ���ļ�
	//���ԭ�ļ���Ŀ�ĵ�ַ�Ƿ���ڲ��Ϸ�
	if (source == "" || des == "") {
		string message = "�������";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	int i, j;
	for (i = 2; i < max_direct_num + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source.c_str()))
			break;
	}
	if (i >= max_direct_num + 2) {
		string message = "�Ҳ���ԭ�ļ�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].oflag == 1) {
		string message = "�ļ��Ѵ򿪣����ȹر�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "�ļ��Ѽ��������Ƚ���";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	bool isdir = 0;
	direct* cur = curDir;//���浱ǰĿ¼
	string oldPath = curPath;//���浱ǰ·��
	string newname;
	char* content = new char[100];
	//��ȡԴ�ļ�����
	open(clientSocket, source, curDir, fat, 1);
	read(clientSocket, source, curDir, fat, content, fdisk, 1);
	close(clientSocket, source, curDir, fat, 1);
	//����Ŀ���ļ���Ŀ¼·���л���ǰĿ¼
	if (des[des.length() - 1] == '\\') {
		string newpath;
		newname = source;
		newpath = des;
		if (!cd_copy(clientSocket, newpath, curPath, curDir, root, userName, fdisk))
		{
			curDir = cur;
			curPath = oldPath;
			delete[] content;
			return;
		}
	}
	else {
		//���Դ�ļ�Ŀ��·��Ϊabc\b,�϶�Ϊ����abc�¸���source������Ϊb
		string newpath;
		int pos = des.rfind('\\', des.length() - 1);
		if (pos != -1) {
			newname = des.substr(pos + 1, des.length() - 1 - pos);
			newpath = des.substr(0, pos + 1);
			if (!cd_copy(clientSocket, newpath, curPath, curDir, root, userName, fdisk))
			{
				curDir = cur;
				curPath = oldPath;
				delete[] content;
				return;
			}
		}
		//Ŀ��·��Ϊb����Ϊֱ���ڵ�ǰĿ¼����b���ֵ��ļ�
		else {
			newname = des;
		}
	}
	//���Ŀ¼�ҵõ�
	//���Ŀ��Ŀ¼������create�ᷢ�͡��Ѿ�������
	if (create(clientSocket, newname, curDir, fat)) {
		for (i = 2; i < max_direct_num + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, newname.c_str()) && curDir->directItem[i].type == 0)
				break;
		//�������������write�ᷢ�͡������Ѿ�����
		if (write_copy(clientSocket, newname, curDir, fat, content, strlen(content), fdisk, i))
		{
			string message = "���Ƴɹ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}

	curDir = cur;
	curPath = oldPath;
	delete[] content;
}
void move(SOCKET clientSocket, string sourcefile, string destination, direct* curDir, string curPath, FAT& fat, char* fdisk, direct*& root, string userName)
{

	string source = sourcefile;//ԭ�ļ�
	string des = destination;//Ŀ���ļ�
	//���ԭ�ļ���Ŀ�ĵ�ַ�Ƿ���ڲ��Ϸ�
	if (source == "" || des == "") {
		string message = "�������";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	int i, j;
	for (i = 2; i < max_direct_num + 2; i++) {
		if (!strcmp(curDir->directItem[i].fileName, source.c_str()))
			break;
	}
	if (i >= max_direct_num + 2) {
		string message = "�Ҳ���ԭ�ļ�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].oflag == 1) {
		string message = "�ļ��Ѵ򿪣����ȹر�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "�ļ��Ѽ��������Ƚ���";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	bool isdir = 0;
	direct* cur = curDir;//���浱ǰĿ¼
	string oldPath = curPath;//���浱ǰ·��
	string newname;
	char* content = new char[100];
	//��ȡԴ�ļ�����
	open(clientSocket, source, curDir, fat, 1);
	read(clientSocket, source, curDir, fat, content, fdisk, 1);
	close(clientSocket, source, curDir, fat, 1);
	//����Ŀ���ļ���Ŀ¼·���л���ǰĿ¼

	string newpath;
	newname = source;
	newpath = des;
	if (!cd_copy(clientSocket, newpath, curPath, curDir, root, userName, fdisk))
	{
		curDir = cur;
		curPath = oldPath;
		delete[] content;
		return;
	}

	//���Ŀ¼�ҵõ�
	//���Ŀ��Ŀ¼������create�ᷢ�͡��Ѿ�������
	if (create(clientSocket, newname, curDir, fat)) {
		for (i = 2; i < max_direct_num + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, newname.c_str()) && curDir->directItem[i].type == 0)
				break;
		//�������������write�ᷢ�͡������Ѿ�����
		if (write_copy(clientSocket, newname, curDir, fat, content, strlen(content), fdisk, i))
		{
			deletes(clientSocket, source, cur, fat, 1);

			string message = "�ƶ��ɹ�";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}

	curDir = cur;
	curPath = oldPath;
	delete[] content;
}

void head(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char*& content, char* fdisk, int num)
{
	read(clientSocket, str, curDir, fat, content, fdisk, 1);
	char head[2000];
	memset(head, 0, sizeof(head));
	int n = 0, z = 0, cnum = 0;
	for (int x = 0; x < strlen(content); x++)
	{
		if (content[x] == '\n')
			cnum++;
	}
	if (num >= cnum + 1)
	{
		Sendtoclient(clientSocket, content);
		Readwait(clientSocket);
		return;
	}
	if (num <= 0)
	{
		string mes = "������СΪ1";
		Sendtoclient(clientSocket, mes);
		Readwait(clientSocket);
		return;
	}

	while (n < num)
	{
		head[z] = content[z];
		z++;
		if (content[z] == '\n' || content[z] == '\0')
			n++;

	}
	head[z] = '\0';
	Sendtoclient(clientSocket, head);
	Readwait(clientSocket);
}

void tail(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char*& content, char* fdisk, int num)
{
	read(clientSocket, str, curDir, fat, content, fdisk, 1);
	char tail[2000];
	memset(tail, 0, sizeof(tail));

	int n = 0, z = 0, cnum = 0, index = 0;
	//��ͨ��n��zԽ��ǰ��ļ����س���z��Ϊ�±꾭��ѭ����Ϳ���ʹ��content[z]��ʾ���棬����index��tail�����0��ʼ��

	//��ͳ�������м���
	if (num <= 0)
	{
		string mes = "������СΪ1";
		Sendtoclient(clientSocket, mes);
		Readwait(clientSocket);
		return;
	}

	for (int x = 0; x < strlen(content); x++)
	{
		if (content[x] == '\n')
			cnum++;
	}
	while (n < cnum + 1 - num)
	{
		if (content[z] == '\n')
			n++;
		z++;
	}
	while (content[z] != '\0')
	{
		tail[index++] = content[z];
		z++;
	}
	tail[index] = '\0';
	Sendtoclient(clientSocket, tail);
	Readwait(clientSocket);

}

void import(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* fdisk)
{
	string filePath = str;
	char* content = new char[2000];//���û�����
	fstream fp;
	fp.open(filePath, ios::in);
	string buf;
	if (fp) {
		while (fp) {
			buf.push_back(fp.get());
		}
		strcpy(content, buf.c_str());
		fp.close();
		int pos = filePath.rfind('\\', filePath.length() - 1);
		string obj = filePath.substr(pos + 1, filePath.length() - pos - 1);
		string newStr = obj;
		if (create(clientSocket, newStr, curDir, fat))//��ȡ�����ļ����ݲ������ļ�д��
		{
			open(clientSocket, newStr, curDir, fat, 1);
			int i, j;
			for (i = 2; i < max_direct_num + 2; i++)
				if (!strcmp(curDir->directItem[i].fileName, newStr.c_str()) && curDir->directItem[i].type == 0)
					break;

			write_copy(clientSocket, newStr, curDir, fat, content, strlen(content) - 1, fdisk, i);

			close(clientSocket, newStr, curDir, fat, 1);
			string message = "�����ļ��ɹ���";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
		delete[] content;
	}
	else {
		string message = "�򿪱����ļ�ʧ�ܣ�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
}

void exportf(SOCKET clientSocket, string name, string filePath, direct*& curDir, FAT& fat, char* fdisk)
{
	char* content = new char[max_filesize];//���û�����
	if (!open(clientSocket, name, curDir, fat, 0))
		return;
	read(clientSocket, name, curDir, fat, content, fdisk, 1);//��ȡ�ļ�����
	close(clientSocket, name, curDir, fat, 1);
	fstream fp;

	int pos = filePath.rfind('\\', filePath.length() - 1);
	string obj = filePath.substr(pos + 1, filePath.length() - pos - 1);
	if (obj.length() <= 4 || obj[obj.length() - 4] != '.')
	{
		filePath += name;
	}

	fp.open(filePath, ios::out);
	if (fp) {
		fp << content;
		string message = "�����ļ��ɹ���";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
	else {
		string message = "�����ļ�ʧ�ܣ�";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
	delete[] content;
}

void HandleClientCommunication(SOCKET clientSocket)
{

	string userName;//��ǰ�û���
	FAT fat = nullptr;//FAT��
	direct* root = nullptr;//��Ŀ¼
	direct* curDir = nullptr;//��ǰĿ¼
	string curPath;//��ǰ·��
	string userDat;//��ǰ�ô����ļ�
	char* fdisk = nullptr;//���������ʼ��ַ
	int login = 0;
	while (true)
	{

		char buffer[1000];

		//��ͻ��˷��͵�ǰĿ¼
		string message = curPath;
		int size = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);

		memset(buffer, 0, sizeof(buffer));
		//�ӿͻ��˽�����Ϣ
		int receivedBytes = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (receivedBytes == SOCKET_ERROR || receivedBytes == 0)
		{
			cout << "�û�" << sockets[clientSocket] << "�Ѿ��˳���" << endl;
			sockets.erase(clientSocket);
			break;
		}

		else
		{
			string command, info;//�ֽ�Ϊ��������Ϣ
			sepStr(command, info, buffer);
			if (login)
			{
				direct* temp = curDir;
				string tempcurPath = curPath;
				readUserDisk(clientSocket, userDat, fdisk, fat, root, curDir, curPath, userName);
				if (temp->directItem[0].sign != 1)
				{
					curDir = temp;
					curPath = tempcurPath;
				}

			}
			if (command == "Login")
			{
				userName = info;
				sockets[clientSocket] = userName;

				userDat = userName + ".dat";
				fstream fp;
				fp.open(userDat, ios::in | ios::binary);
				if (!fp)
					init(clientSocket, userDat, fdisk, fat, root, 0);
				readUserDisk(clientSocket, userDat, fdisk, fat, root, curDir, curPath, userName);
				login = 1;
			}
			else if (command == "tree")
			{
				sendTree(clientSocket, curDir, "", fdisk);
				string message = "tree-end";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else if (command == "exit")
			{
				closesocket(clientSocket);
			}
			else if (command == "create")
			{
				if (create(clientSocket, info, curDir, fat))
				{
					message = "�ļ������ɹ���";
					int size = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "tree-command") {
				string message = "/ (�����Ŀ¼)";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);

				sendTree(clientSocket, root, "", fdisk);

				message = "end";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			else if (command == "dir")
			{
				dir(clientSocket, curDir);
			}
			else if (command == "mkdir")
			{
				if (mkdir(clientSocket, info, curDir, fat, fdisk))
				{
					{
						message = "Ŀ¼�����ɹ���";
						Sendtoclient(clientSocket, message);
						Readwait(clientSocket);
						save(userDat, fdisk);
					}
				}
			}

			else if (command == "rmdir")
			{
				if (rmdir(clientSocket, info, curDir, fat, fdisk))
				{
					{
						message = info + "ɾ���ɹ���";
						Sendtoclient(clientSocket, message);
						Readwait(clientSocket);
						save(userDat, fdisk);
					}
				}
			}
			else if (command == "cd")
			{
				cd(clientSocket, info, curPath, curDir, root, userName, fdisk);
			}
			else if (command == "open")
			{
				if (open(clientSocket, info, curDir, fat, 0))
				{
					string message = info + "�򿪳ɹ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "close")
			{
				if (close(clientSocket, info, curDir, fat, 0))
				{
					string message = info + "�رճɹ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "delete")
			{
				if (deletes(clientSocket, info, curDir, fat, 0))
				{
					message = info + "ɾ���ɹ���";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "write")
			{
				char fName[20];
				strcpy(fName, info.c_str());
				//�ڵ�ǰĿ¼�²���Ŀ���ļ�
				int i, j;
				for (i = 2; i < max_direct_num + 2; i++)
					if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
						break;
				if (i >= max_direct_num + 2) {
					string message = "�Ҳ������ļ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
				}
				else if (curDir->directItem[i].oflag == 0)
				{
					string message = "���ȴ򿪸��ļ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
				}
				else if (curDir->directItem[i].lock == 1)
				{
					string message = "�ļ��Ѽ��������Ƚ���";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return;
				}
				else {
					string message = "����д����ļ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					int curflag = i;
					char content[2000];
					memset(content, 0, sizeof(content));
					int receivedBytes = recv(clientSocket, content, sizeof(content), 0);
					int size = strlen(content);
					Sendtoclient(clientSocket, "ack");
					Readwait(clientSocket);
					write(clientSocket, info, curDir, fat, content, size, fdisk, curflag);
					save(userDat, fdisk);
				}

			}
			else if (command == "read")
			{
				char* content = new char[2000];
				read(clientSocket, info, curDir, fat, content, fdisk, 0);
			}
			else if (command == "copy")
			{
				string source, des;
				sepStr(source, des, info);
				char* content = new char[100];
				copy(clientSocket, source, des, curDir, curPath, fat, fdisk, root, userName);
				save(userDat, fdisk);
			}
			else if (command == "move")
			{
				string source, des;
				sepStr(source, des, info);
				char* content = new char[100];
				move(clientSocket, source, des, curDir, curPath, fat, fdisk, root, userName);
				save(userDat, fdisk);
			}
			else if (command == "lock")
			{
				int lockflag = lock(clientSocket, info, curDir, fat, 0);
				if (lockflag == 1)
				{
					string message = info + "�����ɹ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
				else if (lockflag == 2)
				{
					string message = info + "�����ɹ�";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "head")
			{
				string name, num;
				sepStr(name, num, info);
				char* content = new char[100];
				head(clientSocket, name, curDir, fat, content, fdisk, stoi(num));
			}
			else if (command == "tail")
			{
				string name, num;
				sepStr(name, num, info);
				char* content = new char[100];
				tail(clientSocket, name, curDir, fat, content, fdisk, stoi(num));
			}
			else if (command == "import")
			{
import(clientSocket, info, curDir,fat,fdisk);
				save(userDat, fdisk);
			}
			else if (command == "export")
			{
				string name, path;
				sepStr(name, path, info);
				exportf(clientSocket, name, path, curDir, fat, fdisk);
				save(userDat, fdisk);
			}
			else if (command == "lseek")
			{
				string name, offset;
				sepStr(name, offset, info);
				lseek(clientSocket, name, stoi(offset), curDir, fat);
				save(userDat, fdisk);
			}
			else if (command == "init")
			{
				init(clientSocket, userDat, fdisk, fat, root, 1);
				readUserDisk(clientSocket, userDat, fdisk, fat, root, curDir, curPath, userName);
			}
			else
			{
				message = "ָ����Ч��";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
				continue;
			}
		}
	}
	closesocket(clientSocket);
}

int main()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		cout << "����winsock.dllʧ�ܣ�" << endl;
		return 0;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);//�����׽��֣�ָ����ַ��(AF_INET)���׽�������(SOCK_STREAM)��Э��
	if (serverSocket == INVALID_SOCKET)
	{
		cout << "�����׽���ʧ�ܣ�������룺" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
	if (bind(serverSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cout << "��ַ��ʧ�ܣ�������룺" << WSAGetLastError() << endl;
		Cleanup(serverSocket);
		return 0;
	}
	//��socket����Ϊ����ģʽ
	if (listen(serverSocket, 0) == SOCKET_ERROR)
	{
		cout << "����ʧ�ܣ�������룺" << WSAGetLastError() << endl;
		Cleanup(serverSocket);
		return 0;
	}

	while (true)
	{
		sockaddr_in clientAddress;
		int clientAddressSize = sizeof(clientAddress);
		SOCKET clientSocket = accept(serverSocket, reinterpret_cast<SOCKADDR*>(&clientAddress), &clientAddressSize);
		if (clientSocket != INVALID_SOCKET)
		{
			thread clientThread(HandleClientCommunication, clientSocket);
			clientThread.detach(); // �����̣߳����ȴ��߳̽���
		}
	}

	Cleanup(serverSocket);
	return 0;
}
