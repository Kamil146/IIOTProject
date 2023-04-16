#include <Debounce.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include "Adafruit_CCS811.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Time.h>
#include <Fonts/FreeMono9pt7b.h> //alternatywna czcionka
#include <Preferences.h>

#define BUTTON1_PIN 32 //ekran poprzedni
#define BUTTON2_PIN 33 //ekran nastepny
#define BUTTON3_PIN 25 // lock/alarm
#define BUTTON4_PIN 26 // podekran poprzedni/-
#define BUTTON5_PIN 27 // podekran nastepny/+
#define BUTTON6_PIN 14 // enter/edytuj

#define LED2_PIN 12 //zolta 
#define LED3_PIN 13 //czerw
#define OLED_SDA 21
#define OLED_SCL 22
#define TEMP_SIZE 100 //wielkość tablicy przechowujacej pomiar temperatury
#define ECO2_SIZE 100 // wielkość tablicy przechowujacej pomiar eco2
#define FLOAT_TO_INT(x) ((x)>=0?(int)((x)+0.5):(int)((x)-0.5))


const int oneWireBus = 5;
//deklaracja dla każdego z przycisków oznaczająca ls - last state, oraz cs - current state, pozwala na wykrycie zbocza narastającego
bool button1ls = LOW;  
int button1cs;
//czas debouncingu
unsigned long dTime1 = 30;

bool button2ls = LOW;  
int button2cs;
unsigned long dTime2 = 30;

bool button3ls = LOW;  
int button3cs;
unsigned long dTime3 = 30;

bool button4ls = LOW;  
int button4cs;
unsigned long dTime4 = 30;

bool button5ls = LOW;  
int button5cs;
unsigned long dTime5 = 30;

bool button6ls = LOW;  
int button6cs;
unsigned long dTime6 = 30;
//zmiena oznaczajaca aktualny ekran
int screen = 1;
//czas po jakim nastąpi automatyczna zmiana ekranu
int screenTime = 5000;
unsigned long screenLastTime = 0;
//mierzenie czasu wciśniecia przycisku P3
unsigned long button3last = 0;
int button3time = 5000;

unsigned long disLast = 0;
//ustalenia czasu odswiezania ekranu
int disTime = 1000;



bool alarmOff = 1; 
bool lock = 1;
bool flag1=0;
bool flag2=0;
//aktualny podekran
int disMode=1;



//ustawienie co jaki czas odczytywane zostana wartosci z czujnika CCS811
unsigned long ccsLast = 0;
int ccsTime = 3000;
//ustawienie co jaki czas odczytywane zostana wartosci z czujnika temperatury
unsigned long tempLast = 0;
int tempTime = 9000;

float t=0.0;
int eco2=400;
int tvoc=0;
//prog, po ktorego przekroczeniu wygenerowany zostanie alarm
int threshold = 2000;

int enter = 0;
//zmienne przechowujace czas i date
int h,m,d,mo,year;
//pomocnicze zmienne umozliwiajace wprowadzenie czasu i daty
int h1=0,h2=0,m1=0,m2=0;
int d1=0,d2=0,mo1=0,mo2=0;
int yy1=2,yy2=0,yy3=0,yy4=0;
//tablice przechowujace informacje dotyczace alarmow
String alarmTimeStart[5];
String alarmTimeStop[5];
String alarmDate[5];
int alarmNumber;

unsigned long timeFlash = 0;

float sumTemp=0;
int numTemp=0;
int sumEco2=0;
int numEco2=0;
float maxTemp;
int maxEco2;
float minTemp;
int minEco2;
float tempArray [TEMP_SIZE];
int eco2Array [ECO2_SIZE];



Debounce Button1(BUTTON1_PIN,dTime1,true);
Debounce Button2(BUTTON2_PIN,dTime2,true);
Debounce Button3(BUTTON3_PIN,dTime3,true);
Debounce Button4(BUTTON4_PIN,dTime4,true);
Debounce Button5(BUTTON5_PIN,dTime5,true);
Debounce Button6(BUTTON6_PIN,dTime6,true);
Adafruit_CCS811 ccs;
Adafruit_SH1106 display(OLED_SDA,OLED_SCL);  
OneWire oneWire(oneWireBus);
DallasTemperature sensor  (&oneWire);
ESP32Time rtc;
Preferences preferences;

void setup() {
  Serial.begin(115200);
  preferences.begin("my-app", false);
  flashSetup(); //wczytanie z pamięci flash do tablicy alarmów oraz daty i czasu
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  sensor.begin();
  rtc.setTime(0, m, h, d, mo, year); //ustawienie czasu 
  if(!ccs.begin())
  {
    Serial.println("Bład sensora CCS811, brak polaczenia");
    
    }
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(BUTTON5_PIN, INPUT_PULLUP);
  pinMode(BUTTON6_PIN, INPUT_PULLUP);
  
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);  
}

