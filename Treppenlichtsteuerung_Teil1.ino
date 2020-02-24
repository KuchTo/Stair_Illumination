#include <Wire.h>

#define PWM_Module_Base_Addr 0x40 //10000000b  Das letzte Bit des Adressbytes definiert die auszuführende Operation. Bei Einstellung auf logisch 1  0x41 Modul 2
                            //wird ein Lesevorgang auswählt, während eine logische 0 eine Schreiboperation auswählt.
#define OE_Pin  8           // Pin für Output Enable
#define PIRA_Pin 2
#define PIRB_Pin 3

#define Num_Stages  15
#define Delay_Stages  10
#define Delay_ON_to_OFF  5  // Minimum Delay_ON_to_OFF in Seconds

byte Pwm_Channel = 0;
int Pwm_Channel_Brightness = 0;

bool Motion_Trigger_Down_to_Up = false;
bool On_Delay = false;

// interrupt Control
byte A60telSeconds24 = 0;
byte Seconds24;

ISR(TIMER1_COMPA_vect)        
{
A60telSeconds24++;
  if (A60telSeconds24 > 59)
    {
      A60telSeconds24 = 0;
      Seconds24++;
      if (Seconds24 > 150)
        {
          Seconds24 = 0;
        }
    }
}

void ISR_PIR_A()
{
bool PinState = digitalRead(PIRA_Pin);
if (PinState) 
  {
  Motion_Trigger_Down_to_Up = true; // PIR A ausgelöst
  } 
}

void ISR_PIR_B()
{
bool PinState = digitalRead(PIRB_Pin);
if (PinState) 
  { 
    Motion_Trigger_Down_to_Up = true; // PIR B ausgelöst
  } 
}

void Init_PWM_Module(byte PWM_ModuleAddr)
{
  pinMode(OE_Pin,OUTPUT);
  digitalWrite(OE_Pin,HIGH); // Active LOW-Ausgangsaktivierungs-Pin (OE).    
  Wire.beginTransmission(PWM_ModuleAddr); // Datentransfer initiieren
  Wire.write(0x01);                       // Wähle  Mode 2 Register (Command Register)
  Wire.write(0x04);                       // Konfiguriere Chip: 0x04:  totem pole Ausgang 0x00: Open drain Ausgang. 
  Wire.endTransmission();                 // Stoppe Kommunikation - Sende Stop Bit
  Wire.beginTransmission(PWM_ModuleAddr); // Datentransfer initiieren
  Wire.write(0x00);                      // Wähle Mode 1 Register (Command Register)
  Wire.write(0x10);                      // Konfiguriere SleepMode
  Wire.endTransmission();                // Stoppe Kommunikation - Sende Stop Bit   
  Wire.beginTransmission(PWM_ModuleAddr); // Datentransfer initiieren
  Wire.write(0xFE);                       // Wähle PRE_SCALE register (Command Register)
  Wire.write(0x03);                       // Set Prescaler. Die maximale PWM Frequent ist 1526 Hz wenn das PRE_SCALEer Regsiter auf "0x03h" gesetzt wird. Standard : 200 Hz
  Wire.endTransmission();                 // Stoppe Kommunikation - Sende Stop Bit
  Wire.beginTransmission(PWM_ModuleAddr); // Datentransfer initiieren
  Wire.write(0x00);                       // Wähle Mode 1 Register (Command Register)
  Wire.write(0xA1);                       // Konfiguriere Chip:  ERrlaube All Call I2C Adressen, verwende interne Uhr,                                           // Erlaube Auto Increment Feature
  Wire.endTransmission();                 // Stoppe Kommunikation - Sende Stop Bit    
}


void Init_PWM_Outputs(byte PWM_ModuleAddr)
{
  digitalWrite(OE_Pin,HIGH); // Active LOW-Ausgangsaktivierungs-Pin (OE).   
   for ( int z = 0;z < 16 + 1;z++)
    {
     Wire.beginTransmission(PWM_ModuleAddr); 
     Wire.write(z * 4 +6);       // Wähle PWM_Channel_ON_L register 
     Wire.write(0x00);                     // Wert für o.g. Register 
     Wire.endTransmission();       
     Wire.beginTransmission(PWM_ModuleAddr); 
     Wire.write(z * 4 +7);       // Wähle PWM_Channel_ON_H register 
     Wire.write(0x00);                     // Wert für o.g. Register
     Wire.endTransmission();     
     Wire.beginTransmission(PWM_ModuleAddr); 
     Wire.write(z * 4 +8);    // Wähle PWM_Channel_OFF_L register 
     Wire.write(0x00);        // Wert für o.g. Register
     Wire.endTransmission();       
     Wire.beginTransmission(PWM_ModuleAddr); 
     Wire.write(z * 4 +9);   // Wähle PWM_Channel_OFF_H register 
     Wire.write(0x00);             // Wert für o.g. Register  
     Wire.endTransmission();
    }
 digitalWrite(OE_Pin,LOW); // Active LOW-Ausgangsaktivierungs-Pin (OE).    
}

