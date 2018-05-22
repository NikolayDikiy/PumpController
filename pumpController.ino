//Библиотеки
#include <Arduino.h>
#include <Wire.h> //I2C
#include <ASOLED.h> //Экран
#include "icons.c"
#include <Button.h> //Антидребезг кнопок

//Пины датчиков
#define upSensor 5 //Верхний датчик в бочке (поставить в цифровой 15)
#define midSensor 6 //Средний датчик в бочке (поставить в цифровой 16)
#define downSensor 7 //Нижний датчик в бочке, защита от сухого хода второго насоса (поставить в цифровой 17)
#define pressSensor A0 //Датчик давления

//Пины реле насосов
#define firstPump 8 //Первый насос, из скважины
#define secondPump 9 //Второй насос, из бочки в гидроаккумулятор

//Пины клавиатуры
#define backBtn 4 //вкл. первый насос | уменьшение минимального давления
#define upBtn 5 //вкл. второй насос | увеличение минимального давления
#define downBtn 6 // --- | уменьшение максимального давления
#define okBtn 7 //режим установки давлений | увеличение максимального давления
Button keyboard;

//Переменные состояния датчиков
int upSensorState;
int midSensorState;
int downSensorState;
int pressSensorState;
//Переменные состояния насосов
int minPress;
int maxPress;

//Переменные для моргания
int ledState = LOW;
unsigned long previousMillis = 0;
const long interval = 500;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  //Выставляем режимы пинов
  pinMode(upSensor, INPUT_PULLUP);
  pinMode(midSensor, INPUT_PULLUP);
  pinMode(downSensor, INPUT_PULLUP);
  pinMode(pressSensor, INPUT);

  pinMode(firstPump, OUTPUT);
  digitalWrite(firstPump, HIGH);
  pinMode(secondPump, OUTPUT);
  digitalWrite(secondPump, HIGH);

  //Настройка кнопок
  keyboard.NO(); // N.O. Normal Open
  keyboard.pullUp();
  keyboard.duration_bounce       (  50);
  keyboard.duration_click_Db     ( 250);
  keyboard.duration_inactivity_Up(5000);
  keyboard.duration_inactivity_Dn(1000);
  keyboard.duration_press        ( 500);
  keyboard.button(backBtn, upBtn, downBtn, okBtn); // arduino pins connected to button

  //Первичный опрос датчиков
  upSensorState = digitalRead(upSensor);
  midSensorState = digitalRead(midSensor);
  downSensorState = digitalRead(downSensor);
  pressSensorState = analogRead(pressSensor);

  //Установка начальных давлений
  minPress = 250;
  maxPress = 700;
  
  //Настройка экрана
  LD.init();  //initialze OLED display
  LD.clearDisplay();
  drawInterface();
  drawPressBar(analogRead(pressSensor));
}

