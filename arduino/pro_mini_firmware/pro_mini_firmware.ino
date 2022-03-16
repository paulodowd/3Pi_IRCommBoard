/*
   i2c slave device to handle IR messaging between
   3pi+ robots.
*/
#include <avr/io.h>
#include <Wire.h>
#define I2C_ADDR 8

boolean LED;      // debug LED
unsigned long ts; // general time stamp, timeout for
                  // debug LED.

// Two operational states
#define STATE_IR_TX_ON 1
#define STATE_IR_TX_OFF 2
int state = STATE_IR_TX_ON;


#define MAX_MSG 32
static char tx_buf[MAX_MSG];  // buffer for IR out (serial)
static char rx_buf[MAX_MSG];  // buffer for IR in  (serial)
static char rx_msg[MAX_MSG];  // holding buffer for msg down 
                              // to Master over i2c
int rx_count;

// 38Khz signal generated on
// digital pin 4.
#define TX_CLK_OUT 4

unsigned long msg_ttl; // msg time to live
#define TTL 1000       // keep a message for 1 second
                       // then delete. 

unsigned long tx_ts;    // transmit time-stamp
unsigned long tx_delay; // delay between tx

// Flags
boolean BROADCAST = false;
boolean PROCESS_MSG = false;

void setup() {

  // Set random seed.
  initRandomSeed();

  // Set pin for 38khz carrier as output
  pinMode(TX_CLK_OUT, OUTPUT);

  // Debug led
  pinMode(13, OUTPUT);

  // Set message buffers to invalid (empty) state
  memset( tx_buf, 0, sizeof( tx_buf ));
  memset( rx_buf, 0, sizeof( rx_buf ));
  memset( rx_msg, 0, sizeof( rx_msg ));

  // Setup Timer 2 to help generate a 38khz signal.
  setupTimer2();

  // Begin I2C, note, as a slave device.
  Wire.begin( I2C_ADDR );
  Wire.onRequest( reportLastMessage );
  Wire.onReceive( newMessageToSend );

  // Start Serial.  Note, here we are using
  // serial to transmit strings over IR by
  // modulating it with the 38khz carrier.
  // The IR demodulator chip can support
  // baud up to 4800, but other variants 
  // might not.
  Serial.begin(4800);
  Serial.setTimeout(500);

  // Set initial time-stamp values
  ts = millis();
  msg_ttl = millis();

  tx_ts = millis();
  setTXDelay();
  //disableTX();
  resetRxBuf();
}


// Experiment to see if reading the LSB
// of an analog read can generate a 
// useful random seed.  Nothing connected
// to A0.  I think the LSB fluctuates 
// randomly(?)
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

// Over i2c to Master
// Sends a string of the latest message received from
// other robots.
// If the buffer is empty, a ! character is sent. 
// Note, a correct message should also end in !
void reportLastMessage() {

  // No message.
  if ( rx_msg[0] == 0 ) {
    Wire.write("!");
  } else {
    Wire.write( rx_msg, strlen(rx_msg) );
  }

}

// For IR.
// Function to format a message for our
// IR transmission.  We use:
// * to indicate the start of a message
// @ to indicate a checksum value following
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


// The arduino pro mini has a parallel serial
// interface,meaning with IR it can receive
// it's own tranmission. There, we access the 
// UART register to disable RX functionality.
// When we re-enable it, this has the beneficial
// side-effect of clearing the input buffer.
void disableRX() {
  UCSR0B &= ~( 1 << RXEN0 );
}

// Re-enable the serial port RX hardware.
void enableRX() {

  UCSR0A &= ~(1 << FE0);
  UCSR0A &= ~(1 << DOR0);
  UCSR0A &= ~(1 << UPE0);
  UCSR0B |= ( 1 << RXEN0 );

}

// We need to stop the 38khz carrier
// when no serial print is happening,
// otherwise we saturate the environment
// with a continuous 38khz signal
void disableTX() {
  // Set timer2 prescale to 0,0,0 (off)
  TCCR2B = 0;
}

void enableTX() {

  // Set prescale back to how it was
  TCCR2B |= (1 << CS20);
}

// Clears our received buffer
void resetRxBuf() {
  rx_count = 0;
  PROCESS_MSG = false;
  memset( rx_buf, 0, sizeof( rx_buf ) );
}

