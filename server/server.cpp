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
#define totalsize 1024*1024//总磁盘空间为1M
#define disksize 1024//磁盘块大小1K
#define disknum 1024//磁盘块1K
#define max_direct_num 7//最大子目录数max subdirect
#define fatsize disknum*sizeof(fatItem)//FAT表大小
#define user_root_block fatsize/disksize+1//用户根目录起始盘块号
#define user_root_size sizeof(direct)//用户根目录大小
#define max_filesize 100//最大文件长度

typedef struct FCB {
	char fileName[20];//文件或目录名
	int type;// 0--文件  1--目录 
	char* content = NULL;//文件内容
	int size;//大小
	int firstDisk;//起始盘块
	int sign;//0--普通目录  1--根目录
	int oflag;//0--未打开状态  1--打开状态
	int lock;//0--未加锁   1--已加锁
	int fseek;//文件读写指针的移动，从文件指针当前位置处向后移动 offset，负数时向前移动offset
}FCB;
/*目录*/
struct direct {
	FCB directItem[max_direct_num + 2];
};
/*文件分配表*/
typedef struct fatItem {
	int item;//存放文件下一个磁盘的指针
	int state;//0-- 空闲  1--被占用
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

/*保存磁盘数据*/
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
	fdisk = new char[totalsize * sizeof(char)];//申请磁盘空间
	//初始化FAT表
	fat = (FAT)(fdisk + disksize);//fat地址
	fat[0].item = -1; fat[0].state = 1;//初始化FAT表
	for (int i = 1; i < user_root_block - 1; i++) {
		fat[i].item = i + 1;
		fat[i].state = 1;
	}
	fat[user_root_block].item = -1;
	fat[user_root_block].state = 1;//根目录磁盘块号
	//fat其他块初始化
	for (int i = user_root_block + 1; i < disknum; i++) {
		fat[i].item = -1;
		fat[i].state = 0;
	}
	//根目录初始化
	root = (struct direct*)(fdisk + disksize + fatsize);//根目录地址
	root->directItem[0].sign = 1;
	root->directItem[0].firstDisk = user_root_block;
	strcpy(root->directItem[0].fileName, ".");
	root->directItem[0].type = 1;
	root->directItem[0].size = user_root_size;
	root->directItem[0].oflag = 0;
	root->directItem[0].lock = 0;
	root->directItem[0].fseek = 0;
	//父目录初始化
	root->directItem[1].sign = 1;
	root->directItem[1].firstDisk = user_root_block;
	strcpy(root->directItem[1].fileName, "..");
	root->directItem[1].type = 1;
	root->directItem[1].size = user_root_size;
	root->directItem[1].oflag = 0;
	root->directItem[1].lock = 0;
	root->directItem[1].fseek = 0;
	//子目录初始化
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
			string message = "初始化失败！";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}
	else
	{
		if (flag)
		{
			string message = "初始化成功！";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
	}

}

