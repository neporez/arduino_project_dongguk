
/*
  본 저작물은 '한국환경공단'에서 실시간 제공하는 '한국환경공단_에어코리아_대기오염정보'를 이용하였습니다.
  https://www.data.go.kr/tcs/dss/selectApiDataDetailView.do?publicDataPk=15073861
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
AdafruitIO_Feed *remoteControl = io.feed("remoteControl"); //Adafruit io remoteControl
/**********************************************************/

/************************Timer 관련 변수***********************/
char curTime[20];
unsigned long t_timer = 0;
unsigned long tf_timer = 0;
bool timeCheck_timer = true;
bool timeCheck_timer_flicker = true;
bool flickerState = false;
/**********************************************************/

/************remote Control****************/
bool SelectMode = false;
bool brightnessSelectMode = false;
bool brightnessFlicker = false;
bool hourlyForecastMode = false;

/******************************************/




/***********API 관련 변수******************/
WiFiClient client; //클라이언트 객체 선언

char servername[] = "api.openweathermap.org"; //클라이언트가 접속할 서버이름
char servername_pt[] = "apis.data.go.kr";


String CityID = "1835848";//Seoul,KR
char CITY[] = "중구";
char CITY_ENCODE[120];



String weatherDescripton = ""; //관측하고 있는 도시 날씨의 자세한 설명
String weatherLocation = ""; //관측하고 있는 도시
String weatherCountry;//관측하고 있는 국가
String weatherDate[5]; //관측하고 있는 시간
String forecastTime;
float temperature;//관측하고 있는 도시의 온도
int weatherID; //날씨 상태


#define CURRENT_WEATHER 0
#define HOURLY_FORECAST 1
#define PARTICULATE_MATTER 2

int hourly_forecast_time = 0;

int particulate_state = 0;

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
int weatherIcon[4] = {D5, D6, D7, D8}; //날씨 아이콘 led
int mainLight[2] = {D3, D4};

/******************************************/

/*****************led 관련 변수****************/

#define NUMPIXELS 4
int brightness = 80; // 0~255

Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUMPIXELS, D3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUMPIXELS, D4, NEO_GRB + NEO_KHZ800);


/*********************************************/


/***************함수 설정*******************/

void CurrnetAPIDataRecieved();
void HourlyForecastAPIDataRecieved();
void ParticulateMatterAPIRecieved();
int getScore(float so2, float co, float o3, float no2, float pm10, float pm25);
void setLEDColor(int s);
void APIDataLCDPrint();
void displayGettingData();
void handleLamp();
void handleLampRemoteControl();
void lastMoodLampStateCheck();
void timerUpdate();
void mainLEDON();
void brightnessControl(int brightnessValue);
void forecastControl(int selectTime);
int weatherInfo();
void weatherIconLEDON();
void AllLEDOff();
void UpdateLastData();
static char hex_digit(char c);
char *urlencode(char *dst, char *src);
/******************************************/


/**************무드등 관련 변수**************/
bool moodLampState = false;
bool lastMoodLampState = true;
bool timeCheck = true; //10분에 한번씩 API 값 초기화를 위한 변수
unsigned long t = 0; //API 값을 받아올때 시간을 측정하기 위한 변수

int moodlampModeState = CURRENT_WEATHER;
/*****************************************/

void setup() {
  Serial.begin(115200); //시리얼 통신 Rate:115200
  Serial.println();
  urlencode(CITY_ENCODE, CITY);



  while (! Serial);
  /***************LCD I2C*******************/

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Connecting");
  /******************************************/

  strip1.setBrightness(brightness);
  strip1.begin();
  strip1.show();

  strip2.setBrightness(brightness);
  strip2.begin();
  strip2.show();

  /***************Adafruit io*******************/
  io.connect(); // io.adafruit.com과 MQTT 통신 시작
  moodlamp->onMessage(handleLamp);
  remoteControl->onMessage(handleLampRemoteControl);
  int i = 1;

  while (io.mqttStatus() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    mainLEDON();
    if (i % 4 == 0) {
      i = 1;
      lcd.setCursor(10 + i, 0);
      lcd.print("   ");
      delay(500);
    }
    lcd.setCursor(10 + i, 0);
    i++;
    lcd.print(".");
    delay(500);
  }
  moodlamp->get();
  remoteControl->get();
  Serial.println();
  Serial.println(io.statusText()); // connected!
  /******************************************/
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Connnected!");
  delay(1000);


  UpdateLastData();

}

