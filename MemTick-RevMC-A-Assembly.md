# MemTick Rev MC-A: Assembly and Programming

Hand assembly, about 25 minutes per card. Everything is through-hole except U1 (SOIC-20) and Q1 (SOT-23).

## Tools

- Temperature-controlled soldering iron (fine conical or 1.6 mm chisel tip)
- Flux (pen or gel) and desoldering braid
- Tweezers, flush cutters, multimeter
- UPDI programmer (Arduino Pro Micro with microUPDI firmware, or a USB-serial adapter with a Schottky diode for SerialUPDI)

## Assembly order

Solder from the lowest parts to the tallest.

1. **U1 (ATtiny3216).** Pin 1 is marked by the triangle on the silkscreen; match it to the dot or bevel on the chip. Tack one corner pin, check that all 20 pins are aligned, then solder the rest. Clear any bridges with flux and braid.
2. **Q1 (AO3400A).** Tack one pad, check alignment, solder the other two.
3. **Resistors R1 to R4, R6** (1/4 W, no polarity). R1 = 10k, R2 = 100, R3 = 10k, R4 = 1k, R6 = 1k.
4. **D1 (1N4001).** The band on the diode goes to the **square pad** (marked K).
5. **D2 (LED, optional).** The short leg (flat side of the case) goes to the **square pad**.
6. **C1 (100 nF) and C2 (10 uF).** Ceramic, no polarity.
7. **C3 (100 uF electrolytic). READ THIS:** on Rev MC-A the silkscreen polarity is reversed. The **square pad is GND**. Fit C3 with the **stripe (negative lead) to the square pad** and the long (positive) lead to the round pad. Fitting it to the silkscreen marking will reverse-bias it on 12 V. Fixed in Rev B.
8. **R5 (68 ohm, 2 W).** Mount it 2 to 3 mm above the board; it gets warm.
9. **SW1, SW2, SW3 (DIP switches).** Check that each one sits flat before soldering all 8 pins.
10. **J2 (4-pin) and J3 (3-pin) headers.**
11. **Speaker.** Screw it to the board with M2 hardware (low head on the back of the card, nut on the speaker side). Solder the **red lead to the square LS1 pad** (SPK+) and the **black lead to the round pad**. Coil and tie the extra wire away from R5 and the cone.
12. Clean flux residue, then inspect every joint.

## First power-up checks (before programming)

With a bench supply, or with the card in a PC that is switched off:

- Resistance between +12V and GND, and between +3.3V and GND, should not read as a short.
- Gate of Q1 to GND should read about 10k (R1).

## Programming (UPDI)

1. Install megaTinyCore in the Arduino IDE.
2. Select **Tools > Board > ATtiny3216** (megaTinyCore), **Clock: 10 MHz internal**, and your programmer.
3. Connect the programmer to **J3**: pin 1 (square) = UPDI, pin 2 = GND, pin 3 = VCC.
4. **Program the card out of the PC.** Never connect the programmer's VCC while the card is in a powered PC.
5. Open `MemTick.ino` and click **Upload Using Programmer**.

After programming, the card plays the memory count and beep as soon as it powers up on the bench, because R3 holds PERST# high.

## Switch settings

| Switch | Function |
|---|---|
| SW1 TRBO | ON = Turbo (about 2x tick speed) |
| SW1 LOUD | ON = loud, OFF = soft |
| SW1 COLD | ON = count only on the first boot after power-on; OFF = count on every reboot |
| SW1 PROF | ON = count starts slow and speeds up; OFF = steady rate |
| SW2 128 / 64 / 32 / 16 | Memory size, high bits (GB) |
| SW3 8 / 4 / 2 / 1 | Memory size, low bits (GB) |

Set SW2 and SW3 to the installed RAM in GB (for example 32 GB = SW2 "32" ON, everything else OFF). The card plays 16 ticks per GB.

## Case speaker

J2 matches a standard 4-pin PC speaker cable: pin 1 = SPK+, pin 4 = speaker return. The on-board speaker and J2 are in parallel.
