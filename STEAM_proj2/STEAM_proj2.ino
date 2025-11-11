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
const unsigned long PRESENCE_TIMEOUT = 1000; // если нет чтения за 1 с => карта ушла
const uint8_t MAX_MISSES = 3; // либо считать удалённой после N подряд неудач

struct ReaderState {
  bool present = false;
  String uid = "";
  unsigned long lastSeen = 0;
  uint8_t misses = 0;
};

ReaderState st1, st2;

// ---------- ПОДКЛЮЧЕНИЕ RFID ----------

#define SS_PIN1 8
#define SS_PIN2 10
#define RST_PIN1 9
#define RST_PIN2 7
#define NUM_OF_POWS 6
#define ZOOMER_PIN A5
#define WAIT_LED LED_BUILTIN

MFRC522 rfid1(SS_PIN1, RST_PIN1);
MFRC522 rfid2(SS_PIN2, RST_PIN2);

SimpleDict owes;

bool full[NUM_OF_POWS];
bool real_full[NUM_OF_POWS];

String inputString = "";
bool stringComplete = false;

// ---------- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ----------

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
    }
  }
  digitalWrite(ZOOMER_PIN, flag ? HIGH : LOW);
}

void update_real_full() {
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
// Функция: опрос ридера, поддерживает состояние "карта есть/нет"
void pollReader(MFRC522 &reader, uint8_t which, ReaderState &st) {
  selectReader(which);

  // Пробуем прочитать UID (если карта в поле — ReadCardSerial обычно возвращает true)
  bool got = false;
 
  if (reader.PICC_ReadCardSerial()) {
    got = true;

  if (got) {
    // Сформируем строковый UID
    String cardUID = "";
    for (byte i = 0; i < reader.uid.size; i++) {
      if (reader.uid.uidByte[i] < 0x10) cardUID += "0";
      cardUID += String(reader.uid.uidByte[i], HEX);
    }
    cardUID.toUpperCase();

    // Обновляем состояние "присутствует"
    st.present = true;
    st.uid = cardUID;
    st.lastSeen = millis();
    st.misses = 0;

    // НЕ вызываем HaltA() здесь — иначе карточка "убьётся" и дальнейшие опросы не сработают
    Serial.print("R");
    Serial.print(which);
    Serial.print(" present UID: ");
    Serial.println(cardUID);

    // если нужно, выполняем обработку появления карты только при первом срабатывании:
    static String lastHandledUID1 = "";
    static String lastHandledUID2 = "";
    String *pLastHandled = (which == 1) ? &lastHandledUID1 : &lastHandledUID2;
    if (*pLastHandled != cardUID) {
      // первая фиксация появления новой карты (или повторное поднесение другой карты)
      Serial.print("-> first detection (or changed) for R");
      Serial.println(which);
      // Здесь: вызываем give_power(cardUID) или другую логику по появлению карты
      // give_power(cardUID);
      *pLastHandled = cardUID;
    }

    // Освобождаем считыватель для следующего раза, но не даём ему "заснуть" навсегда
    // (не вызываем reader.PICC_HaltA() тут)
  } else {
    // не получили UID в этом опросе
    // увеличиваем счётчик пропусков
    if (st.present) {
      st.misses++;
      // если пропусков накопилось много — или прошло >PRESENCE_TIMEOUT => считаем карту ушедшей
      if (st.misses >= MAX_MISSES || millis() - st.lastSeen > PRESENCE_TIMEOUT) {
        Serial.print("R");
        Serial.print(which);
        Serial.println(" — карта ушла");
        // корректно завершить сеанс
        reader.PICC_HaltA();
        reader.PCD_StopCrypto1();

        // сброс состояния
        st.present = false;
        st.uid = "";
        st.misses = 0;
      }
    }
  }
  }
  selectReader(0);
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

  pinMode(ZOOMER_PIN, OUTPUT);
  pinMode(WAIT_LED, OUTPUT);

  owes.put("B6E9FC4D", 0);

  Serial.println("Начало работы");
  inputString.reserve(16);
}

void loop() {
  checkReader(rfid1, 1);
  checkReader(rfid2, 2);
  delay(100);
}