void loop() {
  io.run();
  /************************************//**램프 동작**//*****************************************/
  if (moodLampState) {
    lastMoodLampStateCheck();

    if (!SelectMode && !brightnessSelectMode && !hourlyForecastMode) { //모드 선택중이 아니라면 실행
      /************************현재 날씨 모드**************************/
      if (moodlampModeState == CURRENT_WEATHER) {
        if (timeCheck) {
          t = millis();
          timeCheck = false;
        } else if (!timeCheck && millis() - t > 600000) {
          timeCheck = true;
          UpdateLastData();
          mainLEDON();
        }
      }
      /*************************************************************/

      /************************내일 날씨 모드*************************/
      if (moodlampModeState == HOURLY_FORECAST) {
        if (timeCheck) {
          t = millis();
          timeCheck = false;
        } else if (!timeCheck && millis() - t > 600000) {
          timeCheck = true;
          UpdateLastData();
          mainLEDON();
        }
      }
      /*************************************************************/

      /************************미세 먼지 모드*************************/
      if (moodlampModeState == PARTICULATE_MATTER) {
        if (timeCheck) {
          t = millis();
          timeCheck = false;
        } else if (!timeCheck && millis() - t > 600000) {
          timeCheck = true;
          UpdateLastData();
          mainLEDON();
        }
      }
      /*************************************************************/
      /***********************날씨 아이콘 LED**************************/
      if (APIDataReceived) {
        weatherIconLEDON();
      }
      /*************************************************************/
      /***********************시간 표시******************************/
      if (timeCheck_timer && APIDataReceived && (moodlampModeState != HOURLY_FORECAST) && (moodlampModeState != HOURLY_FORECAST)) {
        t_timer = millis();
        timeCheck_timer = false;
      } else if (!timeCheck_timer & millis() - t_timer > 10000) {
        timeCheck_timer = true;
        timerUpdate();
      }
      if (!APIDataReceived) {
        lcd.clear();
        lcd.setCursor(0, 0);
      }
      if (timeCheck_timer_flicker && APIDataReceived && (moodlampModeState != HOURLY_FORECAST)) {
        tf_timer = millis();
        timeCheck_timer_flicker = false;
      } else if (!timeCheck_timer_flicker & millis() - tf_timer > 350 && (moodlampModeState != HOURLY_FORECAST)) {
        timeCheck_timer_flicker = true;
        if (flickerState) {
          lcd.setCursor(13, 0);
          lcd.print(":");
        } else {
          lcd.setCursor(13, 0);
          lcd.print(" ");
        }
        flickerState ^= 1;
      }
      /**************************************************************/
    } else {
      //선택모드일때 타임관련 변수 초기화
      timeCheck = true;
      timeCheck_timer = true;
      timeCheck_timer_flicker = true;
    }
  } else {
    SelectMode = false;
    brightnessSelectMode = false;
    hourlyForecastMode = false;
    lastMoodLampStateCheck(); //램프가 꺼졌을 때
  }
  /***********************************//*램프동작 끝*//***************************************/


}

void AllLEDOff() {
  for (int i = 0; i < 4; i++) {
    strip1.setPixelColor(i, 0, 0, 0);
    strip2.setPixelColor(i, 0, 0, 0);
  }
  strip1.show();
  strip2.show();
}

void UpdateLastData() {
  if (moodlampModeState == CURRENT_WEATHER) {
    CurrnetAPIDataRecieved();
    timerUpdate();
    String _temp = String(temperature) + "℃";
    temp->save(_temp);
    moodlampMode->save("Current Weather");
    hourly_forecast_time = 0;
  }
  if (moodlampModeState == HOURLY_FORECAST) {
    HourlyForecastAPIDataRecieved();
    String _temp = String(temperature) + "℃";
    temp->save(_temp);
    moodlampMode->save("Hourly Forecast");

  }
  if (moodlampModeState == PARTICULATE_MATTER) {
    ParticulateMatterAPIRecieved();
    CurrnetAPIDataRecieved();
    timerUpdate();
    String _temp = String(temperature) + "℃";
    temp->save(_temp);
    moodlampMode->save("Air Pollution");
    hourly_forecast_time = 0;

  }
}


void weatherIconLEDON() {
  for (int i = 0; i < 4; i++) {
    if (weatherInfo() == i) {
      //strip2.setPixelColor(i,255,255,255);
    } else {
      //strip2.setPixelColor(i,0,0,0);
    }
    //strip2.show();
  }
  if (weatherInfo() == 4) {
    for (int i = 0; i < 4; i++) {
      //strip2.setPixelColor(i,255,255,255);
      //strip2.show();
    }
  }
}


