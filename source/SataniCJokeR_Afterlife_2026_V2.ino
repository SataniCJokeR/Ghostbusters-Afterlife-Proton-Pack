

/****************************************************************************************
   SataniC JokeR´s Ghostbusters Afterlife 2026
   Arduino Nano 

   Hardware:
     D2  = 55 WS2812/NeoPixel LEDs: Cyclotron 0..39, PowerCell 40..54
     D3  = 17 WS2812/NeoPixel LEDs: Vent 0..3, SloBlo 4, WandVent 5, Theme 6,
                                    White1 7, White2 8, OrangeHat 9, Jewel 10..16
     D4  = Fire Button 1 (active LOW)
     D5  = Fire Button 2 (active LOW)
     D6  = Wand switch (HIGH = ON, same electrical behavior as before)
     D7  = Pack switch (HIGH = ON, same electrical behavior as before)
     D8  = Music Mode switch (HIGH = ON, same electrical behavior as before)
     D9  = Venting relay output
     D10 = SoftwareSerial RX  <- DFPlayer TX
     D11 = SoftwareSerial TX  -> DFPlayer RX
     D12 = DFPlayer BUSY (LOW = playing, HIGH = finished/idle)
     D13 = Rumble
     A1  = 8 WS2812 status LEDs

   Audio is kept strictly separated !!!!!!:
     Pack sounds: WAV files only in the /mp3 folder
                  /mp3/0001.wav ... /mp3/0032.wav
     Music:       MP3 files only in the /01 folder
                  /01/001.mp3, /01/002.mp3, ...

   IMPORTANT NOTES ABOUT WAV / MP3:
     - Pack sounds are WAV files in this project.
     - Music tracks are MP3 files in this project.
     - The DFPlayer addresses files by folder and track number, NOT by file
       extension. Therefore the library command playFromMP3Folder() is still
       used for pack sounds. It refers to the special /mp3 folder and does not
       define the actual audio file format.
     - The /mp3 folder contains the original WAV files 0001.wav through
       0021.wav plus the crossfade files 0022.wav through 0032.wav.
     - The /01 folder should contain only music files such as 001.mp3,
       002.mp3, and so on.
     - PCM, 16-bit, 44.1 kHz is recommended for the pack WAV files.
       Remove metadata/tags where possible.

   Logical pack sound IDs -> actual WAV file:
     000 Power On             -> /mp3/0001.wav
     001 Boot                 -> /mp3/0002.wav
     002 Pack Idle            -> /mp3/0003.wav
     003 Ramp 1 On            -> /mp3/0004.wav
     004 Ramp 1 Idle          -> /mp3/0005.wav
     005 Ramp 1 Down          -> /mp3/0006.wav
     006 Ramp 2 On            -> /mp3/0007.wav
     007 Ramp 2 Idle          -> /mp3/0008.wav
     008 Ramp 2 Down          -> /mp3/0009.wav
     009 Fire                 -> /mp3/0010.wav
     010 Fire Loop            -> /mp3/0011.wav
     011 Fire Tail            -> /mp3/0012.wav
     012 Pack Alarm           -> /mp3/0013.wav
     013 Vent                 -> /mp3/0014.wav
     014 Off                  -> /mp3/0015.wav
     015 Boot Up After Vent   -> /mp3/0016.wav
     016 Error                -> /mp3/0017.wav
     017 Error Off            -> /mp3/0018.wav
     018 Post Vent            -> /mp3/0019.wav
     019 Standby Loop         -> /mp3/0020.wav
     020 Fire 2 Intro         -> /mp3/0021.wav
     021 XF PowerOn->Standby  -> /mp3/0022.wav
     022 XF Boot->Idle1       -> /mp3/0023.wav
     023 XF Ramp1On->Idle2    -> /mp3/0024.wav
     024 XF Ramp1Down->Idle1  -> /mp3/0025.wav
     025 XF Ramp2On->Idle3    -> /mp3/0026.wav
     026 XF Ramp2Down->Idle2  -> /mp3/0027.wav
     027 XF FireTail->Idle3   -> /mp3/0028.wav
     028 XF Vent->PostVent    -> /mp3/0029.wav (reserve / pair test)
     029 XF PostVent->After   -> /mp3/0030.wav (reserve / pair test)
     030 XF AfterVent->Idle3  -> /mp3/0031.wav (reserve / pair test)
     031 XF complete Vent chain -> /mp3/0032.wav (used by the current sketch)

   Music files:
     /01/001.mp3, /01/002.mp3, /01/003.mp3, ...

****************************************************************************************/

#include <BGSequence.h>
#include "SoftwareSerial.h"
#include <DFPlayerMini_Fast.h>
#include "FastLED.h"
#include <Adafruit_NeoPixel.h>
#include "GBLEDPatterns.h"

/************************* Hardware / Libraries *************************/
BGSequence BarGraph;

enum BarGraphSequences { START, ACTIVE, FIRE1, FIRE2, BGVENT };
BarGraphSequences BGMODE;

SoftwareSerial mySoftwareSerial(10, 11); // RX, TX
DFPlayerMini_Fast myDFPlayer;

const uint8_t STARTWAND_SWITCH = 6;
const uint8_t STARTPACK_SWITCH = 7;
const uint8_t MUSIC_SWITCH     = 8;
const uint8_t FIRE_BUTTON      = 4;
const uint8_t FIRE_BUTTON2     = 5;
const uint8_t VENTING          = 9;
const uint8_t ISPLAYING_OUT    = 12;
const uint8_t RUMBLE           = 13;

const int NeoPixelLEDCount1 = 55;
const int NeoPixelLEDCount2 = 17;
const int AuxLEDCount        = 8;

#define NEO_WAND 3
#define AUX_LED_PIN A1