void loop() {         
  if(millis()-tempLast >= tempTime) //odczyt i zapis do tablicy temperatury co zadany czas
  {
    sensor.requestTemperatures();    
    t = sensor.getTempCByIndex(0);       
    saveTemp(t);    
    sumTemp=t+sumTemp;
    numTemp++;
    if(!flag1){maxTemp=t; minTemp=t; flag1=1;}
    if(t>maxTemp) maxTemp=t;
    if(t<minTemp) minTemp=t;     
    tempLast = millis();
    }
  if(millis()-ccsLast >= ccsTime) //odczyt i zapis eco2 oraz odczyt tvoc co zadany czas
  {
  if(ccs.available()){
    if(!ccs.readData()){  
    eco2 = ccs.geteCO2();
    tvoc = ccs.getTVOC();    
    ccsLast = millis();      
    saveEco2(eco2);    
    sumEco2=eco2+sumEco2;
    numEco2++;    
    if(!flag2){maxEco2=eco2; minEco2=eco2; flag2=1;}
    if(eco2>maxEco2) maxEco2=eco2;
    if(eco2<minEco2) minEco2=eco2;
     }
      }
  else {eco2=400; tvoc=0;}
  }  
  //odczyt stanu przyciskow
  button1cs = Button1.read();
  button2cs = Button2.read();
  button3cs = Button3.read();
  button4cs = Button4.read();
  button5cs = Button5.read();
  button6cs = Button6.read();   
  if(button3cs==LOW)button3last=millis(); //wł/wył możliwość alarmu gdy przycisk wciśnięty dłużej niż zadany czas (5s)
  if(millis()- button3last >= button3time)
    { button3last=millis();
      if(!alarmOff) alarmOff=1;
       else alarmOff=0;
       }       
  if(button3cs==LOW)button3last=millis(); // wł/wył automatyczną zmianę głównych ekranów przy naciśnięciu
  if(button3ls == LOW && button3cs == HIGH)
    { 
      if(!lock) lock=1;
       else {screenLastTime=millis(); lock=0;} }     

  switch(screen) //główny automat do przełączania ekranów
  {
    case 1:  screen1(); if(button5ls == LOW && button5cs == HIGH){if(disMode<5)disMode++; else disMode=1;}
             else if(button4ls == LOW && button4cs == HIGH){if(disMode>1)disMode--; else disMode=5;} 
             else if((button6ls == LOW && button6cs == HIGH) && disMode==4){screen=11; h1=(rtc.getHour(true)/10)%10; h2=rtc.getHour(true)%10; m1=(rtc.getMinute()/10)%10; m2=rtc.getMinute()%10; enter=0;}
             else if((button6ls == LOW && button6cs == HIGH) && disMode==5){screen=12; d1=(rtc.getDay()/10)%10; d2=rtc.getDay()%10; mo1=((rtc.getMonth()+1)/10)%10; mo2=(rtc.getMonth()+1)%10; enter=0;}
             else if((button2ls == LOW && button2cs == HIGH) || (((millis()-screenLastTime)>=screenTime) && !lock)) {screen=2; disMode=1;  screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH) {screen=5; disMode=1;  screenLastTime=millis();}
             else if(eco2>=threshold&&!alarmOff) {  flashUpdateStart(); disMode=1; screen=6;}  break;
    case 11: clockset();
             if(enter==5){screen=1; disMode=4;screenLastTime=millis(); enter=0;}
             else if(button2ls == LOW && button2cs == HIGH){screen=2; disMode=1;screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH){screen=5; disMode=1;screenLastTime=millis();} 
             else if(eco2>=threshold&&!alarmOff) {  flashUpdateStart(); disMode=1; screen=6;} break;
    case 12: dateset();
             if(enter==11){screen=1; disMode=5;screenLastTime=millis(); enter=0;}
             else if(button2ls == LOW && button2cs == HIGH){screen=2; disMode=1;screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH){screen=5; disMode=1;screenLastTime=millis();} 
             else if(eco2>=threshold&&!alarmOff) {  flashUpdateStart(); disMode=1; screen=6;} break;                
    case 2:  screen2(); if((button2ls == LOW && button2cs == HIGH) || (((millis()-screenLastTime)>=screenTime) && !lock)) {screen=3;  screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH) {screen=1;  screenLastTime=millis();}
             else if(eco2>=threshold&&!alarmOff)  {  flashUpdateStart();  screen=6;}  break;
    case 3:  screen3();  if((button2ls == LOW && button2cs == HIGH) || (((millis()-screenLastTime)>=screenTime) && !lock)) {screen=4;  screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH) {screen=2;  screenLastTime=millis();}
             else if(eco2>=threshold&&!alarmOff)  {  flashUpdateStart();  screen=6;}  break;
   case 4:  screen4(); if(button5ls == LOW && button5cs == HIGH){if(disMode<4)disMode++; else disMode=1;} if(button4ls == LOW && button4cs == HIGH){if(disMode>1)disMode--; else disMode=4;} 
            if((button2ls == LOW && button2cs == HIGH) || (((millis()-screenLastTime)>=screenTime) && !lock)) {screen=5; disMode=1; screenLastTime=millis();} 
            else if(button1ls == LOW && button1cs == HIGH) {screen=3; disMode=1;  screenLastTime=millis();}
            else if(eco2>=threshold&&!alarmOff)  {  flashUpdateStart(); disMode=1; screen=6;} break;
    case 5:  screen5(); if(button5ls == LOW && button5cs == HIGH){if(disMode<5)disMode++; else disMode=1;} if(button4ls == LOW && button4cs == HIGH){if(disMode>1)disMode--; else disMode=5;} 
             if((button2ls == LOW && button2cs == HIGH) || (((millis()-screenLastTime)>=screenTime) && !lock)) {screen=1; disMode=1; screenLastTime=millis();}
             else if(button1ls == LOW && button1cs == HIGH) {screen=4; disMode=1;  screenLastTime=millis();}
             else if(eco2>=threshold&&!alarmOff)  {  flashUpdateStart(); disMode=1; screen=6;} break;
    case 6:  screen6(); if((eco2<threshold) || alarmOff)
    {screen=1; screenLastTime=millis();    flashUpdateStop(); if(alarmNumber<4)alarmNumber++; else alarmNumber=0; preferences.putInt("number",alarmNumber);}  break;
  }       
   
   //swiecenie diody gdy eco2 osiąga odpowiednią wartość
   ((eco2>=1000) && (eco2<2000))? digitalWrite(LED2_PIN,HIGH):digitalWrite(LED2_PIN,LOW);
   (eco2>=threshold)? digitalWrite(LED3_PIN,HIGH):digitalWrite(LED3_PIN,LOW);    
       
   
      
   //odświeżanie ekranu co zadany czas lub gdy zostal wcisniety jakis przycisk     
   
   if((millis()-disLast >= disTime) ||  button1ls != button1cs || button2ls != button2cs || button3ls != button3cs || button4ls != button4cs || button5ls != button5cs)
     {      
      display.display();
      disLast = millis();
      }
    
   //zapis aktualnej h i m do pamieci flash co minute 
   if(millis()-timeFlash >= 60000 ){
     timeFlash=millis();
     preferences.putInt("hour",rtc.getHour(true));
     preferences.putInt("minute",rtc.getMinute());         
   }   
   button1ls = button1cs;
   button2ls = button2cs;
   button3ls = button3cs;
   button4ls = button4cs;
   button5ls = button5cs;
   button6ls = button6cs;    
}

