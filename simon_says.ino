#include "Arduino.h"
#include <Audio.h>
#include <FS.h>
#include <SD_MMC.h>

const int SD_MMC_CMD = 15;
const int SD_MMC_CLK = 14;
const int SD_MMC_D0 = 2;
const int I2S_BCLK = 26;
const int I2S_DOUT = 33;
const int I2S_LRC = 25;
const int PIN_BUZZER = 18;
const int CHN = 0;

const char* successAudio[3] = {
	"/audio/success/success.mp3",
	"/audio/success/success 2.mp3",
	"/audio/success/wow.mp3"
};
int successAmount = sizeof(successAudio) / sizeof(successAudio[0]);

const char* failAudio[2] = {
	"/audio/fail/fail.mp3",
	"/audio/fail/fail trumpet.mp3"
};
int failAmount = sizeof(failAudio) / sizeof(failAudio[0]);

const char* failVoices[4] = {
	"/audio/fail/better luck next time.mp3",
	"/audio/fail/not even trying.mp3",
	"/audio/fail/worse than grandma.mp3",
	"/audio/fail/you lost.mp3"
};
int failVoicesAmount = sizeof(failVoices) / sizeof(failVoices[0]);

class Led {
  private:
    int pin;

  public:
    int buzzerFrequency;

    Led(int ledPin, int frequency) {
      pin = ledPin;
      buzzerFrequency = frequency;
    }

    void begin() {
      pinMode(pin, OUTPUT);
    }

    void turnOn() {
      digitalWrite(pin, HIGH);
    }

    void turnOff() {
      digitalWrite(pin, LOW);
    }
};

Led redLed(4, 250);
Led greenLed(5, 300);
Led blueLed(12, 350);
Led yellowLed(13, 400);

Led* leds[4] = {&redLed, &greenLed, &blueLed, &yellowLed};
int ledAmount = sizeof(leds) / sizeof(leds[0]);

const int maxLength = 50;
Led* correctSequence[maxLength];
int correctSequenceLength = 0;

Led* userSequence[maxLength];
int userSequenceLength = 0;

class Button {
  private:
    int pin;
    int lastState;

  public:
    Button(int buttonPin) {
      pin = buttonPin;
      lastState = 1;
    }

    void begin() {
      pinMode(pin, INPUT_PULLUP);
    }

    int currentState() {
      return digitalRead(pin);
    }

    void syncState() {
      lastState = currentState();
    }

    bool isPressed() {
      return (currentState() == 0) && (lastState == 1);
    }
};

Button redButton(19);
Button greenButton(23);
Button blueButton(27);
Button yellowButton(32);

Audio audio;

void setup() {
  Serial.begin(115200);

  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

  pinMode(PIN_BUZZER, OUTPUT);
  ledcAttachChannel(PIN_BUZZER, 400, 10, CHN);
  ledcWriteTone(PIN_BUZZER, 0);

  redLed.begin();
  greenLed.begin();
  blueLed.begin();
  yellowLed.begin();

  redButton.begin();
  greenButton.begin();
  blueButton.begin();
  yellowButton.begin();

  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }

  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  audio.setVolume(16); // 0...21
  audio.connecttoFS(SD_MMC, successAudio[2]);
}

void loop() {
  audio.loop();
  if (Serial.available()) { // put streamURL in serial monitor
    audio.stopSong();
    String r = Serial.readString();
    r.trim();
    if (r.length() > 5) audio.connecttoFS(SD_MMC, r.c_str());
    log_i("free heap=%i", ESP.getFreeHeap());
  }

	updateCorrectSequence();

	for (int i = 0; i < correctSequenceLength; i++) {
		lightLed(correctSequence[i]);
	}

	while (sequencesMatch() && userSequenceLength < correctSequenceLength) {
		Led* pressed = nullptr;
		if (redButton.isPressed())    pressed = &redLed;
		else if (greenButton.isPressed())  pressed = &greenLed;
		else if (blueButton.isPressed())   pressed = &blueLed;
		else if (yellowButton.isPressed()) pressed = &yellowLed;

  	if (pressed) {
			lightLed(pressed);
			updateUserSequence(pressed);
  	}

		redButton.syncState();
		blueButton.syncState();
		greenButton.syncState();
		yellowButton.syncState();
	}

	if (sequencesMatch()) {
		int randomIndex = random(successAmount);
		audio.connecttoFS(SD_MMC, successAudio[randomIndex]);
		while (audio.isRunning()) {
			audio.loop();
		}
		userSequenceLength = 0;
	} else {
		int randomIndex[2] = {random(failAmount), random(failVoicesAmount)};
		audio.connecttoFS(SD_MMC, failAudio[randomIndex[0]]);
		while (audio.isRunning()) {
			audio.loop();
		}
		delay(500);
		audio.connecttoFS(SD_MMC, failVoices[randomIndex[1]]);
		while (audio.isRunning()) {
			audio.loop();
		}
		while (true) {}
	}
}

void lightLed(Led* led) {
  ledcWriteTone(PIN_BUZZER, led->buzzerFrequency);
  ledcWrite(PIN_BUZZER, 20);
  led->turnOn();
  delay(500);
  ledcWriteTone(PIN_BUZZER, 0);
  led->turnOff();
}

void updateCorrectSequence() {
	int randomIndex = random(ledAmount);
	correctSequence[correctSequenceLength] = leds[randomIndex];
	correctSequenceLength++;
}

void updateUserSequence(Led* led) {
	userSequence[userSequenceLength] = led;
	userSequenceLength++;
}

bool sequencesMatch() {
  if (userSequenceLength == 0) {
		return true;
	};
  for (int i = 0; i < userSequenceLength; i++) {
    if (userSequence[i] != correctSequence[i]) return false;
  }
  return true;
}

void audio_info(const char *info) {
  Serial.print("info ");
  Serial.println(info);
}

void audio_id3data(const char *info) { //id3 metadata
  Serial.print("id3data ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) { //end of file
  Serial.print("eof_mp3 ");
  Serial.println(info);
}
void audio_showstation(const char *info) {
  Serial.print("station ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle ");
  Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate ");
  Serial.println(info);
}
void audio_commercial(const char *info) { //duration in sec
  Serial.print("commercial ");
  Serial.println(info);
}
void audio_icyurl(const char *info) { //homepage
  Serial.print("icyurl ");
  Serial.println(info);
}
void audio_lasthost(const char *info) { //stream URL played
  Serial.print("lasthost ");
  Serial.println(info);
}

void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);
void audio_bitrate(const char *info);
void audio_commercial(const char *info);
void audio_icyurl(const char *info);
void audio_lasthost(const char *info);