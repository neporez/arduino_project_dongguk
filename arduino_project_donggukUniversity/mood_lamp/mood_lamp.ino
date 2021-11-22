
/*
 *본 저작물은 '한국환경공단'에서 실시간 제공하는 '한국환경공단_에어코리아_대기오염정보'를 이용하였습니다.
 *https://www.data.go.kr/tcs/dss/selectApiDataDetailView.do?publicDataPk=15073861
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
AdafruitIO_Feed *moodlampMode = io.feed("mode"); //Adafruit io moodlampMode

/**********************************************************/

/************************Timer 관련 변수***********************/
char curTime[20];
unsigned long t_timer=0;
bool timeCheck_timer= true;
/**********************************************************/


/***********API 관련 변수******************/
WiFiClient client; //클라이언트 객체 선언

char servername[] = "api.openweathermap.org"; //클라이언트가 접속할 서버이름
char servername_pt[] = "apis.data.go.kr";


String CityID = "1835848";//Seoul,KR


String weatherDescripton =""; //관측하고 있는 도시 날씨의 자세한 설명
String weatherLocation ="";//관측하고 있는 도시
String weatherCountry;//관측하고 있는 국가
String weatherDate; //관측하고 있는 시간
float temperature;//관측하고 있는 도시의 온도
int weatherID; //날씨 상태


#define CURRENT_WEATHER "current weather"
#define HOURLY_FORECAST "hourly forecast"
#define PARTICULATE_MATTER "particulate_matter"

int hourly_forecast_time = 0;

int particulate_state = 0;


char CITY[] = "중구";
char CITY_ENCODE[120];
/******************************************/

/**************lcd 출력 관련 변수**************/
const int lcdAddress = 0x27;
const int lcdColumns = 16;
const int lcdRows = 2;

String line1String;
String line2String;

bool APIDataReceived = false;
bool PT_APIDataReceived = false;
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

void CurrnetAPIDataRecieved();
void HourlyForecastAPIDataRecieved();
void ParticulateMatterAPIRecieved();
int getScore(float so2, float co, float o3, float no2, float pm10,float pm25);
void setLEDColor(int s);
void APIDataLCDPrint();
void displayGettingData();
void handleLamp();
void lastMoodLampStateCheck();
void timerUpdate();
void mainLEDON();
int weatherInfo();

static char hex_digit(char c);
char *urlencode(char *dst,char *src);
/******************************************/


/**************무드등 관련 변수**************/
bool moodLampState = false;
bool lastMoodLampState = true;
bool timeCheck= true; //10분에 한번씩 API 값 초기화를 위한 변수
unsigned long t =0; //API 값을 받아올때 시간을 측정하기 위한 변수

String moodlampModeState = PARTICULATE_MATTER;
/*****************************************/

