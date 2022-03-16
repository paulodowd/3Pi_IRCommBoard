# 3Pi_IRCommBoard
A pcb design to create infra-red communication between Pololu 3Pi+ robots. 

Software is still very sketchy early work that has passed proof-of-concept.

## Important Considerations

- **IR Demodulator:** I tried a few IR demodulator chips and they are not all created equally.  In particular, those designed to receive bursts of transmission appear poorly suited.  When using ones for burst transmission, I observed that I had to wave the transmitter or receiver around to get a successful transmission (perhaps some inner circuitry reaches a saturation?).  Instead it is better to use those designed for continuous transmission. The Vishay **TSDP34138** (<a href="https://www.farnell.com/datasheets/2245004.pdf">datasheet</a>) seems to fit this and works well / reliably.  The datasheet specifies upto 4800 baud for a 38.4Khz carrier frequency.  If you want to reproduce this board, I recommend you use the same device, or be ready for some trials of different devices.
- **IR Demodulator Placement:** The IR Demodulator (IRD) is placed central on the PCB and facing upwards.  I did consider placing 5 IRD's around the circumference of the PCB between the IR LEDs.  However, the IRD is one of the more expensive components, and so producing 100 of these PCBs would be significantly more expensive with 5 IRD per board.  I have tested this PCB with 1 upward IRD and it can reliably detect IR transmission from neighbours at roughly 30cm.  For communication between Pololu 3Pi+ robots, this is relatively "local" and sufficient.  However, it does mean a robot cannot identify from which direction a transmission was received.  In theory, I think a more advanced version of this board could supply VCC to 5 independent IRD via the arduino digital pins, and therefore poll the IRD around the circumference of the board by enabling each in turn, and so infer directionality.  But I haven't tried this. 

## Bill of Materials (BOM)