void screen1() //elementy wyświetlane na 1 ekranie
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);  
  display.setCursor(1,1);
  display.printf("%d/5",disMode);   
  faceIcon(32,22);
  display.setCursor(45,12);
  //display.print("12:10");
  if(rtc.getHour(true)<10)
  display.print(" ");
  display.print(rtc.getHour(true)); 
  display.print(":"); 
  if(rtc.getMinute()<10)
  display.print(0); 
  display.print(rtc.getMinute()); 
  display.print(":");
  if(rtc.getSecond()<10)
  display.print(0);  
  display.print(rtc.getSecond());  
  lockIcon(101,12);
  alarmIcon(116,19);   
  //pozostałe wartości wyświetlane w zależności od wartosci  disMode, ktora okresla podekrany
  if(disMode==1)
  {    
  display.setCursor(10, 38);
  display.print("eCO2=");
  display.setTextSize(2);
  display.setCursor(40, 35);
  display.print(eco2);
  display.setTextSize(2);
  if(eco2<1000)
  display.setCursor(80, 42);
  else
  display.setCursor(90, 42);
  display.setTextSize(1);
  display.print("ppm");
  display.setCursor( 5, 57);
  display.setTextSize(1);
  
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("T=");
  display.print(t);
  display.cp437(true);  
  display.write(167);
  display.print("C");
    
  }  
  if(disMode==2)
  {  
  
  display.setCursor(20, 38);  
  display.print("TVOC=");
  display.setTextSize(2);
  display.setCursor(50, 35);
  display.print(tvoc);
  display.setTextSize(1);
  if(tvoc<100)
  display.setCursor(80, 42);
  else
  display.setCursor(90, 42);
  display.print("ppb");
  display.setCursor( 5, 57);
  display.setTextSize(1);
  
  display.print("eCO2=");
  display.print(eco2);
  display.setCursor( 65, 57);
  display.print("T=");
  display.print(t);
  display.cp437(true);  
  display.write(167);
  display.print("C");
  //display.setFont(NULL);   
  }  
  if(disMode==3)
  {  
  
  display.setCursor(15, 38);  
  display.print("T=");
  display.setCursor(28, 35);
  display.setTextSize(2);
  display.print(t);  
  display.cp437(true);  
  display.write(167);
  display.print("C");
  display.setTextSize(1);
  display.setCursor( 5, 57);   
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("eCO2=");
  display.print(eco2);
   
  }  
  if(disMode==4)
  {
   
  editIcon(4,18);
  display.setCursor(20, 30);
  display.setTextSize(3);
  if(rtc.getHour(true)<10)
  display.print(" ");
  display.print(rtc.getHour(true));
  display.print(":");
  if(rtc.getMinute()<10)
  display.print(0);  
  display.print(rtc.getMinute());
  
  display.setTextSize(1);
  display.setCursor( 5, 57);   
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("eCO2=");
  display.print(eco2);
  }
  if(disMode==5)
  {
  editIcon(4,18);  
  display.setCursor(2, 30);
  display.setTextSize(2);
  if(rtc.getDay()<10)
  display.print(0);
  display.print(rtc.getDay());
  display.print(".");
  if(rtc.getMonth()<9)
  display.print(0);  
  display.print(rtc.getMonth()+1);
  display.print(".");
  display.print(rtc.getYear());
  
  display.setTextSize(1);
  display.setCursor( 5, 57);   
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("eCO2=");
  display.print(eco2);        
    }       
}

void screen2()
{
  int num = 6;
  int val = min(numEco2/20,5);
  display.clearDisplay();
  lockIcon(101,5);
  alarmIcon(116,13); 
  drawChartEco2(24,52,124,13,400,2000); //wywolanie funkcji rysujacej wykres
  display.setTextColor(WHITE);
  display.setCursor(2, 2);
  display.setTextSize(1);
  display.println("WYKRES ECO2");
  display.drawLine(24, 52,22,52,WHITE); 
  //automatyczne generowanie podpisów na osi x
  for(int i=0; i<num; i++) {
    int tickPos = 24 + min(numEco2,100)-i*20;
    if(val - i >= 0) {
      display.drawLine(tickPos, 52, tickPos, 54, WHITE);
      display.setCursor(tickPos-5, 55);
      display.print(-i);
    } 
    } 
}

void screen3()
{
    
  display.clearDisplay();
  lockIcon(101,5);
  alarmIcon(116,13); 
  int num = 6;
  int val = min(numTemp/20,5);
  drawChartTemp(24,52,124,13);
  display.setTextColor(WHITE);
  display.setCursor(2, 2);
  display.setTextSize(1);
  display.println("WYKRES TEMP");
   
  display.drawLine(24, 52,22,52,WHITE);

  //automatyczne generowanie podpisow na osi x
for(int i=0; i<num; i++) {
    int tickPos = 24 + min(numTemp,100)-i*20;
    if(val - i >= 0) {
      display.drawLine(tickPos, 52, tickPos, 54, WHITE);
      display.setCursor(tickPos-5, 55);
      if(i>3) display.setCursor(tickPos-8, 55);
      display.print(-i*3);
    } 
    }   
}  

