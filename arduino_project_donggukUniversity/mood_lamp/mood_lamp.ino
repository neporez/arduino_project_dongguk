/*
 ESP8266을 구매하였으면 툴->포트 확인!!, 드라이버 CH340(설치완료)->문제 생길수도 있음
 */

/*************************HEADER****************************/
#include<ESP8266WiFi.h> //ESP 8266 와이파이 라이브러리
#include<LiquidCrystal_I2C.h> //lcd 16x2 I2C 라이브러리
#include <ArduinoJson.h> //JSON 데이터 라이브러리
#include<MsTimer2.h> //MsTimer2 라이브러리
#include "config.h" //WiFi 정보, Adafruit io setting, Adafruit io key, API key를 담고있는 헤더
/**********************************************************/

/**********************Adafruit feed***********************/

AdafruitIO_Feed *moodlamp = io.feed("moodlamp"); //Adafruit io On/Off
AdafruitIO_Feed *temp = io.feed("temperature"); //Adafruit io temperature
AdafruitIO_Feed *prec = io.feed("temperature"); //Adafruit io temperature
AdafruitIO_Feed *humd = io.feed("humidity"); //Adafruit io humidity
AdafruitIO_Feed *icon - io.feed("weatherIcon");//Adafruit io weatherIcon

/**********************************************************/



/***********API 관련 변수******************/

WiFiClient client; //클라이언트 객체 선언

char servername[] = "api.openweathermap.org" //클라이언트가 접속할 서버이름

String APIKEY = "API KEY" //API KEY
String CityID = "1835848";//Seoul,KR

String result; //API 데이터를 모두 긁어와 저장하는 변수
String weatherDescripton =""; //관측하고 있는 도시 날씨의 자세한 설명
String weatherLocation ="";//관측하고 있는 도시
String weatherCountry;//관측하고 있는 국가
String weatherIcon; //관측하고 있는 도시의 날씨 아이콘
float temperature;//관측하고 있는 도시의 온도
float precipitation;//관측하고 있는 도시의 강수량(비,눈 안올시 0)

/******************************************/




/****************핀 설정********************/
LiquidCrystal_I2C lcd(0x3F, 16, 2); //lcd 패널 오브젝트
int soundSensor = A0; //사운드 센서
int weatherIcon[4] = {D5,D6,D7,D8};//날씨 아이콘 led
int mainLight[2] = {D3,D4};//led strip

/******************************************/




/***************함수 설정*******************/

void ISR_API_Client();
void displayGettingData();
void handleLamp();

/******************************************/


/**************무드등 관련 함수**************/
bool moodLampState = false;




/******************************************/

void setup() {
  Serial.begin(115200); //시리얼 통신 Rate:115200
  Serial.println();

  while(! Serial);
  
  /***************LCD I2C*******************/

  int cursorPostion = 0;
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Connecting");
  /******************************************/
  
  /***************Adafruit io*******************/
  io.connect(); // io.adafruit.com과 MQTT 통신 시작 
  moodlamp->onMessage(handleLamp);
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  moodlamp->get();
  Serial.println();
  Serial.println(io.statusText()); // connected!
  /******************************************/

  lcd.print(" Connnected!");
  

  /***************MsTimer2*******************/
  MsTimer2::set(600000,ISR_API_Client); //10분에 한번 호출, 보드 도착 후 실행 후 10분인지 10분후 실행인지 확인할 것
  MsTimer2::start(); //타이머 인터럽트 시작
  /******************************************/
}

void loop() {
  io.run();
  
}

void handleLamp(AdafruitIO_Data * data) {
  int state = data->toInt();
  switch(state) {
    case 0: moodLampState = false; break;
    case 1: moodLampState = true; break;
  }
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
