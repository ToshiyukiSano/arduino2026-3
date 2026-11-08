// dai3_2_servo_ble（第3回：パソコンの画面のボタンで、サーボを無線で動かす）
// 【新規作成の教材・代替環境でコンパイルのみ確認・実機未検証】
//
// ボード：Arduino UNO R4 WiFi（UNO R3・R4 Minima では動きません）
// 必要なライブラリ：Servo、ArduinoBLE
//   （UNO R4 WiFi の通信用モジュールのファームウェアが 0.2.0 以上であること）
// 配線：第3回の1時間目と同じ（信号 → D9、電源（＋）→ 5V、GND（−）→ GND）
//
// しくみ：
//   PC（Chrome で開いた index.html）が、Bluetooth Low Energy（BLE）で
//   1文字の指示を送る → Arduino が受け取る → サーボと LED マトリクスを変える。
//     L = 左   C = 中央   R = 右
//   書き込むときは PC と USB でつなぐ（書き込みと電源）。
//   書き込んだあとは PC の USB を外し、別の電源から Arduino の USB 端子へ給電して、
//   PC のブラウザ（index.html）から Bluetooth で指示を送る。
//
// 生徒が変えるのは「★ここを変える」の中だけです。

#include <Servo.h>               // サーボを動かすための機能を読み込む
#include <ArduinoBLE.h>          // Bluetooth（BLE）を使うための機能を読み込む
#include "Arduino_LED_Matrix.h"  // 本体の LED マトリクスを使うための機能を読み込む

// =================== ★ここを変える（ここから）===================

// ① 機器名：先生から割り当てられた自分の番号にする（NGS-01 〜 NGS-11）
const char DEVICE_NAME[] = "NGS-01";

// ② ボタンを押したときの角度（0〜180）
const int LEFT_ANGLE   = 0;    // 「◀ 左」ボタン
const int CENTER_ANGLE = 90;   // 「● 中央」ボタン
const int RIGHT_ANGLE  = 180;  // 「右 ▶」ボタン

// ③ LED マトリクスの絵（横12 × 縦8。「#」が光る、「.」が消える）
//    1行は必ず12文字。行は8行。
const char* PICTURE_LEFT[8] = {
  "....#.......",
  "...##.......",
  "..#########.",
  ".##########.",
  ".##########.",
  "..#########.",
  "...##.......",
  "....#......."
};

const char* PICTURE_CENTER[8] = {
  "............",
  "....####....",
  "...######...",
  "...######...",
  "...######...",
  "...######...",
  "....####....",
  "............"
};

const char* PICTURE_RIGHT[8] = {
  ".......#....",
  ".......##...",
  ".#########..",
  ".##########.",
  ".##########.",
  ".#########..",
  ".......##...",
  ".......#...."
};

// =================== ★ここを変える（ここまで）===================

// ---- 数値の確かめ（範囲の外なら、書き込む前に止まって知らせる）----
static_assert(LEFT_ANGLE >= 0 && LEFT_ANGLE <= 180, "LEFT_ANGLE は 0〜180 にしてください");
static_assert(CENTER_ANGLE >= 0 && CENTER_ANGLE <= 180, "CENTER_ANGLE は 0〜180 にしてください");
static_assert(RIGHT_ANGLE >= 0 && RIGHT_ANGLE <= 180, "RIGHT_ANGLE は 0〜180 にしてください");

// ---- ここから下は変更しない ----
const int SERVO_PIN = 9;  // サーボの信号の線（D9）

// BLE の設定（index.html と同じ番号）
BLEService servoService("e5969935-3a7c-4236-9516-9cd2c60c2c6d");
BLEByteCharacteristic commandChar("1c2fb868-731c-4916-bbc4-952cb7d5133a",
                                  BLEWrite | BLEWriteWithoutResponse);
BLEStringCharacteristic statusChar("a483820f-fee4-499b-b366-738eeb482acb",
                                   BLERead | BLENotify, 32);

Servo servo;
ArduinoLEDMatrix matrix;
bool bleReady = false;
char lastCommand = 'C';
int lastAngle = CENTER_ANGLE;

// 数字（横3 × 縦5）：待っている間、機器名の番号を LED に表示する
const char* DIGITS[10][5] = {
  {"###", "#.#", "#.#", "#.#", "###"}, {".#.", "##.", ".#.", ".#.", "###"},
  {"###", "..#", "###", "#..", "###"}, {"###", "..#", "###", "..#", "###"},
  {"#.#", "#.#", "###", "..#", "..#"}, {"###", "#..", "###", "..#", "###"},
  {"###", "#..", "###", "#.#", "###"}, {"###", "..#", "..#", "..#", "..#"},
  {"###", "#.#", "###", "#.#", "###"}, {"###", "#.#", "###", "..#", "###"}
};

