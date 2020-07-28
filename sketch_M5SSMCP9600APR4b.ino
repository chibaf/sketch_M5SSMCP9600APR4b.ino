#include "Seeed_MCP9600.h"
#include "Seeed_MCP9601.h"
#include <M5Stack.h>

//-----------------------------------------------------------
//ハード概要
//-----------------------------------------------------------
//I2Cを2系統使用し、それぞれに5個のMCP9600を接続しています。
//I2CはWireとWire1で定義それぞれのピン配置は
//Wire SDA:21pin SCL:22pin
//Wire1 SDA:2pin SCL:5pin
//SeedStudio製MCP9600ライブラリは内部でWireのみ対応のハードコ
//ーディングがされていたためSeeed_MCP9600.hをほぼコピー状態で
//Seeed_MCP9601.hとして別途定義し、内部のWire記述をWire1に置換
//したものを用意して2バス構成で使用。
//-----------------------------------------------------------

//-----------------------------------------------------------
//タイマー割り込みの時間間隔 100ms間隔
//-----------------------------------------------------------
//変更不可
//内部時計100ms精度のためのタイマー割り込み間隔を定義したもの
//この時間を変更すると時計精度も変化してしまうのでロガー間隔を
//変更する場合はloop関数内でtickedが何回来たか判定して
//100ms x n時間で処理変更する必要あり
//tickedはタイマー割り込みでシステムタイムをインクリメントした
//際にtrueにセットされるタイマー発火処理フラグ
//-----------------------------------------------------------
#define INT_INTERVAL 100000

//Serial Variable
char receiveBuffer[128];
int receiveIndex = 0;

//Timer Variables
volatile long interruptCounter = 0;
volatile boolean ticked = false;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//Function flag
bool FG_SERIALOUT = true;
bool FG_SDWRITE = false;

//DayOfTime Variables
struct LOG_TIME {
  int t_hour;
  int t_min;
  int t_sec;
  int t_mil;
};

//M5Stack Display Variable
TFT_eSprite img = TFT_eSprite(&M5.Lcd);

MCP9600 sensor6 = MCP9600(0x60);
MCP9600 sensor7 = MCP9600(0x61);
MCP9600 sensor8 = MCP9600(0x63);
MCP9600 sensor9 = MCP9600(0x65);
MCP9600 sensor10 = MCP9600(0x67);

MCP9601 sensor1 = MCP9601(0x60);
MCP9601 sensor2 = MCP9601(0x61);
MCP9601 sensor3 = MCP9601(0x63);
MCP9601 sensor4 = MCP9601(0x65);
MCP9601 sensor5 = MCP9601(0x67);

// 現在時刻をセットする
void setLoggerTime (LOG_TIME vTime) {
  interruptCounter = vTime.t_hour * 10 * 60 * 60
                     + vTime.t_min * 10 * 60
                     + vTime.t_sec * 10
                     + vTime.t_mil;
}

// 現在時刻を返す関数
LOG_TIME getLoggerTime() {
  LOG_TIME result;
  result.t_hour = interruptCounter / (10 * 60 * 60);
  result.t_min = (interruptCounter % (10 * 60 * 60)) / (10 * 60);
  result.t_sec = ((interruptCounter % (10 * 60 * 60)) % (10 * 60)) / 10;
  result.t_mil = ((interruptCounter % (10 * 60 * 60)) % (10 * 60)) % 10;
  return result;
}

