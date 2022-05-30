/*
This sketch will generate a PPM signal that is compatible with a FLYSKY/Turnigy i6 TX
You are abe to control these PPM channels by either using Digital or Analog Inputs

Supported boards: Arduino Mini/Pro Mini 3.3v. 

The mod firmware allows 3 PPM channels (usually an input from a trainer TX) to be used as additonal channels
by selecting PPM1, PPM2 or PPM3 as aux channels. This allows you to send the full 14 channels over iBus. 

The FS-i6 14 Channel firmware mod is required : https://github.com/qba667/FlySkyI6
The iBusBM libary is availble for Arduino based boards and will read all 14 channels: https://github.com/bmellink/IBusBM

Enjoy! =) Cobalt6700 2021

*/

/* 
  adding to Cobalt's work, thank you so much!
  
  streamline of code for multiple switches or analog inputs
  revision 44 19-apr-2021 dremu-at-yahoo

  with the firmware from the qba667 page three channels of externally input PPM data can be available as iBus aux channels
  in short, you can add the following inputs to your FS-i6, with the corresponding results on the PPMx iBus channels
  you then assign PPM1-2-3 to the desired channel in Functions Setup->Aux Channels

  up to six 2-position switches (SPST on/off), returning 1000 or 2000
  up to six 3-positions switches (SPDT on/off/on), returning 1000/1500/2000
  up to three analog reads (potentiometers, etc), returning 1000...2000

  Choosing options is done via digital pins at startup. If you change options, press the reset button to refresh.
  
  As discussed on the project page, you can build the Arduino with a DIP switch (my preference as it allows easy changes)
  or jumpers on a double-row header, if you don't have a 6-position DIP switch
  or just solder pins as required to hard code the options
  In this case, ground the pins D2...7 for DIP switch 1...6 "ON" below, or leave them open for "OFF".

  PPM1 options              DIP switch
                            1     2       Arduino pin(s)
    pot                     ON    ON      A0
    one switch ("SwF")      OFF   ON      A0
    two switches ("SwF+G")  OFF   OFF     A0, A1
    invalid                 ON    OFF     don't use

  PPM2 options              DIP switch
                            3     4       Arduino pin(s)
    pot                     ON    ON      A2
    one switch ("SwH")      OFF   ON      A2
    two switches ("SwH+J")  OFF   OFF     A2, A3
    invalid                 ON    OFF     don't use

  PPM3 options              DIP switch
                            5     6       Arduino pin(s)
    pot                     ON    ON      A4
    one switch ("SwK")      OFF   ON      A4
    two switches (SwK+L")   OFF   OFF     A4, A5
    invalid                 ON    OFF     don't use

  For further details on input connections, see schematic and pictures on the project page.

  Pots should be wired as voltage dividers, with one end to 3v3, the other to ground, and the wiper to the analog pin.

  SPST (on-off) switches should be wired with the analog pin pulled high via 10k-100k resistor going to one side of the switch.
  The other side then goes to ground.

  SPDT switches should be wired with two resistors, one between each and and the center. The analog pin connects to the center,
  and the ends of the switch connect to 3v3 and ground.

  If using a pot or one switch (if A1, A3 or A5 is unused), you should wire that pin to ground to avoid spurious readings.

  "SwI" is skipped to avoid confusion with "Sw1" or "Swl". Hopefully "SwL" can be distinguished.
    
  Arduino PPM output pin connects to "PPM in" on the TX
  either plugged into the rear 6-pin miniDIN (programming/trainer port)
  or soldered to the main board. look for an unsoldered pad to the bottom left of the silver case (radio board)
  to the right of, and at about the middle of the plug to the programming/trainer port 
  as looking at the TX with the back off, the controls down, and the handle up

  there are 3.3V and gnd spots at the bottom center of the PCB
  the 3.3V is used for some of the hardware mods on the qba667 page
  this allows the pro mini to be mounted in or on the TX case if so desired

  pulse widths are specific as measured on one particular pro mini, an 8MHz 3.3V clone
  and may need adjusting for your specific situation
  I tuned these to get the complete range of 1000-1500-2000 iBus data on the RX
  you can do an ibus debug using another Arduino to see what values are received for any given out_val
  see section on analogread below and rounding
  you can also use the TX, albeit with less accuracy, to see what's being received
  assign e.g. PPM1 to e.g. Ch5 in Functions Setup->Aux Channels
  then watch Ch5 in Functions setup->Display, and adjust these values to give maximum range
  also see OCR1A below
*/

