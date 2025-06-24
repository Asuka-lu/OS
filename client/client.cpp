#include <winsock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<algorithm>
#include<conio.h>
#include<fstream>
#include<sstream>
#include<iomanip>
#include<windows.h>
#include<vector>
#pragma comment(lib, "ws2_32.lib")
#define PORT 65432

using namespace std;
vector<string> client;

string userLogin = "C:\\Users\\¬�ҬB\\Desktop\\����ϵͳ����\\client\\user.txt";//�û�Ŀ¼�����ڼ�¼�û���������
string userName, userPassword;//��ǰ�û���������

string curPath;//��ǰ·��
string userDat;//��ǰ�ô����ļ�
char* fdisk;//���������ʼ��ַ
char buffer[2000];

void Cleanup(SOCKET socket)
{
	closesocket(socket);
	WSACleanup();
}
void strip(string& str, char ch)
{
	int i = 0;
	while (str[i] == ch)i++;
	int j = str.size() - 1;
	while (str[j] == ch)j--;
	str = str.substr(i, j + 1 - i);
}
void halfStr(string& userName, string& password, string line)
{
	int i;
	int len = line.length();
	for (i = 0; i < len; i++)
		if (line[i] == ' ')break;
	userName = line.substr(0, i);
	password = line.substr(i + 1, len);
}

void sendtoserver(SOCKET clientSocket, string sendMessage)
{
	int sentBytes = send(clientSocket, sendMessage.c_str(), static_cast<int>(sendMessage.length()), 0);
	if (sentBytes == SOCKET_ERROR || sentBytes == 0)
	{
		cout << "[������Ϣʧ��] ������룺" << WSAGetLastError() << endl;
	}
}

void Readin(SOCKET clientSocket)
{
	memset(buffer, 0, sizeof(buffer));
	int size = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
}

void Sendwait(SOCKET clientSocket)
{
	string createinfo = "ack";
	sendtoserver(clientSocket, createinfo);
}

void help()
{
	cout << fixed << left;
	cout << endl << "\t" << "*------------------�ļ�ϵͳ-����˵�--------------------*" << endl;
	cout << "\t" << "|" << "\t" << 3 << "\t" << "mkdir (name)" << "\t" << "����Ŀ¼" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 4 << "\t" << "create (name)" << "\t" << "�����ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 5 << "\t" << "open (name)" << "\t" << "���ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 6 << "\t" << "close (name)" << "\t" << "�ر��ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 7 << "\t" << "read (name)" << "\t" << "���ļ�" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 8 << "\t" << "del (name)" << "\t" << "ɾ���ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 9 << "\t" << "remove(name)" << "\t" << "ɾ��Ŀ¼" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 10 << "\t" << "cd" << "\t" << "\t" << "�л�Ŀ¼" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 11 << "\t" << "dir" << "\t" << "\t" << "�г�Ŀ¼" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 12 << "\t" << "ls" << "\t" << "\t" << "�г��ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 13 << "\t" << "write" << "\t" << "\t" << "д�ļ�" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 14 << "\t" << "move" << "\t" << "\t" << "�ƶ��ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 15 << "\t" << "copy" << "\t" << "\t" << "�����ļ�" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 16 << "\t" << "lock(name)" << "\t" << "�ļ����� " << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 17 << "\t" << "head -num" << "\t" << "��ʾ�ļ���ǰnum��" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 18 << "\t" << "tail -num" << "\t" << "��ʾ�ļ�β���ϵ�num��" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 19 << "\t" << "lseek -offerset" << "\t" << "�ļ���дָ����ƶ�" << "\t" << "|" << endl;
	cout << "\t" << "|  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - |" << endl;
	cout << "\t" << "|" << "\t" << 20 << "\t" << "import" << "\t" << "\t" << "���뵽��ǰĿ¼" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 21 << "\t" << "export" << "\t" << "\t" << "������Ŀ�ĵ�ַ" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 22 << "\t" << "clear" << "\t" << "\t" << "����" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 23 << "\t" << "help" << "\t" << "\t" << "��ʾ����" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 24 << "\t" << "exit" << "\t" << "\t" << "�˳�ϵͳ" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 25 << "\t" << "tree" << "\t" << "\t" << "��״ͼ" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "*-------------------------------------------------------*" << endl << endl;
}


