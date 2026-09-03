#include "SoundManager.h"

void SoundManager::begin() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void SoundManager::playClick() {
  if (isMuted) return;
  tone(BUZZER_PIN, 1000, 50);
}

void SoundManager::playHappyTone() {
  if (isMuted) return;
  tone(BUZZER_PIN, 1500, 100);
  delay(100);
  tone(BUZZER_PIN, 2000, 150);
}

void SoundManager::playAlarm() {
  if (isMuted) return;
  tone(BUZZER_PIN, 500, 150);
  delay(200);
  tone(BUZZER_PIN, 500, 150);
}

void SoundManager::playSadTone() {
  if (isMuted) return;
  tone(BUZZER_PIN, 400, 300);
  delay(300);
  tone(BUZZER_PIN, 300, 300);
  delay(300);
  tone(BUZZER_PIN, 200, 600);
}