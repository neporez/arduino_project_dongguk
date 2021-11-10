/*
 ESP8266을 구매하였으면 툴->포트 확인!!, 드라이버 CH340(설치완료)->문제 생길수도 있음
 */


#include<ESP8266WiFi.h> //ESP 8266 와이파이 라이브러리
#include<ESP8266WebServer.h> //ESP 8266 웹서버 라이브러리

ESP8266WebServer server(80); //웹서버 오브젝트

int soundSensor = A0; //사운드 센서
int weatherIcon[4] = {D5,D6,D7,D8};//날씨 아이콘 led
int mainLight[2] = {D3,D4};//led strip

void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.begin("wifi-name","wifi-password");//와이파이 이름과 패스워드 *2.4GHz 주파수만 가능

  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED){ //와이파이에 접속중...
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address : ");
  Serial.print(WiFi.localIP()); //현재 접속한 IP주소 출력

  server.on("/",handleRoot);// 루트에 접속하였을때 실행될 함수 지정

  server.begin();//웹서버 시작
  Serial.println("HTTP server started");
  
}

void handleRoot() { //루트에 접속할때 실행
  server.send(200,"text/plain","hello from esp8266");//루트로 접속하면 뜨는 메세지
}

void loop() {
  server.handleClient();
}