void menu(SOCKET clientSocket)
{
	system("cls"); getchar();

	string logininfo = "Login-" + userName;

	sendtoserver(clientSocket, logininfo);

	while (true)
	{
		Readin(clientSocket);
		curPath = buffer;

		cout << curPath << ">";

		string receivedMessage;

		string sendMessage;

		getline(cin, sendMessage);
		stringstream stream;
		stream.str(sendMessage);
		string str[4];
		for (int i = 0; stream >> str[i]; i++);

		if (str[0] == "exit")
		{
			exit(0);
		}
		else if (str[0] == "tree")
		{
			string createinfo = str[0] + "-" + "1";
			sendtoserver(clientSocket, createinfo);
			cout << "Tree structure of directory:" << endl;
			while (true)
			{
				Readin(clientSocket);
				if (strcmp(buffer, "tree-end") == 0)
				{
					sendtoserver(clientSocket, "ack"); // ���� ack ��ʾ����
					break;
				}
				cout << buffer << endl;
				sendtoserver(clientSocket, "ack"); // ���� ack ��ʾ�յ�
			}
		}
		else if (str[0] == "create")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "delete")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "dir")
		{
			string createinfo = str[0] + "-" + "1";
			sendtoserver(clientSocket, createinfo);
			cout << fixed << left << setw(20) << "�ļ���" << setw(10) << "����" << setw(20) << "��ʼ���̿��" << setw(10) << "��С" << setw(10) << "��״̬" << setw(10) << "����״̬" << setw(10) << "lseekָ��λ��" << endl;
			for (int i = 2; i < 9; i++) {
				Readin(clientSocket);
				Sendwait(clientSocket);
				if (strcmp(buffer, "not") != 0)
				{
					cout << fixed << setw(20) << buffer;

					Readin(clientSocket);
					Sendwait(clientSocket);
					cout << fixed << setw(10) << buffer;

					Readin(clientSocket);
					Sendwait(clientSocket);
					cout << setw(20) << buffer;

					Readin(clientSocket);
					Sendwait(clientSocket);
					if (strcmp(buffer, "file") != 0)
						cout << setw(10) << buffer;
					else
						cout << setw(10) << "";
					Readin(clientSocket);
					Sendwait(clientSocket);
					cout << setw(10) << buffer;

					Readin(clientSocket);
					Sendwait(clientSocket);
					cout << setw(10) << buffer;

					Readin(clientSocket);
					Sendwait(clientSocket);
					cout << setw(10) << buffer << endl;

				}
			}

		}
		else if (str[0] == "mkdir")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "rmdir")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "cd")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			if (!strcmp(buffer, "�Ҳ�����Ŀ¼"))
				cout << buffer << endl;
		}
		else if (str[0] == "open")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "close")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "read")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			if (!strcmp(buffer, "-@not"))
			{
				cout << "�ļ�Ϊ�գ�" << endl;
			}
			else if (!strcmp(buffer, "-@notseek"))
			{
				cout << "�ļ�ָ��֮��Ϊ������" << endl;
			}
			else if (!strcmp(buffer, "�Ҳ������ļ�") || !strcmp(buffer, "���ȴ򿪸��ļ�") || !strcmp(buffer, "�ļ��Ѽ��������Ƚ���"))
			{
				cout << buffer << endl;
			}
			else
			{
				cout << "--------------�ļ����ݣ�--------------" << endl;
				cout << buffer << endl;
			}
			Sendwait(clientSocket);
		}

		else if (str[0] == "write")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			if (strcmp(buffer, "����д����ļ�") != 0)
			{
				cout << buffer << endl;
			}
			else {
				cout << "�������ļ����ݣ�����'#'Ϊ������־" << endl;
				char ch;
				string content;
				int size = 0;
				while (1)
				{
					ch = getchar();
					if (ch == '#')break;
					if (size >= 100) {
						cout << "�����ļ����ݹ�����" << endl;
						break;
					}
					content += ch;
				}
				getchar();
				sendtoserver(clientSocket, content);
				Readin(clientSocket);
				Sendwait(clientSocket);

				Readin(clientSocket);
				Sendwait(clientSocket);
				cout << buffer << endl;
			}
		}
		else if (str[0] == "copy")
		{
			string createinfo = str[0] + "-" + str[1] + "-" + str[2];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "move")
		{
			string adds = str[2];
			if (adds[adds.length() - 1] == '\\')
			{
				string createinfo = str[0] + "-" + str[1] + "-" + str[2];
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				Sendwait(clientSocket);
				cout << buffer << endl;
			}
			else
			{
				cout << "Ŀ��·������ȷ��" << endl;
				string createinfo = "meaningless-command";
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				Sendwait(clientSocket);
			}
		}
		else if (str[0] == "lock")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "head")
		{
			if (str[2][0] == '-')
			{
				string createinfo = str[0] + "-" + str[1] + str[2];
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				if (!strcmp(buffer, "-@not"))
				{
					cout << "�ļ�Ϊ�գ�" << endl;
				}
				else if (!strcmp(buffer, "�Ҳ������ļ�") || !strcmp(buffer, "���ȴ򿪸��ļ�") || !strcmp(buffer, "�ļ��Ѽ��������Ƚ���") || !strcmp(buffer, "������СΪ1"))
				{
					cout << buffer << endl;
				}
				else
				{
					cout << "--------------�ļ����ݣ�--------------" << endl;
					cout << buffer << endl;
				}
				Sendwait(clientSocket);
			}
			else {
				cout << "��������-num�������룡" << endl;
				string createinfo = "meaningless-command";
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				Sendwait(clientSocket);
			}
		}
		else if (str[0] == "tail")
		{
			if (str[2][0] == '-')
			{
				string createinfo = str[0] + "-" + str[1] + str[2];
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				if (!strcmp(buffer, "-@not"))
				{
					cout << "�ļ�Ϊ�գ�" << endl;
				}
				else if (!strcmp(buffer, "�Ҳ������ļ�") || !strcmp(buffer, "���ȴ򿪸��ļ�") || !strcmp(buffer, "�ļ��Ѽ��������Ƚ���") || !strcmp(buffer, "������СΪ1"))
				{
					cout << buffer << endl;
				}
				else
				{
					cout << "--------------�ļ����ݣ�--------------" << endl;
					cout << buffer << endl;
				}
				Sendwait(clientSocket);
			}
			else {
				cout << "��������-num�������룡" << endl;
				string createinfo = "meaningless-command";
				sendtoserver(clientSocket, createinfo);
				Readin(clientSocket);
				Sendwait(clientSocket);
			}
		}
		else if (str[0] == "import")
		{
			string createinfo = str[0] + "-" + str[1];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "export")
		{
			string createinfo = str[0] + "-" + str[1] + "-" + str[2];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "lseek")
		{
			string createinfo = str[0] + "-" + str[1] + "-" + str[2];
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else if (str[0] == "help")
		{
			help();
			string createinfo = str[0] + "-1";
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
		}
		else if (str[0] == "clear") {
			system("cls");
			string createinfo = str[0] + "-1";
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
		}
		else if (str[0] == "init") {
			string createinfo = str[0] + "-1";
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
		else
		{
			string createinfo = "meaningless-command";
			sendtoserver(clientSocket, createinfo);
			Readin(clientSocket);
			Sendwait(clientSocket);
			cout << buffer << endl;
		}
	}

	Cleanup(clientSocket);
}
void Login(SOCKET clientSocket)
{
	//�����û�Ŀ¼��
	fstream fp;
	fp.open(userLogin, ios::in);
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int successFlag = 0;//����Ƿ���ֹ��¼
	while (!successFlag)
	{
		int haveUser = 0;//����Ƿ���ڸ��û�
		//��֤�û���
		string inputName;
		cout << "�û���:";
		cin >> inputName;
		strip(inputName, ' ');//���벢�����û���
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					haveUser = 1;//���ȷʵ�и��û�
					userName = name;
					userPassword = password;
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "�򿪴���" << endl;
			return;
		}

		//����ҵ����û���������������
		if (haveUser) {
			int flag = 1;//����Ƿ���Ҫ���³�������
			int times = 0;//��������Ĵ������������������3�Σ�����ֹ��¼
			while (flag)
			{
				string inputPassword;
				cout << "����:";
				cin >> inputPassword; strip(inputPassword, ' ');
				//�������������ȷ��
				if (inputPassword == userPassword) {
					flag = 0;
					menu(clientSocket);//���빦���û�����
					successFlag = 1;//�ɹ���¼
				}
				//�����������
				else {
					times++;
					if (times < 3) {//ѡ���Ƿ�������������
						cout << "�����������" << endl << "�Ƿ��������루y/n��?:";
						int again = 1;
						while (again)
						{
							string choice;
							cin >> choice; strip(choice, ' ');
							if (choice == "y" || choice == "Y")
								again = 0;
							else if (choice == "n" || choice == "N") {
								flag = 0;
								again = 0;
								return;
							}
							else
								cout << "���������������������루y/n��:";
						}
					}
					else {//�����������ﵽ������ֱ���˳�
						cout << "������������Ѵﵽ3�Σ�" << endl;
						flag = 0;
						return;
					}

				}
			}
		}
		else {//���û���ҵ��û������������û�ѡ���Ƿ���������		
			int again = 1;
			cout << "��Ǹû���ҵ����û���" << endl << "�Ƿ��������루y/n��?" << endl;
			while (again)
			{
				string choice;
				cin >> choice; strip(choice, ' ');
				if (choice == "y" || choice == "Y")
					again = 0;
				else if (choice == "n" || choice == "N") {
					again = 0;
					return;
				}
				else
					cout << "���������������������루y/n��:";
			}
		}
	}
}