// タイマー発火時に処理する関数　※短めの処理に！
void IRAM_ATTR onTimer() {  /* this function must be placed in IRAM */
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  ticked = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

err_t sensor_basic_config(MCP9600 vsensor)
{
  err_t ret = NO_ERROR;
  CHECK_RESULT(ret, vsensor.set_filt_coefficients(FILT_OFF));
  CHECK_RESULT(ret, vsensor.set_cold_junc_resolution(COLD_JUNC_RESOLUTION_0_25));
  CHECK_RESULT(ret, vsensor.set_ADC_meas_resolution(ADC_16BIT_RESOLUTION));
  CHECK_RESULT(ret, vsensor.set_burst_mode_samp(BURST_1_SAMPLE));
  //CHECK_RESULT(ret,vsensor.set_sensor_mode(BURST_MODE));
  CHECK_RESULT(ret, vsensor.set_sensor_mode(NORMAL_OPERATION));
  return ret;
}

err_t sensor_basic_config1(MCP9601 vsensor)
{
  err_t ret = NO_ERROR;
  CHECK_RESULT(ret, vsensor.set_filt_coefficients(FILT_OFF));
  CHECK_RESULT(ret, vsensor.set_cold_junc_resolution(COLD_JUNC_RESOLUTION_0_25));
  CHECK_RESULT(ret, vsensor.set_ADC_meas_resolution(ADC_16BIT_RESOLUTION));
  CHECK_RESULT(ret, vsensor.set_burst_mode_samp(BURST_1_SAMPLE));
  //CHECK_RESULT(ret,vsensor.set_sensor_mode(BURST_MODE));
  CHECK_RESULT(ret, vsensor.set_sensor_mode(NORMAL_OPERATION));
  return ret;
}

err_t get_temperature(float *value, MCP9600 vsensor)
{
  err_t ret = NO_ERROR;
  float hot_junc = 0;
  float junc_delta = 0;
  float cold_junc = 0;
  CHECK_RESULT(ret, vsensor.read_hot_junc(&hot_junc));
  //CHECK_RESULT(ret, vsensor.read_junc_temp_delta(&junc_delta));
  //CHECK_RESULT(ret, vsensor.read_cold_junc(&cold_junc));

  *value = hot_junc;

  return ret;
}

err_t get_temperature1(float *value, MCP9601 vsensor)
{
  err_t ret = NO_ERROR;
  float hot_junc = 0;
  float junc_delta = 0;
  float cold_junc = 0;
  CHECK_RESULT(ret, vsensor.read_hot_junc(&hot_junc));
  //CHECK_RESULT(ret, vsensor.read_junc_temp_delta(&junc_delta));
  //CHECK_RESULT(ret, vsensor.read_cold_junc(&cold_junc));

  *value = hot_junc;

  return ret;
}

void setup()
{
  M5.begin();
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(3, 3);
  M5.Lcd.println("Init TCAMPs");
  Wire.begin(21, 22);
  Wire.setClock(50000);
  Wire1.begin(2, 5);
  Wire1.setClock(50000);

  //各MCP9600をイニシャライズ
  if (sensor1.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor1 Init failed");
    //while (1);
  }
  if (sensor2.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor2 Init failed");
    //while (1);
  }
  if (sensor3.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor3 Init failed");
    //while (1);
  }
  if (sensor4.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor4 Init failed");
    //while (1);
  }
  if (sensor5.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor5 Init failed");
    //while (1);
  }
  if (sensor6.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor6 Init failed");
    //while (1);
  }
  if (sensor7.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor7 Init failed");
    //while (1);
  }
  if (sensor8.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor8 Init failed");
    //while (1);
  }
  if (sensor9.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor9 Init failed");
    //while (1);
  }
  if (sensor10.init(THER_TYPE_K)) {
    M5.Lcd.println("Sensor10 Init failed");
    //while (1);
  }
  sensor_basic_config1(sensor1);
  sensor_basic_config1(sensor2);
  sensor_basic_config1(sensor3);
  sensor_basic_config1(sensor4);
  sensor_basic_config1(sensor5);
  sensor_basic_config(sensor6);
  sensor_basic_config(sensor7);
  sensor_basic_config(sensor8);
  sensor_basic_config(sensor9);
  sensor_basic_config(sensor10);

  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);

  Serial.begin(115200);
  Serial.println("Init Serial Terminal.");

  //System Time Set
  LOG_TIME systemtime;
  systemtime.t_hour = 0;
  systemtime.t_min = 0;
  systemtime.t_sec = 0;
  systemtime.t_mil = 0;
  setLoggerTime(systemtime);

  //Timer Initialize
  timer = timerBegin(0, 80, true);  //内部クロックの分周
  timerAttachInterrupt(timer, &onTimer, true);  //タイマー発火時の関数関連付け
  timerAlarmWrite(timer, INT_INTERVAL , true); //100ms間隔でタイマー発火のプロパティセット
  timerAlarmEnable(timer);  //タイマースタート

  //この後表示し続ける固定部分の表示
  img.setColorDepth(8);
  img.createSprite(250, 26);
  img.fillSprite(0xFA00);
  img.setTextColor(TFT_BLACK);
  img.setTextFont(4);
  img.drawString("Thermocouple Temp", 5, 3);
  img.pushSprite(35, 0);
  img.deleteSprite();

  img.createSprite(250, 26);
  img.fillSprite(0xFA00);
  img.setTextColor(TFT_BLACK);
  img.setTextFont(4);
  img.drawString("System Status", 48, 3);
  img.pushSprite(35, 149);
  img.deleteSprite();
}


