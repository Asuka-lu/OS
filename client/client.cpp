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

string userLogin = "~\\user.txt";//用户目录表，用于记录用户名和密码
string userName, userPassword;//当前用户名和密码

string curPath;//当前路径
string userDat;//当前用磁盘文件
char* fdisk;//虚拟磁盘起始地址
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
		cout << "[发送信息失败] 错误代码：" << WSAGetLastError() << endl;
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
	cout << endl << "\t" << "*------------------文件系统-命令菜单--------------------*" << endl;
	cout << "\t" << "|" << "\t" << 3 << "\t" << "mkdir (name)" << "\t" << "创建目录" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 4 << "\t" << "create (name)" << "\t" << "创建文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 5 << "\t" << "open (name)" << "\t" << "打开文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 6 << "\t" << "close (name)" << "\t" << "关闭文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 7 << "\t" << "read (name)" << "\t" << "读文件" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 8 << "\t" << "del (name)" << "\t" << "删除文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 9 << "\t" << "remove(name)" << "\t" << "删除目录" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 10 << "\t" << "cd" << "\t" << "\t" << "切换目录" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 11 << "\t" << "dir" << "\t" << "\t" << "列出目录" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 12 << "\t" << "ls" << "\t" << "\t" << "列出文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 13 << "\t" << "write" << "\t" << "\t" << "写文件" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 14 << "\t" << "move" << "\t" << "\t" << "移动文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 15 << "\t" << "copy" << "\t" << "\t" << "拷贝文件" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 16 << "\t" << "lock(name)" << "\t" << "文件加密 " << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 17 << "\t" << "head -num" << "\t" << "显示文件的前num行" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 18 << "\t" << "tail -num" << "\t" << "显示文件尾巴上的num行" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 19 << "\t" << "lseek -offerset" << "\t" << "文件读写指针的移动" << "\t" << "|" << endl;
	cout << "\t" << "|  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - |" << endl;
	cout << "\t" << "|" << "\t" << 20 << "\t" << "import" << "\t" << "\t" << "导入到当前目录" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 21 << "\t" << "export" << "\t" << "\t" << "导出到目的地址" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 22 << "\t" << "clear" << "\t" << "\t" << "清屏" << "\t" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 23 << "\t" << "help" << "\t" << "\t" << "显示命令" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 24 << "\t" << "exit" << "\t" << "\t" << "退出系统" << "\t" << "\t" << "|" << endl;
	cout << "\t" << "|" << "\t" << 25 << "\t" << "tree" << "\t" << "\t" << "树状图" << "\t" << "\t" << "\t" << "|" << endl;
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
					sendtoserver(clientSocket, "ack"); // 发送 ack 表示结束
					break;
				}
				cout << buffer << endl;
				sendtoserver(clientSocket, "ack"); // 发送 ack 表示收到
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
			cout << fixed << left << setw(20) << "文件名" << setw(10) << "类型" << setw(20) << "起始磁盘块号" << setw(10) << "大小" << setw(10) << "打开状态" << setw(10) << "加锁状态" << setw(10) << "lseek指针位置" << endl;
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
			if (!strcmp(buffer, "找不到该目录"))
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
				cout << "文件为空！" << endl;
			}
			else if (!strcmp(buffer, "-@notseek"))
			{
				cout << "文件指针之后为空内容" << endl;
			}
			else if (!strcmp(buffer, "找不到该文件") || !strcmp(buffer, "请先打开该文件") || !strcmp(buffer, "文件已加锁，请先解锁"))
			{
				cout << buffer << endl;
			}
			else
			{
				cout << "--------------文件内容：--------------" << endl;
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
			if (strcmp(buffer, "可以写入该文件") != 0)
			{
				cout << buffer << endl;
			}
			else {
				cout << "请输入文件内容，并以'#'为结束标志" << endl;
				char ch;
				string content;
				int size = 0;
				while (1)
				{
					ch = getchar();
					if (ch == '#')break;
					if (size >= 100) {
						cout << "输入文件内容过长！" << endl;
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
				cout << "目的路径不正确！" << endl;
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
					cout << "文件为空！" << endl;
				}
				else if (!strcmp(buffer, "找不到该文件") || !strcmp(buffer, "请先打开该文件") || !strcmp(buffer, "文件已加锁，请先解锁") || !strcmp(buffer, "行数最小为1"))
				{
					cout << buffer << endl;
				}
				else
				{
					cout << "--------------文件内容：--------------" << endl;
					cout << buffer << endl;
				}
				Sendwait(clientSocket);
			}
			else {
				cout << "行数请以-num类型输入！" << endl;
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
					cout << "文件为空！" << endl;
				}
				else if (!strcmp(buffer, "找不到该文件") || !strcmp(buffer, "请先打开该文件") || !strcmp(buffer, "文件已加锁，请先解锁") || !strcmp(buffer, "行数最小为1"))
				{
					cout << buffer << endl;
				}
				else
				{
					cout << "--------------文件内容：--------------" << endl;
					cout << buffer << endl;
				}
				Sendwait(clientSocket);
			}
			else {
				cout << "行数请以-num类型输入！" << endl;
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
	//创建用户目录表
	fstream fp;
	fp.open(userLogin, ios::in);
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int successFlag = 0;//标记是否终止登录
	while (!successFlag)
	{
		int haveUser = 0;//标记是否存在该用户
		//验证用户名
		string inputName;
		cout << "用户名:";
		cin >> inputName;
		strip(inputName, ' ');//输入并处理用户名
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					haveUser = 1;//标记确实有该用户
					userName = name;
					userPassword = password;
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "打开错误！" << endl;
			return;
		}

		//如果找到了用户名，则输入密码
		if (haveUser) {
			int flag = 1;//标记是否需要重新出入密码
			int times = 0;//输入密码的次数，如果输入错误大于3次，则终止登录
			while (flag)
			{
				string inputPassword;
				cout << "密码:";
				cin >> inputPassword; strip(inputPassword, ' ');
				//如果密码输入正确！
				if (inputPassword == userPassword) {
					flag = 0;
					menu(clientSocket);//进入功能用户界面
					successFlag = 1;//成功登录
				}
				//密码输入错误
				else {
					times++;
					if (times < 3) {//选择是否重新输入密码
						cout << "输入密码错误！" << endl << "是否重新输入（y/n）?:";
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
								cout << "您的输入有误！请重新输入（y/n）:";
						}
					}
					else {//密码输入错误达到三次则直接退出
						cout << "输入密码错误已达到3次！" << endl;
						flag = 0;
						return;
					}

				}
			}
		}
		else {//如果没有找到用户名，报错并请用户选择是否重新输入		
			int again = 1;
			cout << "抱歉没有找到该用户！" << endl << "是否重新输入（y/n）?" << endl;
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
					cout << "您的输入有误！请重新输入（y/n）:";
			}
		}
	}
}

