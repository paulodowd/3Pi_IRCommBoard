/*
   i2c slave device to handle IR messaging between
   3pi+ robots.
*/
#include <avr/io.h>
#include <Wire.h>
#define I2C_ADDR 8

boolean LED;
unsigned long ts;

#define STATE_IR_TX_ON 1
#define STATE_IR_TX_OFF 2
int state = STATE_IR_TX_ON;


#define MAX_MSG 32
static char tx_buf[MAX_MSG];
static char rx_buf[MAX_MSG];
static char rx_msg[MAX_MSG];
int rx_count;

#define TX_CLK_OUT 4

unsigned long msg_ttl; // msg time to live
#define TTL 1000       // keep a message for 1 second

unsigned long tx_ts;
unsigned long tx_delay;

boolean BROADCAST = false;
boolean PROCESS_MSG = false;

void setup() {

  initRandomSeed();

  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);
  pinMode(TX_CLK_OUT, OUTPUT);

  pinMode(13, OUTPUT);

  // Set message buffers to invalid (empty) state
  memset( tx_buf, 0, sizeof( tx_buf ));
  memset( rx_buf, 0, sizeof( rx_buf ));
  memset( rx_msg, 0, sizeof( rx_msg ));

  setupTimer2();

  Wire.begin( I2C_ADDR );
  Wire.onRequest( reportLastMessage );
  Wire.onReceive( newMessageToSend );

  Serial.begin(4800);
  Serial.setTimeout(500);
  ts = millis();
  msg_ttl = millis();

  tx_ts = millis();
  setTXDelay();
  //disableTX();
  resetRxBuf();
}


// Use analogRead to set a random
// seed for the RNG
void initRandomSeed() {
  pinMode(A0, INPUT);
  byte r = 0x00;
  for ( int i = 0; i < 8; i++ ) {
    byte b = (byte)analogRead(A0);
    b = b & 0x01;
    r |= (b << i);
    delayMicroseconds(2);
  }
  randomSeed( r );
}

void setTXDelay() {


  float t = (float)random(0, 10000);
  t /= 10000;
  t *= 400;
  t += 100;
  // Insert random delay to help
  // break up synchronous tranmission
  // between robots.
  tx_delay = (unsigned long)t;
}

// This ISR simply toggles the state of
// pin D4 to generate a 38khz clock signal.
ISR( TIMER2_COMPA_vect ) {
  PORTD ^= (1 << PD4);

}

// We use Timer2 to generate a 38khz clock
// which is electronically OR'd with the
// serial TX.
void setupTimer2() {

  cli();
  // Setup Timer 2 to fire ISR every 38khz
  // Enable CTC mode
  TCCR2A = 0;
  TCCR2A |= (1 << WGM21);

  // Setup ctc prescaler
  TCCR2B = 0;

  // No prescale?
  //TCCR2B |= (1<<CS21);
  TCCR2B |= (1 << CS20);

  // match value
  OCR2A = 210; // counts to 53, every 38,000hz
  //OCR2A = 53; // counts to 53, every 38,000hz


  // Set up which interupt flag is triggered
  TIMSK2 = 0;
  TIMSK2 |= (1 << OCIE2A);

  // enable interrupts
  sei();

}



byte CRC( String in_string ) {
  String buf;
  byte cs;
  int i;

  cs = 0;
  for ( i = 0;  i < (int)in_string.length(); i++ ) {
    cs = cs ^ in_string.charAt(i);
  }
  cs &= 0xff;

  return cs;
}

// Sends a string of the latest message received from
// other robots.
void reportLastMessage() {

  // No message.
  if ( rx_msg[0] == 0 ) {
    Wire.write("!");
  } else {
    Wire.write( rx_msg, strlen(rx_msg) );
  }

}
void newMessageToSend( int len ) {
  //Serial.println(len);
  //digitalWrite(13, HIGH);
  int count = 0;

  // Stop transmission whilst we update
  // the buffer.
  state = STATE_IR_TX_OFF;

  // Clear tx buffer
  memset( tx_buf, 0, sizeof( tx_buf ) );

  // set first character as our message
  // start byte
  tx_buf[count++] = '*';

  // Should we add a timeout here?
  // Receive up until buffer -1
  while ( Wire.available()  ) {
    char c = Wire.read();
    tx_buf[count] = c;
    count++;
  }

  // Add crc
  byte cs;
  int i;

  cs = 0;
  for ( i = 0;  i < count; i++ ) {
    cs = cs ^ tx_buf[i];
  }
  cs &= 0xff;

  tx_buf[count++] = '@';
  tx_buf[count++] = cs;


  //Serial.println( count );
  //Serial.println( tx_buf );

  // Restart IR transmission
  state = STATE_IR_TX_ON;
}

