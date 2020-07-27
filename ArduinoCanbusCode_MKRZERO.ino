// Arduino based Canbus converter
// Z33 Canbus TACH and Coolant Conversion module
// Designed to take signals from VR30 and transmit them to Z33 dash amplifier. 

#include <SPI.h>
#include <mcp_can.h>

int stringSplit(char *str, const char *delim, int data[]);
bool gotdata;

//Might be bad form, but the final buffer lives here globally
static unsigned char finalbuffer[8]={0,0,0,0,0,0,0,0};
//using a master time to prevent issues with fans turning on and immediately off
unsigned int mastertime = 0;

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
// the MKR Zero shield uses pin 3.
const int SPI_CS_PIN = 3;


MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin
void setup()
{
    while(!Serial);
    delay(500);
    Serial.begin(115200);
    delay(500);
    CanbusStart();
}

// start canbus shield
void CanbusStart(void)
{
    Serial.println("Canbus monitor");
    while (CAN_OK != CAN.begin(CAN_500KBPS))              // init can bus : baudrate = 500k
    {
        Serial.println("CAN BUS Shield init fail");
        Serial.println(" Init CAN BUS Shield again");
        delay(100);
    }
    CAN.init_Mask(0, 0, 0x7FF);
    CAN.init_Mask(1, 0, 0x7FF);
    //This filter is setup to cut down on the noise generated by the VR30 ECU.  Only listens for TACH and Coolant CanIDs.
    //Filters are for x1F9 and x551
    CAN.init_Filt(0, 0, 0x1F9);
    CAN.init_Filt(1, 0, 0x551);
    Serial.println("CAN BUS Shield init ok!");
}

void loop()
{
  
    // check for received message
    gotdata = false;
    unsigned char len = 0;
    unsigned char buf[8]={0};
    char capturetext[16]={0};
    //Unsigned cause Arduino likes to go signed 2's in hex... dunno why.
    static unsigned int capbuffera[8]={0,0,0,0,0,0,0,0};
    static unsigned int capbufferb[8]={0,0,0,0,0,0,0,0};
    
    if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
    {
      //Serial.println("Got Data");
        CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
        unsigned long int canId = CAN.getCanId();
        //Serial.println(canId, HEX);
        char text[50]={0};
        // setup standard 11 bit or extended 29 bit frame and add ID value
        sprintf(&text[strlen(text)],"ID %08lx ", canId);
        // this will capture HEX.
        if (canId == 0x1F9){
            for (int i=0; i<8; i++)
            {
              capturetext[0]=0;;
              sprintf(capturetext, "%02X", buf[i]);
              //Serial.print(capturetext);
              capbuffera[i] = strtoul(capturetext, NULL, 16);
              gotdata = true;  
            }
        
                  
        }
         if (canId == 0x551){
            for (int i=0; i<8; i++)
            {
              capturetext[0]=0;;
              sprintf(capturetext, "%02X", buf[i]);
              //Serial.print(capturetext);
              capbufferb[i] = strtoul(capturetext, NULL, 16);
              gotdata = true;  
            }    
         }
        //Convert 1F9 Tach Message to 23D Tach Message
        convertRPM(capbuffera[2],capbuffera[3]);
        //Toss the Coolant message into 23D
        convertCool(capbufferb[0]);
        for(int i = 0; i<8; i++)    // print the data
        {
            text[0]=0;;
            sprintf(text,"%02X", buf[i]);
            //Serial.print(text);
            //Serial.print(buf[i], HEX);
        }
        Serial.println();
    }
    if (gotdata ==  true){
      // this code will only fire if valid data was receieved.  This keeps the device from blasting the CANBUS with needless noise.
      // Some sort of internal counter used by the Z33 dash.
      if (finalbuffer[0] >= 60){
        finalbuffer[0] = 0;
      }
      else {
        finalbuffer[0] = finalbuffer[0] + 20;
      }
      CAN.sendMsgBuf(0x23D, 0, 0, 8, finalbuffer);
      //Coolant Fan Control
      fancheck(capbufferb[0]);
      } 
}


void convertRPM(unsigned int tacha, unsigned int tachb){
   //First convert the 1F9 signal into a raw number using the two TACH inputs
   int rawrpm = (((tacha * 256) + tachb)/4)/2;
   
   int parta = 0;
   int partb = 0;
   parta = rawrpm / 765;
   partb = (rawrpm - (parta*765))/3;
   if (partb > 255){
    parta += 1;
    partb -= 254;
   }
   finalbuffer[3]=partb;
   finalbuffer[4]=parta;
   
}

void convertCool(unsigned int coolant){
  //Some math goes here maybe... looks like the raw value from ID 551 requires no conversion.
  finalbuffer[7]= coolant;
}

void fancheck(unsigned int coolant) {
  //Based on how high the coolant temp is, trigger relays  (Need trigger message and proper CANiD still.)
  static unsigned char triggerlow[8]={0,0,0,0,0,0,0,0};
  static unsigned char triggermed[8]={0,0,0,0,0,0,0,0};
  static unsigned char triggerhigh[8]={0,0,0,0,0,0,0,0};
  static unsigned char triggeroff[8]={0,0,0,0,0,0,0,0};
  Serial.println();
  Serial.print("Coolant Reading: ");
  Serial.print(coolant);
  if (coolant > 150 && coolant < 160){
    //low+
    Serial.println();
    Serial.print("Coolant Fan Speed: Low");
    //CAN.sendMsgBuf(0x23D, 0, 0, 8, triggerlow);
    mastertime = mastertime++;
  }
  else if (coolant < 170 && coolant > 161){
    //medium
    Serial.println();
    Serial.print("Coolant Fan Speed: Medium");
    //CAN.sendMsgBuf(0x23D, 0, 0, 8, triggermed);
    mastertime = mastertime++;
  }
  else if (coolant > 171){
    //high 
    Serial.println();
    Serial.print("Coolant Fan Speed: High");
    //CAN.sendMsgBuf(0x23D, 0, 0, 8, triggerhigh);
    mastertime = mastertime++;
  }
  else {
    //off with a delay to prevent fans from quickly cycling needlessly. 
    if (mastertime > 50000){
    Serial.println();
    Serial.print("Coolant Fan Speed: Off");
    //CAN.sendMsgBuf(0x23D, 0, 0, 8, triggeroff);
    mastertime = 0;  
    }
    
  }
}
