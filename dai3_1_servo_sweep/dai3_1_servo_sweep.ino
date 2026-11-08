// dai3_1_servo_sweep（第3回：サーボを 0° から 180° まで往復させる）
// ボード：Arduino UNO R4 WiFi（Bluetooth は使わない）
// 配線：サーボの信号の線 → D9
//       サーボの電源の線（＋）→ 5V
//       サーボのGNDの線（−）→ GND
// 使う場面：①ホーンを付ける前に、サーボが動くことを確かめる
//           ④90° に合わせてホーンを付けたあと、ホーンの往復を確かめる
//           （その間に使う「90° に止める」スケッチは dai3_1_servo_90）
// 配線を変えるときは、必ずUSBケーブルを外してから行います。
// 「//」から右は人が読むための説明（コメント）です。実行されません。

#include <Servo.h>

Servo servo;

void setup() {
  // サーボの信号線をD9につなぐ
  servo.attach(9);
}

void loop() {
  // 0°から180°まで、1°ずつ角度を増やす
  for (int angle = 0; angle <= 180; angle++) {
    servo.write(angle);
    delay(20);
  }

  // 180°で少し待つ
  delay(500);

  // 180°から0°まで、1°ずつ角度を減らす
  for (int angle = 180; angle >= 0; angle--) {
    servo.write(angle);
    delay(20);
  }

  // 0°で少し待つ
  delay(500);
}
