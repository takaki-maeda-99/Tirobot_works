#include <MsTimer2.h>
#include <FlexCAN_T4.h>
#include "PID.h"

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;  // can1 port
//FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2;  // can2 port

typedef struct
{
  int16_t rotation;
  int16_t torque;
} wheelEscDataSt;

wheelEscDataSt wEscData[4];   //WheelESCのCANデータ

int u[4] = {0};

static CAN_message_t msg;
static CAN_message_t msg_arm;
static CAN_message_t rxmsg;

Pid pid0;
Pid pid1;
Pid pid2;
Pid pid3;

float vx, vy, vt;

float rpm,mps;
#define tire_dia 100//こういう感じに定数にしておいても良いかも
float rpm2mps(float rpm){//rpmをm/sに変換
  mps=tire_dia*rpm/(60*19);  
  return mps;
}
float mps2rpm(float mps){//m/sをrpmに変換
  rpm=60*19*mps/tire_dia;
  return rpm;
}

void setup() {
  Serial.begin(115200);
  can1.begin();
  can1.setBaudRate(1000000);     // 500kbps data rate
  //can2.begin();
  //can2.setBaudRate(1000000);       // 500kbps data rate

  pinMode(13, OUTPUT);
  Serial1.begin(100000, SERIAL_8E1);
  digitalWrite(13, HIGH);
  delay(2000);
  digitalWrite(13, LOW);
 
  pid0.init(4, 0.001, 0.03); //p,i,dの順に指定できる
  pid1.init(4, 0.001, 0.03); //p,i,dの順に指定できる
  pid2.init(4, 0.001, 0.03); //p,i,dの順に指定できる
  pid3.init(4, 0.001, 0.03); //p,i,dの順に指定できる
  
  msg.id = 0x200;
  msg.len = 8;
  for ( int idx = 0; idx < msg.len; ++idx ) {
    msg.buf[idx] = 0;
  }

  msg_arm.id = 0x100;
  msg_arm.len = 2;
  for ( int idx = 0; idx < msg_arm.len; ++idx ) {
    msg_arm.buf[idx] = 0;
  }

  MsTimer2::set(2, timerInt); // CAN read 用 タイマ
  MsTimer2::start();
}


static unsigned long testch[6];///実際にデータを入れる配列

