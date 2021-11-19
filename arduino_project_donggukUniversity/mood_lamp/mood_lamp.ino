
/*
 ESP8266을 구매하였으면 툴->포트 확인!!, 드라이버 CH340(설치완료)->문제 생길수도 있음
 */

/*************************HEADER****************************/
#include<ESP8266WiFi.h> //ESP 8266 와이파이 라이브러리
#include<LiquidCrystal_I2C.h> //lcd 16x2 I2C 라이브러리
#include <ArduinoJson.h> //JSON 데이터 라이브러리
#include "config.h" //WiFi 정보, Adafruit io setting, Adafruit io key, API key를 담고있는 헤더
#include<time.h> //time 라이브러리
#include<Adafruit_NeoPixel.h> Neopixel 라이브러리
/**********************************************************/

/**********************Adafruit feed***********************/

AdafruitIO_Feed *moodlamp = io.feed("moodlamp"); //Adafruit io On/Off
AdafruitIO_Feed *temp = io.feed("temperature"); //Adafruit io temperature
AdafruitIO_Feed *prec = io.feed("temperature"); //Adafruit io temperature
AdafruitIO_Feed *humd = io.feed("humidity"); //Adafruit io humidity
AdafruitIO_Feed *icon = io.feed("weatherIcon");//Adafruit io weatherIcon

/**********************************************************/

/************************Timer 관련 변수***********************/
char curTime[20];
unsigned long t_timer=0;
bool timeCheck_timer= true;
/**********************************************************/

/***********API 관련 변수******************/
const String API_KEY = "86935eef7f8345573b4384997257271e";

WiFiClient client; //클라이언트 객체 선언

char servername[] = "api.openweathermap.org"; //클라이언트가 접속할 서버이름

String CityID = "1835848";//Seoul,KR


String weatherDescripton =""; //관측하고 있는 도시 날씨의 자세한 설명
String weatherLocation ="";//관측하고 있는 도시
String weatherCountry;//관측하고 있는 국가
float temperature;//관측하고 있는 도시의 온도
float precipitation;//관측하고 있는 도시의 강수량(비,눈 안올시 0)
int weatherID;


/******************************************/

/**************lcd 출력 관련 변수**************/
const int lcdAddress = 0x27;
const int lcdColumns = 16;
const int lcdRows = 2;

String line1String;
String line2String;

bool APIDataReceived = false;

/*******************************************/

/****************핀 설정********************/
LiquidCrystal_I2C lcd(lcdAddress, lcdColumns, lcdRows); //lcd 패널 오브젝트
int weatherIcon[4] = {D5,D6,D7,D8};//날씨 아이콘 led
int mainLight[2] = {D3,D4};

/******************************************/

/*****************led 관련 변수****************/

#define NUMPIXELS 4
#define BRIGHTNESS 30

Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUMPIXELS,D3,NEO_GRB+NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUMPIXELS,D4,NEO_GRB+NEO_KHZ800);


/*********************************************/


/***************함수 설정*******************/

void ISR_API_Client();
void displayGettingData();
void handleLamp();
void lastMoodLampStateCheck();
void timerUpdate();
void ledON();
int weatherInfo();
/******************************************/


/**************무드등 관련 변수**************/
bool moodLampState = false;
bool lastMoodLampState = true;
bool timeCheck= true; //10분에 한번씩 API 값 초기화를 위한 변수
unsigned long t =0;
/*****************************************/

void setup() {
  Serial.begin(115200); //시리얼 통신 Rate:115200
  Serial.println();
  while(! Serial);
  /***************LCD I2C*******************/

  int cursorPostion = 0;
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Connecting");
  /******************************************/
  
  /***************Adafruit io*******************/
  io.connect(); // io.adafruit.com과 MQTT 통신 시작 
  moodlamp->onMessage(handleLamp);
  int i=1;
  
  while(io.mqttStatus() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    if(i%4==0){
      i=1;
      lcd.setCursor(10+i,0);
      lcd.print("   ");
    }
    lcd.setCursor(10+i,0);
    i++;
    lcd.print(".");
    delay(500);
  }
  moodlamp->get();
  Serial.println();
  Serial.println(io.statusText()); // connected!
  /******************************************/
  configTime(9*3600, 0, "pool.ntp.org", "time.nist.gov");
  
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Connnected!");
  delay(1000);

  strip1.setBrightness(BRIGHTNESS);
  strip1.begin();
  strip1.show();

  strip2.setBrightness(BRIGHTNESS);
  strip2.begin();
  
  ISR_API_Client();
  timerUpdate();
}

