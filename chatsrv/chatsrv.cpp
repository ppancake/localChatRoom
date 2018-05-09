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
     }while(0)     // 出错处理  换行加转义
USER_LIST client_list;  //聊天室成员表，全局变量

void do_login(message &msg,int sock,struct sockaddr_in *cliaddr); //登录处理
void do_register(message &msg,int sock,struct sockaddr_in *cliaddr);
void do_logout(message &msg,int sock,struct sockaddr_in *cliaddr);//登出处理
void do_sendlist(int sock,struct sockaddr_in *cliaddr); //发送成员列表


//服务端处理函数
void echo_srv(int sock)
{
   struct sockaddr_in cliaddr; //客户端地址
   socklen_t clilen;  //客户端地址长度
   int ret; //返回值
   message msg;
   while(1)
   {
	  memset(&msg,0,sizeof(msg));
	  clilen=sizeof(cliaddr);
      ret=recvfrom(sock,&msg,sizeof(msg), 0,(struct sockaddr*)&cliaddr, &clilen); //接受客户端的消息
      if(ret==-1)
      {
         if(errno==EINTR) continue;
         ERR_EXIT("recvfrom");
      }
     //对接收的消息种类判断
      int cmd=ntohl(msg.cmd);//客户端命令，字节须转换，网络-主机
      switch(cmd)
      {
      case C2S_LOGIN: //登录，则调用登录函数
    	  do_login(msg,sock,&cliaddr);
    	  break;
      case C2S_REGISTER:
    	  do_register(msg,sock,&cliaddr);
    	  break;
      case C2S_LOGOUT: //登出，则调用登出函数
    	  do_logout(msg,sock,&cliaddr);
    	  break;
      case C2S_ONLINE_USER://请求成员列表
    	  do_sendlist(sock,&cliaddr);
    	  break;
      default:
    	  break;
      }
   }
}
bool is_pass(account acct,account acct1)
{
    if((acct.user_name==acct1.user_name)&&(acct.password==acct1.password))
    	return true;
    else
    	return false;
}

void do_login(message &msg,int sock,struct sockaddr_in *cliaddr)
{
   user_info info;
   account *user=(account *)&msg.body;
   //用msg对user初始化
   strcpy(info.user_name,user->user_name);
   info.ip=cliaddr->sin_addr.s_addr;//都是网络字节序
   info.port=cliaddr->sin_port;

   //用户表中查找用户，遍历client_list;
   USER_LIST::iterator it;
   for(it=client_list.begin();it!=client_list.end();it++)
   {
	   if(strcmp((*it).user_name,user->user_name)==0) //找到用户，已登录
	   {
		   break;
	   }

   }
   if(it==client_list.end()) //在end()，在线的未找到.看是否注册
   {
	   //判断是否有用户：
	   FILE *fd;
	   int ret;
	   bool has=false;
	   fd=fopen("./account","r");//只读打开
	   if(fd==NULL)
	   {
		   ERR_EXIT("open fail");
	   }
	   account acct1;
	   ret=fread(&acct1,sizeof(acct1),1,fd);  //fread函数读取一个到acct1中

	   while(ret>0)
	   {
		   if(is_pass(*user,acct1))
		   {
			   has=true;
			   break;
		   }
		   ret=fread(&acct1,sizeof(acct1),1,fd);
	   }
	   fclose(fd);
	   if(has) //登录成功 S2C_LOGIN_OK
	   {
		   message reply;
		   memset(&reply,0,sizeof(reply));
		   reply.cmd=htonl(S2C_LOGIN_OK);
		   sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
		   client_list.push_back(info);
		   int count=htonl((int)client_list.size());
		   sendto(sock,(int *)&count,sizeof(int),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
		   printf("发送在线人数给%s: 有%d人在线\n",user->user_name,client_list.size());
		   for(it=client_list.begin();it!=client_list.end();it++)
		   {
			   sendto(sock,&*it,sizeof(user_info),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
		   }

		   //向其他用户通知有用户登录
		   for(it=client_list.begin();it!=client_list.end();it++)
		   {
			   message reply;
			   memset(&reply,0,sizeof(reply));
			   if(strcmp((*it).user_name,info.user_name)==0)//刚登录的用户不发acct
				   continue;
			   struct sockaddr_in peeraddr;
			   memset(&peeraddr,0,sizeof(peeraddr));
			   peeraddr.sin_family=AF_INET;
			   peeraddr.sin_port=it->port;
			   peeraddr.sin_addr.s_addr=it->ip;
			   reply.cmd=htonl(S2C_SOMEONE_LOGIN);
			   memcpy(reply.body,&info,sizeof(info));   //将用户的信息转换到msg.body中？？？
			   sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)&peeraddr, sizeof(peeraddr));
		   }
	   }
	   else// 登录失败
	   {
		   message reply;
		   memset(&reply,0,sizeof(reply));
		   reply.cmd=htonl(S2C_LOGIN_ERROR);
		   sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
	   }

   }
   //找到用户，已登录
   else
   {
	   printf("用户%s已登录\n",user->user_name);
	   message reply;
	   memset(&reply,0,sizeof(reply));
	   reply.cmd=htonl(S2C_ALREADY_LOGINED);
	   sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
   }
}