Adafruit_NeoPixel wandLights(NeoPixelLEDCount2, NEO_WAND, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel auxLights(AuxLEDCount, AUX_LED_PIN, NEO_GRB + NEO_KHZ800);

void PackLEDsComplete();
NeoPatterns PackLEDs(NeoPixelLEDCount1, 2, NEO_GRB + NEO_KHZ800, &PackLEDsComplete);

// Keep the legacy Theme ID for compatibility with GBLEDPatterns / fireStrobe.
enum PackTheme { MOVIE, STATIS, SLIME, MESON, CHRISTMAS };
PackTheme THEME = STATIS;

/************************* D3 LED Indices *************************/
const uint8_t ventStart    = 0;
const uint8_t ventEnd      = 3;
const uint8_t SloBloLED    = 4;
const uint8_t WandVentLED  = 5;
const uint8_t ThemeLED     = 6;
const uint8_t WhiteLED1    = 7;
const uint8_t WhiteLED2    = 8;
const uint8_t OrangeHatLED = 9;
const uint8_t GunLEDStart  = 10;
const uint8_t GunLEDEnd    = 16;

/************************* Audio *************************/
// Logical IDs 000..031. The /mp3 track number is addressed internally as ID + 1.
enum PackSound : uint8_t {
  SND_POWER_ON = 0,
  SND_BOOT,
  SND_PACK_IDLE,
  SND_RAMP1_ON,
  SND_RAMP1_IDLE,
  SND_RAMP1_DOWN,
  SND_RAMP2_ON,
  SND_RAMP2_IDLE,
  SND_RAMP2_DOWN,
  SND_FIRE,
  SND_FIRE_LOOP,
  SND_FIRE_TAIL,
  SND_PACK_ALARM,
  SND_VENT,
  SND_OFF,
  SND_BOOT_AFTER_VENT,
  SND_ERROR,
  SND_ERROR_OFF,
  SND_POST_VENT,
  SND_STANDBY_LOOP,
  SND_FIRE2_INTRO,
  SND_XF_POWERON_STANDBY,    // 0022.wav
  SND_XF_BOOT_IDLE1,         // 0023.wav
  SND_XF_RAMP1ON_IDLE2,      // 0024.wav
  SND_XF_RAMP1DOWN_IDLE1,    // 0025.wav
  SND_XF_RAMP2ON_IDLE3,      // 0026.wav
  SND_XF_RAMP2DOWN_IDLE2,    // 0027.wav
  SND_XF_FIRETAIL_IDLE3,     // 0028.wav
  SND_XF_VENT_POSTVENT,      // 0029.wav, reserve
  SND_XF_POSTVENT_AFTERVENT, // 0030.wav, reserve
  SND_XF_AFTERVENT_IDLE3,    // 0031.wav, reserve
  SND_XF_VENT_CHAIN          // 0032.wav
};

const uint8_t MUSIC_FOLDER = 1;
uint8_t currentMusicTrack = 1;
bool musicHasStarted = false;

// BUSY tracking for one-shot sounds whose end triggers a state transition.
bool packSoundLooping = false;
// Tracks whether the DFPlayer is currently in Repeat mode, independent of system state.
// This matters because music also uses Repeat. This allows stopRepeat() to be sent only
// when it is actually required.
bool dfRepeatActive = false;
bool packSoundBusySeen = false;
unsigned long packSoundStartedAt = 0;
// 0023.wav (Boot -> Idle1) is almost 39 seconds long. Therefore the BUSY
// fallback for combined files must be significantly longer than 30 seconds.
const unsigned long SOUND_END_FALLBACK_MS = 60000UL;

// A crossfade file contains the transition AND one complete first pass
// of the following Idle sound. Only after the combined file truly ends
// is the normal repeating loop of the target Idle sound started.
bool combinedTailActive = false;
PackSound combinedTailLoopSound = SND_STANDBY_LOOP;

// FIX17: General loop watchdog for Standby and Idle 1/2/3.
// On the real hardware, after a one-shot sound/crossfade the DFPlayer may
// start the loop track but occasionally fail to accept the immediately following
// startRepeat() command. D12/BUSY is therefore monitored in the stable
// loop states and the expected loop is restarted when necessary.
bool packLoopBusySeen = false;
unsigned long packLoopIdleSince = 0;
PackSound watchedPackLoopSound = SND_STANDBY_LOOP;
const unsigned long PACK_LOOP_IDLE_RESTART_MS = 250UL;
const unsigned long PACK_LOOP_START_TIMEOUT_MS = 1200UL;

/************************* State Machine *************************/
enum SystemState : uint8_t {
  ST_STANDBY,
  ST_MUSIC,
  ST_BOOT,
  ST_IDLE1,
  ST_RAMP1_ON,
  ST_IDLE2,
  ST_RAMP1_DOWN,
  ST_RAMP2_ON,
  ST_IDLE3,
  ST_RAMP2_DOWN,
  ST_FIRING,
  ST_WARNING,
  ST_FASTWARNING,
  ST_FIRE_TAIL,
  ST_VENT_PREDELAY,
  ST_VENT_ACTIVE,
  ST_POST_VENT_SOUND,
  ST_AFTER_VENT,
  ST_ERROR_STANDBY,
  ST_POWERDOWN,
  ST_WAIT_ALL_OFF
};

SystemState state = ST_STANDBY;
unsigned long stateStartedAt = 0;

// While the one-shot Power On sound is still playing, no Standby inputs are processed.
bool powerOnSoundPending = true;

bool firstSuccessfulBootDone = false;
unsigned long activeBootDurationMs = 15000UL;

// PowerDown: Error exit (017) or normal exit (014).
bool powerDownIsError = false;
SystemState powerDownSourceState = ST_STANDBY; // remembers which state initiated PowerDown
// FIX17: For EVERY PowerDown, remember whether during the running shutdown
// all controls have actually been observed OFF/released at least once.
// After that, D7 may immediately interrupt the running PowerDown and start Boot.
bool powerDownAllOffSeen = false;
const unsigned long POWERDOWN_DURATION_MS = 10000UL;
float powerDownStartRPM = 300.0f;

/************************* Timing *************************/
const unsigned long FIRING_WARNING_MS = 5000UL;
const unsigned long FIRING_ALARM_MS   = 10000UL;
const unsigned long VENT_PREDELAY_MS  = 2000UL;

// Source sound lengths used to determine when the state logic changes from
// the transition state to the target state. These values match the files
// that were actually uploaded for the crossfade package.
// Note: the uploaded 0001.wav is 2.905 seconds long.
const unsigned long LEN_0001_MS = 2905UL;
const unsigned long LEN_0002_MS = 17661UL;
const unsigned long LEN_0003_MS = 21495UL;
const unsigned long LEN_0004_MS = 1723UL;
const unsigned long LEN_0005_MS = 20868UL;
const unsigned long LEN_0006_MS = 1985UL;
const unsigned long LEN_0007_MS = 1642UL;
const unsigned long LEN_0008_MS = 14029UL;
const unsigned long LEN_0009_MS = 1879UL;
const unsigned long LEN_0010_MS = 14410UL;
const unsigned long LEN_0011_MS = 7890UL;
const unsigned long LEN_0012_MS = 3678UL;
const unsigned long LEN_0013_MS = 7570UL;
const unsigned long LEN_0014_MS = 5521UL;
const unsigned long LEN_0015_MS = 11910UL;
const unsigned long LEN_0016_MS = 1689UL;
const unsigned long LEN_0017_MS = 467UL;
const unsigned long LEN_0018_MS = 11890UL;
const unsigned long LEN_0019_MS = 1968UL;
const unsigned long LEN_0020_MS = 8403UL;
const unsigned long LEN_0021_MS = 14250UL;

// FIX15 crossfade lengths, for documentation/audio alignment only.
const unsigned long XF_0001_0020_MS = 400UL;
const unsigned long XF_0002_0003_MS = 400UL;
const unsigned long XF_0004_0005_MS = 345UL;
const unsigned long XF_0006_0003_MS = 397UL;
const unsigned long XF_0007_0008_MS = 328UL;
const unsigned long XF_0009_0005_MS = 376UL;
const unsigned long XF_0012_0008_MS = 400UL;
const unsigned long XF_0014_0019_MS = 394UL;
const unsigned long XF_0019_0016_MS = 338UL;
const unsigned long XF_0016_0008_MS = 338UL;

/************************* Input Debouncing *************************/
struct DebouncedInput {
  uint8_t pin;
  bool stable;
  bool lastRaw;
  unsigned long rawChangedAt;
  bool rose;
  bool fell;
};

DebouncedInput inD4 = { FIRE_BUTTON, HIGH, HIGH, 0, false, false };
DebouncedInput inD5 = { FIRE_BUTTON2, HIGH, HIGH, 0, false, false };
DebouncedInput inD6 = { STARTWAND_SWITCH, LOW, LOW, 0, false, false };
DebouncedInput inD7 = { STARTPACK_SWITCH, LOW, LOW, 0, false, false };
DebouncedInput inD8 = { MUSIC_SWITCH, LOW, LOW, 0, false, false };

const unsigned long DEBOUNCE_MS = 25UL;

void initDebouncedInput(DebouncedInput &in) {
  bool raw = digitalRead(in.pin);
  in.stable = raw;
  in.lastRaw = raw;
  in.rawChangedAt = millis();
  in.rose = false;
  in.fell = false;
}

void updateDebouncedInput(DebouncedInput &in, unsigned long now) {
  in.rose = false;
  in.fell = false;

  bool raw = digitalRead(in.pin);
  if (raw != in.lastRaw) {
    in.lastRaw = raw;
    in.rawChangedAt = now;
  }

  if ((now - in.rawChangedAt) >= DEBOUNCE_MS && in.stable != raw) {
    bool oldStable = in.stable;
    in.stable = raw;
    in.rose = (!oldStable && in.stable);
    in.fell = (oldStable && !in.stable);
  }
}

bool fire1Pressed() { return inD4.stable == LOW; }
bool fire2Pressed() { return inD5.stable == LOW; }
bool anyFirePressed() { return fire1Pressed() || fire2Pressed(); }
bool wandSwitchOn() { return inD6.stable == HIGH; }
bool packSwitchOn() { return inD7.stable == HIGH; }
bool musicSwitchOn() { return inD8.stable == HIGH; }

bool allControlsOff() {
  return !wandSwitchOn() && !packSwitchOn() && !musicSwitchOn() &&
         !fire1Pressed() && !fire2Pressed();
}

/************************* Manual Cyclotron *************************/
uint8_t ringHead = 0;
unsigned long lastRingStepMicros = 0;
float currentManualRPM = 300.0f;

// 40 LEDs: interval_us = 60.000.000 / (RPM * 40) = 1.500.000 / RPM
unsigned long rpmToStepMicros(float rpm) {
  if (rpm < 1.0f) rpm = 1.0f;
  return (unsigned long)(1500000.0f / rpm);
}

float lerpFloat(float a, float b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return a + (b - a) * t;
}

float bootRPMForElapsed(unsigned long elapsedMs, unsigned long durationMs) {
  const float startRPM = 60.0f;
  const float kneeRPM  = 200.0f;
  const float endRPM   = 400.0f;

  // 2.5 s at a 15 s Boot = 1/6 of the total duration; 1.667 s for a 10 s Boot.
  unsigned long kneeMs = (durationMs + 3UL) / 6UL; // 15 s -> 2500 ms, 10 s -> 1667 ms
  if (kneeMs < 1UL) kneeMs = 1UL;

  if (elapsedMs <= kneeMs) {
    return lerpFloat(startRPM, kneeRPM, (float)elapsedMs / (float)kneeMs);
  }

  if (elapsedMs >= durationMs) return endRPM;

  return lerpFloat(kneeRPM, endRPM,
                   (float)(elapsedMs - kneeMs) / (float)(durationMs - kneeMs));
}

void drawManualRing(uint8_t headLed) {
  uint32_t colorDim    = PackLEDs.Color(60, 0, 0);
  uint32_t colorMedium = PackLEDs.Color(140, 0, 0);
  uint32_t colorBright = PackLEDs.Color(255, 0, 0);

  for (uint8_t i = 0; i < 40; i++) {
    PackLEDs.setPixelColor(i, 0);
  }

  PackLEDs.setPixelColor(headLed % 40, colorDim);
  PackLEDs.setPixelColor((headLed + 1) % 40, colorMedium);
  PackLEDs.setPixelColor((headLed + 2) % 40, colorBright);
}

void stepManualRingAtRPM(float rpm) {
  currentManualRPM = rpm;
  unsigned long nowUs = micros();
  unsigned long intervalUs = rpmToStepMicros(rpm);

  if ((unsigned long)(nowUs - lastRingStepMicros) >= intervalUs) {
    lastRingStepMicros = nowUs;
    ringHead++;
    if (ringHead >= 40) ringHead = 0;
  }
}

void runManualIdlePack() {
  // Original STATIS Idle: 5 ms/LED = approximately 300 RPM.
  stepManualRingAtRPM(300.0f);
  PackLEDs.Update();                 // PowerCell animation from the library
  drawManualRing(ringHead);          // draw the Cyclotron precisely on top afterwards
  PackLEDs.show();
}

void startManualIdle() {
  PackLEDs.ActivePattern1 = NONE;
  PackLEDs.Powercell(PackLEDs.Color2, 75);
  PackLEDs.setBrightness(10);
  lastRingStepMicros = micros();
}

/************************* DFPlayer - Pack WAV / Music MP3, strictly separated *************************/
uint16_t packTrackNumber(PackSound soundId) {
  return (uint16_t)soundId + 1U; // Pack-WAV: 000 -> 0001.wav, ... 031 -> 0032.wav
}

// PACK SOUNDS: WAV files only, /mp3/0001.wav ... /mp3/0032.wav.
// MUSIC: MP3 files only, /01/001.mp3, /01/002.mp3, ...
// NOTE: playFromMP3Folder() is the name of the DFPlayer protocol command for
// the special /mp3 folder. The function name does NOT define the audio format.
// The DFPlayer selects the numbered WAV file inside the /mp3 folder.

// Disable Repeat only when the DFPlayer is actually in Repeat mode.
// The short delay afterwards is intentional: the tested DFPlayer sometimes
// drops the next Play command when serial commands are sent back-to-back.
// A 20 ms delay was stable on the real hardware in FIX6.
void stopAudioRepeatIfActive() {
  if (dfRepeatActive) {
    myDFPlayer.stopRepeat();
    delay(20);
    dfRepeatActive = false;
  }
}

void playPackSound(PackSound soundId) {
  combinedTailActive = false;
  stopAudioRepeatIfActive();
  myDFPlayer.playFromMP3Folder(packTrackNumber(soundId));
  packSoundLooping = false;
  dfRepeatActive = false;
  packSoundBusySeen = false;
  packSoundStartedAt = millis();
}

void loopPackSound(PackSound soundId) {
  combinedTailActive = false;
  // After the FIX10 hardware test, restore the proven Repeat sequence.
  // The DFPlayer accepts startRepeat() much more reliably with this command spacing
  // than with a delayed, decoupled Repeat activation.
  stopAudioRepeatIfActive();
  myDFPlayer.playFromMP3Folder(packTrackNumber(soundId));
  delay(50);
  myDFPlayer.startRepeat();
  packSoundLooping = true;
  dfRepeatActive = true;
  packSoundBusySeen = false;
  packSoundStartedAt = millis();
}

void resetPackLoopWatchdog(PackSound expectedSound) {
  watchedPackLoopSound = expectedSound;
  packLoopBusySeen = false;
  packLoopIdleSince = 0;
}

void startPackLoopReliable(PackSound soundId) {
  loopPackSound(soundId);
  resetPackLoopWatchdog(soundId);
}

void startStandbyLoopReliable() {
  startPackLoopReliable(SND_STANDBY_LOOP);
}

// For a direct transition between one-shot sounds. No stopRepeat/delay is used,
// because Repeat should not be active at this point.
void playPackSoundAfterFinished(PackSound soundId) {
  combinedTailActive = false;
  myDFPlayer.playFromMP3Folder(packTrackNumber(soundId));
  packSoundLooping = false;
  dfRepeatActive = false;
  packSoundBusySeen = false;
  packSoundStartedAt = millis();
}

// Starts a pre-rendered crossfade file. It is a one-shot sound and already
// contains one complete first pass of the target Idle sound.
void playCombinedTransition(PackSound transitionSound, PackSound loopAfter) {
  playPackSound(transitionSound);
  combinedTailLoopSound = loopAfter;
  combinedTailActive = true;
}

// Keep the combined file running in the target Idle state. Only when the
// combined file actually ends is the normal target loop started.
void serviceCombinedTailLoop() {
  if (!combinedTailActive) return;

  if (packSoundFinished()) {
    PackSound nextLoop = combinedTailLoopSound;
    combinedTailActive = false;
    startPackLoopReliable(nextLoop);
  }
}

// FIX17: General D12/BUSY watchdog for Standby and Idle loops.
// While D12 is LOW, the expected track is playing. If D12 remains HIGH
// for more than 250 ms after a LOW has already been observed, Repeat has
// apparently failed to engage and the expected loop is restarted cleanly.
// If the Play command produces no BUSY LOW at all after 1.2 seconds,
// the loop is restarted as well.
void servicePackLoopWatchdog(unsigned long now, PackSound expectedSound) {
  if (combinedTailActive) {
    serviceCombinedTailLoop();
    return;
  }

  // If we entered this state without the expected loop being marked active,
  // immediately restore the correct loop.
  if (!packSoundLooping || watchedPackLoopSound != expectedSound) {
    startPackLoopReliable(expectedSound);
    return;
  }

  bool busyNow = (digitalRead(ISPLAYING_OUT) == LOW);

  if (busyNow) {
    packLoopBusySeen = true;
    packLoopIdleSince = 0;
    return;
  }

  if (!packLoopBusySeen) {
    if ((now - packSoundStartedAt) >= PACK_LOOP_START_TIMEOUT_MS) {
      startPackLoopReliable(expectedSound);
    }
    return;
  }

  if (packLoopIdleSince == 0) {
    packLoopIdleSince = now;
    return;
  }

  if ((now - packLoopIdleSince) >= PACK_LOOP_IDLE_RESTART_MS) {
    startPackLoopReliable(expectedSound);
  }
}

bool packSoundFinished() {
  if (packSoundLooping) return false;

  // DFPlayer BUSY is LOW during playback and HIGH when idle.
  bool busyNow = (digitalRead(ISPLAYING_OUT) == LOW);
  if (busyNow) packSoundBusySeen = true;

  if (packSoundBusySeen && !busyNow && (millis() - packSoundStartedAt) > 100UL) {
    return true;
  }

  // Fallback in case BUSY does not behave correctly.
  if ((millis() - packSoundStartedAt) >= SOUND_END_FALLBACK_MS) {
    return true;
  }

  return false;
}

int16_t musicTrackCount() {
  mySoftwareSerial.listen();
  int16_t count = myDFPlayer.numTracksInFolder(MUSIC_FOLDER);
  if (count < 1 || count > 255) return 1;
  return count;
}

void loopMusicTrack(uint8_t track) {
  combinedTailActive = false;
  // Music also uses Repeat. Keep the short command delays
  // so the DFPlayer processes the commands reliably.
  stopAudioRepeatIfActive();
  myDFPlayer.playFolder(MUSIC_FOLDER, track);
  delay(50);
  myDFPlayer.startRepeat();
  dfRepeatActive = true;
  packSoundLooping = false;
  musicHasStarted = true;
}

void selectNextMusicTrack() {
  int16_t count = musicTrackCount();

  if (!musicHasStarted) {
    currentMusicTrack = 1;
  } else {
    currentMusicTrack++;
    if (currentMusicTrack == 0 || currentMusicTrack > count) currentMusicTrack = 1;
  }
  loopMusicTrack(currentMusicTrack);
}

void selectPreviousMusicTrack() {
  int16_t count = musicTrackCount();

  if (!musicHasStarted) {
    currentMusicTrack = (uint8_t)count; // first press of D5 -> last song
  } else {
    if (currentMusicTrack <= 1) currentMusicTrack = (uint8_t)count;
    else currentMusicTrack--;
  }
  loopMusicTrack(currentMusicTrack);
}

/************************* LED Helpers *************************/
void clearWandLEDs() {
  for (uint8_t i = 0; i < NeoPixelLEDCount2; i++) wandLights.setPixelColor(i, 0);
}

void clearGunLEDs() {
  for (uint8_t i = GunLEDStart; i <= GunLEDEnd; i++) wandLights.setPixelColor(i, 0);
}

void clearPackLEDsAll() {
  for (uint8_t i = 0; i < NeoPixelLEDCount1; i++) PackLEDs.setPixelColor(i, 0);
  PackLEDs.show();
}

void clearAuxLEDs() {
  for (uint8_t i = 0; i < AuxLEDCount; i++) auxLights.setPixelColor(i, 0);
  auxLights.show();
}

bool blinkPhase(unsigned long now, unsigned long halfPeriodMs) {
  return ((now / halfPeriodMs) % 2UL) == 0UL;
}

void setVentPixels(bool on) {
  for (uint8_t i = ventStart; i <= ventEnd; i++) {
    wandLights.setPixelColor(i, on ? wandLights.Color(255, 255, 255) : 0);
  }
}

void renderBaseIdleWand(bool themeRed, bool white1Blink, unsigned long now) {
  clearWandLEDs();
  setVentPixels(false);
  wandLights.setPixelColor(WandVentLED, wandLights.Color(255, 255, 255));
  wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));

  if (themeRed) wandLights.setPixelColor(ThemeLED, wandLights.Color(255, 0, 0));
  if (white1Blink && blinkPhase(now, 500UL)) {
    wandLights.setPixelColor(WhiteLED1, wandLights.Color(255, 255, 255));
  }
}

