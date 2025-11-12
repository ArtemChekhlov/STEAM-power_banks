#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>

// ---------- СЛОВАРЬ UID → значение (0/1) ----------

struct Pair {
  String key;
  int value;
};

class SimpleDict {
  static const int MAX_SIZE = 50;
  Pair data[MAX_SIZE];
  int size = 0;

public:
  void put(String key, int value) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) {
        data[i].value = value;
        return;
      }
    }
    if (size < MAX_SIZE) {
      data[size].key = key;
      data[size].value = value;
      size++;
    }
  }

  int get(String key) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) return data[i].value;
    }
    return -1;
  }

  bool has(String key) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) return true;
    }
    return false;
  }

  void printAll() {
    Serial.println("=== Содержимое словаря ===");
    for (int i = 0; i < size; i++) {
      Serial.print(data[i].key);
      Serial.print(": ");
      Serial.println(data[i].value);
    }
    Serial.println("==========================");
  }
};


// -----------------настройки таймаута (ms)--------------------
// Параметры
const unsigned long PRESENCE_TIMEOUT = 1200; // ms для определения что карта ушла
const uint8_t MAX_MISSES = 4;                // сколько подряд неудачных опросов считаем "ушла"
const uint8_t READ_ATTEMPTS = 3;             // сколько попыток ReadCardSerial за цикл

struct ReaderState {
  bool present = false;
  String uid = "";
  unsigned long lastSeen = 0;
  uint8_t misses = 0;
};

ReaderState st1, st2;
//----------- МАССИВЫ И ПЕРЕМЕННЫЕ -----

SimpleDict owes;


#define NUM_OF_POWS 6

bool full[NUM_OF_POWS];
bool real_full[NUM_OF_POWS];
String powsUIDs[NUM_OF_POWS];

String inputString = "";
bool stringComplete = false;
bool alarm = false;
//---------- КЛАВА -------------
const byte ROWS = 4; // 4 строки
const byte COLS = 4; // 4 столбца
char keys[ROWS][COLS] = {
  {'A','1','2','3'},
  {'B','4','5','6'},
  {'C','7','8','9'},
  {'D','*','0','#'}
}; 
byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {A0, A1, A2, 6};
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
String keyboard_input = "";


// ---------- ПОДКЛЮЧЕНИЕ RFID ----------

#define SS_PIN1 8
#define SS_PIN2 10
#define RST_PIN1 9
#define RST_PIN2 7
#define ZOOMER_PIN A0
#define WAIT_LED LED_BUILTIN

MFRC522 rfid1(SS_PIN1, RST_PIN1);
MFRC522 rfid2(SS_PIN2, RST_PIN2);

// ---------- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ----------
bool inArray(String value, String array[], int size) {
  for (int i = 0; i < size; i++) {
    if (array[i] == value) return true;
  }
  return false;
}
void selectReader(uint8_t n) {
  digitalWrite(SS_PIN1, n == 1 ? LOW : HIGH);
  digitalWrite(SS_PIN2, n == 2 ? LOW : HIGH);
  delay(5);
}

void security_check() {
  bool flag = false;
  for (int i = 0; i < NUM_OF_POWS; i++) {
    if (full[i] != real_full[i]) {
      Serial.println("⚠️ ALARM! Несоответствие состояний!");
      flag = true;
      alarm = true;
    }
  if (!flag){
    alarm = false;
  }
  }
  digitalWrite(ZOOMER_PIN, flag ? HIGH : LOW);
}

void update_real_full_old() {
  if (stringComplete) {
    Serial.println("updating...");
    for (int i = 0; i < NUM_OF_POWS && i < inputString.length(); i++) {
      real_full[i] = (inputString[i] == '1');
    }
    Serial.print("real_full: ");
    for (int i = 0; i < NUM_OF_POWS; i++) Serial.print(real_full[i]);
    Serial.println();
    inputString = "";
    stringComplete = false;
  }
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') stringComplete = true;
  }
}
// ---------- ОСНОВНАЯ ЛОГИКА ----------
void give_power(String UID) {
  if (!owes.has(UID)) {
    Serial.println("❌ Пользователь не идентифицирован");
    return;
  }

  Serial.println("✅ Пользователь идентифицирован");

  bool f = true;
  bool prev_real_full[NUM_OF_POWS];
  for (int i = 0; i < NUM_OF_POWS; i++) prev_real_full[i] = real_full[i];

  while (f) {
    serialEvent();
    update_real_full();

    for (int i = 0; i < NUM_OF_POWS; i++) {
      if ((real_full[i] == (owes.get(UID) > 0)) && (prev_real_full[i] != real_full[i])) {
        Serial.println("⚙️ ВЗЯЛИ/ОТДАЛИ ПАВЕР");
        full[i] = real_full[i];

        if (full[i]) {
          Serial.println(UID + " сдал павер");
          owes.put(UID, 0);
        } else {
          Serial.println(UID + " взял павер");
          owes.put(UID, 1);
        }

        f = false;
        break;
      }
    }
    security_check();
    delay(500);
  }
}
// Вспомогательная функция: пытаемся прочитать UID с несколькими попытками
bool tryReadUID(MFRC522 &reader, String &outUID) {
  // несколько попыток с небольшими паузами — повышает устойчивость
  for (uint8_t attempt = 0; attempt < READ_ATTEMPTS; attempt++) {
    // Сначала пробуем ReadCardSerial напрямую (работает, если карта уже в поле)
    if (reader.PICC_ReadCardSerial()) {
      outUID = "";
      for (byte i = 0; i < reader.uid.size; i++) {
        if (reader.uid.uidByte[i] < 0x10) outUID += "0";
        outUID += String(reader.uid.uidByte[i], HEX);
      }
      outUID.toUpperCase();
      // НЕ вызваем HaltA() — оставляем карту "в поле"
      return true;
    }

    // Если прямой read не сработал, попробуем "обычный" путь: проверить new card и затем читать
    if (reader.PICC_IsNewCardPresent()) {
      if (reader.PICC_ReadCardSerial()) {
        outUID = "";
        for (byte i = 0; i < reader.uid.size; i++) {
          if (reader.uid.uidByte[i] < 0x10) outUID += "0";
          outUID += String(reader.uid.uidByte[i], HEX);
        }
        outUID.toUpperCase();
        return true;
      }
    }

    delay(20); // небольшая пауза между попытками
  }
  return false;
}