void do_register(message &msg,int sock,struct sockaddr_in *cliaddr)
{
	account *user=(account *)&msg.body;
	FILE *fd,*rd;
	int ret;
	bool has=false;
	fd=fopen("./account.txt","r");//只读打开
	if(fd==NULL)
	{
	  ERR_EXIT("open fail");
    }
	account acct1;
	ret=fread(&acct1,sizeof(acct1),1,fd);  //fread函数读取一个到acct1中
    while(ret>0)
	{
		if(is_pass(*user,acct1))
		{
			has=true;
			break;
		}
		ret=fread(&acct1,sizeof(acct1),1,fd);
	}
    fclose(fd);
    rd = fopen("./account.txt", "a+");//打开可读写，不在会建立
    if(has)
    {
    	printf("注册失败\n",user->user_name);
        message reply;
    	memset(&reply,0,sizeof(reply));
    	reply.cmd=htonl(S2C_REGISTER_ERROR);
    	sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
    }
    else
    {
    	printf("注册成功\n",user->user_name);
    	ret=fwrite(user,sizeof(account),1,rd);
    	if (ret == 0)
    	{
    		ERR_EXIT("write fail");
    	}
    	fclose(rd);
    	message reply;
    	memset(&reply,0,sizeof(reply));
    	reply.cmd=htonl(S2C_REGISTER_OK);
    	sendto(sock,&reply,sizeof(reply),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
    }
}

void do_logout(message &msg,int sock,struct sockaddr_in *cliaddr)
{
	printf("用户等出:%s-%s:%d\n",msg.body,inet_ntoa(cliaddr->sin_addr),ntohs(cliaddr->sin_port));
	//删除此用户
	USER_LIST::iterator it;
	for(it=client_list.begin();it!=client_list.end();it++)
	{
		if(strcmp(it->user_name,msg.body)==0)
			break;
	}
	if(it!=client_list.end())
		client_list.erase(it);
	//通知其他用户
	for(it=client_list.begin();it!=client_list.end();it++)
	{
		if(strcmp(it->user_name,msg.body)==0)
			continue;
	    struct sockaddr_in peeraddr;
	    memset(&peeraddr,0,sizeof(peeraddr));
	    peeraddr.sin_family=AF_INET;
        peeraddr.sin_port=it->port;
	    peeraddr.sin_addr.s_addr=it->ip;
	    msg.cmd=htonl(S2C_SOMEONE_LOGOUT);
     	sendto(sock,&msg,sizeof(msg),0,(struct sockaddr *)&peeraddr, sizeof(peeraddr));
	}
}
void do_sendlist(int sock,struct sockaddr_in *cliaddr)
{
    message msg;
    msg.cmd=htonl(S2C_ONLINE_USER);
    sendto(sock,&msg,sizeof(msg),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
    int count=htonl((int)client_list.size());
    sendto(sock,&count,sizeof(int),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
    for(USER_LIST::iterator it=client_list.begin();it!=client_list.end();it++)
    	 sendto(sock,&*it,sizeof(user_info),0,(struct sockaddr*)cliaddr, sizeof(*cliaddr));
}

int main(void)
{
   int sock;
   if((sock=socket(PF_INET,SOCK_DGRAM,0))<0)   //创建socket
      ERR_EXIT("socket");
   struct sockaddr_in srvaddr;
   memset(&srvaddr,0,sizeof(srvaddr));
   srvaddr.sin_family=AF_INET;  //初始化服务端地址
   srvaddr.sin_port=5188;
   srvaddr.sin_addr.s_addr=htonl(INADDR_ANY);
   if(bind(sock, (struct sockaddr*)&srvaddr,sizeof(srvaddr))<0)  //绑定地址
      ERR_EXIT("bind");
   echo_srv(sock);  //服务端处理函数
   return 0; 
}