void renderBootWand(unsigned long now) {
  clearWandLEDs();
  wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));
  if (blinkPhase(now, 500UL)) {
    wandLights.setPixelColor(SloBloLED, wandLights.Color(255, 0, 0));
  }
}

void renderFiringWand(unsigned long now, SystemState fireState) {
  // Do not clear the Jewel area here; fireStrobe writes it separately.
  for (uint8_t i = 0; i < GunLEDStart; i++) wandLights.setPixelColor(i, 0);
  setVentPixels(false);
  wandLights.setPixelColor(WandVentLED, wandLights.Color(255, 255, 255));

  // WhiteLED1 uses the normal firing blink rate.
  if (blinkPhase(now, 500UL)) {
    wandLights.setPixelColor(WhiteLED1, wandLights.Color(255, 255, 255));
  }

  // WhiteLED2 follows the legacy WARNING/FASTWARNING behavior.
  if (fireState == ST_FIRING) {
    wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));
  } else if (fireState == ST_WARNING) {
    if (blinkPhase(now, 500UL)) wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));
  } else {
    if (blinkPhase(now, 100UL)) wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));
  }

  // ThemeLED: normal in FIRING, faster in WARNING, fastest in FASTWARNING.
  unsigned long themeHalfPeriod = 500UL;
  if (fireState == ST_WARNING) themeHalfPeriod = 250UL;
  if (fireState == ST_FASTWARNING) themeHalfPeriod = 100UL;
  if (blinkPhase(now, themeHalfPeriod)) {
    wandLights.setPixelColor(ThemeLED, wandLights.Color(255, 0, 0));
  }
}