//メインループ
void loop()
{
  //シリアル通信の処理
  if (processSerial()) {
    Serial.println("OK");
    interruptCounter = 0; //シリアル処理で肯定が帰ったら内部時計をリセット（仮の実装）
  }

  //tickedフラグが立っていれば10msec経過しているので温度取得、表示、シリアル処理を行う
  if (ticked) {
    float temp1 = 0;
    float temp2 = 0;
    float temp3 = 0;
    float temp4 = 0;
    float temp5 = 0;

    float temp6 = 0;
    float temp7 = 0;
    float temp8 = 0;
    float temp9 = 0;
    float temp10 = 0;

    //温度取得　2チャンネルあるI2Cバスを交互に使って負荷を落とす  // commented out 
//    get_temperature1(&temp1, sensor1);
//    get_temperature(&temp6, sensor6);
//
//    get_temperature1(&temp2, sensor2);
//    get_temperature(&temp7, sensor7);
//
//    get_temperature1(&temp3, sensor3);
//    get_temperature(&temp8, sensor8);
//
//    get_temperature1(&temp4, sensor4);
//    get_temperature(&temp9, sensor9);
//
//    get_temperature1(&temp5, sensor5);
//    get_temperature(&temp10, sensor10);

    temp1=random(20,25);  // random temp 20~25
    temp2=random(20,25);
    temp3=random(20,25);
    temp4=random(20,25);
    temp5=random(20,25);
    temp6=random(20,25);
    temp7=random(20,25);
    temp8=random(20,25);
    temp9=random(20,25);
    temp10=random(20,25);

    //ディスプレイ表示関係
    img.createSprite(150, 110);
    img.fillSprite(TFT_BLACK);
    img.setTextColor(TFT_RED);
    img.setTextFont(4);
    char buf[128];
    sprintf(buf, "ch06:%4.1f", temp6);
    img.drawString(String(buf), 2, 3);
    sprintf(buf, "ch07:%4.1f", temp7);
    img.drawString(String(buf), 2, 23);
    sprintf(buf, "ch08:%4.1f", temp8);
    img.drawString(String(buf), 2, 43);
    sprintf(buf, "ch09:%4.1f", temp9);
    img.drawString(String(buf), 2, 63);
    sprintf(buf, "ch10:%4.1f", temp10);
    img.drawString(String(buf), 2, 83);
    img.drawRoundRect(0, 0, 150, 110, 6, 0xFA00);
    img.pushSprite(165, 32);

    img.fillSprite(TFT_BLACK);
    img.setTextColor(TFT_RED);
    img.setTextFont(4);
    sprintf(buf, "ch01:%4.1f", temp1);
    img.drawString(String(buf), 2, 3);
    sprintf(buf, "ch02:%4.1f", temp2);
    img.drawString(String(buf), 2, 23);
    sprintf(buf, "ch03:%4.1f", temp3);
    img.drawString(String(buf), 2, 43);
    sprintf(buf, "ch04:%4.1f", temp4);
    img.drawString(String(buf), 2, 63);
    sprintf(buf, "ch05:%4.1f", temp5);
    img.drawString(String(buf), 2, 83);
    img.drawRoundRect(0, 0, 150, 110, 6, 0xFA00);
    img.pushSprite(5, 32);
    img.deleteSprite();

    LOG_TIME nowtime = getLoggerTime();

    img.createSprite(310, 60);
    img.fillSprite(TFT_BLACK);
    img.setTextColor(TFT_ORANGE);
    img.setTextFont(4);
    img.drawRoundRect(0, 0, 310, 60, 6, 0xFA00);
    sprintf(buf, "CLOCK:%02d:%02d:%02d.%01d"
            , nowtime.t_hour
            , nowtime.t_min
            , nowtime.t_sec
            , nowtime.t_mil
           );
    img.drawString(String(buf), 2, 3);
    if (FG_SERIALOUT) {
      img.setTextColor(TFT_ORANGE);
      if (nowtime.t_mil % 5 != 0) {
        img.drawString("SERIAL_OUT", 2, 28);
      }
    } else {
      img.setTextColor(0x2000);
      img.drawString("SERIAL_OUT", 2, 28);
    }
    if (FG_SDWRITE) {
      img.setTextColor(TFT_ORANGE);
    } else {
      img.setTextColor(0x2000);
    }
    img.drawString("SD_WRITE", 170, 28);
    img.pushSprite(5, 180 );
    img.deleteSprite();

    //シリアル出力
   // if (FG_SERIALOUT) { commend out
    sprintf(buf, "%02d:%02d:%02d.%01d,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f"
              , nowtime.t_hour
              , nowtime.t_min
              , nowtime.t_sec
              , nowtime.t_mil
              , temp1
              , temp2
              , temp3
              , temp4
              , temp5
              , temp6
              , temp7
              , temp8
              , temp9
              , temp10
             );
    Serial.println(buf);
   // } commend out

    ticked = false;
  }
}

//シリアルターミナル処理
//シリアルの受信バッファにとどいたものを内部バッファに積む処理(max 128byte)
//改行コードを受信したらコマンド解釈と実行を行う予定（コマンド処理は未実装）
//現状改行コードがきたらtrueを返す
boolean processSerial() {
  char in;
  boolean recvLine = false;
  int receivedLength = Serial.available();
  if (receivedLength > 0) {
    for (int i = 0; i < receivedLength; i++) {
      in = Serial.read();
      Serial.print(in); //local echo
      if (in == '\r') {
        //command complete
        receiveIndex = 0;
        recvLine = true;
      } else {
        //command continue
        receiveBuffer[receiveIndex] = in;
        receiveIndex++;
        recvLine = false;
        if (receiveIndex > 127) {
          //receive overflow
          receiveIndex = 0;
          Serial.println("ERROR");
        }
      }
    }
  }
  return recvLine;
}