/*读取用户磁盘数据内容*/
void readUserDisk(SOCKET clientSocket, string userDat, char*& fdisk, FAT& fat, direct*& root, direct*& curDir, string& curPath, string userName)
{
	fdisk = new char[totalsize];//申请磁盘大小缓冲区
	ifstream fp;
	fp.open(userDat, ios::in | ios::binary);
	if (fp)
		fp.read(fdisk, totalsize);
	else
	{
		string message = userDat + "打开失败！";
		Sendtoclient(clientSocket, message);
	}
	fp.close();
	fat = (FAT)(fdisk + disksize);//fat地址
	root = (struct direct*)(fdisk + disksize + fatsize);//根目录地址
	curDir = root;
	curPath = userName + ":\\";
}
int create(SOCKET clientSocket, string str, direct* curDir, FAT fat)
{
	char fName[20];
	strcpy(fName, str.c_str());
	//检查文件名是否合法
	if (!strcmp(fName, ""))return 0;
	if (str.length() > 20) {
		string message = "文件名太长！请重新输入！";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//找当前目录下是否有文件重名
	for (int pos = 2; pos < max_direct_num + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			string message = "当前目录下已存在文件重名!";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//检查当前目录下空间是否已满
	int i;
	for (i = 2; i < max_direct_num + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= max_direct_num + 2) {
		string message = "当前目录下空间已满";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//检查是否有空闲磁盘块
	int j;
	for (j = user_root_block + 1; j < disknum; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= disknum) {
		string message = "无空闲盘块";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//文件初始化
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
	//检查文件名是否合法
	if (!strcmp(fName, ""))return 0;
	if (str.length() > 20) {
		string message = "文件名太长！请重新输入！";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//找当前目录下是否有文件重名
	for (int pos = 2; pos < max_direct_num + 2; pos++) {
		if (!strcmp(curDir->directItem[pos].fileName, fName) && curDir->directItem[pos].type == 0) {
			string message = "当前目录下已存在文件重名!";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//检查当前目录下空间是否已满
	int i;
	for (i = 2; i < max_direct_num + 2; i++)
		if (curDir->directItem[i].firstDisk == -1)break;
	if (i >= max_direct_num + 2) {
		string message = "当前目录下空间已满";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//检查是否有空闲磁盘块
	int j;
	for (j = user_root_block + 1; j < disknum; j++)
		if (fat[j].state == 0) {
			fat[j].state = 1;
			break;
		}
	if (j >= disknum) {
		string message = "无空闲盘块";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//文件初始化
	curDir->directItem[i].sign = 0;
	curDir->directItem[i].firstDisk = j;
	strcpy(curDir->directItem[i].fileName, fName);
	curDir->directItem[i].type = 1;
	curDir->directItem[i].size = user_root_size;


	direct* cur_mkdir = (direct*)(fdisk + curDir->directItem[i].firstDisk * disksize);//创建目录的物理地址
	//指向当前目录的目录项
	cur_mkdir->directItem[0].sign = 0;
	cur_mkdir->directItem[0].firstDisk = curDir->directItem[i].firstDisk;
	strcpy(cur_mkdir->directItem[0].fileName, ".");
	cur_mkdir->directItem[0].type = 1;
	cur_mkdir->directItem[0].size = user_root_size;
	//指向上一级目录的目录项
	cur_mkdir->directItem[1].sign = curDir->directItem[0].sign;
	cur_mkdir->directItem[1].firstDisk = curDir->directItem[0].firstDisk;
	strcpy(cur_mkdir->directItem[1].fileName, "..");
	cur_mkdir->directItem[1].type = 1;
	cur_mkdir->directItem[1].size = user_root_size;
	//子目录初始化
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
		string message = "找不到该目录";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}

	direct* temp = (direct*)(fdisk + curDir->directItem[i].firstDisk * disksize);
	for (j = 2; j < max_direct_num + 2; j++)
	{
		//若目录中有子目录或文件，删除掉文件和子目录
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
			//释放目录项
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
	string objPath = str;//目标路径
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1; int end = 0;//以\为分割，每段起始下标和结束下标
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//保存切换前的路径
	string temp;
	for (i = 0; i < len; i++) {
		//如果目标路径以\开头，则从根目录开始查询
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
			//检查目录是否存在
			for (j = 0; j < max_direct_num + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= max_direct_num + 2) {
				curPath = oldPath;
				string message = "找不到该目录";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
				return;
			}
			//如果目标路径为".."，则返回上一级
			if (temp == "..") {
				//避免在根目录输入cd ..导致退出用户目录
				if (tempDir->directItem[0].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//如果目标路径为"."，则指向当前目录
			else {
				if (temp != ".")
					curPath = curPath + objPath.substr(start, end - start) + "\\";
			}
			start = -1;
			tempDir = (direct*)(fdisk + tempDir->directItem[j].firstDisk * disksize);
		}
	}
	string message = "查找成功";
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
	curDir = tempDir;
}
int cd_copy(SOCKET clientSocket, string str, string& curPath, direct*& curDir, direct*& root, string userName, char*& fdisk)
{
	char fName[20]; strcpy(fName, str.c_str());
	string objPath = str;//目标路径
	if (objPath[objPath.size() - 1] != '\\')objPath += "\\";
	int start = -1; int end = 0;//以\为分割，每段起始下标和结束下标
	int i, j, len = objPath.length();
	direct* tempDir = curDir;
	string oldPath = curPath;//保存切换前的路径
	string temp;
	for (i = 0; i < len; i++) {
		//如果目标路径以\开头，则从根目录开始查询
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
			//检查目录是否存在
			for (j = 0; j < max_direct_num + 2; j++)
				if (!strcmp(tempDir->directItem[j].fileName, temp.c_str()) && tempDir->directItem[j].type == 1)
					break;
			if (j >= max_direct_num + 2) {
				curPath = oldPath;
				string message = "找不到该目录";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
				return 0;
			}
			//如果目标路径为".."，则返回上一级
			if (temp == "..") {
				//避免在根目录输入cd ..导致退出用户目录
				if (tempDir->directItem[j - 1].sign != 1) {
					int pos = curPath.rfind('\\', curPath.length() - 2);
					curPath = curPath.substr(0, pos + 1);
				}
			}
			//如果目标路径为"."，则指向当前目录
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
	//在该目录下找目标文件
	int i, temp;
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!move)
	{
		if (i >= max_direct_num + 2) {
			string message = "找不到该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].oflag == 1)
		{
			string message = "文件已打开，请先关闭该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "文件已加锁，请先解锁";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	int item = curDir->directItem[i].firstDisk;//目标文件的起始磁盘块号
	//根据文件分配表依次删除文件所占据的磁盘块
	while (item != -1)
	{
		temp = fat[item].item;
		fat[item].item = -1;
		fat[item].state = 0;
		item = temp;
	}
	//释放目录项
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
	//检查文件名是否合法
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "找不到该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "文件已加锁，请先解锁";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}

	//打开状态赋值1
	curDir->directItem[i].oflag = 1;
	return 1;
}
int close(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//检查文件名是否合法
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "找不到该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "文件已加锁，请先解锁";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}
	//打开状态赋值0
	curDir->directItem[i].oflag = 0;
	return 1;
}
int lock(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	int i;
	//检查文件名是否合法
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "找不到该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
		if (curDir->directItem[i].oflag == 1)
		{
			string message = "文件已打开，请先关闭";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return 0;
		}
	}

	//通过一个函数来加锁/解锁
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
	//检查文件名是否合法
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (i >= max_direct_num + 2) {
		string message = "找不到该文件";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "文件已加锁，请先解锁";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return 0;
	}
	//offset加上移动变量
	curDir->directItem[i].fseek += offset;
	if (curDir->directItem[i].fseek > curDir->directItem[i].size)
		curDir->directItem[i].fseek = curDir->directItem[i].size;
	if (curDir->directItem[i].fseek < 0)
		curDir->directItem[i].fseek = 0;
	string message = str + "的fseek移动成功，当前fseek值为：" + to_string(curDir->directItem[i].fseek);
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
	return 1;
}
void read(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char*& content, char* fdisk, int flag)
{
	char fName[20];
	strcpy(fName, str.c_str());
	//在当前目录下查找目标文件
	int i, j;
	for (i = 2; i < max_direct_num + 2; i++)
		if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
			break;
	if (!flag)
	{
		if (i >= max_direct_num + 2) {
			string message = "找不到该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}

		if (curDir->directItem[i].oflag == 0)
		{
			string message = "请先打开该文件";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}
		if (curDir->directItem[i].lock == 1)
		{
			string message = "文件已加锁，请先解锁";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
			return;
		}
	}


	int curflag = i;
	int fSize = curDir->directItem[curflag].size;//目标文件大小
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
	int firstblock = curDir->directItem[curflag].firstDisk;//寻找目标文件的起始磁盘块号
	char* start = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//计算该文件的起始地址
	int curread = 0;
	int needDisk = fSize / disksize;//占据的磁盘块数量
	int needRes = fSize % disksize;//占据最后一块磁盘块的大小
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
	if (!flag)//如果不是仅使用功能就给content传出去
	{
		Sendtoclient(clientSocket, content);
		Readwait(clientSocket);
	}

}
void write(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* content, int size, char* fdisk, int curflag)
{
	int i, j;
	int fSize = curDir->directItem[curflag].size;//目标文件大小
	int firstblock = curDir->directItem[curflag].firstDisk;
	int lastblock = firstblock;//向后寻找目标文件的最后磁盘块号
	while (fat[lastblock].item != -1)
		lastblock = fat[lastblock].item;//计算保存该文件的最后一块盘块号
	char* last = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//计算该文件的指针地址
	char* temp = fdisk + firstblock * disksize + fSize % disksize;//计算文件末地址
	//如果盘块剩余部分够写，则直接写入剩余部分
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
	//如果盘块剩余部分不够写，则找到空闲磁盘块写入
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


		//先将起始磁盘剩余部分写完
		for (j = 0; j < disksize - fSize % disksize + tempsize; j++) {
			last[j] = totalcontent[j];
		}

		int curwrite = j + 1;
		int last_size = strlen(totalcontent) - (disksize - fSize % disksize + tempsize);//剩余要写的内容大小

		int needDisk = last_size / disksize;//占据的磁盘块数量

		int needRes = last_size % disksize;//占据最后一块磁盘块的大小
		if (needRes > 0)needDisk += 1;
		for (j = 0; j < needDisk; j++) {

			if (fat[curDir->directItem[curflag].firstDisk].item != -1)
			{
				i = fat[curDir->directItem[curflag].firstDisk].item;
				last = fdisk + i * disksize;//空闲磁盘起始盘物理地址
				//当写到最后一块磁盘，则只写剩余部分内容
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
					string message = "磁盘已被分配完";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return;
				}
				last = fdisk + i * disksize;//空闲磁盘起始盘物理地址
				//当写到最后一块磁盘，则只写剩余部分内容
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				//修改文件分配表内容
				fat[lastblock].item = i;
				fat[i].state = 1;
				fat[i].item = -1;
			}
		}
		curDir->directItem[curflag].size += size;
	}
	string message = "写入完成！";
	Sendtoclient(clientSocket, message);
	Readwait(clientSocket);
}
int write_copy(SOCKET clientSocket, string str, direct*& curDir, FAT& fat, char* content, int size, char* fdisk, int curflag)
{
	int i, j;
	int fSize = curDir->directItem[curflag].size;//目标文件大小
	int firstblock = curDir->directItem[curflag].firstDisk;
	int lastblock = firstblock;//向后寻找目标文件的最后磁盘块号
	while (fat[lastblock].item != -1)
		lastblock = fat[lastblock].item;//计算保存该文件的最后一块盘块号
	char* last = fdisk + firstblock * disksize + curDir->directItem[curflag].fseek;//计算该文件的指针地址
	char* temp = fdisk + firstblock * disksize + fSize % disksize;//计算文件末地址
	//如果盘块剩余部分够写，则直接写入剩余部分
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
	//如果盘块剩余部分不够写，则找到空闲磁盘块写入
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


		//先将起始磁盘剩余部分写完
		for (j = 0; j < disksize - fSize % disksize + tempsize; j++) {
			last[j] = totalcontent[j];
		}

		int curwrite = j + 1;
		int last_size = strlen(totalcontent) - (disksize - fSize % disksize + tempsize);//剩余要写的内容大小

		int needDisk = last_size / disksize;//占据的磁盘块数量

		int needRes = last_size % disksize;//占据最后一块磁盘块的大小
		if (needRes > 0)needDisk += 1;
		for (j = 0; j < needDisk; j++) {

			if (fat[curDir->directItem[curflag].firstDisk].item != -1)
			{
				i = fat[curDir->directItem[curflag].firstDisk].item;
				last = fdisk + i * disksize;//空闲磁盘起始盘物理地址
				//当写到最后一块磁盘，则只写剩余部分内容
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
					string message = "磁盘已被分配完";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return 0;
				}
				last = fdisk + i * disksize;//空闲磁盘起始盘物理地址
				//当写到最后一块磁盘，则只写剩余部分内容
				if (j == needDisk - 1) {
					for (int k = 0; k < last_size - (needDisk - 1) * disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				else {
					for (int k = 0; k < disksize; k++)
						last[k] = totalcontent[curwrite++];
				}
				//修改文件分配表内容
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
			if (curDir->directItem[i].type == 0) {//显示文件
				message = "<FILE>";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else {//显示目录
				message = "<DIR>";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			message = to_string(curDir->directItem[i].firstDisk);
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);

			if (curDir->directItem[i].type == 1) {//显示文件
				message = "file";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else {//显示目录
				message = to_string(curDir->directItem[i].size);
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			if (curDir->directItem[i].oflag)
			{
				message = "已打开";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else
			{
				message = "未打开";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}

			if (curDir->directItem[i].lock)
			{
				message = "已加锁";
				Sendtoclient(clientSocket, message);
				Readwait(clientSocket);
			}
			else
			{
				message = "未加锁";
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
	// 收集当前目录下所有有效条目
	vector<FCB*> items;
	for (int i = 2; i < max_direct_num + 2; ++i)
	{
		auto& f = dir->directItem[i];
		if (f.firstDisk != -1)
		{
			items.push_back(&f);
		}
	}

	// 遍历，按树状结构发送每一项
	for (size_t k = 0; k < items.size(); ++k)
	{
		bool last = (k + 1 == items.size());
		string branch = prefix + (last ? "└── " : "├── ");
		FCB* f = items[k];

		// 构造显示行
		string line = branch + f->fileName + (f->type == 1 ? "/" : "");
		line += (f->type == 0) ? " (FILE, Size: " + to_string(f->size) + " bytes)" : " (DIR)";

		// 发送给客户端并等待 ack
		Sendtoclient(clientSocket, line);
		Readwait(clientSocket);

		// 目录则递归
		if (f->type == 1)
		{
			direct* sub = (direct*)(fdisk + f->firstDisk * disksize);
			string childPrefix = prefix + (last ? "    " : "│   ");
			sendTree(clientSocket, sub, childPrefix, fdisk);
		}
	}
}

void copy(SOCKET clientSocket, string sourcefile, string destination, direct* curDir, string curPath, FAT& fat, char* fdisk, direct*& root, string userName)
{

	string source = sourcefile;//原文件
	string des = destination;//目标文件
	//检查原文件与目的地址是否存在并合法
	if (source == "" || des == "") {
		string message = "输入错误";
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
		string message = "找不到原文件";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].oflag == 1) {
		string message = "文件已打开，请先关闭";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "文件已加锁，请先解锁";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	bool isdir = 0;
	direct* cur = curDir;//保存当前目录
	string oldPath = curPath;//保存当前路径
	string newname;
	char* content = new char[100];
	//读取源文件内容
	open(clientSocket, source, curDir, fat, 1);
	read(clientSocket, source, curDir, fat, content, fdisk, 1);
	close(clientSocket, source, curDir, fat, 1);
	//根据目标文件或目录路径切换当前目录
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
		//如果源文件目标路径为abc\b,认定为是在abc下复制source，名字为b
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
		//目标路径为b，视为直接在当前目录创建b名字的文件
		else {
			newname = des;
		}
	}
	//如果目录找得到
	//如果目标目录有重名create会发送“已经重名”
	if (create(clientSocket, newname, curDir, fat)) {
		for (i = 2; i < max_direct_num + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, newname.c_str()) && curDir->directItem[i].type == 0)
				break;
		//如果磁盘已满则write会发送“磁盘已经满”
		if (write_copy(clientSocket, newname, curDir, fat, content, strlen(content), fdisk, i))
		{
			string message = "复制成功";
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

	string source = sourcefile;//原文件
	string des = destination;//目标文件
	//检查原文件与目的地址是否存在并合法
	if (source == "" || des == "") {
		string message = "输入错误";
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
		string message = "找不到原文件";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].oflag == 1) {
		string message = "文件已打开，请先关闭";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	if (curDir->directItem[i].lock == 1)
	{
		string message = "文件已加锁，请先解锁";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
		return;
	}
	bool isdir = 0;
	direct* cur = curDir;//保存当前目录
	string oldPath = curPath;//保存当前路径
	string newname;
	char* content = new char[100];
	//读取源文件内容
	open(clientSocket, source, curDir, fat, 1);
	read(clientSocket, source, curDir, fat, content, fdisk, 1);
	close(clientSocket, source, curDir, fat, 1);
	//根据目标文件或目录路径切换当前目录

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

	//如果目录找得到
	//如果目标目录有重名create会发送“已经重名”
	if (create(clientSocket, newname, curDir, fat)) {
		for (i = 2; i < max_direct_num + 2; i++)
			if (!strcmp(curDir->directItem[i].fileName, newname.c_str()) && curDir->directItem[i].type == 0)
				break;
		//如果磁盘已满则write会发送“磁盘已经满”
		if (write_copy(clientSocket, newname, curDir, fat, content, strlen(content), fdisk, i))
		{
			deletes(clientSocket, source, cur, fat, 1);

			string message = "移动成功";
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
		string mes = "行数最小为1";
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
	//先通过n和z越过前面的几个回车，z作为下标经过循环后就可以使用content[z]表示后面，利用index让tail数组从0开始填

	//先统计内容有几行
	if (num <= 0)
	{
		string mes = "行数最小为1";
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
	char* content = new char[2000];//设置缓冲区
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
		if (create(clientSocket, newStr, curDir, fat))//读取本地文件内容并创建文件写入
		{
			open(clientSocket, newStr, curDir, fat, 1);
			int i, j;
			for (i = 2; i < max_direct_num + 2; i++)
				if (!strcmp(curDir->directItem[i].fileName, newStr.c_str()) && curDir->directItem[i].type == 0)
					break;

			write_copy(clientSocket, newStr, curDir, fat, content, strlen(content) - 1, fdisk, i);

			close(clientSocket, newStr, curDir, fat, 1);
			string message = "导入文件成功！";
			Sendtoclient(clientSocket, message);
			Readwait(clientSocket);
		}
		delete[] content;
	}
	else {
		string message = "打开本地文件失败！";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
}

void exportf(SOCKET clientSocket, string name, string filePath, direct*& curDir, FAT& fat, char* fdisk)
{
	char* content = new char[max_filesize];//设置缓冲区
	if (!open(clientSocket, name, curDir, fat, 0))
		return;
	read(clientSocket, name, curDir, fat, content, fdisk, 1);//读取文件内容
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
		string message = "导出文件成功！";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
	else {
		string message = "导出文件失败！";
		Sendtoclient(clientSocket, message);
		Readwait(clientSocket);
	}
	delete[] content;
}

void HandleClientCommunication(SOCKET clientSocket)
{

	string userName;//当前用户名
	FAT fat = nullptr;//FAT表
	direct* root = nullptr;//根目录
	direct* curDir = nullptr;//当前目录
	string curPath;//当前路径
	string userDat;//当前用磁盘文件
	char* fdisk = nullptr;//虚拟磁盘起始地址
	int login = 0;
	while (true)
	{

		char buffer[1000];

		//向客户端发送当前目录
		string message = curPath;
		int size = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);

		memset(buffer, 0, sizeof(buffer));
		//从客户端接收消息
		int receivedBytes = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (receivedBytes == SOCKET_ERROR || receivedBytes == 0)
		{
			cout << "用户" << sockets[clientSocket] << "已经退出！" << endl;
			sockets.erase(clientSocket);
			break;
		}

		else
		{
			string command, info;//分解为命令与信息
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
					message = "文件创建成功！";
					int size = send(clientSocket, message.c_str(), static_cast<int>(message.length()), 0);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "tree-command") {
				string message = "/ (虚拟根目录)";
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
						message = "目录创建成功！";
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
						message = info + "删除成功！";
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
					string message = info + "打开成功";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "close")
			{
				if (close(clientSocket, info, curDir, fat, 0))
				{
					string message = info + "关闭成功";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "delete")
			{
				if (deletes(clientSocket, info, curDir, fat, 0))
				{
					message = info + "删除成功！";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
			}
			else if (command == "write")
			{
				char fName[20];
				strcpy(fName, info.c_str());
				//在当前目录下查找目标文件
				int i, j;
				for (i = 2; i < max_direct_num + 2; i++)
					if (!strcmp(curDir->directItem[i].fileName, fName) && curDir->directItem[i].type == 0)
						break;
				if (i >= max_direct_num + 2) {
					string message = "找不到该文件";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
				}
				else if (curDir->directItem[i].oflag == 0)
				{
					string message = "请先打开该文件";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
				}
				else if (curDir->directItem[i].lock == 1)
				{
					string message = "文件已加锁，请先解锁";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					return;
				}
				else {
					string message = "可以写入该文件";
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
					string message = info + "加锁成功";
					Sendtoclient(clientSocket, message);
					Readwait(clientSocket);
					save(userDat, fdisk);
				}
				else if (lockflag == 2)
				{
					string message = info + "解锁成功";
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
				message = "指令无效！";
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
		cout << "加载winsock.dll失败！" << endl;
		return 0;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);//创建套接字，指定地址族(AF_INET)、套接字类型(SOCK_STREAM)和协议
	if (serverSocket == INVALID_SOCKET)
	{
		cout << "创建套接字失败！错误代码：" << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
	if (bind(serverSocket, reinterpret_cast<SOCKADDR*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
	{
		cout << "地址绑定失败！错误代码：" << WSAGetLastError() << endl;
		Cleanup(serverSocket);
		return 0;
	}
	//将socket设置为监听模式
	if (listen(serverSocket, 0) == SOCKET_ERROR)
	{
		cout << "监听失败！错误代码：" << WSAGetLastError() << endl;
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
			clientThread.detach(); // 分离线程，不等待线程结束
		}
	}

	Cleanup(serverSocket);
	return 0;
}
