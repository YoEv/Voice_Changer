# Voice-Changer and Effects Processor for Real-Time Audio Manipulation

This repository contains the implementation of a **Voice-Changer and Effects Processor** developed as part of my thesis, *Development and Implementation of a Voice-Changer and Effects Processor for Real-Time Audio Manipulation*. The system leverages advanced Digital Signal Processing (DSP) techniques to transform live audio input in real-time.

## Thesis Link
For detailed insights into the development and design, you can read the full thesis here:  
[Thesis PDF](https://03f1e01a-c69a-4db6-ac02-775b26f3a83a.filesusr.com/ugd/87f27a_20e8783319144c2bb409478890312b07.pdf)

---

## Features
- **Real-Time Audio Effects**: Supports multiple effects such as robotic voice, whisper, pitch shift (e.g., little girl, demon), echo, and alien voice.
- **User Interaction**: Integrates touch-sensitive controls via the Trill Bar and MIDI for dynamic parameter adjustment.
- **Efficient Design**: Built on the Bela platform for sub-millisecond latency and high-quality sound processing.
- **Versatility**: Applies to vocals, instruments, and synthesized audio inputs.

---

## How to Use

### Prerequisites
- **Hardware**: Bela platform, microphone, MIDI controller, and compatible audio interface.
- **Software**: Bela IDE and necessary libraries (e.g., NEON FFT for DSP computations).

### Steps
1. Clone the repository to your Bela environment.
2. Connect your input and output devices (e.g., microphone, speakers, MIDI controller) to the Bela platform.
3. Upload and build the code using the Bela IDE.
4. Start the system and select your desired audio effect via:
   - **MIDI Controller**: Adjust parameters and select effects dynamically.
   - **Trill TouchBar**: Modify effect parameters like pitch and amplitude through touch input.

### Demo
A live demonstration of the Voice-Changer system is available here:  
[Voice-Changer Demo](https://evelyneehevler.wixsite.com/evelyneehevler/projects-6-1)

---

## System Highlights
- **Core Technology**: 
  - **FFT-Based Processing**: Efficient spectral transformations for high-fidelity audio effects.
  - **Real-Time Buffering**: Overlap-add technique ensures smooth transitions during playback.
- **Supported Effects**:
  - Robotic Voice
  - Whisper
  - Little Girl Voice
  - Demon Voice
  - Echo
  - Alien Voice
- **Applications**: Suitable for live performances, sound design, and creative audio experimentation.

---

## Future Directions
This system demonstrates significant advancements in real-time audio manipulation. Future research may include:
- Adaptive AI-based effects for enhanced interactivity.
- Expanding compatibility to additional hardware and software platforms.

---

For additional questions or collaboration opportunities, please feel free to reach out.

**Author**: Xiaosha Li (Evelyne Ehevler)  
**Contact**: [xli5@berklee.edu](mailto:xli5@berklee.edu)
