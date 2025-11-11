#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h> // Подключаем библиотеки

// до ~ 80 строки - chatGPT(реализация словаря)


struct Pair {
  String key;
  int value;
};

class SimpleDict {
  static const int MAX_SIZE = 50;   // максимум пар ключ-значение
  Pair data[MAX_SIZE];
  int size = 0;

public:
  // Добавление или изменение значения по ключу
  void put(String key, unsigned char value) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) {
        data[i].value = value;  // если ключ найден — обновляем
        return;
      }
    }
    if (size < MAX_SIZE) {      // если ключ новый и есть место
      data[size].key = key;
      data[size].value = value;
      size++;
    }
  }

  // Получение значения по ключу
  int get(String key) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) return data[i].value;
    }
    return -1; // если не найдено
  }

  // Проверка наличия ключа
  bool has(String key) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) return true;
    }
    return false;
  }

  // Удаление по ключу
  void remove(String key) {
    for (int i = 0; i < size; i++) {
      if (data[i].key == key) {
        // сдвигаем элементы влево
        for (int j = i; j < size - 1; j++) {
          data[j] = data[j + 1];
        }
        size--;
        return;
      }
    }
  }

  // Вывод всех пар в Serial (для отладки)
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






#define RST_PIN 9
#define SS_PIN 8
#define SS_PIN2 10
#define num_of_pows 6 //кол-во павер банков
#define waiting_indicator_pin LED_BUILTIN //номер пина для светодиода, 
                                          //показывающего состояние ожидания дейстия
#define zoomer_pin A5

/*
//клава
const byte ROWS = 4; // 4 строки
const byte COLS = 4; // 4 столбца
char keys[ROWS][COLS] = {
  {'A','1','2','3'},
  {'B','4','5','6'},
  {'C','7','8','9'},
  {'D','*','0','#'}
}; 
byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] =  {A0, A1, A2, A3}; 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
String keyboard_input = "";
*/

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522 rfid2(SS_PIN2, RST_PIN);

SimpleDict owes; //словарь вида UID: 0(не взял)/1(взял)




String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

bool full[num_of_pows]; //ожидаемое состояние хранилища
bool real_full[num_of_pows]; //реальное состояние хранилища

String cardUID = "";