void Register()
{
	//创建用户目录表
	fstream fp;
	fp.open(userLogin, ios::in);
	if (!fp) {
		fp.close();
		fp.open(userLogin, ios::out);
		fp.close();
	}
	int flag = 1;//标记是否终止注册
	while (flag)
	{
		string inputName, password, password2;//用户名，密码，确认密码
		int used = 0;//标记用户名是否已被使用过

		//验证用户名
		cout << "请输入注册用户名：";
		cin >> inputName;
		ifstream fp;
		fp.open(userLogin);
		if (fp) {
			//从文件中按行读取用户信息，并根据空格划分用户名和密码
			string userLine;
			while (getline(fp, userLine)) {
				string name, password;
				halfStr(name, password, userLine);
				if (name == inputName) {
					used = 1;//标记已存在该用户
				}
			}
			fp.close();
		}
		else {
			cout << userLogin << "打开错误！" << endl;
			return;
		}

		//验证密码
		if (!used) {
			cout << "请输入密码：";
			cin >> password;
			cout << "请再次输入密码：";
			cin >> password2;
			//如果两次输入密码都有效且一致，则保存用户名和密码，并提示注册成功
			if (password == password2) {
				fstream fp;
				fp.open(userLogin, ios::app);
				if (fp) {
					fp << inputName << " " << password2 << endl;
					fp.close();
					cout << "注册成功！" << endl;
				}
				else {
					cout << userLogin << "打开错误！" << endl;
					return;
				}
				flag = 0;
			}
			//如果两次输入密码不一致，则提示是否重新输入
			else {
				int again = 1;
				cout << "两次密码输入不一致！" << endl << "是否重新输入（y/n）?:";
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
						cout << "您的输入有误！请重新输入（y/n）:";
					}
				}
			}
		}
		//如果用户名已被使用，则提示是否重新输入
		else
		{
			int again = 1;
			cout << "用户名已存在！" << endl << "是否重新输入（y/n）?" << endl;
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
					cout << "您的输入有误！请重新输入（y/n）:";
				}
			}
		}
	}
}

int main()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	// 初始化Winsock库
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		cout << "加载winsock.dll失败！" << endl;
		return 0;
	}
	SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "创建套接字失败！错误代码：" << WSAGetLastError() << endl;
		Cleanup(clientSocket);
		return 0;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

	if (connect(clientSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cout << "连接失败！错误代码：" << WSAGetLastError() << endl;
		Cleanup(clientSocket);
		return 0;
	}
	int i, j, k, n = 0, x = 0, y = 50;
	getchar();
	system("cls");
	cout << "系统加载中……" << endl;
	cout << "加载完成!" << endl;
	Sleep(800);
	while (true)
	{
		cout << "请登录或注册" << endl;
		string choice;
		cin >> choice;
		if (choice == "login") Login(clientSocket);
		else if (choice == "register")
		{
			Register(); getchar();
		}
		else if (choice == "exit") return 0;
		else cout << "输入错误" << endl;
		getchar();
	}

	return 0;
}