void screen4()
 {
  float avgTemp=0;
  if(numTemp>0) avgTemp=sumTemp/numTemp;
  int avgEco2=400;
  if(numEco2>0) avgEco2=int(sumEco2/numEco2+0.5);     
  display.clearDisplay();
  lockIcon(101,5);
  alarmIcon(116,13); 
  display.setCursor(110,55);
  display.printf("%d/4",disMode);  
  if(disMode==1){ 
        
  display.setCursor(5, 5);
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.println("eCO2(ppm)");   
  display.print("AVG=");
  display.print(avgEco2);
  display.setCursor(1, 46);
  display.print("MIN=");
  display.print(minEco2);
  display.setCursor(1, 63);
  display.print("MAX=");
  display.print(maxEco2);
  display.setFont(NULL);    
  }
  if(disMode==2)
  {
  int maxE=0,minE=5000,sumE=0;
  int avgE=400;
  for(int i = 0; i < min(numEco2,ECO2_SIZE); i++)
      {
         if(eco2Array[i]>maxE) maxE=eco2Array[i];
         if(eco2Array[i]<minE) minE=eco2Array[i];
         sumE+=eco2Array[i];
      }
  
  if(numEco2>0) avgE=int(sumE/min(numEco2,ECO2_SIZE)+0.5);   
  display.setCursor(5, 5);
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.println("eCO2-5m");   
  display.print("AVG=");
  display.print(avgE);
  display.setCursor(1, 46);
  display.print("MIN=");
  display.print(minE);
  display.setCursor(1, 63);
  display.print("MAX=");
  display.print(maxE);
  display.setFont(NULL);    
  }
  if(disMode==3){
  display.setCursor(5, 5);
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.print("TEMP(");
  display.setFont(NULL); 
  display.setTextSize(1);
  display.cp437(true);  
  display.write(167);
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.println("C)");
  display.print("AVG=");
  display.print(avgTemp,1);
  display.setCursor(1, 46);
  display.print("MIN=");
  display.print(minTemp,1); 
  display.setCursor(1, 63);
  display.print("MAX=");
  display.print(maxTemp,1);
  display.setFont(NULL);   
  }
  if(disMode==4)
  {
    float maxT=0,minT=50,sumT=0,avgT;
  for(int i = 0; i < min(numTemp,TEMP_SIZE); i++)
      {
         if(tempArray[i]>maxT) maxT=tempArray[i];
         if(tempArray[i]<minT) minT=tempArray[i];
         sumT+=tempArray[i];
      }
  avgT=sumT/min(numTemp,TEMP_SIZE);       
  display.setCursor(5, 5);
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.println("TEMP-15m");
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.print("AVG=");
  display.print(avgT,1);
  display.setCursor(1, 46);
  display.print("MIN=");
  display.print(minT,1); 
  display.setCursor(1, 63);
  display.print("MAX=");
  display.print(maxT,1);
  display.setFont(NULL);
  }   
}

void screen5()
{
  String aTimeStart;
  String aTimeStop;
  String aDate;
  display.clearDisplay();
  lockIcon(101,5);
  alarmIcon(116,13);
  switch(disMode)
  {
    case 1: aTimeStart = alarmTimeStart[0];  aTimeStop = alarmTimeStop[0]; aDate = alarmDate[0];
    break;
    case 2: aTimeStart = alarmTimeStart[1]; aTimeStop = alarmTimeStop[1]; aDate = alarmDate[1];
    break;
    case 3: aTimeStart = alarmTimeStart[2]; aTimeStop = alarmTimeStop[2]; aDate = alarmDate[2];
    break;
    case 4: aTimeStart = alarmTimeStart[3]; aTimeStop = alarmTimeStop[3]; aDate = alarmDate[3];
    break;
    case 5: aTimeStart = alarmTimeStart[4]; aTimeStop = alarmTimeStop[4]; aDate = alarmDate[4];
    break;
    
    
    }
   
  display.setCursor(15, 5);
  display.setTextSize(1);  
  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.println("ALARMY"); 
  display.print(aTimeStart);
  display.print("-");
  display.println(aTimeStop);
  display.setCursor(10,46);
  display.print(aDate);
  display.setCursor(48,63);
  display.printf("%d/5",disMode);
  
  
  display.setFont(NULL);     
}

void screen6()
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(35, 15);
  display.setTextSize(2);
  display.println("UWAGA!");      
  display.setCursor(3, 42);
  display.setTextSize(2);
  display.print("eCO2=");
  display.print(eco2);
  display.setTextSize(1);
  if(eco2<1000)
  display.setCursor(100, 50);
  else
  display.setCursor(109, 50);
  display.print("ppm");
  
  for(int i=0; i<128; i=i+2) display.drawPixel(i,0,WHITE);
  for(int i=0; i<128; i=i+2) display.drawPixel(i,63,WHITE);
  for(int i=0; i<64; i=i+2) display.drawPixel(0,i,WHITE);
  for(int i=0; i<64; i=i+2) display.drawPixel(127,i,WHITE);
    
}

//rysowanie wykresu dla eco2, argumenty, które przyjmue funkcja:
// startx - punkt początkowy wykresu, x |  starty - punkt początkowy wykresu, y
// endx - punk końcowy na osi x |  endy - punk końcowy na osi y
// valMin - minimalna dopuszczalna wartość | valMax - maksymalna dopuszczalna wartość
// valLow - granica dolna dla wartości na osi Y | valLow - granica gorna dla wartości na osi Y
void drawChartEco2(int startx, int starty, int endx, int endy,int valMin, int valMax)
{
  int valHigh=1000,valLow=400;
  //automatyczne dostosowanie zakresu gdy wartosci sa wyzsze aby lepiej uwidocznic roznice
  if(eco2Array[0]>700){
  valLow = max(eco2Array[0]-300,valMin);
  //zaokrąlenie do setek aby wartosci na osi y nie zmienialy sie tak czesto 
  valLow =  round(valLow/100.0)*100;
  valHigh = min(eco2Array[0]+300, valMax);
  valHigh = round(valHigh/100.0)*100; }  
  
  int n = min(numEco2,ECO2_SIZE);  
  display.drawLine(startx, endy, startx, starty, WHITE); // os Y
  display.drawLine(startx,starty, endx ,starty, WHITE); // os X
  display.setCursor(0, 13);
  display.print(valHigh); 
  display.setCursor(4, 48);
  display.print(valLow);
  
   
    for (int i = 0; i < n; i++) {
    int x1 = startx+i;
    int y1 = map(constrain(eco2Array[n-1-i],valLow,valHigh), valLow, valHigh, starty, endy);
    int x2 = startx+i+1;
    int y2 = map(constrain(eco2Array[n-i-2],valLow,valHigh), valLow, valHigh, starty, endy);
    
    display.drawLine(x1, y1, x2, y2, WHITE);
    display.drawLine(x2, y2, x2, starty, WHITE);
    }    
}