void loop() {
  static int data[18];                      //入力の生データ入れる配列
  static int dataNumber = 0;                //入力データの数(Serial1.available()の返値),受信バッファの数を見る変数
  static unsigned long lastConnectTime = 0; //直前の通信の時間?
  if (Serial1.available() > 0) {//受信バッファが0以上=何か受信している
    for (int dataNum = Serial1.available(); dataNum > 0; dataNum--) {//受信したバイト数を見る
      if (dataNumber < 0) {
        Serial1.read();
        dataNumber++;
        continue;
      }
      data[dataNumber % 18] = Serial1.read();
      dataNumber++;
      if (dataNumber > 18) {
        dataNumber = 0;
      }
      else if (dataNumber == 18) {//データが揃ったとき
        testch[0] = (((data[1] & 0x07) << 8) | data[0]);          //ch0(364～1024～1684)
        testch[1] = (((data[2] & 0x3F) << 5) | (data[1] >> 3));   //ch1(364～1024～1684)
        testch[2] = (((data[4] & 0x01) << 10) | (data[3] << 2) | (data[2] >> 6)); //ch2(364～1024～1684)
        testch[3] = (((data[5] & 0x0F) << 7) | (data[4] >> 1));   //ch3(364～1024～1684)
        if (!(364 <= testch[0] && testch[0] <= 1684 && 364 <= testch[1] && testch[1] <= 1684 && 364 <= testch[2] && testch[2] <= 1684 && 364 <= testch[3] && testch[3] <= 1684)) {
          for (int i = 1; i < 18; i++) {
            testch[0] = (((data[(1 + i) % 18] & 0x07) << 8) | data[(0 + i) % 18]);  //ch0(364～1024～1684)
            testch[1] = (((data[(2 + i) % 18] & 0x3F) << 5) | (data[(1 + i) % 18] >> 3)); //ch1(364～1024～1684)
            testch[2] = (((data[(4 + i) % 18] & 0x01) << 10) | (data[(3 + i) % 18] << 2) | (data[2] >> 6)); //ch2(364～1024～1684)
            testch[3] = (((data[(5 + i) % 18] & 0x0F) << 7) | (data[(4 + i) % 18] >> 1)); //ch3(364～1024～1684)
            if (364 <= testch[0] && testch[0] <= 1684 && 364 <= testch[1] && testch[1] <= 1684 && 364 <= testch[2] && testch[2] <= 1684 && 364 <= testch[3] && testch[3] <= 1684) {
              dataNumber = -i;
              break;
            }
          }
          if (dataNumber > 18) {
            dataNumber = -1;
          }
        }
        else {
          dataNumber = 0;
        }
      }
    }
    #define sinphi 0.707106781   //三角関数の計算は重たいので近似値を置いておくのが良さそう
    #define cosphi 0.707106781  

    float vx = map(testch[2], 364, 1684, -300, 300);
    float vy = map(testch[3], 364, 1684, -300, 300);//目標最大rpmは100
    float vt = map(testch[0], 364, 1684, -300, 300);
    float L = 0.5;

    u[0]=mps2rpm(-sinphi*vx+cosphi*vy+L*vt); //右前
    u[1]=mps2rpm(-cosphi*vx-sinphi*vy+L*vt); //左前
    u[2]=mps2rpm(sinphi*vx-cosphi*vy+L*vt);//左後
    u[3]=mps2rpm(cosphi*vx+sinphi*vy+L*vt);//右後

    u[0] = pid0.pid_out(u[0]);
    u[1] = pid1.pid_out(u[1]);
    u[2] = pid2.pid_out(u[2]);
    u[3] = pid3.pid_out(u[3]);

  }
  else {  //何も受信していない=通信がロスとしている->非常停止した方が良さそう
    digitalWrite(13,LOW);
    u[0]=0;
    u[1]=0;
    u[2]=0;
    u[3]=0;
  }

  // Serial.print(u[0]);//目標速度
  // Serial.print(",");
  // Serial.print(u[1]);
  // Serial.print(",");
  // Serial.print(u[2]);
  // Serial.print(",");
  // Serial.print(u[3]);
  // Serial.print(",");


  for (int i = 0; i < 4; i++) {
    msg.buf[i * 2] = u[i] >> 8;
    msg.buf[i * 2 + 1] = u[i] & 0xFF;
  }

  
  pid0.debug();
  pid1.debug();
  pid2.debug();
  pid3.debug();
  // Serial.print(pid0.debug());//現在速度
  // Serial.println("");
  // Serial.print(pid1.debug());
  // Serial.print(",");
  // Serial.print(pid2.debug());  
  // Serial.print(",");
  // Serial.print(pid3.debug());
  // Serial.println("");
  delay(10);
}

void timerInt() {
  while ( can1.read(rxmsg) ) {
    if (rxmsg.id == 0x201) {
      pid0.now_value(rxmsg.buf[2] * 256 + rxmsg.buf[3]);
    }   
    if (rxmsg.id == 0x202) {
      pid1.now_value(rxmsg.buf[2] * 256 + rxmsg.buf[3]);
    }
    
    if (rxmsg.id == 0x203) {
      pid2.now_value(rxmsg.buf[2] * 256 + rxmsg.buf[3]);
    }
    if (rxmsg.id == 0x204) {
      pid3.now_value(rxmsg.buf[2] * 256 + rxmsg.buf[3]);
    }
  }
  can1.write(msg); // write to can1
  //can2.write(msg); // write to can2
}

  // int arrayLength = sizeof(u) / sizeof(u[0]); // 配列の長さを取得する
  // String binaryString[4]; // 2進数の文字列を保存するための配列

  // for (int i = 0; i < arrayLength; i++) {
  //   int decimalValue = u[i];
  //   binaryString[i] = decimalToBinary(decimalValue);
  // }

  // for (int i = 0; i < 4; i++) {
  //   int binaryValue = binaryStringToInt(binaryString[i]);
  //   msg.buf[i * 2] = binaryValue >> 8;
  //   msg.buf[i * 2 + 1] = binaryValue & 0xFF;
  // }

// String decimalToBinary(int decimalValue) {
//   String binaryString = "";
//   int bitCount = 8 * sizeof(decimalValue);
  
//   for (int i = 0; i < bitCount; i++) {
//     binaryString = String(decimalValue & 1) + binaryString;
//     decimalValue >>= 1;
//   }
  
//   return binaryString;
// }

// int binaryStringToInt(String binaryString) {
//   int intValue = 0;
//   int stringLength = binaryString.length();
  
//   for (int i = 0; i < stringLength; i++) {
//     intValue = (intValue << 1) | (binaryString.charAt(i) - '0');
//   }
  
//   return intValue;
// }