void Register()
{
	//�����û�Ŀ¼��
	fstream fp;
	fp.open(userLogin, ios::in);
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int flag = 1;//����Ƿ���ֹע��
	while (flag)
	{
		string inputName, password, password2;//�û��������룬ȷ������
		int used = 0;//����û����Ƿ��ѱ�ʹ�ù�

		//��֤�û���
		cout << "������ע���û�����";
		cin >> inputName;
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			//���ļ��а��ж�ȡ�û���Ϣ�������ݿո񻮷��û���������
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					used = 1;//����Ѵ��ڸ��û�
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "�򿪴���" << endl;
			return;
		}

		//��֤����
		if (!used) {
			cout << "���������룺";
			cin >> password;
			cout << "���ٴ��������룺";
			cin >> password2;
			//��������������붼��Ч��һ�£��򱣴��û��������룬����ʾע��ɹ�
			if (password == password2) {
				fstream fp;
				fp.open(userLogin, ios::app);
				if (fp) {
					fp << inputName << " " << password2 << endl;
					fp.close();
					cout << "ע��ɹ���" << endl;
				}
				else {
					cout << userLogin << "�򿪴���" << endl;
					return;
				}
				flag = 0;
			}
			//��������������벻һ�£�����ʾ�Ƿ���������
			else {
				int again = 1;
				cout << "�����������벻һ�£�" << endl << "�Ƿ��������루y/n��?:";
				while (again)
				{
					string choice;
					cin >> choice; strip(choice, ' ');
					if (choice == "y" || choice == "Y") {
						again = 0;
					}
					else if (choice == "n" || choice == "N")
					{
						again = 0;
						return;
					}
					else {
						cout << "���������������������루y/n��:";
					}
				}
			}
		}
		//����û����ѱ�ʹ�ã�����ʾ�Ƿ���������
		else
		{
			int again = 1;
			cout << "�û����Ѵ��ڣ�" << endl << "�Ƿ��������루y/n��?" << endl;
			while (again)
			{
				string choice;
				cin >> choice; strip(choice, ' ');
				if (choice == "y" || choice == "Y") {
					again = 0;
				}
				else if (choice == "n" || choice == "N")
				{
					again = 0;
					return;
				}
				else {
					cout << "���������������������루y/n��:";
				}
			}
		}
	}
}

int main()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	// ��ʼ��Winsock��
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		cout << "����winsock.dllʧ�ܣ�" << endl;
		return 0;
	}
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "�����׽���ʧ�ܣ�������룺" << WSAGetLastError() << endl;
		Cleanup(clientSocket);
		return 0;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

	if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cout << "����ʧ�ܣ�������룺" << WSAGetLastError() << endl;
		Cleanup(clientSocket);
		return 0;
	}
	int i, j, k, n = 0, x = 0, y = 50;
	getchar();
	system("cls");
	cout << "ϵͳ�����С���" << endl;
	cout << "�������!" << endl;
	Sleep(800);
	while (true)
	{
		cout << "���¼��ע��" << endl;
		string choice;
		cin >> choice;
		if (choice == "login") Login(clientSocket);
		else if (choice == "register")
		{
			Register(); getchar();
		}
		else if (choice == "exit") return 0;
		else cout << "�������" << endl;
		getchar();
	}

	return 0;
}
