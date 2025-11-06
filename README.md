Project Overview: Omi Glass Dev Board Audio-Photo Logger

This project is designed to test and evaluate the Omi Glass Development Kit, which is based on the Seeed Studio XIAO ESP32S3 Sense module. The goal is to explore the board’s onboard camera and microphone capabilities by automating data capture and verifying system performance under real-world conditions.

The program periodically captures images every 10 seconds using the onboard camera, while continuously monitoring the microphone for sound activity. When a sound is detected above a set threshold, the system records audio snippets for later analysis.

This setup helps validate:

Camera initialization and image capture stability

Microphone sensitivity and sound-triggered recording functionality

Timing and synchronization between camera and audio tasks

Power performance during continuous sensor operation

Ultimately, the project provides a controlled testing environment to assess and optimize the Omi Glass firmware for applications involving context-aware capture, audio-visual sensing, and edge AI data collection.


Key Features

Automatic image capture every 10 seconds

Sound-triggered audio recording via onboard microphone

Configurable thresholds for sound detection

Simple modular firmware structure for future AI or sensor integrations

Designed for use with Omi Glass (ESP32S3 Sense) hardware


Technical Notes

Firmware written in Arduino C++ for the ESP32S3 platform

Utilizes the ESP-IDF and Seeed Studio camera/microphone drivers

Can be expanded to include image processing, ML inference, or network transmission