void loop() {  

  //Контроль скважинного насоса c опросом датчиков
  switch (checkSensors()){
    case 18: //Верхний сухой, давление ниже минимума. Наполняем гидробак.
      secondPumpOn();
      break;
    case 2: //Верхний сухой, ничего не делаем
    case 34: //Верхний сухой, давление в норме. Ничего не делаем.
      break;
    case 66: //Верхний сухой, давление выше максимума. Останавливаем второй насос.
      secondPumpOff();
      break;
    case 22: //Верхний и средний сухие, давление ниже минимума наполняем бочку и гидробак
      firstPumpOn();
      secondPumpOn();
      break;
    case 6: //Верхний и средний сухие, наполняем бочку
    case 38: //Верхний и средний сухие, давление в норме наполняем бочку
      firstPumpOn();
      break;
    case 70: //Верхний и средний сухие, давление выше максимума. Наполняем бочку, останавливаем второй насос.
      firstPumpOn();
      secondPumpOff();
      break;
    case 16: //Датчики с водой, давление ниже минимума. Наполняем гидробак
      firstPumpOff();
      secondPumpOn();
      break;
    case 0: //На всех сенсорах вода.
    case 32: //Датчики с водой, давление в норме. Останавливаем первый насос.
      firstPumpOff();
      break;
    case 64: //Датчики с водой, давление выше максимума. Останавливаем насосы.
      firstPumpOff();
      secondPumpOff();
      break;
    case 14: //Все дачики сухие. Наполняем бочку, отключить второй насос
    case 30: //Все дачики сухие, давление ниже минимума. Наполняем бочку, отключить второй насос
    case 46: //Все дачики сухие, давление в норме. Наполняем бочку, отключить второй насос
    case 78: //Все дачики сухие, давление выше максимума. Наполняем бочку, отключить второй насос
      tankEmpty();
      firstPumpOn();
      secondPumpOff();
      break;
    //Без давления, ошибки датчиков
    case 4: //Средний сухой, остальные вода. Ошибка датчика.
    case 8: //Нижний сухой, остальные вода. Ошибка датчика.
    case 10: //Средний вода, остальные сухо. Ошибка датчика
    case 12: //Нижний сухой, средний сухой, верхний вода. Ошибка датчика.
    //Давление ниже минимума, ошибки датчиков
    case 20: //Средний сухой, остальные вода. Ошибка датчика.
    case 24: //Нижний сухой, остальные вода. Ошибка датчика.
    case 26: //Средний вода, остальные сухо. Ошибка датчика
    case 28: //Нижний сухой, средний сухой, верхний вода. Ошибка датчика.
    //Давление в норме, ошибки датчиков
    case 36: //Средний сухой, остальные вода. Ошибка датчика.
    case 40: //Нижний сухой, остальные вода. Ошибка датчика.
    case 42: //Средний вода, остальные сухо. Ошибка датчика
    case 44: //Нижний сухой, средний сухой, верхний вода. Ошибка датчика.
    //Давление выше максимума, ошибки датчиков
    case 68: //Средний сухой, остальные вода. Ошибка датчика.
    case 72: //Нижний сухой, остальные вода. Ошибка датчика.
    case 74: //Средний вода, остальные сухо. Ошибка датчика
    case 76: //Нижний сухой, средний сухой, верхний вода. Ошибка датчика.
      sensorError();
      firstPumpOff();
      secondPumpOff();
      break;
    default:
      //По умолчанию ничего не делать.
      break;
  }

  //Работа с клавиатурой
  keyboard.read();
  if (keyboard.event_press_short  (0) == 1) {
    firstPumpOn(); //Вкл первый насос
  }
  if (keyboard.event_press_short  (1) == 1) {
    secondPumpOn(); //Вкл первый насос
  }
  if (keyboard.event_press_short (3) == 1){
    setupMode(); //Режим задания давления
  }

  //Работа с экраном
  drawInterface();
  drawPressBar(analogRead(pressSensor)); //Полоса давления в гидроаккумуляторе
}

void firstPumpOn(){
  digitalWrite(firstPump, LOW);
}

void firstPumpOff(){
  digitalWrite(firstPump, HIGH);
}

void secondPumpOn(){
  digitalWrite(secondPump, LOW);
}

void secondPumpOff(){
  digitalWrite(secondPump, HIGH);
}


char* pumpState(int pumpNum){
  char* state;
  state  = "Неиз";
  if (digitalRead(pumpNum) == 0){
    state = "Вкл.";
  } else if (digitalRead(pumpNum) == 1){
    state = "Выкл";
  } else {
    state = "Ошиб";
  }
  return state;
}

int checkSensors(){
  int exitCode;
  int pressure; //Текущее давление
  pressure = analogRead(pressSensor); //Читаем давление
  exitCode = 0;
  if(digitalRead(upSensor) == 1){
    upSensorState = 2;
  } else {
    upSensorState = 0;
  }

  if(digitalRead(midSensor) == 1){
    midSensorState = 4;
  } else {
    midSensorState = 0;
  }

  if(digitalRead(downSensor) == 1){
    downSensorState = 8;
  } else {
    downSensorState = 0;
  }

  if(pressure < minPress){ //Здесь мы подменим значение давление на код ошибки
    pressSensorState = 16;
  } else if (pressure > minPress & pressure < maxPress){
    pressSensorState = 32;
  } else if(pressure > maxPress){
    pressSensorState = 64;
  } else {
    pressSensorState = 128;
  }
  
  exitCode = upSensorState + midSensorState + downSensorState + pressSensorState;
  pressSensorState = pressure; //Возвращаем значение давления на место
  return exitCode;
}