//rysowanie wykresu dla temp, argumenty, które przyjmue funkcja:
// startx - punkt początkowy wykresu, x |  starty - punkt początkowy wykresu, y
// endx - punk końcowy na osi x |  endy - punk końcowy na osi y
// valLow - granica dolna dla wartości na osi Y | valLow - granica gorna dla wartości na osi Y
void drawChartTemp(int startx, int starty, int endx, int endy)
{
  int valLow = (FLOAT_TO_INT(tempArray[0])-2)*100;
  int valHigh = (FLOAT_TO_INT(tempArray[0])+2)*100;
  int n = min(numTemp,TEMP_SIZE);
  display.setCursor(4, 13);
  display.print(valHigh/100);
  display.setCursor(4, 48);
  display.print(valLow/100);
  
  display.drawLine(startx, endy, startx, starty, WHITE); // os Y
  display.drawLine(startx,starty, endx ,starty, WHITE); // os X
  
  
    for (int i = 0; i < n ; i++) {
    int x1 = startx+i;
    int y1 = map(max(int(tempArray[n-1-i]*100),valLow), valLow, valHigh, starty, endy);
    int x2 = startx+i+1;
    int y2 = map(max(int(tempArray[n-i-2]*100), valLow), valLow, valHigh, starty, endy);
    
    display.drawLine(x1, y1, x2, y2, WHITE);
    display.drawLine(x2, y2, x2, starty, WHITE);
    }
    
}

void saveTemp(float tempS) //zapis temperatury i przesuniecie elementow aby najnowszy znajdował się w [0]
 {
  for (int i = TEMP_SIZE-1 ; i > 0; i--) {
    tempArray[i] = tempArray[i-1];
  }
  tempArray[0]=tempS;
 }
void saveEco2(int eco2S) //zapis eco2 i przesuniecie elementow
{
  for (int i = ECO2_SIZE-1; i > 0; i--) {
    eco2Array[i] = eco2Array[i-1];
  }
  eco2Array[0]=eco2S;
  
  }  
 void flashSetup() //wczytanie wartosci z pamieci flash do tablic (tylko na poczatku programu)
  {
    alarmNumber = preferences.getInt("number",0);
    
    alarmTimeStart[0]=preferences.getString("start1","00:00");
    alarmTimeStart[1]=preferences.getString("start2","00:00");
    alarmTimeStart[2]=preferences.getString("start3","00:00");
    alarmTimeStart[3]=preferences.getString("start4","00:00");
    alarmTimeStart[4]=preferences.getString("start5","00:00");
    
    alarmTimeStop[0] = preferences.getString("stop1","12:00");
    alarmTimeStop[1] = preferences.getString("stop2","12:00");
    alarmTimeStop[2] = preferences.getString("stop3","12:00");
    alarmTimeStop[3] = preferences.getString("stop4","12:00");
    alarmTimeStop[4] = preferences.getString("stop5","12:00");

    alarmDate[0] = preferences.getString("date1","01.01.2023");
    alarmDate[1] = preferences.getString("date2","01.01.2023");
    alarmDate[2] = preferences.getString("date3","01.01.2023");
    alarmDate[3] = preferences.getString("date4","01.01.2023");
    alarmDate[4] = preferences.getString("date5","01.01.2023");

    h = preferences.getInt("hour",12);
    m = preferences.getInt("minute",45);

    d = preferences.getInt("day",3);
    mo = preferences.getInt("month",6);
    year = preferences.getInt("year",2023);    
  }
  
 void flashUpdateStart() //fortamowanie i dodanie czasu poczatku wystapienia alarmu do odpowiedniego miejsca w pamieci flash
    {
     alarmTimeStart[alarmNumber]=String(rtc.getHour(true))+":";
    if(rtc.getMinute()>=10) alarmTimeStart[alarmNumber]+=String(rtc.getMinute());
    else alarmTimeStart[alarmNumber]+="0"+String(rtc.getMinute());
     
     switch(alarmNumber){
     case 0: preferences.putString("start1",alarmTimeStart[0]); break;
     case 1: preferences.putString("start2",alarmTimeStart[1]); break;
     case 2: preferences.putString("start3",alarmTimeStart[2]); break;
     case 3: preferences.putString("start4",alarmTimeStart[3]); break;
     case 4: preferences.putString("start5",alarmTimeStart[4]); break;
     }    
    }  

 void flashUpdateStop() //formatowanie i dodanie czasu i daty zakonczenia alarmu do odpowiedniego miejsca w pamieci flash
   {
    if(rtc.getDay()>=10) alarmDate[alarmNumber]=String(rtc.getDay())+".";
    else alarmDate[alarmNumber]="0"+String(rtc.getDay())+".";
    if(rtc.getMonth()>=9) alarmDate[alarmNumber]+=String(rtc.getMonth()+1)+".";
    else alarmDate[alarmNumber]+="0"+String(rtc.getMonth()+1)+".";
    alarmDate[alarmNumber]+=String(rtc.getYear());    
    alarmTimeStop[alarmNumber]=String(rtc.getHour(true))+":";
    if(rtc.getMinute()>=10) alarmTimeStop[alarmNumber]+=String(rtc.getMinute());
    else alarmTimeStop[alarmNumber]+="0"+String(rtc.getMinute());    
      switch(alarmNumber){
     case 0: preferences.putString("stop1",alarmTimeStop[0]); preferences.putString("date1",alarmDate[0]); break;     
     case 1: preferences.putString("stop2",alarmTimeStop[1]); preferences.putString("date2",alarmDate[1]); break;
     case 2: preferences.putString("stop3",alarmTimeStop[2]); preferences.putString("date3",alarmDate[2]); break;
     case 3: preferences.putString("stop4",alarmTimeStop[3]); preferences.putString("date4",alarmDate[3]); break;
     case 4: preferences.putString("stop5",alarmTimeStop[4]); preferences.putString("date5",alarmDate[4]); break;
     }      
    }    


