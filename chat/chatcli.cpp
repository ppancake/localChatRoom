#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>   //sockaddr_in
#include <unistd.h>      //close
#include <arpa/inet.h>
#include "pub.h"
#define ERR_EXIT(m) \
    do\
     { \
       perror(m);  \
       exit(EXIT_FAILURE);\
     }while(0)     //换行加转义


char username[16];    //客户的用户名，全局通用
USER_LIST client_list;  //聊天室成员表，全局变量
char password[20];


void do_someone_login(message msg); //收到用户登录响应
void do_someone_logout(message msg);//收到用户退出响应
void do_recv_list(int sock);        //获得用户列表
void do_chat(message msg);                        //聊天
void do_cmd(char*cmd,int sock,struct sockaddr_in *srvaddr); //命令处理
bool sendmsgto(int sock,char*name,char*msg); //像其他客户发送消息

void do_someone_login(message msg) //收到用户登录响应,打印并添加至链表
{
	user_info *user=(user_info *)msg.body;//强制转换。msg内容变为用户信息??？
	in_addr temp;
	temp.s_addr=user->ip;
    printf("用户：%s上线了\n",user->user_name);
    client_list.push_back(*user);
}

void do_someone_logout(message msg)//收到用户退出响应,list 删除此用户
{
	USER_LIST::iterator it;
	 for(it=client_list.begin();it!=client_list.end();it++)
	 {
		 if(strcmp((*it).user_name,msg.body)==0)//自己不发
			 break;
	 }
	 if(it!=client_list.end()) //在list 找到此用户
		 client_list.erase(it);
	 printf("用户：%s以下线\n",msg.body);

}

void do_recv_list(int sock) //获得用户列表,打印并更新链表
{
	int count;
	recvfrom(sock,&count,sizeof(int),0,NULL,NULL);
	client_list.clear();

	int n=ntohl(count);
	printf("当前在线人数有：%d人\n",n);
	printf("在线用户是：\n");
	for(int i=0;i<n;i++)
	{
		user_info user;
		recvfrom(sock,&user,sizeof(user_info),0,NULL,NULL);
		client_list.push_back(user);
		in_addr temp;
		temp.s_addr=user.ip;
	    printf("用户：%s\n",user.user_name);
	}
}

void do_chat(message msg)
{
    chat_msg *cm=(chat_msg *)&msg.body;
    printf("收到来自[%s]的一条消息：[%s]\n",cm->username,cm->msg);
}
void do_cmd(char*cmdline,int sock,struct sockaddr_in *srvaddr)
{
	char cmd[10]={0};
	char *p;
	p=strchr(cmdline,' '); //可以查找字符串s中首次出现字符c的位置，将p定位至空格处。
	if(p!=NULL)
		*p='\0';
	strcpy(cmd,cmdline);
	if(strcmp(cmd,"exit")==0)  //命令cmd=exit，退出
	{
		//告知服务器退出
		message msg;
		memset(&msg,0,sizeof(msg));
		msg.cmd=htonl(C2S_LOGOUT);
		strcpy(msg.body,username);
		if(sendto(sock,&msg,sizeof(msg),0,(struct sockaddr*)srvaddr, sizeof(*srvaddr))<0)
			ERR_EXIT("sendto");
		printf("用户%s已退出\n",username);
		sleep(1);
		exit(EXIT_SUCCESS); //退出程序
	}
	else if(strcmp(cmd,"send")==0) //命令cmd=send，发送消息
	{
		char peername[16]={0};
		char msg[MSG_LEN]={0};
		while(*p++==' ');//定位空格后
		char *p2;
		p2=strchr(p,' ');
		if(p2==NULL)
		{
			printf("该聊天室没有此项功能\n");
			printf("\t--------聊天室功能:-------\n");
			printf("向其他用户发送消息：输入格式为send xx msg 其中xx是发向的用户名，msg是消息内容\n");
			printf("获取当前在线用户:输入 list\n");
			printf("退出聊天:输入 exit\n");
			printf("\n");
		}
		*p2='\0';
		strcpy(peername,p);  //由于命令格式为 send xx msg，需要通过2个指针跳过空格识别用户名和消息
		while(*p2++==' ');
		strcpy(msg,p2);
	    sendmsgto(sock,peername,msg);
	}
	else if(strcmp(cmd,"list")==0)//命令cmd=list，请求列表，套接字收到消息
	{
		message msg;
		memset(&msg,0,sizeof(msg));
		msg.cmd=htonl(C2S_ONLINE_USER);
		if(sendto(sock,&msg,sizeof(msg),0,(struct sockaddr*)srvaddr, sizeof(*srvaddr))<0)
			ERR_EXIT("sendto");
	}
	else
	{
		printf("该聊天室没有此项功能\n");
		printf("\t--------聊天室功能:-------\n");
		printf("向其他用户发送消息：输入格式为send xx msg 其中xx是发向的用户名，msg是消息内容\n");
		printf("获取当前在线用户:输入 list\n");
		printf("退出聊天:输入 exit\n");
		printf("\n");;
	}
}

