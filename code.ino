#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Servo.h>

// ================= PIN MAP =================
// RC522: SDA=D10, RST=D9, SCK=D13, MOSI=D11, MISO=D12
#define SS_PIN   10
#define RST_PIN   9

// Buttons
#define REMOVE_BUTTON 2
#define ADD_BUTTON    3
#define RESET_BUTTON  4

// Buzzer
#define BUZZER        5

// LCD 16x2
LiquidCrystal lcd(7, 6, 8, A0, A1, A2);

// IR sensor + Servo
#define IR_SENSOR A3
#define SERVO_PIN A4
Servo myservo;

// ================= RFID =================
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ================= Items =================
struct Item {
  const char* name;
  const char* uid;
  int price;
};

const Item ITEM_LIST[] = {
  {"Tata salt",   "F7 14 CA 01", 100},
  {"Milk Powder", "A2 DB B3 02",  50},
  {"Bournvita",   "13 1F A3 0D",  20},
  {"Ball Pen",    "E3 68 DC 12",  10},
};
const int NUM_ITEMS = sizeof(ITEM_LIST) / sizeof(ITEM_LIST[0]);

int cart_count[NUM_ITEMS] = {0};
int bill_amount = 0;

// Modes
bool add_mode = true;
bool remove_mode = false;

// RFID cooldown
String lastUID = "";
unsigned long lastReadMs = 0;
const unsigned long CARD_COOLDOWN_MS = 1200;

// ================= Helpers =================
void beep(unsigned ms = 160) {
  digitalWrite(BUZZER, HIGH);
  delay(ms);
  digitalWrite(BUZZER, LOW);
}

String readUIDString() {
  String s = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) s += "0";
    s += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) s += " ";
  }
  s.toUpperCase();
  return s;
}

int findItemIndexByUID(const String& uid) {
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (uid == ITEM_LIST[i].uid) return i;
  }
  return -1;
}

void showTotal() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total: ");
  lcd.print(bill_amount);
  lcd.print(" Rs");
  lcd.setCursor(0, 1);
  lcd.print(add_mode ? "Mode: ADD   " : "Mode: REMOVE");
}

void showWelcome() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome to the");
  lcd.setCursor(0, 1);
  lcd.print("    Store    ");
  delay(2000);
}

void setModeAdd() {
  add_mode = true;  remove_mode = false;
  lcd.clear();
  lcd.print("Mode: ADD");
  delay(600);
  showTotal();
}

void setModeRemove() {
  add_mode = false; remove_mode = true;
  lcd.clear();
  lcd.print("Mode: REMOVE");
  delay(600);
  showTotal();
}

// Servo open/close lid
void openLid() {
  myservo.write(90);   // open lid
  delay(2000);         // wait
  myservo.write(0);    // close lid
  delay(800);
}

// ================= Setup =================
void setup() {
  pinMode(ADD_BUTTON,    INPUT);
  pinMode(REMOVE_BUTTON, INPUT);
  pinMode(RESET_BUTTON,  INPUT);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  pinMode(IR_SENSOR, INPUT);

  myservo.attach(SERVO_PIN);
  myservo.write(0); // lid closed initially

  lcd.begin(16, 2);
  delay(100);
  showWelcome();
  lcd.clear();
  lcd.print("Initializing...");

  SPI.begin();
  mfrc522.PCD_Init();
  delay(150);

  showTotal();

  Serial.begin(9600);
  Serial.println("Smart Trolley Ready");
}

// ================= Loop =================
void loop() {
  // --- Buttons ---
  if (digitalRead(ADD_BUTTON)) {
    setModeAdd();
    delay(150);
  }
  if (digitalRead(REMOVE_BUTTON)) {
    setModeRemove();
    delay(150);
  }
  if (digitalRead(RESET_BUTTON)) {
    bill_amount = 0;
    for (int i = 0; i < NUM_ITEMS; i++) cart_count[i] = 0;
    lcd.clear();
    lcd.print("Cart Reset!");
    beep();
    delay(800);
    showTotal();
  }

  // --- IR Sensor + Servo ---
  int irValue = digitalRead(IR_SENSOR);
  if (irValue == LOW) {   // Object detected (depends on module, adjust if needed)
    openLid();
  }

  // --- RFID ---
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  String uid = readUIDString();
  unsigned long now = millis();

  if (uid == lastUID && (now - lastReadMs) < CARD_COOLDOWN_MS) {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  lastUID = uid;
  lastReadMs = now;

  Serial.print("UID: "); Serial.println(uid);

  int idx = findItemIndexByUID(uid);
  if (idx < 0) {
    lcd.clear();
    lcd.print("Unknown tag");
    delay(900);
    showTotal();
  } else {
    const Item& it = ITEM_LIST[idx];
    if (add_mode) {
      cart_count[idx]++;
      bill_amount += it.price;
      beep();
      lcd.clear();
      lcd.print("Added: ");
      lcd.print(it.price);
      lcd.print("Rs");
      lcd.setCursor(0, 1);
      lcd.print(it.name);
    } else {
      if (cart_count[idx] > 0) {
        cart_count[idx]--;
        bill_amount -= it.price;
        beep();
        lcd.clear();
        lcd.print("Removed: ");
        lcd.print(it.price);
        lcd.print("Rs");
        lcd.setCursor(0, 1);
        lcd.print(it.name);
      } else {
        lcd.clear();
        lcd.print("Not in cart");
        lcd.setCursor(0, 1);
        lcd.print(it.name);
      }
    }
    delay(1200);
    showTotal();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
