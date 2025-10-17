#include <SPI.h>
#include <MFRC522.h>


// до ~ 80 строки - chatGPT(реализация словаря)


struct Pair {
  String key;
  int value;
};

class SimpleDict {
  static const int MAX_SIZE = 100;   // максимум пар ключ-значение
  Pair data[MAX_SIZE];
  int size = 0;

public:
  // Добавление или изменение значения по ключу
  void put(String key, int value) {
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
#define SS_PIN 10
#define num_of_pows 6

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

SimpleDict owes; //словарь должников





bool alarm_on = true;


bool full[num_of_pows];
bool real_full[num_of_pows];

String cardUID = "";






void setup() {
  Serial.begin(9600);

  SPI.begin();
  Serial.println("SPI init done");

  rfid.PCD_Init();
  Serial.println("RFID init done");


  Serial.println("начало работы");



  //заполняем паверами хранилище
  for (byte i = 0; i < num_of_pows; i++){
    full[i] = 1;
  }
  //массив с реальными показаниями, есть ли датчик
  for (byte i = 0; i < num_of_pows; i++){
    real_full[i] = 0;
  }
    //???
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF; // стандартный ключ
  }
}



void security_check(){
  //проверка соответствия full (состояния по протоколу) real_full(состоянию с датчиков)
  //перебираем ячейки
  for (int i=0; i < num_of_pows;i++){
    if (full[i] != real_full[i]){
      //alarm
      0;
    }
  }
}

void check(){
  //проверка приложили ли карточку
  
  if ( ! mfrc522.PICC_IsNewCardPresent()) { 
    return; 
  } 
  if ( ! mfrc522.PICC_ReadCardSerial()) { 
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


  //do_smth(cardUID); // даём пользователю возможность что-то сделать
}

void update_real_full(){
  0;
}

// даём пользователю возможность что-то сделать
void do_smth(String UID){
  if (!owes.has(UID)){
    //не авторизован, мб что-то допилить надо?
    Serial.println("Неидентифицированы");
  }else{
    Serial.println("Идентифицированы");
    //вроде не надо
    //if (owes.get(UID) == 0){
    //  alarm_on = False
    //}
    bool f = true;
    while (f){
      //обновление массивов
      int prev_real_full[num_of_pows];
      for (byte i = 0; i < num_of_pows;i++){
        prev_real_full[i] = real_full[i];
      }
      update_real_full();

      //перебираем ячейки
      for (byte i = 0; i<num_of_pows; i++){
        //проверка, что сделали то действие, которое нужно
        if ((real_full[i] != owes.get(UID))  && (prev_real_full[i] != real_full[i])){
          //ВЗЯЛИ/ОТДАЛИ ПАВЕР
          full[i] = owes.get(UID);

          if (full[i] == 1){
            Serial.println("сдал павер");
          }else{
            Serial.println("взял павер");
          }

          owes.put(UID, 1 - owes.get(UID));
          alarm_on = true;
          f = false;
          break;
        }  
      }
      //security_check();
    }
  }
}



void loop() {
  check();
  delay(500);
}