void setup() {
  
  Serial.begin(115200);
  delay(200);
  Serial.println("Serial started");


  pinMode(SS_PIN, OUTPUT);
  pinMode(SS_PIN2, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  digitalWrite(SS_PIN2, HIGH);

  digitalWrite(SS_PIN, HIGH);
  digitalWrite(SS_PIN2, HIGH);
  delay(10);


  SPI.begin();
  Serial.println("SPI init done");

  selectReader(1);
  rfid.PCD_Init();
  byte v1 = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("RFID1 version: 0x");
  Serial.println(v1, HEX);

  selectReader(2);
  rfid2.PCD_Init();
  byte v2 = rfid2.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("RFID2 version: 0x");
  Serial.println(v2, HEX);

  selectReader(0); // оба выключены
  Serial.println("RFID init done");

  Serial.println("начало работы");



  //заполняем паверами хранилище
  for (byte i = 0; i < num_of_pows; i++){
    full[i] = 1;
  }
  //массив с реальными показаниями, есть ли датчик
  for (byte i = 0; i < num_of_pows; i++){
    real_full[i] = 1;
  }
    //???
  //for (byte i = 0; i < num_of_pows; i++) {
    //key.keyByte[i] = 0xFF; // стандартный ключ
  //}



  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print("Version: ");
  Serial.println(v, HEX);



  pinMode(zoomer_pin, OUTPUT);

  owes.put("B6C77905", 0);

  inputString.reserve(5);
}

void selectReader(uint8_t which) {
  // which: 1 -> rfid, 2 -> rfid2, 0 -> ни один
  if (which == 1) {
    digitalWrite(SS_PIN, LOW);
    digitalWrite(SS_PIN2, HIGH);
  } else if (which == 2) {
    digitalWrite(SS_PIN, HIGH);
    digitalWrite(SS_PIN2, LOW);
  } else {
    digitalWrite(SS_PIN, HIGH);
    digitalWrite(SS_PIN2, HIGH);
  }
  delay(5); // пауза для стабилизации SPI-линий
}
void security_check(){
  //проверка соответствия full (состояния по протоколу) real_full(состоянию с датчиков)
  //перебираем ячейки
  bool flag = 0;
  for (int i=0; i < num_of_pows;i++){
    if (full[i] != real_full[i]){
      bool alarm = true;
      Serial.println("alarm!!!");
      flag = 1;
      0;
  if (flag == 0){
    alarm = false;
  }
  //digitalWrite(zoomer_pin, alarm);
    }
  }
}
// даём пользователю возможность что-то сделать
void give_power(String UID){
  if (!owes.has(UID)){
    //не авторизован, мб что-то допилить надо?
    Serial.println("Пользователь неидентифицирован");
  }else{
    Serial.println("Пользователь идентифицирован");

    //переходим в режим ожидания действия
    bool f = true;

    int prev_real_full[num_of_pows];
      for (byte i = 0; i < num_of_pows;i++){
        prev_real_full[i] = real_full[i];
      }

    while (f){
      //обновление массивов
      serialEvent();
      update_real_full();
      //перебираем ячейки
      for (byte i = 0; i<num_of_pows; i++){
        //проверка, что сделали то действие, которое нужно 
        if ((real_full[i] == (owes.get(UID) > 0))  && (prev_real_full[i] != real_full[i])){
          Serial.println("ВЗЯЛИ/ОТДАЛИ ПАВЕР");
          //ВЗЯЛИ/ОТДАЛИ ПАВЕР
          full[i] = real_full[i];

          if (full[i] == 1){
            Serial.print(UID);
            Serial.println(" сдал павер");
          }else{
            Serial.print(UID);
            Serial.println(" взял павер");
          }

          if (owes.get(UID) == 0){
            owes.put(UID, 1); // переделать в int, заменить 1 на  TIME.milliis !!!
          }
          else{
            owes.put(UID, 0);
          }
          f = false;
          break;
        }  
      }
      security_check();;
      delay(500);
    }
  }
}
void check() {
  selectReader(1); // выбираем первый считыватель

  if (!rfid.PICC_IsNewCardPresent()) {
    selectReader(0);
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) {
    selectReader(0);
    return;
  }

  String cardUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(rfid.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();

  Serial.println("RFID1 UID считан: " + cardUID);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  selectReader(0);

  give_power(cardUID);
}
void check2() {
  selectReader(2); // выбираем второй считыватель

  if (!rfid2.PICC_IsNewCardPresent()) {
    selectReader(0);
    return;
  }
  if (!rfid2.PICC_ReadCardSerial()) {
    selectReader(0);
    return;
  }

  String cardUID = "";
  for (byte i = 0; i < rfid2.uid.size; i++) {
    if (rfid2.uid.uidByte[i] < 0x10) cardUID += "0";
    cardUID += String(rfid2.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();

  Serial.println("RFID2 UID считан: " + cardUID);

  rfid2.PICC_HaltA();
  rfid2.PCD_StopCrypto1();
  selectReader(0);

  give_power(cardUID);
}
 //допилить!!!
void update_real_full(){ //обновляем реальное состояние хранилища
    Serial.println("updating...");
    if (stringComplete){
      for (char i = 0; i < num_of_pows; i ++){
        real_full[i] = (inputString[i] == '1');;
      }
      

      for (int i = 0; i < num_of_pows; i++) {
        Serial.print(real_full[i]);  // выведет 1 или 0
      }
      Serial.println();  // перевод строки
      inputString = "";
      stringComplete = false;
      Serial.println("updated");
    }
}
void serialEvent() {//считывание Serial для тестирования update_full
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
void serial_check(){
  if (stringComplete){
    if (inputString == "auth\n"){
      Serial.println("auth!");
      inputString = "";
      stringComplete = false;
      give_power("B6C77905");
    }else{
      update_real_full();
      inputString = "";
      stringComplete = false;
    }
    // clear the string:
  }
}
/*
void keyboard_check(){
  char key = keypad.getKey();
  if (key){
    if (key == '#'){
      keyboard_input = "";
    }else{
      keyboard_input = keyboard_input + key;
    } 
    if (keyboard_input.length() == 6){
      Serial.print("password entered:");
      Serial.println(keyboard_input);
      keyboard_input = "";
      
    }
  }
}
*/
void check_new(){
  digitalWrite(SS_PIN, LOW);
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  Serial.print("UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
}

void loop() {
  check();
  delay(100);
}