char* sensState(int sensNum){
  char* state;
  state  = "Неиз";
  int stat = digitalRead(sensNum);
  if (stat == 0){
    state = "Вода";
  } else if (stat == 1){
    state = "Сухо";
  } else {
    state = "Ошиб";
  }
  return state;
}

void drawInterface(){
  LD.printString_6x8("ВД = ", 0, 2);
  LD.printString_6x8(sensState(upSensor)); //Верхний датчик
  LD.printString_6x8("СД = ", 0, 3);
  LD.printString_6x8(sensState(midSensor)); //Средний датчик
  LD.printString_6x8("НД = ", 0, 4);
  LD.printString_6x8(sensState(downSensor)); //Нижний датчик
  LD.printString_6x8("     ");
  LD.printNumber ((long)checkSensors(), 60, 4); //Код ошибки
  LD.printString_6x8("Насос 1 = ", 0, 5);
  LD.printString_6x8(pumpState(firstPump)); //Скважинный насос
  LD.printString_6x8("Давл. = ", 0, 6);
  LD.printString_6x8("    ", 48, 6);
  LD.printNumber((long)analogRead(pressSensor), 48, 6); //Давление
  LD.printString_6x8("Насос 2 = ", 0, 7);
  LD.printString_6x8(pumpState(secondPump)); //Насос в гидроаккумулятор
  //Рисуем заданные границы давления в гидроаккумуляторе
  LD.VertBar(93, 48, 0, 64);
  LD.printNumber((long)minPress, 95, 6);
  LD.printNumber((long)maxPress, 95, 3);
  LD.VertBar(118, 48, 0, 64);
}

void drawPressBar(int val){
  //val = map(val, 0, 1024, 0, 64);
  for (int i = 0; i < 8; i++){
    LD.VertBar(120+i, val, 0, 1024);
  }
}

void tankEmpty(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
      LD.printString_12x16("БАК ПУСТ!",0, 0);
      LD.drawBitmap(error_v, 60, 2);
    } else {
      ledState = LOW;
      LD.drawBitmap(lamp_on, 60, 2);
      LD.printString_12x16("         ", 0, 0);
    }
  }
}

void sensorError(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
      LD.drawBitmap(error_v, 60, 2);
      LD.printString_12x16("ОШ. ДАТЧ!", 0, 0);
    } else {
      ledState = LOW;
      LD.drawBitmap(lamp_on, 60, 2);
      LD.printString_12x16("         ", 0, 0);
    }
  }
}

void setupMode(){
  Serial.println("Setup mode");
  unsigned long startMillis;
  startMillis = millis();
  LD.printString_12x16("Настройка",0, 0);
  LD.setFont(Font_6x8);
  LD.SetInverseText();
  while (millis() - startMillis <= 5000){
    keyboard.read();
    if (keyboard.event_press_short  (0) == 1) {
      startMillis = millis();
      minPress--;
      Serial.println("key0");
    }
    if (keyboard.event_press_short  (1) == 1) {
      startMillis = millis();
      minPress++;
      Serial.println("key1");
    }
    if (keyboard.event_press_short  (2) == 1) {
      startMillis = millis();
      maxPress--;
      Serial.println("key2");
    }
    if (keyboard.event_press_short  (3) == 1) {
      startMillis = millis();
      maxPress++;
      Serial.println("key3");
    }
    LD.printNumber((long)minPress, 95, 6);
    LD.printNumber((long)maxPress, 95, 3);
    //Serial.println(keyboard.state_inactivity_Up(0));
  }
  LD.SetNormalText();
  LD.printString_12x16("         ",0, 0);
  LD.setFont(Font_6x8);
  Serial.println("Exit setup mode");
}

