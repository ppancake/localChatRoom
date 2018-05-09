#ifndef  _PUB_H_
#define _PUB_H
#include <list>
#include <algorithm>
using std::list;

//C2S
#define C2S_LOGIN    0x01    //客户端登陆
#define C2S_LOGOUT   0x02    //客户端登出
#define C2S_ONLINE_USER   0x03  //查看在线列表
#define C2S_REGISTER 0x04     //用户注册
#define MSG_LEN     512      //消息长度

//S2C
#define S2C_LOGIN_OK   0x01    //对客户端登陆响应成功
#define S2C_ALREADY_LOGINED 0x02  //用户已经登陆
#define S2C_SOMEONE_LOGIN 0x03  //有用户登陆，服务器像其他客户端响应
#define S2C_SOMEONE_LOGOUT 0x04  //有客户登出，服务器像其他客户端响应
#define S2C_ONLINE_USER 0x05  //客户请求在线列表
#define S2C_LOGIN_ERROR 0x06   //登录错误
#define S2C_REGISTER_ERROR 0x07   //注册错误
#define S2C_REGISTER_OK 0x08   //注册成功
//C2C
#define C2C_CHAT 0x06  //客户端之间私聊  //公聊实现：服务器通知/客户端和其他所有私聊

//消息结构
struct message
{
	int cmd;   //命令
	char body[MSG_LEN]; //内容
};

//用户ip信息
struct user_info
{
	char user_name[16]; //名字
	unsigned int ip;  //IP地址，4位字节
	unsigned short port; //端口号
};
//用户帐号信息
struct account
{
    char user_name[16];
    char password[20];
};
//客户端之间消息
struct chat_msg
{
	char username[16];
	char msg[100];
};

typedef list<user_info> USER_LIST;
#endif // ! _PUB_H_