void setup()  
{
   //Initalisierung 
   pinMode(PIRA_Pin,INPUT);
   pinMode(PIRB_Pin,INPUT);
   Serial.begin(9600);   
   Wire.begin(); // Initalisiere I2C Bus A4 (SDA), A5 (SCL) 
   Init_PWM_Module(PWM_Module_Base_Addr);                
   Init_PWM_Outputs(PWM_Module_Base_Addr);
   noInterrupts();
   attachInterrupt(0, ISR_PIR_A, CHANGE);
   attachInterrupt(1, ISR_PIR_B, CHANGE); 
   TCCR1A = 0x00;  
   TCCR1B = 0x02;
   TCNT1 = 0;      // Register mit 0 initialisieren
   OCR1A =  33353;      // Output Compare Register vorbelegen 
   TIMSK1 |= (1 << OCIE1A);  // Timer Compare Interrupt aktivieren
   interrupts();
}

void Down_to_Up_ON()
{
 Pwm_Channel = 0;
 Pwm_Channel_Brightness = 0;
 while (Pwm_Channel < Num_Stages +1)
    {
    Wire.beginTransmission( PWM_Module_Base_Addr); 
    Wire.write(Pwm_Channel * 4 +8);    // Wähle PWM_Channel_0_OFF_L register 
    Wire.write((byte)Pwm_Channel_Brightness & 0xFF);        // Wert für o.g. Register
    Wire.endTransmission(); 
    Wire.beginTransmission( PWM_Module_Base_Addr); 
    Wire.write(Pwm_Channel * 4 +9);   // Wähle PWM_Channel_0_OFF_H register 
    Wire.write((Pwm_Channel_Brightness >> 8));             // Wert für o.g. Register  
    Wire.endTransmission();
    if (Pwm_Channel_Brightness < 4095)
      {
      Pwm_Channel_Brightness = Pwm_Channel_Brightness + Delay_Stages;
      if (Pwm_Channel_Brightness > 4095) {Pwm_Channel_Brightness = 4095;}
      } else if ( Pwm_Channel < Num_Stages +1)
      {
        Pwm_Channel_Brightness = 0;
        delay(200); 
        Pwm_Channel++;
      }

    } 
} 

void Down_to_Up_OFF()
{
 Pwm_Channel = 0;
 Pwm_Channel_Brightness = 4095;
 while (Pwm_Channel < Num_Stages +1)
    {
    Wire.beginTransmission( PWM_Module_Base_Addr); 
    Wire.write(Pwm_Channel * 4 +8);    // Wähle PWM_Channel_0_OFF_L register 
    Wire.write((byte)Pwm_Channel_Brightness & 0xFF);        // Wert für o.g. Register
    Wire.endTransmission(); 
    Wire.beginTransmission( PWM_Module_Base_Addr); 
    Wire.write(Pwm_Channel * 4 +9);   // Wähle PWM_Channel_0_OFF_H register 
    Wire.write((Pwm_Channel_Brightness >> 8));             // Wert für o.g. Register  
    Wire.endTransmission();
    if (Pwm_Channel_Brightness > 0)
      {
      Pwm_Channel_Brightness = Pwm_Channel_Brightness - Delay_Stages;
      if (Pwm_Channel_Brightness < 0) {Pwm_Channel_Brightness = 0;}
      } else if ( Pwm_Channel < Num_Stages +1)
      {
        Pwm_Channel_Brightness = 4095;
        delay(200); 
        Pwm_Channel++;
      }
      
    }
} 

void loop() {
 if ((Motion_Trigger_Down_to_Up) and !(On_Delay))
  {
  Seconds24 = 0;
  On_Delay = true;
  Down_to_Up_ON();
  Motion_Trigger_Down_to_Up = false;
  }
 if ((On_Delay) and  (Seconds24 > Delay_ON_to_OFF))
  {
   Motion_Trigger_Down_to_Up = false; 
   On_Delay = false; 
   Down_to_Up_OFF();
  }
}
