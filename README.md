# 3Pi_IRCommBoard (work in progress)
A pcb design to create infra-red (IR) communication between Pololu 3Pi+ robots. The design utilises an Arduino Pro Mini as an I2C Slave device.  The pro mini handles all transmit and receive operations over IR. The Master device (inteded as the 3Pi+, but could be any device with I2C) can upload a string to transmit via I2C, and also request the latest string received over I2c, if any.

IR communication is implemented using the standard Serial functionality common to Arduino (e.g. `Serial.print()`).  The detection (receiving) circuit uses a Demodulator chip that is tuned for a carrier frequency of 38.4Khz (more details below).  Therefore, the pro mini generates a 38Khz signal via an internal timer interrupt to provide the carrier, output on digital pin 4.  The 38Khz carrier signal is combined electronically with the TX pin state through an OR gate (74xx32).  The logical (OR) of this operation is defined by the gates sinking the current through the tranmitting IR LEDs.  Therefore, the LEDs are not driven by the 74xx32, and are considered active low.  A nice unintended side effect of this is that when TX is inactive (LOW, not transmitting), the 74xx32 output is HIGH which means the IR LEDs are off and not saturating the environment - even though the 38Khz signal generation continues. 

Some messages transmitted over IR do get garbled.  As a quick solution, the firmware uses a CRC checksum.  I've used the same implementation as detailed on the RepRap wiki for GCode (<a href="https://reprap.org/wiki/G-code#Checking">here</a>).  Prior to transmitting, the pro mini generates a single byte checksum value, and appends this to the message string.  Therefore, when a message is received, the receiving pro mini completes the same CRC check on the received message for itself, and then compares if the checksum values match.  If not, the message is discarded.  

The message strings transmitted between the pro mini devices would look something like:

`*message string@f`

Here, the `*` is used to identify the start of a message, the `@` is used to identify the end of the message and that the next byte is the checksum value.  In the example above, I haven't used the correct checksum value, it is just an illustrative example.  The transmission is also made with a `\n` character at the end, but this is discarded.  





Software is still very sketchy early work that has passed proof-of-concept.

## Important Considerations

- **IR Demodulator:** I tried a few IR demodulator chips and they are not all created equally.  In particular, those designed to receive bursts of transmission appear poorly suited.  When using ones for burst transmission, I observed that I had to wave the transmitter or receiver around to get a successful transmission (perhaps some inner circuitry reaches a saturation?).  Instead it is better to use those designed for continuous transmission. The Vishay **TSDP34138** (<a href="https://www.farnell.com/datasheets/2245004.pdf">datasheet</a>) seems to fit this and works well / reliably.  The datasheet specifies upto 4800 baud for a 38.4Khz carrier frequency.  If you want to reproduce this board, I recommend you use the same device, or be ready for some trials of different devices.
- **IR Demodulator Placement:** The IR Demodulator (IRD) is placed central on the PCB and facing upwards - this probably isn't the manufacturers recommendation for optimal performance.  I did consider placing 5 IRD's around the circumference of the PCB facing outward between the IR LEDs.  However, the IRD is one of the more expensive components, and so producing 100 of these PCBs would be significantly more expensive with 5 IRD per board.  I have tested this PCB with 1 upward IRD and it can reliably detect IR transmission from neighbours at roughly 30cm distance.  For communication between Pololu 3Pi+ robots, this is relatively "local" and sufficient.  However, it does mean a robot cannot identify from which direction a transmission was received.  In theory, I think a more advanced version of this board could supply VCC to 5 independent IRD via the arduino digital pins, and therefore poll the IRD around the circumference of the board by enabling each in turn, and so infer directionality.  But I haven't tried this. 
- **Programming the Pro Mini:** It is important to note that the programming of the Pro Mini will most often fail if the IR Demodulator is electrically connected.  Therefore, the circuit design has a jumper (labelled, "RX Break") intended for a connecting removable sleeve, to allow the connection to be broken for programming.  Importantly, no IR messages can be received if this jumper is not closed with the sleeve.  
- **I2C requests from Master:** so far, it seems that the Master device has to request a specific number of bytes from the Slave device.  Because we don't necessarily know this before hand (and to take a short cut), the firmware has been written to append a `!` character to the end of the message string.  Therefore, the Master requests the maximum data length, and then parses for the `!` character.  After this, the data is gibberish.  In the future, it might be better to have a two step operation where the Master first asks how large the data to transfer is.  

## Bill of Materials (BOM)