int weatherInfo() { // 날씨 정보 받아오기
  if ((weatherID / 100 == 8) && (weatherID % 100 == 0)) return 0; //Clear
  if ((weatherID / 100 == 7) || (weatherID / 100 == 8)) return 1; //Clouds
  if ((weatherID / 100 == 3) || (weatherID / 100 == 5)) return 2; //Rain
  if (weatherID / 100 == 6) return 3; //Snow
  if (weatherID / 100 == 2) return 4; // Thunderstorm
}

void mainLEDON() { //모드에 따라 메인 led의 불빛이 다르다.
  strip1.setBrightness(brightness);
  strip2.setBrightness(brightness);
  if ((moodlampModeState == CURRENT_WEATHER) || (moodlampModeState == HOURLY_FORECAST) || (!PT_APIDataReceived)) {
    for (int i = 0; i < 4; i++) {
      strip1.setPixelColor(i, 255, 100, 0);
      strip2.setPixelColor(i, 255, 100, 0);
      strip1.show();
      strip2.show();
    }
  }
  if (moodlampModeState == PARTICULATE_MATTER && (PT_APIDataReceived)) {
    setLEDColor(particulate_state);
  }
}

void timerUpdate() {
  int timeIndex = 0;

  time_t now = time(nullptr);
  struct tm *tt;
  tt = localtime(&now);
  if (tt->tm_hour >= 10) {
    timeIndex = 1;
  } else {
    timeIndex = 0;
  }
  sprintf(curTime, "%02d %02d", tt->tm_hour, tt->tm_min);
  lcd.setCursor(10 + timeIndex, 0);
  lcd.print(curTime);
}

void lastMoodLampStateCheck() { //램프의 상태가 변경되었을때 마지막 상태를 저장하고 램프를 초기화
  if (lastMoodLampState != moodLampState) {
    moodlamp->save(moodLampState);
    lastMoodLampState = moodLampState;
    Serial.print("MoodLampState: ");
    Serial.println(moodLampState);
    if (moodLampState) {
      mainLEDON();
      lcd.clear();
      lcd.backlight();
      UpdateLastData();
    } else {
      AllLEDOff();
      lcd.clear();
      lcd.noBacklight();
    }
  }
}

void handleLamp(AdafruitIO_Data * data) { //Adafruit io 에서 램프 스위치의 상태를 변화시키는 함수
  int state = data->toInt();
  switch (state) {
    case 0: moodLampState = false; break;
    case 1: moodLampState = true; break;
  }
  Serial.print("received <- ");
  Serial.println(data->value());
}