void setup() {
  Serial.begin(115200); //시리얼 통신 Rate:115200
  Serial.println();
  urlencode(CITY_ENCODE,CITY);

  
  
  while(! Serial);
  /***************LCD I2C*******************/
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Connecting");
  /******************************************/

  strip1.setBrightness(BRIGHTNESS);
  strip1.begin();
  strip1.show();

  strip2.setBrightness(BRIGHTNESS);
  strip2.begin();
  strip2.show();
  
  /***************Adafruit io*******************/
  io.connect(); // io.adafruit.com과 MQTT 통신 시작 
  moodlamp->onMessage(handleLamp);
  int i=1;
  
  while(io.mqttStatus() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    mainLEDON();
    if(i%4==0){
      i=1;
      lcd.setCursor(10+i,0);
      lcd.print("   ");
      delay(500);
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
  if(moodlampModeState == CURRENT_WEATHER) {
   CurrnetAPIDataRecieved(); 
   timerUpdate();
  }
  if(moodlampModeState == HOURLY_FORECAST) {
   HourlyForecastAPIDataRecieved(); 
  }
  if(moodlampModeState == PARTICULATE_MATTER){
    ParticulateMatterAPIRecieved();
    CurrnetAPIDataRecieved();
    timerUpdate();
  }
  
  
 
}

void loop() {
  io.run();
  /************************************//**램프 동작**//*****************************************/
  if(moodLampState) {
    lastMoodLampStateCheck();
    mainLEDON();
    /************************현재 날씨 모드**************************/
    if(moodlampModeState == CURRENT_WEATHER) {
      if(timeCheck) {
        t= millis();
        timeCheck= false;
      } else if(!timeCheck && millis()-t > 10000){
        timeCheck = true;
        CurrnetAPIDataRecieved();
      }
    }
    /*************************************************************/

    /************************내일 날씨 모드*************************/
    if(moodlampModeState == HOURLY_FORECAST) {
      if(timeCheck) {
        t= millis();
        timeCheck= false;
      } else if(!timeCheck && millis()-t > 10000){
        timeCheck = true;
        HourlyForecastAPIDataRecieved();
      }
    }
    /*************************************************************/

    /************************미세 먼지 모드*************************/
    if(moodlampModeState == PARTICULATE_MATTER) {
      if(timeCheck) {
        t= millis();
        timeCheck= false;
      } else if(!timeCheck && millis()-t > 10000){
        timeCheck = true;
        ParticulateMatterAPIRecieved();
        CurrnetAPIDataRecieved();
      }
    }
 

    /*************************************************************/


    /***********************시간 표시******************************/
    if(timeCheck_timer && APIDataReceived && (moodlampModeState != HOURLY_FORECAST)) {
        t_timer = millis();
        timeCheck_timer = false;
      } else if(!timeCheck_timer & millis()-t_timer > 10000) {
        timeCheck_timer = true;
        timerUpdate();
    }
    if(!APIDataReceived) {
      lcd.clear();
      lcd.setCursor(0,0);
    }
    /**************************************************************/

    /*****************모드에 따라 LED 점등***************************/
    
    
    /*************************************************************/

    
  } else {
    lastMoodLampStateCheck(); //램프가 꺼졌을 때
  }
  /***********************************//*램프동작 끝*//***************************************/
  
  
}

int weatherInfo(){ // 날씨 정보 받아오기
  if((weatherID / 100 == 8) && (weatherID % 100 == 0)) return 0;//Clear
  if((weatherID / 100 == 7) || (weatherID / 100 == 8)) return 1;//Clouds
  if((weatherID / 100 == 3)|| (weatherID / 100 == 5)) return 2;//Rain
  if(weatherID / 100 == 6) return 3; //Snow
  if(weatherID / 100 == 2) return 4;  // Thunderstorm
}

void mainLEDON() { //모드에 따라 메인 led의 불빛이 다르다.
  if((moodlampModeState == CURRENT_WEATHER) || (moodlampModeState == HOURLY_FORECAST) || (!PT_APIDataReceived)) { 
    for(int i=0;i<4;i++) {
      strip1.setPixelColor(i,255,100,0);
      strip2.setPixelColor(i,255,100,0);
      strip1.show();
      strip2.show();
    }
  }
  if(moodlampModeState == PARTICULATE_MATTER && (PT_APIDataReceived)) {
    setLEDColor(particulate_state);
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

void lastMoodLampStateCheck() { //램프의 상태가 변경되었을때 마지막 상태를 저장하고 램프를 초기화
  if(lastMoodLampState != moodLampState) {
      moodlamp->save(moodLampState);
      lastMoodLampState = moodLampState;
      Serial.print("MoodLampState: ");
      Serial.println(moodLampState);
      if(moodLampState) {
        lcd.clear();
        CurrnetAPIDataRecieved();
        timerUpdate();
        lcd.backlight();
      } else {
        lcd.clear();
        lcd.noBacklight();
      }
  }
}

void handleLamp(AdafruitIO_Data * data) { //Adafruit io 에서 램프 스위치의 상태를 변화시키는 함수
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

void APIDataLCDPrint() {
  if(moodlampModeState == CURRENT_WEATHER) {//현재 날씨 모드
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
  }

  if(moodlampModeState == HOURLY_FORECAST) {//시간별 날씨 모드
    line1String="";
    line2String="";
  
    line1String+=weatherLocation+", "+weatherDate.substring(5,13)+"h";
    line2String+=weatherDescripton+", T: "+String(temperature);
  
    APIDataReceived = true;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(line1String);
    lcd.setCursor(0,1);
    lcd.print(line2String);
  }

  if(moodlampModeState == PARTICULATE_MATTER) {//미세 먼지 모드
    line1String="";
    line2String="";
    
    line1String+=weatherLocation+", "+weatherCountry+" "+String(particulate_state);
    line2String+=weatherDescripton+", T: "+String(temperature);
  
    APIDataReceived = true;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(line1String);
    lcd.setCursor(0,1);
    lcd.print(line2String);
    
  }
}



/*
https://diy-project.tistory.com/73#google_vignette 참조
*/
void CurrnetAPIDataRecieved() {
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
  
  result = client.readStringUntil('\r');
  
  //API 정보 가져오기
  client.stop();
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
  String description = root["weather"][0]["description"];
  float _temperature = root["main"]["temp"];
  int ID = root["weather"][0]["id"];



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


  //lcd 에 API 정보 담기

  
  //데이터 가져오기

  APIDataLCDPrint();
}

void HourlyForecastAPIDataRecieved() {
  displayGettingData();
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  delay(1000);
  if(client.connect(servername, 80)) {
    client.println("GET /data/2.5/forecast?id="+CityID+"&appid="+API_KEY+"&mode=json&units=metric&cnt=11");
    client.println("Host: api.openweathermap.org");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }

  //클라이언트 API 연결
  while(client.connected() && !client.available()) delay(10);

  result = client.readStringUntil('\r');

  Serial.println(result);
  String result_short = "{\"list\":[{\"main\":{";
  String result_short_middle;
  int lastIndex = 0;

  //result_short_middle += result.substring(result.indexOf("\"temp\""),result.indexOf(",",result.indexOf("\"temp\"")))+"},";
  lastIndex = result.indexOf(",",result.indexOf("\"temp\"")); 
  //result_short_middle += result.substring(result.indexOf("\"weather\"",lastIndex),result.indexOf("],",lastIndex))+"],";
  lastIndex = result.indexOf("],",lastIndex);
  //result_short_middle += result.substring(result.indexOf("\"dt_txt\"",lastIndex),result.indexOf(":00\"},",lastIndex))+"\"},{\"main\":{";

  
  lastIndex = result.indexOf(":00\"},",lastIndex);

   
  for(int i=0;i<9;i++) {
    if(i%2==0) {
      lastIndex = result.indexOf(",",lastIndex); 
      lastIndex = result.indexOf("],",lastIndex);
      lastIndex = result.indexOf(":00\"},",lastIndex);
    } else {
    result_short_middle += result.substring(result.indexOf("\"temp\"",lastIndex),result.indexOf(",",result.indexOf("\"temp\"",lastIndex)))+"},";
    lastIndex = result.indexOf(",",lastIndex); 
    result_short_middle += result.substring(result.indexOf("\"weather\"",lastIndex),result.indexOf("],",lastIndex))+"],";
    lastIndex = result.indexOf("],",lastIndex);
    result_short_middle += result.substring(result.indexOf("\"dt_txt\"",lastIndex),result.indexOf(":00\"},",lastIndex))+"\"},{\"main\":{";
    lastIndex = result.indexOf(":00\"},",lastIndex);
    }
  }
    result_short_middle += result.substring(result.indexOf("\"temp\"",lastIndex),result.indexOf(",",result.indexOf("\"temp\"",lastIndex)))+"},";
    lastIndex = result.indexOf(",",lastIndex); 
    result_short_middle += result.substring(result.indexOf("\"weather\"",lastIndex),result.indexOf("],",lastIndex))+"],";
    lastIndex = result.indexOf("],",lastIndex);
    result_short_middle += result.substring(result.indexOf("\"dt_txt\"",lastIndex));
  

  result_short+=result_short_middle;
  Serial.println(result_short);
  
  //API 정보 가져오기
  client.stop();
  
  char jsonArray [result_short.length()+1];
  result_short.toCharArray(jsonArray,sizeof(jsonArray));
  jsonArray[result_short.length()+1] = '\0';

  StaticJsonBuffer<1536> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if(!root.success()) {
    Serial.println("parseObject() failed");
  }
  //JsonArray 형태로 데이터 정리

  String location = root["city"]["name"];
  String country = root["country"];
  String description = root["list"][hourly_forecast_time]["weather"][0]["description"];
  String date = root["list"][hourly_forecast_time]["dt_txt"];
  float _temperature = root["list"][hourly_forecast_time]["main"]["temp"];
  int ID = root["list"][hourly_forecast_time]["weather"][0]["id"];
  

  weatherLocation = location;
  weatherCountry = country;
  weatherDescripton = description;
  weatherDate = date;
  temperature = _temperature;
  weatherID = ID;
  

  APIDataLCDPrint();
}
 
void ParticulateMatterAPIRecieved() {
  displayGettingData();
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  delay(1000);
  if(client.connect(servername_pt, 80)) {
    client.println("GET /B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?stationName="+String(CITY_ENCODE)+"&dataTerm=month&pageNo=1&numOfRows=1&returnType=json&serviceKey="+API_KEY_PT);
    client.println("Host: apis.data.go.kr");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }

  //클라이언트 API 연결
  while(client.connected() && !client.available()) delay(10);

  result = client.readString();

  client.stop();

  char jsonArray [result.length()+1];
  result.toCharArray(jsonArray,sizeof(jsonArray));
  jsonArray[result.length()+1] = '\0';

  StaticJsonBuffer<800> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if(!root.success()) {
    Serial.println("parseObject() failed");
  }

  float so2 = String(root["response"]["body"]["items"][0]["so2Value"]).toFloat();
  float co = String(root["response"]["body"]["items"][0]["coValue"]).toFloat();
  float o3 = String(root["response"]["body"]["items"][0]["o3Value"]).toFloat();
  float no2 = String(root["response"]["body"]["items"][0]["no2Value"]).toFloat();
  float pm10 = String(root["response"]["body"]["items"][0]["pm10Value"]).toFloat();
  float pm25 = String(root["response"]["body"]["items"][0]["pm25Value"]).toFloat();

  
  particulate_state = getScore(so2,co,o3,no2,pm10,pm25);
  PT_APIDataReceived = true;
  
}
int getScore(float so2, float co, float o3, float no2, float pm10,float pm25) {
  int s = -1;
  if (pm10 >= 151 || pm25 >= 76 ||  o3 >= 0.38 || no2 >= 1.1 || co >= 32 || so2 > 0.6) // 최악
    s = 7;
  else if (pm10 >= 101  || pm25 >= 51 || o3 >= 0.15 || no2 >= 0.2 || co >= 15 || so2 > 0.15) // 매우 나쁨
    s = 6;
  else if (pm10 >= 76 || pm25 >= 38 || o3 >= 0.12 || no2 >= 0.13 || co >= 12 || so2 > 0.1) // 상당히 나쁨
    s = 5;
  else if (pm10 >= 51  || pm25 >= 26 || o3 >= 0.09 || no2 >= 0.06 || co >= 9 || so2 > 0.05) // 나쁨
    s = 4;
  else if (pm10 >= 41 || pm25 >= 21 || o3 >= 0.06 || no2 >= 0.05 || co >= 5.5 || so2 > 0.04) // 보통
    s = 3;
  else if (pm10 >= 31  || pm25 >= 16 || o3 >= 0.03 || no2 >= 0.03 || co >= 2 || so2 > 0.02) // 양호
    s = 2;
  else if (pm10 >= 16  || pm25 >= 9 || o3 >= 0.02 || no2 >= 0.02 || co >= 1 || so2 > 0.01) // 좋음
    s = 1;
  else // 최고
    s = 0;
  return s;
}

void setLEDColor(int s) {
  int color;
  if (s == 0) {// 최고
    color = strip1.Color(0, 63, 255);
    color = strip2.Color(0, 63, 255);
  } else if (s == 1) { // 좋음
    color = strip1.Color(0, 127, 255);
    color = strip2.Color(0, 127, 255);
  } else if (s == 2) {// 양호
    color = strip1.Color(0, 255, 255);
    color = strip2.Color(0, 255, 255);
  }else if (s == 3){ // 보통
    color = strip1.Color(0, 255, 63);
    color = strip2.Color(0, 255, 63);
  }else if (s == 4) {// 나쁨
    color = strip1.Color(255, 127, 0);
    color = strip2.Color(255, 127, 0);
  }else if (s == 5){ // 상당히 나쁨
    color = strip1.Color(255, 63, 0);
    color = strip2.Color(255, 63, 0);
  }else if (s == 6) {// 매우 나쁨
    color = strip1.Color(255, 31, 0);
    color = strip2.Color(255, 31, 0);
  }else {// 최악
    color = strip1.Color(255, 0, 0);
    color = strip2.Color(255, 0, 0);
  }
  for (int i = 0; i < 4; i++) {
    strip1.setPixelColor(i, color);
    strip2.setPixelColor(i, color);
  }
  strip1.show();
  strip2.show();
}

static char hex_digit(char c) {
  return "0123456789ABCDEF"[c&0x0F];
}

char *urlencode(char *dst,char *src) {
  char c, *d = dst;
  while(c= *src++) {
    if(strchr(CITY,c)) {
      *d++ = '%';
      *d++ = hex_digit(c>>4);
      c = hex_digit(c);
    }
    *d++ = c;
  }
  *d = '\0';
  return dst;
}
