#include "Arduino.h"
#include <Audio.h>
#include <FS.h>
#include <SD_MMC.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const int SD_MMC_CMD = 15;
const int SD_MMC_CLK = 14;
const int SD_MMC_D0 = 2;
const int I2S_BCLK = 26;
const int I2S_DOUT = 33;
const int I2S_LRC = 25;
const int PIN_BUZZER = 18;
const int CHN = 0;
const int SDA = 21;
const int SCL = 22;

LiquidCrystal_I2C lcd(0x27,16,2);

enum GameState {
  SHOWING_SEQUENCE,
  WAITING_FOR_PRESS,
  DECIDE_ROUND,
	ADVANCE,
  GAME_OVER,
};

GameState state = SHOWING_SEQUENCE;

unsigned long FLASH_DURATION = 500;
unsigned long GAP_DURATION = 200;

int score = 0;
int ledsFlashed = 0;

Led* pressedLed = nullptr;

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
		bool lit;
		unsigned long onSince;
		unsigned long onDuration;

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

		bool update() {
			if (lit && (millis() - onSince) >= onDuration) {
				turnOff();
				ledcWriteTone(PIN_BUZZER, 0);
				lit = false;
				return true;
			}
			return false;
		}

		void activateLed(unsigned long duration) {
			ledcWriteTone(PIN_BUZZER, buzzerFrequency);
			ledcWrite(PIN_BUZZER, 20);
			turnOn();
			lit = true;
			onSince = millis();
			onDuration = duration;
		}

		bool isLit() {
			return lit;
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
		int lastStableState;
		unsigned long lastDebounceTime;
		unsigned long debounceDelay;

  public:
    Button(int buttonPin) {
      pin = buttonPin;
      lastState = 1;
			lastStableState = 1;
			lastDebounceTime = 0;
			debounceDelay = 25;
    }

    void begin() {
      pinMode(pin, INPUT_PULLUP);
    }

    int currentState() {
      return digitalRead(pin);
    }

    void update() {
      int reading = currentState();
			if (reading != lastState) {
				lastDebounceTime = millis();
			}
			if ((millis() - lastDebounceTime) > debounceDelay) {
				lastStableState = reading;
			}
			lastState = reading;
    }

    bool isPressed() {
			static int prevStable = 1;
			bool pressed = (lastStableState == 0 && prevStable == 1);
			prevStable = lastStableState;
			return pressed;
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

  Wire.begin(SDA, SCL);
  if (!i2CAddrTest(0x27)) {
  lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Simon says");

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

  lcd.setCursor(0,1);
  lcd.print("Score:");
  lcd.print(score);

	redButton.update();
	greenButton.update();
	blueButton.update();
	yellowButton.update();

  redLed.update();
  greenLed.update();
  blueLed.update();
  yellowLed.update();

	switch (state) {

		case SHOWING_SEQUENCE: {
			bool sequenceUpdated = correctSequenceLength != score;
			static bool ledActive = false;
			static bool inGap = false;
			static unsigned long gapStart = 0;

			if (!sequenceUpdated) {
				updateCorrectSequence();
			}

			if (inGap) {
				if (millis() - gapStart >= GAP_DURATION) {
					inGap = false;
				}
			} else if (!ledActive) {
				correctSequence[ledsFlashed]->activateLed(FLASH_DURATION);
				ledActive = true;
			} else if (!correctSequence[ledsFlashed]->isLit()) {
				ledsFlashed++;
				ledActive = false;

				if (ledsFlashed == correctSequenceLength) {
					ledsFlashed = 0;
					state = WAITING_FOR_PRESS;
				} else {
					inGap = true;
					gapStart = millis();
				}
			}
			break;
		}

    case WAITING_FOR_PRESS: {
			pressedLed = nullptr;
      if (redButton.isPressed())         pressedLed = &redLed;
      else if (greenButton.isPressed())  pressedLed = &greenLed;
      else if (blueButton.isPressed())   pressedLed = &blueLed;
      else if (yellowButton.isPressed()) pressedLed = &yellowLed;

      if (pressedLed) {
        updateUserSequence(pressedLed);
        pressedLed->activateLed(500);
        state = DECIDE_ROUND;
      }
      break;
    }

    case DECIDE_ROUND: {
      if (pressedLed->isLit()) {
				break;
      }

			if (!sequencesMatch()) {
        state = GAME_OVER;
			} else if (userSequenceLength == correctSequenceLength) {
				state = ADVANCE;
			} else {
				state = WAITING_FOR_PRESS;
			}
      break;
    }

		case ADVANCE: {
			playResultAudio(true);
			score++;
			userSequenceLength = 0;
			if (!audio.isRunning()) {
        state = SHOWING_SEQUENCE;
      }
			break;
		}

		case GAME_OVER: {
			playResultAudio(false);
    	while (true) {}
    }
  }
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

void playResultAudio(bool success) {
  if (success) {
    int randomIndex = random(successAmount);
    audio.connecttoFS(SD_MMC, successAudio[randomIndex]);
		while (audio.isRunning()) {
			audio.loop();
		}
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
  }
}

bool i2CAddrTest(uint8_t addr) {
Wire.begin();
Wire.beginTransmission(addr);
if (Wire.endTransmission() == 0) {
return true;
}
return false;
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