void handleLampRemoteControl(AdafruitIO_Data * data) {//Adafruit io 에서 리모컨값을 가져와 모드 변환
  Serial.println(String(data->value()));

  if (String(data->value()) == "6" && !SelectMode && !brightnessSelectMode) { // MODE SECELT
    SelectMode = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("-Mode Selection-");
    lcd.setCursor(0, 1);
    switch (moodlampModeState) {
      case CURRENT_WEATHER : lcd.print("<CurrentWeather>"); break;
      case HOURLY_FORECAST : lcd.print("<HourlyForecast>"); break;
      case PARTICULATE_MATTER : lcd.print("< AirPollution >"); break;
    }
  }
  if (String(data->value()) == "8" ) { // LEFT
    if (SelectMode && !brightnessSelectMode && !hourlyForecastMode) {
      if (moodlampModeState == 0) moodlampModeState = 3;
      moodlampModeState--;
      lcd.setCursor(0, 1);
      switch (moodlampModeState) {
        case CURRENT_WEATHER : lcd.print("<CurrentWeather>"); break;
        case HOURLY_FORECAST : lcd.print("<HourlyForecast>"); break;
        case PARTICULATE_MATTER : lcd.print("< AirPollution >"); break;
      }
    }
    if (!SelectMode && brightnessSelectMode && !hourlyForecastMode) {
      brightnessControl(brightness - 25);
    }
    if (hourlyForecastMode && !SelectMode && !brightnessSelectMode) {
      forecastControl(--hourly_forecast_time);
    }
  }
  if (String(data->value()) == "10") { //RIGHT
    if (SelectMode && !brightnessSelectMode && !hourlyForecastMode) {
      if (moodlampModeState == 2) moodlampModeState = -1;
      moodlampModeState++;
      lcd.setCursor(0, 1);
      switch (moodlampModeState) {
        case CURRENT_WEATHER : lcd.print("<CurrentWeather>"); break;
        case HOURLY_FORECAST : lcd.print("<HourlyForecast>"); break;
        case PARTICULATE_MATTER : lcd.print("< AirPollution >"); break;
      }
    }
    if (!SelectMode && brightnessSelectMode && !hourlyForecastMode) {
      brightnessControl(brightness + 25);
    }
    if (hourlyForecastMode && !SelectMode && !brightnessSelectMode) {
      forecastControl(++hourly_forecast_time);
    }
  }
  if (String(data->value()) == "9") { //ENTER/SAVE
    if (SelectMode && !brightnessSelectMode && !hourlyForecastMode) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Selected!");
      delay(500);
      lcd.clear();
      UpdateLastData();
      SelectMode = false;
    }
    if (!SelectMode && brightnessSelectMode && !hourlyForecastMode) {

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Selected!");
      delay(500);
      lcd.clear();
      APIDataLCDPrint();
      if (moodlampModeState != HOURLY_FORECAST) {
        timerUpdate();
      }
      brightnessSelectMode = false;
    }
    if (hourlyForecastMode && !SelectMode && !brightnessSelectMode) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Selected!");
      delay(500);
      lcd.clear();
      UpdateLastData();
      hourlyForecastMode = false;
    }
  }
  if (String(data->value()) == "4" && !brightnessSelectMode && !SelectMode && !hourlyForecastMode) { //SETUP
    brightnessSelectMode = true;
    brightnessControl(brightness);
  }
  if (String(data->value()) == "12" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 0 or 10
    if (brightnessFlicker) {
      brightnessControl(255);
    } else {
      brightnessControl(5);
    }
    brightnessFlicker ^= 1;
  }
  if (String(data->value()) == "16" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 1
    brightnessControl(5 + 1 * 25);
  }
  if (String(data->value()) == "17" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 2
    brightnessControl(5 + 2 * 25);
  }
  if (String(data->value()) == "18" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 3
    brightnessControl(5 + 3 * 25);
  }
  if (String(data->value()) == "20" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 4
    brightnessControl(5 + 4 * 25);
  }
  if (String(data->value()) == "21" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 5
    brightnessControl(5 + 5 * 25);
  }
  if (String(data->value()) == "22" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 6
    brightnessControl(5 + 6 * 25);
  }
  if (String(data->value()) == "24" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 7
    brightnessControl(5 + 7 * 25);
  }
  if (String(data->value()) == "25" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 8
    brightnessControl(5 + 8 * 25);
  }
  if (String(data->value()) == "26" && brightnessSelectMode && !SelectMode && !hourlyForecastMode) {// 9
    brightnessControl(5 + 9 * 25);
  }

  if (String(data->value()) == "1" && moodlampModeState == HOURLY_FORECAST && !hourlyForecastMode && !brightnessSelectMode && !SelectMode) {
    hourlyForecastMode = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("-Time Selection-");
    lcd.setCursor(0, 1);
    lcd.print(weatherDate[hourly_forecast_time]);
    Serial.print("weatherDate: ");
    Serial.println(weatherDate[hourly_forecast_time]);
  }

  /**********remote control value********/
  /*VOL-       : 0
    >ll        : 1
    VOL+       : 2
    SETUP      : 4
    UP         : 5
    STOP/MODE  : 6
    LEFT       : 8
    ENTER/SAVE : 9
    RIGHT      : 10
    0 10+      : 12
    DOWN       : 13
    RETURN     : 14
     1         : 16
     2         : 17
     3         : 18
     4         : 20
     5         : 21
     6         : 22
     7         : 24
     8         : 25
     9         : 26
  */
  /***************************************/
}

void forecastControl(int selectTime) {
  if (selectTime > 4) {
    selectTime = 0;
  }
  if (selectTime < 0) {
    selectTime = 4;
  }
  hourly_forecast_time = selectTime;
  lcd.setCursor(0, 1);
  lcd.print(weatherDate[selectTime]);
  Serial.print("weatherDate: ");
  Serial.println(weatherDate[hourly_forecast_time]);
}