void renderErrorWand(unsigned long now) {
  clearWandLEDs();
  bool on = blinkPhase(now, 150UL);
  if (on) {
    wandLights.setPixelColor(SloBloLED, wandLights.Color(255, 0, 0));
    wandLights.setPixelColor(WandVentLED, wandLights.Color(255, 0, 0));
    wandLights.setPixelColor(ThemeLED, wandLights.Color(255, 0, 0));
    wandLights.setPixelColor(WhiteLED1, wandLights.Color(255, 255, 255));
    wandLights.setPixelColor(WhiteLED2, wandLights.Color(255, 255, 255));
    wandLights.setPixelColor(OrangeHatLED, wandLights.Color(255, 0, 0));
  }
}

void renderVentPreDelayWand(unsigned long now) {
  // No Vent animation yet: keep the warning/alarm visuals visible until Vent starts.
  renderFiringWand(now, ST_FASTWARNING);
}

void renderVentActiveWand() {
  clearWandLEDs();
  setVentPixels(true);
  wandLights.setPixelColor(ThemeLED, wandLights.Color(255, 0, 0));
}

/************************* A1 *************************/
uint8_t musicAuxLevel = 1;
int8_t musicAuxDirection = 1;
unsigned long lastMusicAuxStep = 0;

void setAuxOffAll() {
  for (uint8_t i = 0; i < AuxLEDCount; i++) auxLights.setPixelColor(i, 0);
}

void renderAuxBooting(unsigned long now) {
  setAuxOffAll();
  if (blinkPhase(now, 500UL)) {
    uint32_t c = auxLights.Color(255, 255, 255);
    auxLights.setPixelColor(0, c); // LED 1
    auxLights.setPixelColor(4, c); // LED 5
    auxLights.setPixelColor(5, c); // LED 6
  }
}

void renderAuxIdle(unsigned long now) {
  setAuxOffAll();
  uint32_t c = auxLights.Color(255, 255, 255);
  auxLights.setPixelColor(1, c); // LED 2 steady

  if (blinkPhase(now, 500UL)) auxLights.setPixelColor(6, c); // LED 7
  else                        auxLights.setPixelColor(7, c); // LED 8
}

void renderAuxStandby() {
  setAuxOffAll();
  auxLights.setPixelColor(1, auxLights.Color(255, 255, 255)); // LED 2 only
}

void renderAuxWarning(unsigned long now) {
  setAuxOffAll();
  if (blinkPhase(now, 500UL)) {
    uint32_t c = auxLights.Color(255, 255, 255);
    auxLights.setPixelColor(0, c);
    auxLights.setPixelColor(1, c);
    auxLights.setPixelColor(4, c);
    auxLights.setPixelColor(5, c);
    auxLights.setPixelColor(6, c);
    auxLights.setPixelColor(7, c);
  }
}

void renderAuxFiring(unsigned long now) {
  setAuxOffAll();
  uint32_t c = auxLights.Color(255, 255, 255);
  if (blinkPhase(now, 500UL)) {
    auxLights.setPixelColor(0, c); // LED 1
    auxLights.setPixelColor(4, c); // LED 5
    auxLights.setPixelColor(5, c); // LED 6
  } else {
    auxLights.setPixelColor(1, c); // LED 2 opposite phase
  }
}

void renderAuxVenting() {
  setAuxOffAll();
  uint32_t c = auxLights.Color(255, 255, 255);
  auxLights.setPixelColor(0, c); // LED 1
  auxLights.setPixelColor(2, c); // LED 3
  auxLights.setPixelColor(3, c); // LED 4
}

