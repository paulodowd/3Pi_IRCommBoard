# 3Pi_IRCommBoard (work in progress)

<p align="center">
<img width="80%" src="https://github.com/paulodowd/3Pi_IRCommBoard/blob/main/images/prototype_on_robots.jpg?raw=true">
  </p>

## Features

- Can be considered a general purpose I2C extension board to provide local infra-red (IR) communication.
- IR Serial transmission at 4800 Baud.
- Transmit/receive upto 28 bytes (currently, extensible).
- Range can be reduced/set with a trimmer potentiometer.
- Message corruption detection implemented with a CRC checksum.
- Designed to conveniently fit to an expansion header of the Pololu 3Pi+ mobile robot. 

## Summary

A pcb design to create infra-red (IR) communication between Pololu 3Pi+ robots. The design utilises an Arduino Pro Mini as an I2C Slave device.  The pro mini handles all transmit and receive operations over IR. The Master device (inteded as the 3Pi+, but could be any device with I2C) can upload a string to transmit, and also request the latest string received, if any.  This can get a little confusing to discuss: I2C is used to transfer messages between Master and Slave, and Serial/IR is used to tranmit messages between Slave devices.  

IR communication is implemented using the standard Serial functionality common to Arduino (e.g. `Serial.print()`).  The detection (receiving) circuit uses a Demodulator chip that is tuned for a carrier frequency of 38.4Khz (more details below).  Therefore, the pro mini generates a 38Khz signal via an internal timer interrupt to provide the carrier, output on digital pin 4.  

The 38Khz carrier signal is combined electronically with the TX pin state through an OR gate (74xx32). The logical (OR) of this operation is defined by the gates sinking the current through the tranmitting IR LEDs.  Therefore, the LEDs are not driven by the 74xx32, and are considered active low.  A nice unintended side effect of this is that when TX is inactive (LOW, not transmitting), the 74xx32 output is HIGH despite the continued 38Khz signal generation - which means the IR LEDs are off and not saturating the environment.

In this circuit design, a single gate is used to sink current.  It might be important to note that the <a href="https://www.ti.com/lit/ds/symlink/sn54hc32-sp.pdf">datasheet</a> for the logic gates specifies a max current sink of 50ma through ground - and here we are sinking 250ma.    So far, I've not seen any significant changes to performance and this may be because I am not sinking current continuously. In a subsequent design (available <a href="https://github.com/paulodowd/Swarm-Bs">here</a>), all 4 gates are ganged, but the internal ground connection may still be a limitation. In either case, I haven't yet seen any problems or difference in performance.  

Some messages transmitted over IR do get garbled.  As a quick solution, the firmware uses a CRC checksum.  I've used the same implementation as detailed on the RepRap wiki for GCode (<a href="https://reprap.org/wiki/G-code#Checking">here</a>).  Prior to transmitting, the pro mini generates a single byte checksum value, and appends this to the message string.  Therefore, when a message is received, the receiving pro mini completes the same CRC check on the received message for itself, and then compares if the checksum values match.  If not, some data must be corrupt and the message is discarded.  

The message strings transmitted between the pro mini devices would look something like:

`*message string@f`

Here, the `*` is used to identify the start of a message, the `@` is used to identify the end of the message and that the next byte is the checksum value.  In the example above, I haven't used the correct checksum value, it is just an illustrative example.  The transmission is also made with a `\n` character at the end, but this is discarded.  

At the moment, the pro mini firmware will only store 1 message, the latest message received.  Furthermore, it is programmed to erase this message after 1 second.  This suits the intended use of the board, but will likely need updating for other uses.  

When the Master device requests the latest message from the pro mini Slave device, it will receive a string terminated with a `!` character.  If the Master receives:

`!` 

This means that no messages are available.  Otherwise, the Master can parse the received bytes over I2C and use the `!` to identify the end of the string (more details below).  

If the Master is to set a string for the pro mini Slave to transmit over IR, this should be sent over I2C with no terminating character (e.g., no newline, or anything specific, just the string you wish to transmit).  Here, it is unknown (not tested!) what will happen if your string has an `*`, `\n` or `!` in it - it will probably break functionality!  Whilst this is inconsistent from the format used for messages between the pro mini Slave devices, it presents the simplest usage/interface when working with the Master device - all the tokens and CRC are handled for the user.

Software is still sketchy early work for proof-of-concept.

## Important Considerations

