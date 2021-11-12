/*
 ESP8266을 구매하였으면 툴->포트 확인!!, 드라이버 CH340(설치완료)->문제 생길수도 있음
 */
#include<ESP8266WiFi.h> //ESP 8266 와이파이 라이브러리
#include<ESP8266WebServer.h> //ESP 8266 웹서버 라이브러리
#include<LiquidCrystal_I2C.h> //lcd 16x2 I2C 라이브러리
#include <ArduinoJson.h> //JSON 데이터 라이브러리
#include<MsTimer2.h> //MsTimer2 라이브러리

LiquidCrystal_I2C lcd(0x3F, 16, 2); //lcd 패널 오브젝트
ESP8266WebServer server(80); //웹서버 오브젝트

WiFiClient client; //클라이언트 객체 선언

char servername[] = "api.openweathermap.org" //클라이언트가 접속할 서버이름


/*
 API 값을 담을 변수
 */
String result;
String weatherDescripton ="";
String weatherLocation ="";
String weatherCountry;
float temperature;

const char*ssid = "wifi-name"; //접속할 와이파이 이름
const char*password = "wifi-password";//접속할 와이파이 비밀번호

String APIKEY = "API KEY" //API KEY
String CityID = "1835848";//Seoul,KR

int soundSensor = A0; //사운드 센서
int weatherIcon[4] = {D5,D6,D7,D8};//날씨 아이콘 led
int mainLight[2] = {D3,D4};//led strip

void handleRoot();
void ISR_API_Client();
void displayGettingData();




void setup() {
  Serial.begin(115200); //시리얼 통신 Rate:115200
  Serial.println();

  int cursorPostion = 0;
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Connecting");
  /*
   lcd 초기화
   */


  WiFi.begin(ssid,password);//와이파이 이름과 패스워드 *2.4GHz 주파수만 가능

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
  lcd.clear();
  lcd.print(" Connnected!");
  MsTimer2::set(600000,ISR_API_Client); //10분에 한번 호출, 보드 도착 후 실행 후 10분인지 10분후 실행인지 확인할 것
  MsTimer2::start(); //타이머 인터럽트 시작
  
}

void loop() {
  server.handleClient();
  
}

void handleRoot() { //루트에 접속할때 실행
  server.send(200,"text/plain","hello from esp8266");//루트로 접속하면 뜨는 메세지
}

void displayGettingData() {
  lcd.clear();
  lcd.print("Getting data");
}

/*
https://diy-project.tistory.com/73#google_vignette 참조
타이머 인터럽트 함수(10분에 한번씩 API 정보 호출)
*/
void ISR_API_Client() {
  displayGettingData();
  delay(1000);
  if(client.connect(servername, 80)) {
    client.println("GET /data/2.5/weather?id="+CityID+"&units=metric&APPID="+APIKEY);
    client.println("Host: api.openweathermap.org");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }
  //클라이언트 API 연결
  while(client.connected() && !client.available()) delay(1);
  while(client.connected() || client.available()) {
    char c = client.read();
    result = result+c;
  }
  //API 정보 가져오기
  client.stop();
  result.replace('[',' ');
  result.replace(']',' ');
  Serial.println(result);

  char jsonArray [result.length()+1];
  result.toCharArray(jsonArray,sizeof(jsonArray));
  jsonArray[result.length()+1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if(!root.success()) {
    Serial.println("parseObject() failed");
  }
  //JsonArray 형태로 데이터 정리
  String location = root["name"];
  String country = root["sys"]["country"];
  String description = root["weather"]["description"];
  float _temperature = root["main"]["temp"];

  weatherLocation = location;
  weatherCountry = country;
  weatherDescripton = description;
  temperature = _temperature;
  
  //데이터 가져오기
}
