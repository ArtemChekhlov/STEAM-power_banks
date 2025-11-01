#include <SPI.h>
#include <MFRC522.h>


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
#define zoomer_pin 1

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522 rfid2(SS_PIN2, RST_PIN);
MFRC522::MIFARE_Key key;

SimpleDict owes; //словарь вида UID: 0(не взял)/1(взял)




String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete

bool full[num_of_pows]; //ожидаемое состояние хранилища
bool real_full[num_of_pows]; //реальное состояние хранилища

String cardUID = "";


void setup() {
  Serial.begin(9600);
  delay(200);
  Serial.println("Serial started");

  SPI.begin();
  Serial.println("SPI init done");

  rfid.PCD_Init();
  Serial.println("RFID1 init done");

  rfid2.PCD_Init();
  Serial.println("RFID2 init done");



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
  for (byte i = 0; i < num_of_pows; i++) {
    key.keyByte[i] = 0xFF; // стандартный ключ
  }


  pinMode(zoomer_pin, OUTPUT);

  owes.put("B6C77905", 0);

  inputString.reserve(5);
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

void check(){
  digitalWrite(SS_PIN2, HIGH); // отключаем второй RFID
  digitalWrite(SS_PIN, LOW);  // активируем первый RFID

  //проверка приложили ли карточку к первому RFID
  if ( ! rfid.PICC_IsNewCardPresent()) { 
    return; 
  } 
  if ( ! rfid.PICC_ReadCardSerial()) { 
    return; 
  }


  // Считываем UID в строку в 16-ричном виде
  String cardUID = ""; // очищаем перед новым чтением
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) cardUID += "0"; // добавляем ведущий 0
    cardUID += String(rfid.uid.uidByte[i], HEX);
  }

  cardUID.toUpperCase(); // для удобства — делаем буквы заглавными

  
  Serial.println("UID считан: " + cardUID); // можно вывести для отладки


  // Отключаем карту
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  rfid.PCD_Init();  // сброс считывателя


  give_power(cardUID); // даём пользователю возможность что-то сделать
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


void check2(){
  digitalWrite(SS_PIN, HIGH); // отключаем первый RFID
  digitalWrite(SS_PIN2, LOW); // включаем второй RFID

  //проверка приложили ли карточку ко второму RFID
  if ( ! rfid2.PICC_IsNewCardPresent()) { 
    return; 
  } 
  if ( ! rfid2.PICC_ReadCardSerial()) { 
    return; 
  }


  // Считываем UID в строку в 16-ричном виде
  String cardUID = ""; // очищаем перед новым чтением
  for (byte i = 0; i < rfid2.uid.size; i++) {
    if (rfid2.uid.uidByte[i] < 0x10) cardUID += "0"; // добавляем ведущий 0
    cardUID += String(rfid2.uid.uidByte[i], HEX);
  }

  cardUID.toUpperCase(); // для удобства — делаем буквы заглавными

  
  Serial.println("rfid_2 UID считан: " + cardUID); // можно вывести для отладки


  // Отключаем карту
  rfid2.PICC_HaltA();
  rfid2.PCD_StopCrypto1();
  rfid2.PCD_Init();  // сброс считывателя


  give_power(cardUID); // даём пользователю возможность что-то сделать
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



void loop() {
  if (stringComplete){
    if (inputString == "auth\n"){
      Serial.println("auth!");
      inputString = "";
      stringComplete = false;
      give_power("B6C77905");
    }
    else{
      update_real_full();
      inputString = "";
      stringComplete = false;
    }
    // clear the string:
  }

  delay(100);
  security_check();
}