void renderAuxPowerDown(unsigned long now) {
  setAuxOffAll();
  if (blinkPhase(now, 500UL)) {
    uint32_t c = auxLights.Color(255, 255, 255);
    auxLights.setPixelColor(0, c); // LED 1
    auxLights.setPixelColor(2, c); // LED 3
    auxLights.setPixelColor(3, c); // LED 4
  }
}

void renderAuxMusic(unsigned long now) {
  // White PowerCell-like up/down sequence across all eight A1 LEDs:
  // 1 -> 2 -> ... -> 8 filled LEDs, then 7 -> ... -> 1.
  if ((now - lastMusicAuxStep) >= 75UL) {
    lastMusicAuxStep = now;

    if (musicAuxDirection > 0) {
      if (musicAuxLevel >= AuxLEDCount) {
        musicAuxDirection = -1;
        if (musicAuxLevel > 1) musicAuxLevel--;
      } else {
        musicAuxLevel++;
      }
    } else {
      if (musicAuxLevel <= 1) {
        musicAuxDirection = 1;
        musicAuxLevel++;
      } else {
        musicAuxLevel--;
      }
    }
  }

  setAuxOffAll();
  uint32_t c = auxLights.Color(255, 255, 255);
  for (uint8_t i = 0; i < musicAuxLevel && i < AuxLEDCount; i++) {
    auxLights.setPixelColor(i, c);
  }
}

void updateAuxLights(unsigned long now) {
  switch (state) {
    case ST_STANDBY:
      renderAuxStandby();
      break;

    case ST_MUSIC:
      renderAuxMusic(now);
      break;

    case ST_BOOT:
    case ST_RAMP1_ON:
    case ST_RAMP1_DOWN:
    case ST_RAMP2_ON:
    case ST_RAMP2_DOWN:
    case ST_FIRE_TAIL:
    case ST_AFTER_VENT:
      renderAuxBooting(now);
      break;

    case ST_IDLE1:
    case ST_IDLE2:
    case ST_IDLE3:
      renderAuxIdle(now);
      break;

    case ST_FIRING:
      renderAuxFiring(now);
      break;

    case ST_WARNING:
    case ST_FASTWARNING:
    case ST_ERROR_STANDBY:
      renderAuxWarning(now);
      break;

    case ST_VENT_PREDELAY:
    case ST_VENT_ACTIVE:
      renderAuxVenting();
      break;

    case ST_POST_VENT_SOUND:
      // A1 keeps the final Venting pattern during 0019.
      break;

    case ST_POWERDOWN:
      if (powerDownIsError) renderAuxWarning(now);
      else renderAuxPowerDown(now);
      break;

    case ST_WAIT_ALL_OFF:
      setAuxOffAll();
      break;
  }

  auxLights.show();
}

/************************* Firing Animation from Legacy Sketch *************************/
unsigned long ThemeGunColors1[5]  = {255, 255, 0, 255, 255};
unsigned long ThemeGunColors2[5]  = {255, 255, 255, 255, 0};
unsigned long ThemeGunColors3[5]  = {255, 255, 0, 0, 0};
unsigned long ThemeGunColors12[5] = {255, 255, 0, 255, 0};
unsigned long ThemeGunColors22[5] = {0, 0, 255, 255, 255};
unsigned long ThemeGunColors32[5] = {0, 0, 0, 0, 0};
unsigned long ThemeGunColors13[5] = {0, 0, 0, 0, 0};
unsigned long ThemeGunColors23[5] = {0, 0, 0, 0, 0};
unsigned long ThemeGunColors33[5] = {255, 255, 0, 0, 0};
unsigned long ThemeGunColors14[5] = {255, 255, 0, 0, 0};
unsigned long ThemeGunColors24[5] = {0, 0, 0, 0, 0};
unsigned long ThemeGunColors34[5] = {255, 255, 0, 0, 0};
unsigned long ThemeGunColors15[5] = {0, 0, 0, 0, 0};
unsigned long ThemeGunColors25[5] = {255, 255, 0, 0, 0};
unsigned long ThemeGunColors35[5] = {0, 0, 0, 0, 0};

unsigned long prevFireMillis = 0;
const unsigned long fire_interval2 = 50UL;
uint8_t fireSeqNum = 0;
const uint8_t fireSeqTotal = 5;