bool sendmsgto(int sock,char*name,char*msg)
{
	if(strcmp(name,username)==0)  //自己给自己发
	{
		printf("不能给自己发消息");
		return false;
	}
	USER_LIST::iterator it;
	for(it=client_list.begin();it!=client_list.end();it++)
	{
		if(strcmp(it->user_name,name)==0) //用户列表有发送用户，也可以实现群发功能
			break;
	}
	if(it==client_list.end())
	{
		printf("用户：%s不在线，无法发送消息\n",name);
		return false;
	}
	//将自己的名字和消息放到新的要发送消息m中
	message m;
	memset(&m,0,sizeof(m));
	m.cmd=htonl(C2C_CHAT);
	chat_msg cm;
	strcpy(cm.username,username);
	strcpy(cm.msg,msg);
	memcpy(m.body,&cm,sizeof(cm));

	struct sockaddr_in peeraddr;//初始化对方客户it地址
	memset(&peeraddr,0,sizeof(peeraddr));
	peeraddr.sin_family=AF_INET;
	peeraddr.sin_port=it->port;
	peeraddr.sin_addr.s_addr=it->ip;
	in_addr temp;
	temp.s_addr=it->ip;
	printf("正在发送给：%s\n",name);
	sendto(sock,&m,sizeof(m),0,(struct sockaddr *)&peeraddr, sizeof(peeraddr));
	return true;
}