// 絵（8行 × 12文字）を LED マトリクスに表示する
void showPicture(const char* picture[8]) {
  uint8_t frame[8][12] = {0};
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 12 && picture[y][x] != '\0'; x++) {
      if (picture[y][x] == '#') frame[y][x] = 1;
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

// 機器名の最後の2文字（番号）を LED マトリクスに表示する
void showNumber() {
  uint8_t frame[8][12] = {0};
  int len = strlen(DEVICE_NAME);
  for (int k = 0; k < 2; k++) {
    int i = len - 2 + k;
    if (i < 0) continue;
    char c = DEVICE_NAME[i];
    if (c < '0' || c > '9') continue;
    int left = (k == 0) ? 2 : 7;  // 1けた目は左から3列目、2けた目は8列目から
    for (int y = 0; y < 5; y++) {
      for (int x = 0; x < 3; x++) {
        if (DIGITS[c - '0'][y][x] == '#') frame[y + 1][left + x] = 1;
      }
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

// PC へ今の状態を知らせる（例：「L:0:NGS-01」）
void report() {
  char text[32];
  snprintf(text, sizeof(text), "%c:%d:%s", lastCommand, lastAngle, DEVICE_NAME);
  Serial.println(text);
  if (bleReady) statusChar.writeValue(text);
}

// 指示を受け取ったときの動き
void handleCommand(char cmd) {
  if (cmd >= 'a' && cmd <= 'z') cmd = cmd - 'a' + 'A';  // 小文字も受け付ける
  if (cmd == 'L') {              // L なら左
    servo.write(LEFT_ANGLE);     // 左の角度へ
    showPicture(PICTURE_LEFT);   // 左の絵を表示
    lastAngle = LEFT_ANGLE;
  } else if (cmd == 'C') {       // そうでなく、C なら中央
    servo.write(CENTER_ANGLE);   // 中央の角度へ
    showPicture(PICTURE_CENTER); // 中央の絵を表示
    lastAngle = CENTER_ANGLE;
  } else if (cmd == 'R') {       // そうでなく、R なら右
    servo.write(RIGHT_ANGLE);    // 右の角度へ
    showPicture(PICTURE_RIGHT);  // 右の絵を表示
    lastAngle = RIGHT_ANGLE;
  } else {
    return;  // L・C・R 以外（改行など）は何もしない
  }
  lastCommand = cmd;
  report();
}

// Bluetooth で指示が届いたとき
void onCommandWritten(BLEDevice central, BLECharacteristic characteristic) {
  handleCommand((char)commandChar.value());
}

// PC とつながったとき
void onConnected(BLEDevice central) {
  Serial.print("connected: ");
  Serial.println(central.address());
  report();  // 今の状態を PC が読めるようにする
}

// PC との接続が切れたとき
void onDisconnected(BLEDevice central) {
  Serial.println("disconnected");
  showNumber();     // 番号の表示に戻す（サーボはその角度のまま）
  BLE.advertise();  // もう一度つなげるように待つ
}

void setup() {
  Serial.begin(9600);
  matrix.begin();

  servo.attach(SERVO_PIN);
  servo.write(CENTER_ANGLE);  // 電源を入れたら中央へ
  lastCommand = 'C';
  lastAngle = CENTER_ANGLE;

  if (BLE.begin()) {
    BLE.setLocalName(DEVICE_NAME);
    BLE.setDeviceName(DEVICE_NAME);
    BLE.setAdvertisedService(servoService);
    servoService.addCharacteristic(commandChar);
    servoService.addCharacteristic(statusChar);
    BLE.addService(servoService);
    commandChar.setEventHandler(BLEWritten, onCommandWritten);
    BLE.setEventHandler(BLEConnected, onConnected);
    BLE.setEventHandler(BLEDisconnected, onDisconnected);
    bleReady = true;
    report();
    BLE.advertise();
    showNumber();  // 番号が出たら「接続を待っている」
    Serial.print("BLE OK: ");
    Serial.println(DEVICE_NAME);
  } else {
    // Bluetooth を始められなかった：LED に「×」を出す
    const char* PICTURE_ERROR[8] = {
      "............", "...#....#...", "....#..#....", ".....##.....",
      ".....##.....", "....#..#....", "...#....#...", "............"
    };
    showPicture(PICTURE_ERROR);
    Serial.println("BLE start failed. Serial Monitor (9600): L / C / R");
  }
}

void loop() {
  if (bleReady) BLE.poll();  // Bluetooth の処理（止めずに毎回まわす）

  // 予備：シリアルモニター（9600）から L・C・R を送っても動く
  while (Serial.available() > 0) {
    handleCommand((char)Serial.read());
  }
}