void loop() {
  io.run();
  /************램프 동작************/
  if(moodLampState) {
    lastMoodLampStateCheck();
    if(timeCheck) {
      t= millis();
      timeCheck= false;
    } else if(!timeCheck && millis()-t > 600000){
      timeCheck = true;
      ISR_API_Client();
    }
    if(timeCheck_timer && APIDataReceived) {
      t_timer = millis();
      timeCheck_timer = false;
    } else if(!timeCheck_timer & millis()-t_timer > 10000) {
      timeCheck_timer = true;
      timerUpdate();
    }
    if(APIDataReceived) {
      ledON();
    }
    
    
  } else {
    lastMoodLampStateCheck();
  }
  /********************************/
  
  
}

int weatherInfo(){
  if((weatherID / 100 == 8) && (weatherID % 100 == 0)) return 0;
  if((weatherID / 100 == 7) || (weatherID / 100 == 8)) return 1;
  if((weatherID / 100 == 3)|| (weatherID / 100 == 5)) return 2;//Rain
  if(weatherID / 100 == 6) return 3; //Snow
  if(weatherID / 100 == 2) return 4;  // Thunderstorm
}

void ledON() {
  Serial.println(weatherInfo());
  for(int i=0;i<4;i++) {
    if(i==weatherInfo()) {
      strip1.setPixelColor(i,255,255,255);
      strip1.show();
    } else {
      strip1.setPixelColor(i,0,0,0);
      strip1.show();
    }
    if(weatherInfo() == 4) {
      strip1.setPixelColor(i,255,255,255);
    }
  }
}

void timerUpdate() {
  int timeIndex= 0;
      
      time_t now = time(nullptr);
      struct tm *tt;
      tt = localtime(&now);
      if(tt->tm_hour >= 10) {
        timeIndex =1;
      } else {
        timeIndex =0;
      }
      sprintf(curTime,"%02d:%02d",tt->tm_hour, tt->tm_min);
      lcd.setCursor(10+timeIndex,0);
      lcd.print(curTime);
}

void lastMoodLampStateCheck() {
  if(lastMoodLampState != moodLampState) {
      moodlamp->save(moodLampState);
      lastMoodLampState = moodLampState;
      Serial.print("MoodLampState: ");
      Serial.println(moodLampState);
      if(moodLampState) {
        lcd.clear();
        ISR_API_Client();
        timerUpdate();
        lcd.backlight();
      } else {
        lcd.clear();
        lcd.noBacklight();
      }
  }
}

void handleLamp(AdafruitIO_Data * data) {
  int state = data->toInt();
  switch(state) {
    case 0: moodLampState = false; break;
    case 1: moodLampState = true; break;
  }
  Serial.print("received <- ");
  Serial.println(data->value());
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
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  delay(1000);
  if(client.connect(servername, 80)) {
    client.println("GET /data/2.5/weather?id="+CityID+"&units=metric&APPID="+API_KEY);
    client.println("Host: api.openweathermap.org");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }
  //클라이언트 API 연결
  while(client.connected() && !client.available()) delay(10);
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
  int ID = root["weather"]["id"];



  weatherLocation = location;
  weatherCountry = country;
  weatherDescripton = description;
  temperature = _temperature;
  weatherID = ID;

  Serial.print("weatherLocation: ");
  Serial.println(weatherLocation);
  Serial.print("weatherCountry: ");
  Serial.println(weatherCountry);
  Serial.print("weatherDescripton: ");
  Serial.println(weatherDescripton);
  Serial.print("temperature: ");
  Serial.println(temperature);

  String _temp = String(temperature)+"℃";
  temp->save(_temp);

  line1String="";
  line2String="";
  
  line1String+=weatherLocation+", "+weatherCountry;
  line2String+=weatherDescripton+", T: "+String(temperature);
  APIDataReceived = true;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1String);
  lcd.setCursor(0,1);
  lcd.print(line2String);
  //데이터 가져오기
}