void brightnessControl(int brightnessValue) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  <Brightness>  ");
  lcd.setCursor(0, 1);
  brightness = brightnessValue;
  if (brightness > 255) brightness = 255;
  if (brightness < 0) brightness = 5;
  Serial.print("brightness :");
  Serial.println(brightness);
  mainLEDON();
  int i = map(brightness, 5, 255, 0, 10); // 빛의 세기를 5~255 -> 0~10
  for (int j = 0; j < i; j++) {
    lcd.print("=");
    lcd.setCursor(j + 1, 1);
  }
  lcd.print("O");
  lcd.setCursor(13 - (i / 10), 1);
  if (i != 0) { //
    lcd.print(String(i));
  }
  lcd.setCursor(14, 1);
  lcd.print("0");
  lcd.setCursor(15, 1);
  lcd.print("%");
}



void displayGettingData() {
  lcd.clear();
  lcd.print("Getting data");
}

void APIDataLCDPrint() {
  if (moodlampModeState == CURRENT_WEATHER) { //현재 날씨 모드
    line1String = "";
    line2String = "";

    line1String += weatherLocation + ", " + weatherCountry;
    line2String += weatherDescripton + ", T: " + String(temperature);

    APIDataReceived = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1String);
    lcd.setCursor(0, 1);
    lcd.print(line2String);
  }

  if (moodlampModeState == HOURLY_FORECAST) { //시간별 날씨 모드
    line1String = "";
    line2String = "";

    line1String += weatherLocation + " " + weatherDate[hourly_forecast_time].substring(5, 13) + "h";
    line2String += weatherDescripton + ", T: " + String(temperature);

    APIDataReceived = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1String);
    lcd.setCursor(0, 1);
    lcd.print(line2String);
  }

  if (moodlampModeState == PARTICULATE_MATTER) { //미세 먼지 모드
    line1String = "";
    line2String = "";

    line1String += weatherLocation + ", " + weatherCountry + " ";
    line2String += weatherDescripton + ", T: " + String(temperature);

    APIDataReceived = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1String);
    lcd.setCursor(0, 1);
    lcd.print(line2String);

  }
}



/*
  https://diy-project.tistory.com/73#google_vignette 참조
*/
void CurrnetAPIDataRecieved() {
  displayGettingData();
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  if (client.connect(servername, 80)) {
    client.println("GET /data/2.5/weather?id=" + CityID + "&units=metric&APPID=" + API_KEY);
    client.println("Host: api.openweathermap.org");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }
  //클라이언트 API 연결
  while (client.connected() && !client.available()) delay(10);

  result = client.readStringUntil('\r');

  //API 정보 가져오기
  client.stop();
  Serial.println(result);

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success()) {
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

  //lcd 에 API 정보 담기


  //데이터 가져오기

  APIDataLCDPrint();
}