bool SwF;
bool SwG;

bool SwH;
bool SwJ;

bool SwK;
bool SwL;

#define pulseMin 580    // pulse minimum width minus start in uS (was 500)
#define pulseMax 1700   // pulse maximum width in uS
#define Fixed_uS 400    // PPM frame fixed LOW phase

// initial output values for PPM signal
// declared as float so as to avoid having to re-cast when scaling
float out1_val = pulseMin;
float out2_val = pulseMin;
float out3_val = pulseMin;

unsigned long lastdisp=0;

int analogValue[6];

/* 
  this packs two 3-position switches into one channel, mirroring the existing "SwB+C" combined values
  not all states are possible, since a switch can't be both up and down at once
  some permutations therefore go unused (set to 42 here)
  for 3-position switches, strictly speaking indices 0-3 aren't applicable either
  for 2-position, the same goes for 4-15, but it's simpler to share the same array

  the analog value is scaled from 0...1023 to 0...2
  
  for e.g. SwF+G, the possible permutations are

  SwF   A'log Dig   SwG   A'log Dig   bin   dec SwF+G iBus
  down  0     0     down  0     0     0000  0   1000
  down  0     0     ctr   500   1     0001  1   1200
  down  0     0     up    1000  2     0010  2   1400
  
  ctr   500   1     down  0     0     0100  4   1300
  ctr   500   1     ctr   500   1     0101  5   1500
  ctr   500   1     up    1000  2     0110  6   1700
              
  up    1000  2     down  0     0     1000  8   1600
  up    1000  2     ctr   500   1     1001  9   1800
  up    2000  2     up    1000  2     1010  10  2000

  if instead you're using 2-position switches, they still pack
  the possible permutations are e.g.

  SwF   A'log D     SwG   A'log Dig   bin   dec SwF+G iBus
  down  0     0     down  0     0     0000  0   1000
  down  0     0     up    1000  2     0010  2   1400
  up    1000  2     down  0     0     1000  8   1600
  up    1000  2     up    1000  2     1010  10  2000

  2-position or 3-position switches conveniently return the same values for up and down
  just with additional values for center on the 3-position

  note that the array values are offsets from 1000, so have 1000 subtracted from the above iBus data
*/

int combosw[]={0,200,400,42,300,500,700,42,600,800,1000,42,42,42,42,42};

int i;

#define PPMpin 10   // PPM output to TX PCB

#define DEBUG  false  // dump PPM data to Serial once a second

void setup()
{
  pinMode(PPMpin, OUTPUT);
  Serial.begin(115200);
  Serial.println("And so it begins");

  // set up timer
  TCCR1A = B00110001; // Compare register B used in mode '3'
  TCCR1B = B00010010; // WGM13 and CS11 set to 1
  TCCR1C = B00000000; // All set to 0
  TIMSK1 = B00000010; // Interrupt on compare B
  TIFR1  = B00000010; // Interrupt on compare B
  OCR1A = 10000;      // 10.0mS PPM output refresh (was 10200 = 10.2mS)
  OCR1B = 1000;       // 1mS PPM timer cycle

  // read DIP switch

  for (i=2; i<=7; i++)
  {
    pinMode(i, INPUT_PULLUP);
  }

  SwF=digitalRead(2);
  SwG=digitalRead(3);

  Serial.print("PPM1: ");
  if ((!SwF) && (!SwG)) {Serial.println("pot");}
  if ((!SwF) && (SwG))  {Serial.println("invalid");}
  if ((SwF) && (!SwG))  {Serial.println("single switch");}
  if ((SwF) && (SwG))   {Serial.println("combined switches");}
  
  SwH=digitalRead(4);
  SwJ=digitalRead(5);

  Serial.print("PPM2: ");
  if ((!SwH) && (!SwJ)) {Serial.println("pot");}
  if ((!SwH) && (SwJ))  {Serial.println("invalid");}
  if ((SwH) && (!SwJ))  {Serial.println("single switch");}
  if ((SwH) && (SwJ))   {Serial.println("combined switches");}
  
  SwK=digitalRead(6);
  SwL=digitalRead(7);
  Serial.print("PPM3: ");
  if ((!SwK) && (!SwL)) {Serial.println("pot");}
  if ((!SwK) && (SwL))  {Serial.println("invalid");}
  if ((SwK) && (!SwL))  {Serial.println("single switch");}
  if ((SwK) && (SwL))   {Serial.println("combined switches");}
}