void loop() {


  // Toggle whether we are broadcasting
  // or listening for IR messages
  if ( millis() - tx_ts > tx_delay ) {
    BROADCAST = !BROADCAST;
    tx_ts = millis();

    // Set a different delay for next
    // iteration
    setTXDelay();
  }


  // If we are in broadcast mode and the LEDS
  // are active.
  if ( state == STATE_IR_TX_ON && BROADCAST) {

    // If we have not received a message from
    // the Master over i2c, do nothing.
    if ( tx_buf[0] == 0 ) {
      // don't send, empty buffer.
      
    } else { // Message from Master to send.

      // We have a problem where we pick up
      // our own reflected transmission.
      disableRX();
      //enableTX();

      // Using Serial.print transmits over
      // IR.  Serial TX is modulated with
      // the 38Khz carrier in hardware. 
      Serial.println( tx_buf );
      Serial.flush(); // wait for send to complete
      //disableTX();

      // With TX finished, we can receive again
      // and not get our own transmission.
      enableRX();

    }
  }


  // Clear old messages received via IR.
  if ( millis() - msg_ttl  > TTL ) {
    memset( rx_msg, 0, sizeof( rx_msg ) );
    msg_ttl = millis();
  }

  // Clear status LED
  if ( millis() - ts > 100 ) {
    ts = millis();
    digitalWrite( 13, LOW );

    // Debug
    //Serial.print("last message: ");
    //Serial.print( rx_msg );
    //Serial.print(" ");
    //Serial.println( ts );
  }


  // See if we have received over IR to
  // process. 
  while ( Serial.available() && !PROCESS_MSG ) {

    // If we find a newline, we flag we have
    // a message to attempt to process.
    rx_buf[rx_count] = Serial.read();
    if ( rx_buf[ rx_count ] == '\n' ) PROCESS_MSG = true;


    // Note that, it might seem like rx_count will
    // exceed the buffer. The next check should 
    // catch this, and cause the buffer to be 
    // processed.  We don't zero rx_count on each
    // call of loop() because we may only have a
    // few characters in the Serial buffer.  So we
    // need to incrementally fill our rx_buf with
    // many calls to this while loop.
    // Processing the message will reset rx_count
    // to 0.
    rx_count++;

    // If we exceed the buffer size, we also
    // will try to process the message.
    if ( rx_count >= MAX_MSG ) PROCESS_MSG = true;
  }

  // Do we think we have a message to process?
  if ( PROCESS_MSG ) {

    // Debug
    //Serial.print("got "); Serial.print(rx_buf); Serial.print(":"); Serial.println(rx_count);

    // Invalid conditions:
    if ( rx_count <= 0 || rx_buf[0] == 0 ) {
      // bad read.
      //Serial.println("bad serial");

     
    } else {  // Valid rx buf

      // Check for start byte
      int start = -1;  // -1 = default error state

      // Search string for * symbol (start)
      // Save index
      for ( int i = 0; i < rx_count; i++ ) {
        if ( rx_buf[i] == '*') {
          start = i;
          break;
        }
      }

      // Message too short or bad
      // e.g. a useless message would be:
      //      *abcdef@a  (start,message,CRC token, CRC value)
      //      minimum (absurd) message would be *@_
      //      min message length = 3
      // if start < 0, then no * symbol was found
      if ( (rx_count - start) <= 3 || start < 0 ) {
        //Serial.println("message too short :(");

      } else { // message valid, * index found

        // Check CRC, find @ token to separate
        // out CRC value.
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

              // Debug
              //Serial.println("CRC GOOD!");

              // Flash so we can see when robots are
              // receiving messages correctly
              digitalWrite(13, HIGH );


              // Make sure where we will store this
              // received message is clear.
              memset( rx_msg, 0, sizeof( rx_msg ));
              
              // Copy message across, ignoring the *
              for ( int i = 0; i < b - 1; i++ ) {
                rx_msg[i] = buf[i + 1];
              }

              // Add a ! to the message, which will be
              // used when later sending down I2C to 
              // the Master to indicate the end of the
              // string.
              rx_msg[b-1] = '!';

              // Reset TTL timestamp, so this message
              // will only exist temporarily.
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
} // end of loop