void dateset() //specjalny ekran do ustawiania ręcznie daty
{
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  editIcon(4,18);
  display.setCursor(1,1);
  display.printf("%d/5",disMode);
  display.setCursor(4,10);  
  faceIcon(32,22);
  display.setCursor(50,12);  
  display.setTextSize(1);
  if(rtc.getDay()<10)
  display.print(0);
  display.print(rtc.getDay());
  display.print(".");
  if(rtc.getMonth()<9)
  display.print(0);  
  display.print(rtc.getMonth()+1);  
  lockIcon(101,12);
  alarmIcon(116,19); 

  //przycisk enter pozwala na ustawianie wartosci dla kolejnych cyfr
  switch(enter){
      //przy kazdej cyfrze oczekujemy na klawisz +/- badz zatwierdzenie cyfry enterem i przejscie dalej, dla kazdej cyfry sprawdzane sa warunki czy nie przekroczono dozwolonej wartosci
      case 0: display.drawLine(20,28,32,28,WHITE); display.drawLine(20,27,32,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(d1>0)d1=d1-1; else d1=3;}
      else if(button5cs==HIGH && button5ls==LOW) {if(d1<3)d1=d1+1; else d1=0;}
      else if (button6cs==HIGH && button6ls==LOW) enter=1; break;
      case 1:  display.drawLine(39,28,51,28,WHITE); display.drawLine(39,27,51,27,WHITE); if(d1==0 && d2==0) d2=1; if(d1==3 && d2>1) d2=0;
      if(button4cs==HIGH && button4ls==LOW)
       {if(d1==0){if(d2>1)d2=d2-1; else d2=9;}
       else if(d1>0 && d1<3){if(d2>0)d2=d2-1; else d2=9;}
       else if(d1==3){if(d2>0)d2=d2-1; else d2=1;}}
      if(button5cs==HIGH && button5ls==LOW)
       {if(d1==0){if(d2<9)d2=d2+1; else d2=1;}
       else if(d1>0 && d1<3){if(d2<9)d2=d2+1; else d2=0;}
       else if(d1==3){if(d2<1)d2=d2+1; else d2=0;}}
       else if (button6cs==HIGH && button6ls==LOW) enter=2;break;
      case 2:  display.drawLine(76,28,88,28,WHITE); display.drawLine(76,27,88,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(mo1>0)mo1=mo1-1; else mo1=1;}
      if(button5cs==HIGH && button5ls==LOW) {if(mo1<1)mo1=mo1+1; else mo1=0;}
       else if (button6cs==HIGH && button6ls==LOW) enter=3; break;
      case 3:  display.drawLine(94,28,106,28,WHITE); display.drawLine(94,27,106,27,WHITE); if((mo1==1) && (mo2>2)) mo2=0; if(mo1==0 && mo2==0) mo2=1;
      if(button4cs==HIGH && button4ls==LOW)
       {if(mo1==0){if(mo2>1)mo2=mo2-1; else mo2=9;}
        else if(mo1==1){if(mo2>0)mo2=mo2-1; else mo2=2;} }
      if(button5cs==HIGH && button5ls==LOW) 
       {if(mo1==0){if(mo2<9)mo2=mo2+1; else mo2=1;}
        else if(mo1==1){if(mo2<2)mo2=mo2+1; else mo2=0;}}
        else if (button6cs==HIGH && button6ls==LOW) enter=4; break; 
      case 4: display.setCursor(0, 50); display.setTextSize(2);  display.print("TAK");  display.setCursor(82, 50); display.print("NIE");
              display.setCursor(40,30); display.print("Rok?");
              display.drawLine(82,47,112,47,WHITE); display.drawLine(82,48,112,48,WHITE);
              if(button6cs==HIGH && button6ls==LOW) {year=rtc.getYear(); enter = 10;}
              else if( (button4cs==HIGH && button4ls==LOW ) || (button5cs==HIGH && button5ls==LOW)) enter=5; break;
              
      case 5: display.setCursor(0, 50); display.setTextSize(2);  display.print("TAK");  display.setCursor(82, 50); display.print("NIE");
              display.setCursor(40,30); display.print("Rok?");
              display.drawLine(0,47,30,47,WHITE); display.drawLine(0,48,30,48,WHITE);
              if(button6cs==HIGH && button6ls==LOW) enter = 6;
              else if(button4cs==HIGH && button4ls==LOW  || button5cs==HIGH && button5ls==LOW) enter=4;   break;      
      case 6: display.drawLine(20,28,32,28,WHITE); display.drawLine(20,27,32,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(yy1>0)yy1=yy1-1; else yy1=9;}
      else if(button5cs==HIGH && button5ls==LOW) {if(yy1<9)yy1=yy1+1; else yy1=0;}
      else if (button6cs==HIGH && button6ls==LOW) enter=7; break;
      case 7: display.drawLine(39,28,51,28,WHITE); display.drawLine(39,27,51,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(yy1>0)yy2=yy2-1; else yy2=9;}
      else if(button5cs==HIGH && button5ls==LOW) {if(yy2<9)yy2=yy2+1; else yy2=0;}
      else if (button6cs==HIGH && button6ls==LOW) enter=8; break;
      case 8: display.drawLine(57,28,69,28,WHITE); display.drawLine(57,27,69,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(yy3>0)yy3=yy3-1; else yy3=9;}
      else if(button5cs==HIGH && button5ls==LOW) {if(yy3<9)yy3=yy3+1; else yy3=0;}
      else if (button6cs==HIGH && button6ls==LOW) enter=9; break;
      case 9: display.drawLine(75,28,87,28,WHITE); display.drawLine(75,27,87,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(yy4>0)yy4=yy4-1; else yy4=9;}
      else if(button5cs==HIGH && button5ls==LOW) {if(yy4<9)yy4=yy4+1; else yy4=0;}
      else if (button6cs==HIGH && button6ls==LOW) enter=10; break;
      
      //gdy wprowadzono wszystkie cyfry, nowy czas jest zapisywany zarowno do aktualnego programu jak i pamieci flash aby po resecie pamietal nowa date    
      case 10: d=d1*10+d2; mo=mo1*10+mo2; year=yy1*1000+yy2*100+yy3*10+yy4;
      rtc.setTime(0, m, h, d, mo, year);  preferences.putInt("day",rtc.getDay()); preferences.putInt("month",rtc.getMonth()+1); preferences.putInt("year",rtc.getYear()); enter=11; break;
  }

  display.setTextSize(3);
  
  display.setCursor(20, 30);

  if(enter>=0 && enter <4){
    
  display.print(d1);
  display.print(d2);
  display.print(".");
  display.print(mo1);
  display.print(mo2);}
  else if(enter>=6&&enter<10){
    
  display.print(yy1);
  display.print(yy2);
  display.print(yy3);
  display.print(yy4);}
  
  
  if(enter!=4 && enter!=5){
  display.setTextSize(1);
  display.setCursor( 5, 57);   
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("eCO2=");
  display.print(eco2);
  }
  
  }