- **IR Demodulator:** I tried a few IR demodulator chips and they are not all created equally.  In particular, those designed to receive bursts of transmission appear poorly suited.  When using ones for burst transmission, I observed that I had to wave the transmitter or receiver around to get a successful transmission (perhaps some inner circuitry reaches a saturation?).  Instead it is better to use those designed for continuous transmission. The Vishay **TSDP34138** (<a href="https://www.farnell.com/datasheets/2245004.pdf">datasheet</a>) seems to fit this and works well / reliably.  The datasheet specifies upto 4800 baud for a 38.4Khz carrier frequency.  If you want to reproduce this board, I recommend you use the same device, or be ready for some trials of different devices.
- **IR Demodulator Placement:** The IR Demodulator (IRD) is placed central on the PCB and facing upwards - this probably isn't the manufacturers recommendation for optimal performance.  I did consider placing 5 IRD's around the circumference of the PCB facing outward between the IR LEDs.  However, the IRD is one of the more expensive components, and so producing 100 of these PCBs would be significantly more expensive with 5 IRD per board.  I have tested this PCB with 1 upward IRD and it can reliably detect IR transmission from neighbours at roughly 30cm distance.  For communication between Pololu 3Pi+ robots, this is relatively "local" and sufficient.  However, it does mean a robot cannot identify from which direction a transmission was received.  In theory, I think a more advanced version of this board could supply VCC to 5 independent IRD via the arduino digital pins, and therefore poll the IRD around the circumference of the board by enabling each in turn, and so infer directionality.  But I haven't tried this. 
- **Self-Receiving:** I did have an issue of a single board receiving it's own IR transmissions, and not being able to receive transmission from neighbours.  I believe this is because the UART implementation (on the Atmega328 at least) does a parallel send and receive operation.  When using IR light, this means it will detect it's own transmission. After attempting a lot of different solutions, it was as simple as disabling the RX functionality of the UART by a register operation.  I'm glad this was available!  You can review the details of this on page 171 of the Atmega328 datasheet (<a href="https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf">here</a>).  You can find this operation in the firmware about <a href="https://github.com/paulodowd/3Pi_IRCommBoard/blob/922e04242b78fc39a44c0d518355c1a5028df249/arduino/pro_mini_firmware/pro_mini_firmware.ino#L243">here</a>.
- **Programming the Pro Mini:** It is important to note that the programming of the Pro Mini will most often fail if the IR Demodulator is electronically connected.  Therefore, the circuit design has a jumper (labelled, "RX Break") intended for a connecting removable sleeve, to allow the connection to be broken for programming.  Importantly, no IR messages can be received if this jumper is not closed with the sleeve.  
- **I2C requests from Master:** so far, it seems that the Master device has to request a specific number of bytes from the Slave device.  Because we don't necessarily know this before hand (and to take a short cut), the firmware has been written to append a `!` character to the end of the message string.  Therefore, the Master requests the maximum data length, and then parses for the `!` character.  After this, the data is gibberish.  In the future, it might be better to have a two step operation where the Master first asks how large the data to transfer is.  
- **Buffer Sizes:** The current buffers for sending and receiving IR messages in the firmware are set to 32 bytes (`MAX_MSG = 32`).  The IR transmission is operated by the Arduino Serial functionality.  By default, the Arduino environment sets the Serial buffer to 64 bytes.  Here, I decided to keep the Serial buffer under-utilised (half-full).  My un-tested rationale was to stand a chance of successfully parsing a whole 32-byte message within a 64-byte buffer.  I believe it is possible to expand the default Serial buffer beyond 64 bytes.  These aspects are open for investigation.  But for now, it is recommended to send a string of max length 28 bytes (leaving 4 bytes free for `*`, `@` and the checksum value).  The current buffer implementation for the IR messages is not very clever, e.g. not circular or anything like that.  I haven't validated the reliability of the transmission when the maximum buffer is used. 
- **Buffer Sizes:** The current buffers for sending and receiving IR messages in the firmware are set to 32 bytes (`MAX_MSG = 32`).  The IR transmission is operated by the Arduino Serial functionality.  By default, the Arduino environment sets the Serial buffer to 64 bytes.  Here, I decided to keep the Serial buffer under-utilised (half-full).  My un-tested rationale was to stand a chance of successfully parsing a whole 32-byte message within a 64-byte buffer.  I believe it is possible to expand the default Serial buffer beyond 64 bytes.  These aspects are open for investigation.  But for now, it is recommended to send a string of max length 28 bytes (leaving 3 bytes free for `*`, `@`, the checksum value and `\n`).  The current buffer implementation for the IR messages is not very clever, e.g. not circular or anything like that.  I haven't validated the reliability of the transmission when the maximum buffer is used. 
- **IR Message Buffers:** to buffer messages being sent and received, there are 3 buffers in total. 1) a buffer to hold the message to transmit as set by the Master 2) a buffer to process a received message 3) a buffer to hold the last correctly validated message to relay to the Master.  Therefore, buffer 2 could be considered volatile - hence the need for 3 buffers.  

## Bill of Materials (BOM)