void HourlyForecastAPIDataRecieved() {
  displayGettingData();
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  if (client.connect(servername, 80)) {
    client.println("GET /data/2.5/forecast?id=" + CityID + "&appid=" + API_KEY + "&mode=json&units=metric&cnt=11");
    client.println("Host: api.openweathermap.org");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }

  //클라이언트 API 연결
  while (client.connected() && !client.available()) delay(10);

  result = client.readStringUntil('\r');

  Serial.println(result);
  String result_short = "{\"list\":[{\"main\":{";
  String result_short_middle;
  int lastIndex = 0;

  //result_short_middle += result.substring(result.indexOf("\"temp\""),result.indexOf(",",result.indexOf("\"temp\"")))+"},";
  lastIndex = result.indexOf(",", result.indexOf("\"temp\""));
  //result_short_middle += result.substring(result.indexOf("\"weather\"",lastIndex),result.indexOf("],",lastIndex))+"],";
  lastIndex = result.indexOf("],", lastIndex);
  //result_short_middle += result.substring(result.indexOf("\"dt_txt\"",lastIndex),result.indexOf(":00\"},",lastIndex))+"\"},{\"main\":{";


  lastIndex = result.indexOf(":00\"},", lastIndex);


  for (int i = 0; i < 9; i++) {
    if (i % 2 == 0) {
      lastIndex = result.indexOf(",", lastIndex);
      lastIndex = result.indexOf("],", lastIndex);
      lastIndex = result.indexOf(":00\"},", lastIndex);
    } else {
      result_short_middle += result.substring(result.indexOf("\"temp\"", lastIndex), result.indexOf(",", result.indexOf("\"temp\"", lastIndex))) + "},";
      lastIndex = result.indexOf(",", lastIndex);
      result_short_middle += result.substring(result.indexOf("\"weather\"", lastIndex), result.indexOf("],", lastIndex)) + "],";
      lastIndex = result.indexOf("],", lastIndex);
      result_short_middle += result.substring(result.indexOf("\"dt_txt\"", lastIndex), result.indexOf(":00\"},", lastIndex)) + "\"},{\"main\":{";
      lastIndex = result.indexOf(":00\"},", lastIndex);
    }
  }
  result_short_middle += result.substring(result.indexOf("\"temp\"", lastIndex), result.indexOf(",", result.indexOf("\"temp\"", lastIndex))) + "},";
  lastIndex = result.indexOf(",", lastIndex);
  result_short_middle += result.substring(result.indexOf("\"weather\"", lastIndex), result.indexOf("],", lastIndex)) + "],";
  lastIndex = result.indexOf("],", lastIndex);
  result_short_middle += result.substring(result.indexOf("\"dt_txt\"", lastIndex));


  result_short += result_short_middle;
  Serial.println(result_short);

  //API 정보 가져오기
  client.stop();

  char jsonArray [result_short.length() + 1];
  result_short.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result_short.length() + 1] = '\0';

  StaticJsonBuffer<1536> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success()) {
    Serial.println("parseObject() failed");
  }
  //JsonArray 형태로 데이터 정리

  String location = root["city"]["name"];
  String country = root["country"];
  String description = root["list"][hourly_forecast_time]["weather"][0]["description"];
  String date[5];
  for (int i = 0; i < 5; i++) {
    date[i] = String(root["list"][i]["dt_txt"]);
  }
  float _temperature = root["list"][hourly_forecast_time]["main"]["temp"];
  int ID = root["list"][hourly_forecast_time]["weather"][0]["id"];


  weatherLocation = location;
  weatherCountry = country;
  weatherDescripton = description;
  for (int i = 0; i < 4; i++) {
    weatherDate[i] = date[i];
  }
  weatherDate[4] = date[4].substring(0, date[4].length() - 3);
  temperature = _temperature;
  weatherID = ID;

  APIDataLCDPrint();
}

void ParticulateMatterAPIRecieved() {
  displayGettingData();
  String result; //API 데이터를 모두 긁어와 저장하는 변수
  if (client.connect(servername_pt, 80)) {
    client.println("GET /B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?stationName=" + String(CITY_ENCODE) + "&dataTerm=month&pageNo=1&numOfRows=1&returnType=json&serviceKey=" + API_KEY_PT);
    client.println("Host: apis.data.go.kr");
    client.println("Connection : close");
    client.println();
  } else {
    Serial.println("client connection failed");
    Serial.println();
  }

  //클라이언트 API 연결
  while (client.connected() && !client.available()) delay(10);

  result = client.readString();

  client.stop();

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<800> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success()) {
    Serial.println("parseObject() failed");
  }

  float so2 = String(root["response"]["body"]["items"][0]["so2Value"]).toFloat();
  float co = String(root["response"]["body"]["items"][0]["coValue"]).toFloat();
  float o3 = String(root["response"]["body"]["items"][0]["o3Value"]).toFloat();
  float no2 = String(root["response"]["body"]["items"][0]["no2Value"]).toFloat();
  float pm10 = String(root["response"]["body"]["items"][0]["pm10Value"]).toFloat();
  float pm25 = String(root["response"]["body"]["items"][0]["pm25Value"]).toFloat();


  particulate_state = getScore(so2, co, o3, no2, pm10, pm25);
  PT_APIDataReceived = true;


}
int getScore(float so2, float co, float o3, float no2, float pm10, float pm25) {
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
  } else if (s == 3) { // 보통
    color = strip1.Color(0, 255, 63);
    color = strip2.Color(0, 255, 63);
  } else if (s == 4) {// 나쁨
    color = strip1.Color(255, 127, 0);
    color = strip2.Color(255, 127, 0);
  } else if (s == 5) { // 상당히 나쁨
    color = strip1.Color(255, 63, 0);
    color = strip2.Color(255, 63, 0);
  } else if (s == 6) {// 매우 나쁨
    color = strip1.Color(255, 31, 0);
    color = strip2.Color(255, 31, 0);
  } else {// 최악
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
  return "0123456789ABCDEF"[c & 0x0F];
}

char *urlencode(char *dst, char *src) {
  char c, *d = dst;
  while (c = *src++) {
    if (strchr(CITY, c)) {
      *d++ = '%';
      *d++ = hex_digit(c >> 4);
      c = hex_digit(c);
    }
    *d++ = c;
  }
  *d = '\0';
  return dst;
}