// interrupt code to send pulses

ISR(TIMER1_COMPA_vect)
{
  // Channel 1
  digitalWrite(PPMpin, LOW);   
  delayMicroseconds(Fixed_uS);    // drive signal low for fixed length pulse
  digitalWrite(PPMpin, HIGH);
  delayMicroseconds(out1_val);    // drive signal high for controlled length pulse
  
  // Channel 2
  digitalWrite(PPMpin, LOW);
  delayMicroseconds(Fixed_uS);   
  digitalWrite(PPMpin, HIGH);
  delayMicroseconds(out2_val);      
  
  // Channel 3
  digitalWrite(PPMpin, LOW);
  delayMicroseconds(Fixed_uS);  
  digitalWrite(PPMpin, HIGH);
  delayMicroseconds(out3_val); 

  // Synchro pulse
  digitalWrite(PPMpin, LOW);
  delayMicroseconds(Fixed_uS); // drive signal low for fixed length pulse
  digitalWrite(PPMpin, HIGH);  // Start Synchro pulse
}

void loop()
{
  // PPM1
  
  if (SwF)
  {
    analogValue[0]=500*int(analogRead(0)/500); // round analog value to avoid fluctuations at middle
  }
  else
  {
    analogValue[0]=map(analogRead(0),0,1023,0,1000); // scale the analogRead from 0...1023 to 0...1000 for RC channel
  }
    
  if (SwG) // if PPM1 = SwF+G
  {
    analogValue[1]=500*int(analogRead(1)/500); // round analog value to avoid fluctuations at middle
    out1_val = pulseMin+(pulseMax-pulseMin)/1000*combosw[int(analogValue[0]/500)+4*int(analogValue[1]/500)];
  }
  else // otherwise, PPM1 = SwF or a pot
  {
    out1_val = pulseMin+(pulseMax-pulseMin)/1000*analogValue[0];
  }

  // PPM2
  
  if (SwH)
  {
    analogValue[2]=500*int(analogRead(2)/500); // round analog value to avoid fluctuations at middle
  }
  else
  {
    analogValue[2]=map(analogRead(2),0,1023,0,1000); // scale the analogRead from 0...1023 to 0...1000 for RC channel
  }
    
  if (SwJ) // if PPM2 = SwH+J
  {
    analogValue[3]=500*int(analogRead(3)/500); // round analog value to avoid fluctuations at middle
    out2_val = pulseMin+(pulseMax-pulseMin)/1000*combosw[int(analogValue[2]/500)+4*int(analogValue[3]/500)];
  }
  else // otherwise, PPM2 = SwH or a pot
  {
    out2_val = pulseMin+(pulseMax-pulseMin)/1000*analogValue[2];
  }

  // PPM3
  
  if (SwK)
  {
    analogValue[4]=500*int(analogRead(4)/500); // round analog value to avoid fluctuations at middle
  }
  else
  {
    analogValue[4]=map(analogRead(4),0,1023,0,1000); // scale the analogRead from 0...1023 to 0...1000 for RC channel
  }
    
  if (SwL) // if PPM3 = SwK+L
  {
    analogValue[5]=500*int(analogRead(5)/500); // round analog value to avoid fluctuations at middle
    out3_val = pulseMin+(pulseMax-pulseMin)/1000*combosw[int(analogValue[4]/500)+4*int(analogValue[5]/500)];
  }
  else // otherwise, PPM3 = SwK or a pot
  {
    out3_val = pulseMin+(pulseMax-pulseMin)/1000*analogValue[4];
  }

  if (DEBUG)
  {
    if ((millis() - lastdisp) > 1000) // only display outputs once a second so as not to flood console
    {
      Serial.print("PPM1 "); Serial.print(int(out1_val));
      Serial.print("\tPPM2 "); Serial.print(int(out2_val));
      Serial.print("\tPPM3 "); Serial.println(int(out3_val));
  
      lastdisp=millis();
    }
  }
}