String pollReader(MFRC522 &reader, uint8_t which, ReaderState &st) {
  selectReader(which);

  String readUID;
  bool got = tryReadUID(reader, readUID);

  if (got) {
    // Успешное чтение — обновляем состояние
    st.present = true;
    st.uid = readUID;
    st.lastSeen = millis();
    st.misses = 0;
    Serial.print("R"); Serial.print(which); Serial.print(" present UID: "); Serial.println(readUID);
    return readUID;
  } else {
    // Не смогли прочитать в этом цикле
    if (st.present) {
      st.misses++;
      // если накопились пропуски или превысил таймаут — считаем карту ушедшей
      if (st.misses >= MAX_MISSES || millis() - st.lastSeen > PRESENCE_TIMEOUT) {
        // Корректно завершить только если уверены, что карта ушла.
        // reader.PICC_HaltA();  // НЕ обязательно, но можно вызывать
        reader.PCD_StopCrypto1();
        st.present = false;
        st.uid = "";
        st.misses = 0;
      }
    } else {
      // карта и так считается отсутствующей — ничего не делаем
    }

    // Если ридер вообще перестал отвечать (много циклов без чтений), можно реинициализировать:
    static unsigned long lastReinit[3] = {0,0,0}; // индекс 1 и 2 используем
    if (millis() - lastReinit[which] > 5000 && st.misses >= (MAX_MISSES + 4)) {
      Serial.print("R"); Serial.print(which); Serial.println(" — реинициализация модуля (восстановление)");
      reader.PCD_Init();
      lastReinit[which] = millis();
      delay(50);
    }
    return "00000000";
  }

  selectReader(0);
}
void update_real_full(){
  if (inArray(pollReader(rfid1, 1, st1), powsUIDs, NUM_OF_POWS)){
    real_full[0] = true;
  }else{
    real_full[0] = false;
  }
}
void checkReader(MFRC522 &reader, uint8_t which){
  selectReader(which);
  if (!reader.PICC_IsNewCardPresent()) { selectReader(0); return; }
  if (!reader.PICC_ReadCardSerial()) { selectReader(0); return; }

  String cardUID = "";
  for (byte i = 0; i < reader.uid.size; i++) {
    if (reader.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(reader.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();

  Serial.print("RFID");
  Serial.print(which);
  Serial.print(" UID считан: ");
  Serial.println(cardUID);

  reader.PICC_HaltA();
  reader.PCD_StopCrypto1();
  selectReader(0);

  give_power(cardUID);
}

void serial_check() {
  if (!stringComplete) return;
  if (inputString == "auth\n") {
    Serial.println("auth!");
    inputString = "";
    stringComplete = false;
    give_power("B6C77905");
  } else {
    update_real_full();
  }
}
void keyboard_check(){
  char key = keypad.getKey();
  if (key){
    if (key == '#'){
      Serial.println("Clearing");
      keyboard_input = "";
    }else{
      keyboard_input = keyboard_input + key;
    } 
    if (keyboard_input.length() == 6){
      Serial.print("password entered:");
      Serial.println(keyboard_input);
      give_power(keyboard_input);



      keyboard_input = "";
      
    }
  }
}

// ---------- SETUP / LOOP ----------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Serial started");

  pinMode(SS_PIN1, OUTPUT);
  pinMode(SS_PIN2, OUTPUT);
  digitalWrite(SS_PIN1, HIGH);
  digitalWrite(SS_PIN2, HIGH);

  SPI.begin();
  delay(50);

  // Инициализация обоих считывателей
  selectReader(1);
  rfid1.PCD_Init();
  delay(50);
  Serial.print("RFID1 version: 0x");
  Serial.println(rfid1.PCD_ReadRegister(MFRC522::VersionReg), HEX);

  selectReader(2);
  rfid2.PCD_Init();
  delay(50);
  Serial.print("RFID2 version: 0x");
  Serial.println(rfid2.PCD_ReadRegister(MFRC522::VersionReg), HEX);

  selectReader(0);
  Serial.println("RFID init done");

  for (int i = 0; i < NUM_OF_POWS; i++) {
    full[i] = true;
    real_full[i] = true;
  }

  powsUIDs[0] = "129AA104";
  powsUIDs[1] = "129AA104";
  powsUIDs[2] = "129AA104";
  powsUIDs[3] = "129AA104";
  powsUIDs[4] = "129AA104";
  powsUIDs[5] = "129AA104";


  pinMode(ZOOMER_PIN, OUTPUT);
  pinMode(WAIT_LED, OUTPUT);

  owes.put("A25F6206", 0);


  Serial.println("Начало работы");
  inputString.reserve(16);
}
void loop() {
  if (!alarm){
     checkReader(rfid2, 2);
  }
  pollReader(rfid1, 1, st1);
  update_real_full();
  security_check();
  //keyboard_check();
  delay(100);
}