void clockset() //specjalny ekran do ustawiania ręcznie czasu
{
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  editIcon(4,18);
  display.setCursor(1,1);
  display.printf("%d/5",disMode);
  display.setCursor(4,10);  
  faceIcon(32,22);
  display.setCursor(45,12);  
  
  display.print(rtc.getHour(true)); 
  display.print(":"); 
  if(rtc.getMinute()<10)
  display.print(0); 
  display.print(rtc.getMinute()); 
  display.print(":");
  if(rtc.getSecond()<10)
  display.print(0);  
  display.print(rtc.getSecond()); 
  lockIcon(101,12);
  alarmIcon(116,19); 
  
  switch(enter){
      case 0: display.drawLine(20,28,32,28,WHITE); display.drawLine(20,27,32,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(h1>0)h1=h1-1; else h1=2;}
      else if(button5cs==HIGH && button5ls==LOW) {if(h1<2)h1=h1+1; else h1=0;}
      else if(button6cs==HIGH && button6ls==LOW) enter=1; break;
      case 1:  display.drawLine(39,28,51,28,WHITE); display.drawLine(39,27,51,27,WHITE); if(h1==2 && h2>3) h2=0;
      if(button4cs==HIGH && button4ls==LOW){if(h1<2){if(h2>0)h2=h2-1; else h2=9;} else {if(h2>0)h2=h2-1; else h2=3;} } 
      else if(button5cs==HIGH && button5ls==LOW) {if(h1<2) {if(h2<9)h2=h2+1; else h2=0;} else {if(h2<3)h2=h2+1; else h2=0;} } 
      else if(button6cs==HIGH && button6ls==LOW) enter=2; break;
      case 2:  display.drawLine(76,28,88,28,WHITE); display.drawLine(76,27,88,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(m1>0)m1=m1-1; else m1=5;}
      else if(button5cs==HIGH && button5ls==LOW) {if(m1<5)m1=m1+1; else m1=0;}
      else if(button6cs==HIGH && button6ls==LOW) enter=3; break;
      case 3:  display.drawLine(94,28,106,28,WHITE); display.drawLine(94,27,106,27,WHITE); if(button4cs==HIGH && button4ls==LOW){if(m2>0)m2=m2-1; else m2=9;}
      else if(button5cs==HIGH && button5ls==LOW) {if(m2<9)m2=m2+1; else m2=0;}
      else if(button6cs==HIGH && button6ls==LOW) enter=4; break;
      case 4: h=h1*10+h2; m=m1*10+m2; rtc.setTime(0, m, h, d, mo, 2023); preferences.putInt("hour",rtc.getHour(true)); preferences.putInt("minute",rtc.getMinute()); enter=5;  break;
  }
  display.setTextSize(3);  
  display.setCursor(20, 30);  
  display.print(h1);
  display.print(h2);
  display.print(":");
  display.print(m1);
  display.print(m2);   
  display.setTextSize(1);
  display.setCursor( 5, 57);   
  display.print("TVOC=");
  display.print(tvoc);
  display.setCursor( 65, 57);
  display.print("eCO2=");
  display.print(eco2);  
  }