void disableRX() {
  UCSR0B &= ~( 1 << RXEN0 );
}
void enableRX() {

  UCSR0A &= ~(1 << FE0);
  UCSR0A &= ~(1 << DOR0);
  UCSR0A &= ~(1 << UPE0);
  UCSR0B |= ( 1 << RXEN0 );

}

// We need to stop the 38khz carrier
// when no serial print is happening,
// otherwise we saturate the environment
// with a continuous 38khz pulse
void disableTX() {

  // Set timer2 prescale to 0,0,0 (off)
  TCCR2B = 0;


}

void enableTX() {

  // Set prescale back to how it was
  TCCR2B |= (1 << CS20);
}

void resetRxBuf() {
  rx_count = 0;
  PROCESS_MSG = false;
  memset( rx_buf, 0, sizeof( rx_buf ) );
}

void loop() {


  if ( millis() - tx_ts > tx_delay ) {
    BROADCAST = !BROADCAST;
    tx_ts = millis();
    setTXDelay();
  }
  
  if ( state == STATE_IR_TX_ON && BROADCAST) {
    if ( tx_buf[0] == 0 ) {
      // don't send, empty buffer.
    } else {

      // We have a problem where we pick up
      // our own reflected transmission.
      disableRX();
      //enableTX();
      Serial.println( tx_buf );
      Serial.flush(); // wait for send to complete
      //disableTX();
      enableRX();

      //while( Serial.available() ) Serial.read();
      //Serial.flush();
      //digitalWrite(9, HIGH );
    }
  }
  //
  //}


  // Clear old messages
  if ( millis() - msg_ttl  > TTL ) {
    memset( rx_msg, 0, sizeof( rx_msg ) );
    msg_ttl = millis();
  }

  // Clear status LED
  if ( millis() - ts > 100 ) {
    ts = millis();
    digitalWrite( 13, LOW );

    //Serial.print("last message: ");
    //Serial.print( rx_msg );
    //Serial.print(" ");
    //Serial.println( ts );
  }


  while ( Serial.available() ) {

    rx_buf[rx_count] = Serial.read();
    if ( rx_buf[ rx_count ] == '\n' ) PROCESS_MSG = true;

    rx_count++;
    if ( rx_count >= MAX_MSG ) PROCESS_MSG = true;
  }

  if ( PROCESS_MSG ) {

    //Serial.print("got "); Serial.print(rx_buf); Serial.print(":"); Serial.println(rx_count);
    if ( rx_count <= 0 || rx_buf[0] == 0 ) {
      // bad read.
      //Serial.println("bad serial");
    } else {

      // Check for start byte
      int start = -1;
      for ( int i = 0; i < rx_count; i++ ) {
        if ( rx_buf[i] == '*') {
          start = i;
          break;
        }
      }

      // Message too short or bad
      // e.g. a useless message would be:
      //      *@_  (start,CRC token, CRC value)
      //      n = 3 (message length)
      //      start = 0
      if ( (rx_count - start) <= 3 || start < 0 ) {
        //Serial.println("message too short :(");

      } else {

        // Check CRC
        int last = -1;
        for ( int i = 0; i < rx_count; i++ ) {
          if ( rx_buf[i] == '@') {
            last = i;
            break;
          }
        }

        // Start should be 0 or more
        // Note, we include the start byte *
        // in the checksum
        if ( last > start && last < (rx_count - 1) ) {
          char buf[rx_count];
          int b = 0;  // count how many bytes
          // we copy.
          for ( int i = start; i < last; i++ ) {
            buf[b] = rx_buf[i];
            b++;
          }

          // We need at least 1 byte of data
          // between start and checksum
          if ( b >= 1 ) {
            //Serial.print("copied: " );
            //Serial.print( buf );
            //Serial.print( " b = ");
            //Serial.println(b);

            // Reconstruct checksum
            byte cs;
            cs = 0;
            for ( int i = 0;  i < b; i++ ) {
              cs = cs ^ buf[i];
            }
            cs &= 0xff;

            if ( cs == rx_buf[last + 1] ) {
              //Serial.println("CRC GOOD!");
              // Copy message across, ignoring the *

              // Flash so we can see when robots are
              // receiving messages correctly
              digitalWrite(13, HIGH );

              memset( rx_msg, 0, sizeof( rx_msg ));
              for ( int i = 0; i < b - 1; i++ ) {
                rx_msg[i] = buf[i + 1];
              }
              rx_msg[b-1] = '!';
              msg_ttl = millis();

            } else {
              //Serial.println("CRC BAD!");
            }
          } else {
            //Serial.println("Message payload < 1 ");
          }

        }
      }
    }

    // reset receive 
    resetRxBuf();

  }
  //delay(50);
}