/*套接字处理程序*/
void echo_cli(int sock)
{
   struct sockaddr_in srvaddr;//初始化服务端地址
   memset(&srvaddr,0,sizeof(srvaddr));
   srvaddr.sin_family=AF_INET;
   srvaddr.sin_port=5188;
   srvaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
   int srvlen=sizeof(srvaddr);
   int a=0;//选择选项
   account acct;
   user_info user;
   int flag=0;
   int login=0;
   message msg;
   int cmd;
   //登录页面选项情况，，直到登录成功，以logined=1为准
   while(1)
   {
	   if(login==1)
		    break;
       system("clear");
	   printf("\t********************************************************\n");
	   printf("\t*        欢 迎 使 用 聊 天 系 统            *\n");
	   printf("\t*------------------------------------------------------*\n");
	   printf("\t*             1、登  陆                                *\n");
	   printf("\t*------------------------------------------------------*\n");
	   printf("\t*             2、注  册                                *\n");
	   printf("\t*------------------------------------------------------*\n");
	   printf("\t*             3、退  出                                *\n");
	   printf("\t*------------------------------------------------------*\n");
	   printf("\t*             请  选  择:                              *\n");
	   printf("\t********************************************************\n");
	   scanf("%d",&a);
	   setbuf(stdin,NULL);//输入清零
	   switch(a)
	   {
	   //a=1,处理登录
	   case 1:
		   system("clear");
		   printf("\t*********************************************************\n");
		   printf("\t*             请  登  录                                 *\n");
		   printf("\t*------------------------------------------------------*\n");
		   printf("\t*             用  户  名：                               *\n");
		   printf("\t*-------------------------------------------------------*\n");
		   printf("\t*             密      码:                               *\n");
		   printf("\t********************************************************\n");
		   scanf("%s",acct.user_name);
		   strcpy(user.user_name,acct.user_name);
		   setbuf(stdin,NULL);
		   scanf("%s",acct.password);
		   setbuf(stdin,NULL);
		   printf("正在登录中...\n");
		   msg.cmd=htonl(C2S_LOGIN);
		   memcpy(msg.body,&acct,sizeof(acct));  //！！！！这样使用的嘛
		   sendto(sock,&msg,sizeof(msg),0,(struct sockaddr*)&srvaddr, sizeof(srvaddr));
		   memset(&msg,0,sizeof(msg));
           recvfrom(sock,&msg,sizeof(msg),0,NULL,NULL);
           cmd=ntohl(msg.cmd);
		   if(cmd==S2C_LOGIN_OK) //验证通过
		   {
			   strcpy(username, acct.user_name);
			   login=1;		   //登录成功认证
		   }
		   else if (cmd == S2C_ALREADY_LOGINED)//重复登录
		   {
				printf("该帐号已经登录!\n");
				sleep(1);
		   }
		   else //验证失败
		   {
			    printf("您的用户名或密码有误,请重新登录!\n");
			    sleep(1);
		   }
		   break;
	   case 2:
		   system("clear");
		   printf("\t*******************************************************\n");
		   printf("\t*             请    注   册                            *\n");
		   printf("\t*------------------------------------------------------*\n");
		   printf("\t*             用 户 名:                                *\n");
		   printf("\t*------------------------------------------------------*\n");
		   printf("\t*             密 码:                                   *\n");
		   printf("\t*------------------------------------------------------*\n");
		   printf("\t*             确 认 密 码:                             *\n");
		   printf("\t********************************************************\n");
		   scanf("%s",acct.user_name);
		   setbuf(stdin,NULL);
		   scanf("%s",acct.password);
		   setbuf(stdin,NULL);
		   scanf("%s",password);
		   setbuf(stdin,NULL);
		   if(strcmp(acct.password,password)==0)
		   {
			   memset(&msg,0,sizeof(msg));
			   msg.cmd=htonl(C2S_REGISTER);
			   memcpy(msg.body,&acct,sizeof(acct));  //！！！！这样使用的嘛
			   sendto(sock,&msg,sizeof(msg),0,(struct sockaddr*)&srvaddr, sizeof(srvaddr));
			   memset(&msg,0,sizeof(msg));
			   recvfrom(sock,&msg,sizeof(int),0,NULL, NULL);
			   cmd=ntohl(msg.cmd);
			   if(msg.cmd==S2C_REGISTER_OK)
				   printf("注册成功!\n");
			   else
				   printf("该用户已被注册!\n");
		   }
		   else
			   printf("两次密码不一致!请重新输入!\n");
		   sleep(1);
		   break;
	   case 3:
		   exit(0);
		   break;
	   default:
		   break;
	   }
   }
   printf("登录成功!n");
   system("reset");
   printf("“%s”你好!", username);


   //在线人数
   int count;
   recvfrom(sock,(int *)&count,sizeof(int),0,NULL,NULL);
   int n=ntohl((int)count);
   printf("目前在线人数=%d \n",n);
   //接收用户列表
   for(int i=0;i<n;i++)
   {
	   user_info info;
	   recvfrom(sock,&info,sizeof(user_info),0,NULL,NULL);
	   client_list.push_back(info);
	   char name[16];
	   strcpy(name,info.user_name);
	   printf("用户：%s在线中\n",name);
   }

   //已登录客户可以使用命令：
   printf("\t--------聊天室功能:-------\n");
   printf("向其他用户发送消息：输入格式为send xx msg 其中xx是发向的用户名，msg是消息内容\n");
   printf("获取当前在线用户:输入 list\n");
   printf("退出聊天:输入 exit\n");
   printf("\n");

   //使用select处理多个事件。后续可以换成epoll
   fd_set rset;   //文件描述符集合
   FD_ZERO(&rset);
   int nready;    //返回的事件个数
   struct sockaddr_in peeraddr;
   socklen_t peerlen;
   int fd_stdin=fileno(stdin);
   while(1)
   {
	   FD_SET(fd_stdin,&rset);//添加标准输入描述符
	   FD_SET(sock,&rset);        //添加套接字描述符
	   nready=select(sock+1,&rset,NULL,NULL,NULL);
	   if(nready==-1)
	       ERR_EXIT("select");
	   if(nready==0)
		   continue;
	   if(FD_ISSET(sock,&rset)) //套接字接口产生可读事件
	   {
		   peerlen=sizeof(peeraddr);
		   memset(&msg,0,sizeof(msg));
		   recvfrom(sock,&msg,sizeof(msg),0,(struct sockaddr *)&peeraddr,&peerlen);
		   int cmd=ntohl(msg.cmd);
		   //对接收的命令处理
		   switch(cmd)
		   {
		      case S2C_SOMEONE_LOGIN: //有人登录，则调用登录函数
		          do_someone_login(msg);
		       	  break;
		     case S2C_SOMEONE_LOGOUT: //有人登出，则调用登出函数
		       	  do_someone_logout(msg);
		       	  break;
		     case S2C_ONLINE_USER://获取在线列表
		       	  do_recv_list(sock);
		       	  break;
		     case C2C_CHAT://收到私聊
		    	 do_chat(msg);
		    	 break;
		     default:
		       	  break;
	       }
       }
	   if(FD_ISSET(STDIN_FILENO,&rset))
	   {
		   char cmd[100]={0}; //输入命令
		   if(fgets(cmd,sizeof(cmd),stdin)==NULL)
			   break;
		   if(cmd[0]=='\n')
			   continue;
		   cmd[strlen(cmd)-1]='\0';
		   do_cmd(cmd,sock,&srvaddr);//处理输入的命令；
	   }
    }
}

int main()       //()和(void)都表示没有参数
{
   int sock;
   if((sock=socket(PF_INET,SOCK_DGRAM,0))<0)
      ERR_EXIT("socket");
   echo_cli(sock);

   return 0;
}