void fireStrobe(unsigned long currentMillis) {
  if ((unsigned long)(currentMillis - prevFireMillis) < fire_interval2) return;
  prevFireMillis = currentMillis;

  switch (fireSeqNum) {
    case 0:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(11, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(12, 0);
      wandLights.setPixelColor(13, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(14, 0);
      wandLights.setPixelColor(15, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(16, 0);
      break;

    case 1:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      wandLights.setPixelColor(11, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(12, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(13, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(14, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(15, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(16, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      break;

    case 2:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(11, 0);
      wandLights.setPixelColor(12, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      wandLights.setPixelColor(13, 0);
      wandLights.setPixelColor(14, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      wandLights.setPixelColor(15, 0);
      wandLights.setPixelColor(16, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      break;

    case 3:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      wandLights.setPixelColor(11, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(12, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(13, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(14, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(15, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(16, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      break;

    case 4:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(11, 0);
      wandLights.setPixelColor(12, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(13, 0);
      wandLights.setPixelColor(14, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(15, 0);
      wandLights.setPixelColor(16, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      break;

    case 5:
      wandLights.setPixelColor(10, wandLights.Color(ThemeGunColors14[THEME], ThemeGunColors24[THEME], ThemeGunColors34[THEME]));
      wandLights.setPixelColor(11, wandLights.Color(ThemeGunColors15[THEME], ThemeGunColors25[THEME], ThemeGunColors35[THEME]));
      wandLights.setPixelColor(12, wandLights.Color(ThemeGunColors12[THEME], ThemeGunColors22[THEME], ThemeGunColors32[THEME]));
      wandLights.setPixelColor(13, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      wandLights.setPixelColor(14, wandLights.Color(ThemeGunColors14[THEME], ThemeGunColors24[THEME], ThemeGunColors34[THEME]));
      wandLights.setPixelColor(15, wandLights.Color(ThemeGunColors1[THEME], ThemeGunColors2[THEME], ThemeGunColors3[THEME]));
      wandLights.setPixelColor(16, wandLights.Color(ThemeGunColors13[THEME], ThemeGunColors23[THEME], ThemeGunColors33[THEME]));
      break;
  }

  fireSeqNum++;
  if (fireSeqNum > fireSeqTotal) fireSeqNum = 0;
}

/************************* Firing Pack Acceleration *************************/
unsigned long prevCycMillis = 0;
unsigned long prevBGMillis = 0;
unsigned long FireRate = 0;
const unsigned long fire_interval = 100UL;
unsigned long baseBarGraphInterval = 0;

void runFiringPackAcceleration(unsigned long now) {
  if ((now - prevCycMillis) >= fire_interval) {
    FireRate++;
    prevCycMillis = now;

    if (FireRate > 95UL) {
      PackLEDs.CyclotronInterval(1);
      PackLEDs.PowercellInterval(1);
    } else {
      PackLEDs.CyclotronInterval(95UL - FireRate);
      PackLEDs.PowercellInterval(95UL - FireRate);
    }
  }

  if (baseBarGraphInterval > 0 &&
      (now - prevBGMillis) >= (unsigned long)((float)baseBarGraphInterval * 4.75f)) {
    prevBGMillis = now;
    if (BarGraph.IntervalBG >= 1) BarGraph.changeInterval(BarGraph.IntervalBG - 1);
  }
}

/************************* State Transitions *************************/
void setState(SystemState newState) {
  state = newState;
  stateStartedAt = millis();
}

void enterStandby() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  musicHasStarted = false;
  clearWandLEDs();
  wandLights.show();
  clearPackLEDsAll();
  BarGraph.clearLEDs();

  // In Standby, repeat 0020.wav continuously until another state
  // starts a new pack sound or music. FIX17 monitors D12/BUSY
  // for Standby and all three Idle loops.
  startStandbyLoopReliable();
  setState(ST_STANDBY);
}

void enterBoot() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  musicHasStarted = false;

  activeBootDurationMs = firstSuccessfulBootDone ? 10000UL : 15000UL;

  playCombinedTransition(SND_XF_BOOT_IDLE1, SND_PACK_IDLE);
  BarGraph.initiateVariables(ACTIVE);

  PackLEDs.setBrightness(10);
  PackLEDs.ActivePattern1 = NONE;
  PackLEDs.PowercellBoot(PackLEDs.Wheel(170), 30);

  ringHead = 0;
  currentManualRPM = 60.0f;
  lastRingStepMicros = micros();

  setState(ST_BOOT);
}

void enterIdle1() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  startPackLoopReliable(SND_PACK_IDLE);
  setState(ST_IDLE1);
}

// Same as enterIdle1(), but do NOT interrupt the already running crossfade file.
// It already contains the first 0003 pass.
void enterIdle1KeepAudio() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  setState(ST_IDLE1);
}

void enterIdle2() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  startPackLoopReliable(SND_RAMP1_IDLE);
  setState(ST_IDLE2);
}

void enterIdle2KeepAudio() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  setState(ST_IDLE2);
}

void enterIdle3() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  startPackLoopReliable(SND_RAMP2_IDLE);
  setState(ST_IDLE3);
}

void enterIdle3KeepAudio() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  setState(ST_IDLE3);
}

void enterRamp1On() {
  playCombinedTransition(SND_XF_RAMP1ON_IDLE2, SND_RAMP1_IDLE);
  setState(ST_RAMP1_ON);
}

void enterRamp1Down() {
  playCombinedTransition(SND_XF_RAMP1DOWN_IDLE1, SND_PACK_IDLE);
  setState(ST_RAMP1_DOWN);
}

void enterRamp2On() {
  playCombinedTransition(SND_XF_RAMP2ON_IDLE3, SND_RAMP2_IDLE);
  setState(ST_RAMP2_ON);
}

void enterRamp2Down() {
  playCombinedTransition(SND_XF_RAMP2DOWN_IDLE2, SND_RAMP1_IDLE);
  setState(ST_RAMP2_DOWN);
}

BarGraphSequences activeFireBarGraph = FIRE1;
bool fireIntroFinished = false;

void enterFiring() {
  // D5 (Fire Button 2) uses its own firing intro sound, 0021.wav.
  // D4 keeps 0010.wav. If both buttons are pressed simultaneously, D4
  // retains priority as before and 0010.wav is used.
  if (fire2Pressed() && !fire1Pressed()) {
    activeFireBarGraph = FIRE2;
    playPackSound(SND_FIRE2_INTRO);
  } else {
    activeFireBarGraph = FIRE1;
    playPackSound(SND_FIRE);
  }
  fireIntroFinished = false;

  // Switch from the manual STATIS ring to the legacy library firing logic.
  PackLEDs.ActivePattern1 = NONE;
  PackLEDs.IndexCLEDArray = 0;
  PackLEDs.IndexC = 0;
  PackLEDs.Cyclotron(PackLEDs.Color1, 5, STATIS);
  PackLEDs.Powercell(PackLEDs.Color2, 75);
  PackLEDs.PowercellClear();
  PackLEDs.setBrightness(10);
  PackLEDs.AL_Fire(true);

  FireRate = 0;
  prevCycMillis = millis();
  prevBGMillis = millis();

  BarGraph.initiateVariables(activeFireBarGraph);
  BarGraph.clearLEDs();
  baseBarGraphInterval = BarGraph.IntervalBG;

  digitalWrite(RUMBLE, HIGH);
  setState(ST_FIRING);
}

void enterFireTail() {
  digitalWrite(RUMBLE, LOW);
  FireRate = 0;
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  playCombinedTransition(SND_XF_FIRETAIL_IDLE3, SND_RAMP2_IDLE);
  setState(ST_FIRE_TAIL);
}

void enterVentPreDelay() {
  digitalWrite(RUMBLE, LOW);
  digitalWrite(VENTING, LOW);
  FireRate = 0;
  PackLEDs.AL_Fire(false);
  PackLEDs.CyclotronInterval(5);
  PackLEDs.PowercellInterval(75);
  BarGraph.clearLEDs();

  // FIX15: The complete fixed audio chain 0014 -> 0019 -> 0016 -> 0008
  // is stored in 0032.wav. For the first 2 seconds only audio plays and D9 stays LOW.
  // State and relay timing remain unchanged.
  playCombinedTransition(SND_XF_VENT_CHAIN, SND_RAMP2_IDLE);
  setState(ST_VENT_PREDELAY);
}

void enterVentActive() {
  digitalWrite(VENTING, HIGH);
  PackLEDs.VentPack();
  BarGraph.initiateVariables(BGVENT);
  setState(ST_VENT_ACTIVE);
}

void enterPostVentSoundKeepAudio() {
  // 0032.wav keeps playing. Change only relay/state; send NO new audio command.
  digitalWrite(VENTING, LOW);
  setState(ST_POST_VENT_SOUND);
}

void enterAfterVentKeepAudio() {
  // 0032.wav continues into the 0016 section and later the 0008 section.
  digitalWrite(VENTING, LOW);
  PackLEDs.AL_Fire(false);
  startManualIdle();
  BarGraph.initiateVariables(ACTIVE);
  setState(ST_AFTER_VENT);
}

void enterStandbyError() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  playPackSound(SND_ERROR);
  BarGraph.initiateVariables(FIRE1); // base pattern for warning display
  BarGraph.clearLEDs();
  setState(ST_ERROR_STANDBY);
}

void startPowerDown(bool errorExit) {
  digitalWrite(VENTING, LOW); // immediately force relay OFF on every abort
  digitalWrite(RUMBLE, LOW);

  // Save the source state before changing to ST_POWERDOWN.
  // FIX17 allows a reboot during any PowerDown after ALL-OFF has been observed once.
  powerDownSourceState = state;
  powerDownIsError = errorExit;
  powerDownAllOffSeen = false;
  if (errorExit) playPackSound(SND_ERROR_OFF);
  else playPackSound(SND_OFF);

  // Choose a sensible starting RPM from the current state.
  if (state == ST_FIRING || state == ST_WARNING || state == ST_FASTWARNING) {
    powerDownStartRPM = 400.0f;
  } else if (state == ST_BOOT) {
    powerDownStartRPM = currentManualRPM;
  } else {
    powerDownStartRPM = currentManualRPM;
    if (powerDownStartRPM < 60.0f) powerDownStartRPM = 300.0f;
  }

  PackLEDs.AL_Fire(false);
  PackLEDs.PowerDown(100); // PowerCell/Pack PowerDown from the existing library
  BarGraph.IntervalBG = 80;

  lastRingStepMicros = micros();
  setState(ST_POWERDOWN);
}

void enterWaitAllOff() {
  digitalWrite(VENTING, LOW);
  digitalWrite(RUMBLE, LOW);
  clearPackLEDsAll();
  clearWandLEDs();
  wandLights.show();
  BarGraph.clearLEDs();
  setState(ST_WAIT_ALL_OFF);
}

/************************* PackLEDs Callback *************************/
void PackLEDsComplete() {
  // State transitions are intentionally handled only by the new state machine.
  // The library may finish its individual animation without changing our state timing.
}

/************************* Visual Update by State *************************/
void updatePackAndBarGraph(unsigned long now) {
  switch (state) {
    case ST_BOOT: {
      unsigned long elapsed = now - stateStartedAt;
      float rpm = bootRPMForElapsed(elapsed, activeBootDurationMs);
      stepManualRingAtRPM(rpm);
      PackLEDs.Update();
      drawManualRing(ringHead);
      PackLEDs.show();
      BarGraph.sequencePackOn(now);
      break;
    }

    case ST_IDLE1:
    case ST_RAMP1_ON:
    case ST_IDLE2:
    case ST_RAMP1_DOWN:
    case ST_RAMP2_ON:
    case ST_IDLE3:
    case ST_RAMP2_DOWN:
    case ST_FIRE_TAIL:
    case ST_AFTER_VENT:
      runManualIdlePack();
      BarGraph.sequencePackOn(now);
      break;

    case ST_FIRING:
    case ST_WARNING:
    case ST_FASTWARNING:
      PackLEDs.Update();
      runFiringPackAcceleration(now);
      if (activeFireBarGraph == FIRE2) BarGraph.sequenceFire2(now);
      else BarGraph.sequenceFire1(now);
      break;

    case ST_VENT_PREDELAY:
      // 013 is already playing; Venting animation has not started yet.
      PackLEDs.Update();
      break;

    case ST_VENT_ACTIVE:
      PackLEDs.Update();
      BarGraph.sequenceVent(now);
      break;

    case ST_POST_VENT_SOUND:
      // 0019: Vent sequence has ended; intentionally hold the final Vent frame.
      break;

    case ST_POWERDOWN: {
      unsigned long elapsed = now - stateStartedAt;
      float t = (float)elapsed / (float)POWERDOWN_DURATION_MS;
      float rpm = lerpFloat(powerDownStartRPM, 60.0f, t);
      stepManualRingAtRPM(rpm);

      // Update library PowerCell PowerDown, then draw the Cyclotron manually on top.
      PackLEDs.Update();
      drawManualRing(ringHead);
      PackLEDs.show();
      BarGraph.sequenceShutdown(now);
      break;
    }

    case ST_ERROR_STANDBY:
      // Bargraph warning pattern based on the existing firing sequence.
      BarGraph.sequenceFire1(now);
      break;

    case ST_STANDBY:
    case ST_MUSIC:
    case ST_WAIT_ALL_OFF:
      break;
  }
}

void updateWandLights(unsigned long now) {
  switch (state) {
    case ST_STANDBY:
    case ST_MUSIC:
    case ST_WAIT_ALL_OFF:
      clearWandLEDs();
      break;

    case ST_BOOT:
      renderBootWand(now);
      break;

    case ST_IDLE1:
    case ST_RAMP1_ON:
      renderBaseIdleWand(false, false, now);
      break;

    case ST_IDLE2:
      renderBaseIdleWand(true, false, now);
      break;

    case ST_RAMP1_DOWN:
      // Ramp 1 is powering down: turn ThemeLED off immediately.
      renderBaseIdleWand(false, false, now);
      break;

    case ST_RAMP2_ON:
      renderBaseIdleWand(true, false, now);
      break;

    case ST_IDLE3:
      renderBaseIdleWand(true, true, now);
      break;

    case ST_RAMP2_DOWN:
      // Returning toward Idle 2: WhiteLED1 off immediately, ThemeLED remains red.
      renderBaseIdleWand(true, false, now);
      break;

    case ST_FIRING:
    case ST_WARNING:
    case ST_FASTWARNING:
      renderFiringWand(now, state);
      fireStrobe(now);
      break;

    case ST_FIRE_TAIL:
    case ST_AFTER_VENT:
      renderBaseIdleWand(true, true, now);
      break;

    case ST_VENT_PREDELAY:
      renderVentPreDelayWand(now);
      break;

    case ST_VENT_ACTIVE:
      renderVentActiveWand();
      break;

    case ST_POST_VENT_SOUND:
      // 0019: hold the final Vent frame until 0016 starts.
      break;

    case ST_ERROR_STANDBY:
      renderErrorWand(now);
      break;

    case ST_POWERDOWN:
      if (powerDownIsError) renderErrorWand(now);
      else clearWandLEDs();
      break;
  }

  wandLights.show();
}

/************************* Abort Rules *************************/
bool handleActiveAbortSwitches() {
  // For FIRING, WARNING, FASTWARNING, FIRE TAIL, and both VENTSTATE phases:
  // D7 OFF = 014 normal PowerDown; D6 or D8 OFF = 017 Error Off.
  if (!packSwitchOn()) {
    startPowerDown(false);
    return true;
  }
  if (!wandSwitchOn() || !musicSwitchOn()) {
    startPowerDown(true);
    return true;
  }
  return false;
}

/************************* SETUP *************************/
void setup() {
  Serial.begin(9600);

  mySoftwareSerial.begin(9600);
  myDFPlayer.begin(mySoftwareSerial);
  myDFPlayer.normalMode();
  myDFPlayer.startDAC();
  myDFPlayer.volume(30);

  BarGraph.BGSeq();
  baseBarGraphInterval = BarGraph.IntervalBG;

  // Same electrical input configuration as the legacy sketch:
  // INPUT + HIGH enables the internal pull-up.
  pinMode(STARTWAND_SWITCH, INPUT);
  digitalWrite(STARTWAND_SWITCH, HIGH);
  pinMode(STARTPACK_SWITCH, INPUT);
  digitalWrite(STARTPACK_SWITCH, HIGH);
  pinMode(MUSIC_SWITCH, INPUT);
  digitalWrite(MUSIC_SWITCH, HIGH);
  pinMode(FIRE_BUTTON, INPUT);
  digitalWrite(FIRE_BUTTON, HIGH);
  pinMode(FIRE_BUTTON2, INPUT);
  digitalWrite(FIRE_BUTTON2, HIGH);

  pinMode(VENTING, OUTPUT);
  digitalWrite(VENTING, LOW);
  pinMode(RUMBLE, OUTPUT);
  digitalWrite(RUMBLE, LOW);
  pinMode(ISPLAYING_OUT, INPUT);
  pinMode(AUX_LED_PIN, OUTPUT);

  wandLights.begin();
  wandLights.setBrightness(240);
  clearWandLEDs();
  wandLights.show();

  auxLights.begin();
  auxLights.setBrightness(30);
  clearAuxLEDs();

  PackLEDs.begin();
  PackLEDs.Color1 = PackLEDs.Wheel(255);
  PackLEDs.Color2 = PackLEDs.Wheel(170);
  PackLEDs.setBrightness(10);
  clearPackLEDsAll();

  delay(100);
  initDebouncedInput(inD4);
  initDebouncedInput(inD5);
  initDebouncedInput(inD6);
  initDebouncedInput(inD7);
  initDebouncedInput(inD8);

  // Play the Power On sound only once per real power-up/reset.
  // A1 LED 2 should already remain steadily lit. While 0001 is playing,
  // ST_STANDBY does not accept inputs yet.
  setState(ST_STANDBY);
  renderAuxStandby();
  auxLights.show();
  powerOnSoundPending = true;
  playCombinedTransition(SND_XF_POWERON_STANDBY, SND_STANDBY_LOOP);
}

/************************* LOOP *************************/
void loop() {
  unsigned long now = millis();

  updateDebouncedInput(inD4, now);
  updateDebouncedInput(inD5, now);
  updateDebouncedInput(inD6, now);
  updateDebouncedInput(inD7, now);
  updateDebouncedInput(inD8, now);

  /********************* State Logic *********************/
  switch (state) {
    case ST_STANDBY:
      // On a real power-up, 0022.wav plays: 0001 -> crossfade -> 0020.
      // Inputs remain locked until the nominal end of the uploaded 0001 section.
      // After that, the 0020 section of the combined file may continue playing.
      if (powerOnSoundPending) {
        if ((now - packSoundStartedAt) >= LEN_0001_MS) {
          powerOnSoundPending = false;
        }
        break;
      }

      // Priority order: switch errors first, D7 starts Boot,
      // Fire buttons control music. Any new state interrupts 0022 immediately.
      if (wandSwitchOn() || musicSwitchOn()) {
        enterStandbyError();
      } else if (packSwitchOn()) {
        enterBoot();
      } else if (inD4.fell) {
        musicHasStarted = false;
        selectNextMusicTrack();
        setState(ST_MUSIC);
      } else if (inD5.fell) {
        musicHasStarted = false;
        selectPreviousMusicTrack();
        setState(ST_MUSIC);
      } else {
        servicePackLoopWatchdog(now, SND_STANDBY_LOOP);
      }
      break;

    case ST_MUSIC:
      if (wandSwitchOn() || musicSwitchOn()) {
        enterStandbyError();
      } else if (packSwitchOn()) {
        enterBoot(); // playPackSound safely ends Music Repeat
      } else if (inD4.fell) {
        selectNextMusicTrack();
      } else if (inD5.fell) {
        selectPreviousMusicTrack();
      }
      break;

    case ST_BOOT: {
      // 0023.wav contains 0002 -> crossfade -> 0003. Fire D4/D5 remain
      // intentionally ignored during Boot. After the real 0002 duration, only
      // state/animation change to Idle 1; the combined WAV keeps playing.
      if (!packSwitchOn()) {
        startPowerDown(false); // 0015 Off
      } else if (wandSwitchOn() || musicSwitchOn()) {
        startPowerDown(true);  // 0018 Error Off
      } else if ((now - stateStartedAt) >= LEN_0002_MS) {
        firstSuccessfulBootDone = true;
        enterIdle1KeepAudio();
      }
      break;
    }

    case ST_IDLE1:
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (musicSwitchOn()) {
        startPowerDown(true);
      } else if (wandSwitchOn()) {
        enterRamp1On();
      } else if (fire1Pressed() || fire2Pressed()) {
        startPowerDown(true);
      } else {
        servicePackLoopWatchdog(now, SND_PACK_IDLE);
      }
      break;

    case ST_RAMP1_ON:
      // 0024.wav = 0004 -> crossfade -> 0005.
      // FIX18: D6 may be switched OFF again while Ramp 1 On is still running.
      // Immediately start the opposite direction 0006 -> 0003, with no Error/Idle wait.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        enterRamp1Down();
      } else if (musicSwitchOn() || anyFirePressed()) {
        startPowerDown(true);
      } else if ((now - stateStartedAt) >= LEN_0004_MS) {
        enterIdle2KeepAudio();
      }
      break;

    case ST_IDLE2:
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (fire1Pressed() || fire2Pressed()) {
        startPowerDown(true);
      } else if (!wandSwitchOn()) {
        enterRamp1Down();
      } else if (musicSwitchOn()) {
        enterRamp2On();
      } else {
        servicePackLoopWatchdog(now, SND_RAMP1_IDLE);
      }
      break;

    case ST_RAMP1_DOWN:
      // 0025.wav = 0006 -> crossfade -> 0003.
      // FIX18: D6 may be switched ON again while Ramp 1 Down is still running.
      // Immediately start the opposite direction 0004 -> 0005, with no Error/Idle wait.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (wandSwitchOn()) {
        enterRamp1On();
      } else if (musicSwitchOn() || anyFirePressed()) {
        startPowerDown(true);
      } else if ((now - stateStartedAt) >= LEN_0006_MS) {
        enterIdle1KeepAudio();
      }
      break;

    case ST_RAMP2_ON:
      // 0026.wav = 0007 -> crossfade -> 0008.
      // FIX18: D8 OFF immediately reverses direction while Ramp 2 On is still running.
      // D6 must remain ON for Ramp 2.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        startPowerDown(true);
      } else if (!musicSwitchOn()) {
        enterRamp2Down();
      } else if (anyFirePressed()) {
        startPowerDown(true);
      } else if ((now - stateStartedAt) >= LEN_0007_MS) {
        enterIdle3KeepAudio();
      }
      break;

    case ST_IDLE3:
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        startPowerDown(true);
      } else if (!musicSwitchOn()) {
        enterRamp2Down();
      } else if (fire1Pressed() || fire2Pressed()) {
        enterFiring();
      } else {
        servicePackLoopWatchdog(now, SND_RAMP2_IDLE);
      }
      break;

    case ST_RAMP2_DOWN:
      // 0027.wav = 0009 -> crossfade -> 0005.
      // FIX18: D8 ON immediately reverses direction while Ramp 2 Down is still running.
      // D6 must remain ON for Ramp 2.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        startPowerDown(true);
      } else if (musicSwitchOn()) {
        enterRamp2On();
      } else if (anyFirePressed()) {
        startPowerDown(true);
      } else if ((now - stateStartedAt) >= LEN_0009_MS) {
        enterIdle2KeepAudio();
      }
      break;

    case ST_FIRING:
    case ST_WARNING: {
      if (handleActiveAbortSwitches()) break;

      unsigned long firingElapsed = now - stateStartedAt;

      // Play 0010 (D4) or 0021 (D5) completely, then 0011 Fire Loop
      // as long as FASTWARNING has not yet been reached.
      if (!fireIntroFinished && packSoundFinished()) {
        fireIntroFinished = true;
        loopPackSound(SND_FIRE_LOOP);
      }

      // The 10-second threshold has priority over button release in the same loop cycle.
      if (firingElapsed >= FIRING_ALARM_MS) {
        playPackSound(SND_PACK_ALARM);
        setState(ST_FASTWARNING);
      } else if (!anyFirePressed()) {
        enterFireTail();
      } else if (state == ST_FIRING && firingElapsed >= FIRING_WARNING_MS) {
        // Important: preserve total firing time. Do NOT reset stateStartedAt.
        state = ST_WARNING;
      }
      break;
    }

    case ST_FASTWARNING:
      if (handleActiveAbortSwitches()) break;
      if (!anyFirePressed()) {
        enterVentPreDelay();
      }
      break;

    case ST_FIRE_TAIL:
      if (handleActiveAbortSwitches()) break;
      // 0028.wav = 0012 -> crossfade -> 0008. The Fire buttons themselves remain
      // ignored during the actual 0012 section.
      if ((now - stateStartedAt) >= LEN_0012_MS) {
        enterIdle3KeepAudio();
      }
      break;

    case ST_VENT_PREDELAY:
      if (handleActiveAbortSwitches()) break;
      if ((now - stateStartedAt) >= VENT_PREDELAY_MS) {
        enterVentActive();
      }
      break;

    case ST_VENT_ACTIVE:
      if (handleActiveAbortSwitches()) break;
      // The 0014 section of 0032 ends after approximately 5.521 seconds total.
      // Since ST_VENT_ACTIVE begins after 2 seconds, 3.521 seconds remain here.
      if ((now - stateStartedAt) >= (LEN_0014_MS - VENT_PREDELAY_MS)) {
        enterPostVentSoundKeepAudio();
      }
      break;

    case ST_POST_VENT_SOUND:
      // 0032 keeps playing. FIX19: From the start of 0019, D8 may already
      // be switched OFF to immediately start Ramp 2 Down, without Error
      // and without waiting for 0016 / Idle 3.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        startPowerDown(true);
      } else if (!musicSwitchOn()) {
        enterRamp2Down();
      } else if ((now - stateStartedAt) >= LEN_0019_MS) {
        // If D8 remains ON, change only the state; 0032 keeps playing
        // into the 0016 section without sending a new DFPlayer command.
        enterAfterVentKeepAudio();
      }
      break;

    case ST_AFTER_VENT:
      // Special rule: D8 OFF during 0016 -> 0009 Ramp 2 Down -> Idle 2.
      if (!packSwitchOn()) {
        startPowerDown(false);
      } else if (!wandSwitchOn()) {
        startPowerDown(true);
      } else if (!musicSwitchOn()) {
        enterRamp2Down();
      } else if ((now - stateStartedAt) >= LEN_0016_MS) {
        // The 0008 section is already part of 0032 and keeps playing.
        enterIdle3KeepAudio();
      }
      // D4/D5 are intentionally ignored during the 0016 section.
      break;

    case ST_ERROR_STANDBY: {
      // Continuously monitor BUSY even while a switch is still ON.
      // This prevents missing the sound ending. Afterwards, continue waiting for ALL OFF.
      bool errorSoundDone = packSoundFinished();
      if (errorSoundDone && allControlsOff()) {
        enterStandby();
      }
      break;
    }

    case ST_POWERDOWN: {
      // FIX17: Normal PowerDown (0015) and Error PowerDown (0018) behave
      // the same when powering back on. During the running PowerDown,
      // ALL controls must first have been observed OFF once. Then D7 may be
      // switched ON again and the still-running PowerDown is immediately replaced
      // by Boot.
      if (allControlsOff()) {
        powerDownAllOffSeen = true;
      }

      if (powerDownAllOffSeen && packSwitchOn() &&
          !wandSwitchOn() && !musicSwitchOn() && !anyFirePressed()) {
        enterBoot();
        break;
      }

      // Continuously monitor BUSY while the PowerDown animation is running.
      // This prevents missing an earlier sound ending.
      bool powerDownSoundDone = packSoundFinished();

      // 014 Off or 017 Error Off plays in parallel with the PowerDown animation.
      // If no early D7 reboot occurs, WAIT-ALL-OFF still begins only
      // after BOTH the animation and sound are completely finished.
      if ((now - stateStartedAt) >= POWERDOWN_DURATION_MS && powerDownSoundDone) {
        enterWaitAllOff();
      }
      break;
    }

    case ST_WAIT_ALL_OFF:
      if (allControlsOff()) {
        enterStandby();
      }
      break;
  }

  /********************* Update Animations *********************/
  updatePackAndBarGraph(now);
  updateWandLights(now);
  updateAuxLights(now);
}