void lockIcon(int x, int y)
{
  
  display.drawRect(x,y,11,9,WHITE);
  if(lock==1)  { 
  display.drawPixel(x+2,y-1,WHITE);
  display.drawPixel(x+3,y-1,WHITE);
   display.drawPixel(x+2,y-2,WHITE);
  display.drawPixel(x+3,y-2,WHITE);
   display.drawPixel(x+2,y-3,WHITE);
  display.drawPixel(x+3,y-3,WHITE);
  display.drawPixel(x+4,y-4,WHITE);
  display.drawPixel(x+5,y-4,WHITE);
  display.drawPixel(x+6,y-4,WHITE);
  display.drawPixel(x+4,y-5,WHITE);
  display.drawPixel(x+5,y-5,WHITE);
  display.drawPixel(x+6,y-5,WHITE);
   display.drawPixel(x+7,y-1,WHITE);
  display.drawPixel(x+8,y-1,WHITE);
   display.drawPixel(x+7,y-2,WHITE);
  display.drawPixel(x+8,y-2,WHITE);
   display.drawPixel(x+7,y-3,WHITE);
  display.drawPixel(x+8,y-3,WHITE);}
  
  else
  {
    display.drawPixel(x+6,y-1,WHITE);
    display.drawPixel(x+7,y-1,WHITE);
    display.drawPixel(x+6,y-2,WHITE);
    display.drawPixel(x+7,y-2,WHITE);
    display.drawPixel(x+6,y-3,WHITE);
    display.drawPixel(x+7,y-3,WHITE);
    display.drawPixel(x+6,y-4,WHITE);
    display.drawPixel(x+7,y-4,WHITE);
    display.drawPixel(x+6,y-4,WHITE);
    display.drawPixel(x+5,y-4,WHITE);
    display.drawPixel(x+4,y-4,WHITE);
    display.drawPixel(x+3,y-4,WHITE);
  }   
  }
void wifiIcon(int x, int y)
 { 
     
   display.drawRect(x,y,2,2,WHITE);
   display.drawRect(x-3,y-2,2,2,WHITE);
   display.drawRect(x+3,y-2,2,2,WHITE);
   display.drawRect(x-5,y-5,2,2,WHITE);
   display.drawRect(x+5,y-5,2,2,WHITE);
   display.drawRect(x-8,y-7,2,2,WHITE);
   display.drawRect(x+8,y-7,2,2,WHITE);
   display.drawRect(x-1,y-4,4,2,WHITE);
   display.drawRect(x-1,y-9,4,2,WHITE);
   display.drawRect(x-2,y-13,6,2,WHITE);  
   display.drawRect(x-3,y-7,2,2,WHITE);
   display.drawRect(x-6,y-9,2,2,WHITE);
   display.drawRect(x-4,y-11,2,2,WHITE);
   display.drawRect(x+3,y-7,2,2,WHITE);
   display.drawRect(x+6,y-9,2,2,WHITE);
   display.drawRect(x+4,y-11,2,2,WHITE);
  }
void faceIcon(int x, int y)
  {
    display.drawRect(x,y,5,1,WHITE);
    display.drawRect(x-2,y-1,2,1,WHITE);
    display.drawRect(x-3,y-2,1,1,WHITE);
    display.drawRect(x-4,y-4,1,2,WHITE);    
    display.drawRect(x+5,y-1,2,1,WHITE);
    display.drawRect(x+7,y-2,1,1,WHITE);
    display.drawRect(x+8,y-4,1,2,WHITE);
    display.drawRect(x-5,y-9,1,5,WHITE);
    display.drawRect(x+9,y-9,1,5,WHITE);    
    display.drawRect(x,y-14,5,1,WHITE);
    display.drawRect(x-2,y-13,2,1,WHITE);
    display.drawRect(x-3,y-12,1,1,WHITE);
    display.drawRect(x-4,y-11,1,2,WHITE);
    display.drawRect(x+5,y-13,2,1,WHITE);
    display.drawRect(x+7,y-12,1,1,WHITE);
    display.drawRect(x+8,y-11,1,2,WHITE);      
    display.drawRect(x-1,y-9,2,2,WHITE);
    display.drawRect(x+4,y-9,2,2,WHITE);
    if(eco2<=800)
     {
        display.drawPixel(x,y-4,WHITE);
        display.drawRect(x+1,y-3,3,1,WHITE);
        display.drawPixel(x+4,y-4,WHITE);
      }
    else if (eco2>=801 && eco2<=1400)
    {
      display.drawRect(x,y-3,5,1,WHITE);
      }   
    else
      {
        display.drawPixel(x,y-3,WHITE);
        display.drawRect(x+1,y-4,3,1,WHITE);
        display.drawPixel(x+4,y-3,WHITE);    
        }  
    }
void alarmIcon(int x, int y)
{
        display.drawRect(x,y,9,1,WHITE);
        display.drawRect(x+1,y-2,1,2,WHITE);
        display.drawRect(x+2,y-4,1,2,WHITE);
        display.drawRect(x+3,y-5,3,1,WHITE);
        display.drawRect(x+6,y-4,1,2,WHITE);
        display.drawRect(x+7,y-2,1,2,WHITE);
        if(!alarmOff)
        {
          display.drawPixel(x+1,y-5,WHITE);
          display.drawPixel(x+7,y-5,WHITE);
          
          display.drawLine(x+7,y-7,x+9,y-5,WHITE);
          display.drawLine(x+1,y-7,x-1,y-5,WHITE);

          display.drawLine(x+7,y-9,x+11,y-5,WHITE);
          display.drawLine(x+1,y-9,x-3,y-5,WHITE);
          }   
  }
void editIcon(int x, int y)
{
  display.drawPixel(x,y-4,WHITE);
  display.drawPixel(x+1,y-4,WHITE);
  display.drawPixel(x+2,y-3,WHITE);
  display.drawPixel(x+2,y-4,WHITE);
  display.drawPixel(x+2,y-5,WHITE);
  display.drawPixel(x+3,y-2,WHITE);
  display.drawPixel(x+3,y-6,WHITE);
  display.drawPixel(x+4,y-1,WHITE);
  display.drawPixel(x+4,y-7,WHITE);  
  display.drawPixel(x+5,y-1,WHITE);
  display.drawPixel(x+5,y-7,WHITE);
  display.drawRect(x+6,y,8,1,WHITE);
  display.drawRect(x+6,y-8,8,1,WHITE);
  display.drawRect(x+13,y-8,1,8,WHITE);
  display.drawRect(x+7,y-2,4,1,WHITE);
  display.drawRect(x+7,y-6,1,4,WHITE);
  display.drawRect(x+7,y-4,4,1,WHITE);
  display.drawRect(x+7,y-6,4,1,WHITE);     
  }  
