#include "M5Sound.h"

#include <FreeRTOS.h>
#include <driver/i2s.h>
#include "utility/Config.h"
#include "AXP192.h"



/////////////////////////////////////////////////////////////////////////////
//
// M5Sound
//
/////////////////////////////////////////////////////////////////////////////


/* static */ M5Sound* M5Sound::instance;

M5Sound::M5Sound() {
  if (!instance) instance = this;
  _bytes_left = 0;
}

void M5Sound::begin() {
  // Set up I2S driver
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLERATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  // Configure pins
  i2s_pin_config_t pin_config= {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  // Set up clock
  i2s_set_clk(I2S_NUM_0, SAMPLERATE, I2S_BITS_PER_SAMPLE_16BIT,
              I2S_CHANNEL_STEREO);
  // Turn on system speaker
  AXP->SetSpkEnable(true);
  _silentSince = millis();
}

void M5Sound::delay(uint16_t msec) {
  uint32_t start = millis();
  while (millis() - start < msec) {
    update();
    yield();
  }
}

void M5Sound::waitForSilence(uint16_t msec /* = 0 */) {
  while (!silence(msec)) {
    update();
    yield();
  }
}

bool M5Sound::silence(uint16_t msec /* = 0 */) {
  if (!_silentSince) return false;
  if (millis() - _silentSince >= msec) return true;
  return false;
}


void M5Sound::update() {
  // If last packet is gone, make a new one
  if (!_bytes_left) {
    // Ask synths what they have and mix in 32-bit signed mix buffer
    memset(_mixbuf, 0, BUFLEN * 4);   // 32 bits
    _silentSince = millis();
    for (auto synth : Synth::instances) {
      if (synth->fillSbuf()) {
        _silentSince = 0;
        for (uint16_t i = 0; i < BUFLEN; i++) {
          _mixbuf[i] += synth->_sbuf[i];
        }
      }
    }
    for (uint16_t i = 0; i < BUFLEN; i++) {
      int32_t m = _mixbuf[i];
      // clip to 16-bit signed so we get "real" distortion not noise
      if (m <= -32768) m = -32768;
       else if (m >= 32767) m = 32767;
      _buf[i * 2] = m;
      _buf[(i * 2) + 1] = m;
    }
    _bytes_left = BUFLEN * 4;
  }

  if (_bytes_left) {
    // Send what can be sent but don't hang around, send rest next time.
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, _buf + (BUFLEN * 4) - _bytes_left, _bytes_left,
              &bytes_written, 0);
    _bytes_left -= bytes_written;
  }
}



/////////////////////////////////////////////////////////////////////////////
//
// Synth
//
/////////////////////////////////////////////////////////////////////////////

/* static */ std::vector<Synth*> Synth::instances;

Synth::Synth(waveform_t waveform_ /* = SINE */, float freq_ /* = 0 */,
             uint16_t attack_ /* = 10 */, uint16_t decay_ /* = 0 */,
             float sustain_ /* = 1.0 */, uint16_t release_ /* = 10 */,
             float gain_ /* = 0.5 */) {
  waveform = waveform_;
  freq = freq_;
  attack = attack_;
  decay = decay_;
  sustain = sustain_;
  release = release_;
  gain = gain_;
  phase = 0;
  instances.push_back(this);
  startTime = stopTime = 0;
}

Synth::~Synth() {
  for (int i = 0; i < instances.size(); ++i) {
    if (instances[i] == this) {
      instances.erase(instances.begin() + i);
      return;
    }
  }
}

bool Synth::fillSbuf() {
  if (!freq || !startTime) return false;
  // Envelope calculation
  uint32_t duration = millis() - startTime;
  if (duration < attack) {
    envelope = (float)duration / attack;
  } else if (decay && sustain < 1 && duration < attack + decay) {
    envelope = 1 - (((float)(duration - attack) / decay) * (1 - sustain));
  } else {
    envelope = sustain;
  }
  if (stopTime && millis() > stopTime) {
    uint32_t stopping_for = millis() - stopTime;
    if (stopping_for < release) {
      envelope = sustain - (((float)stopping_for / release) * sustain);
    } else {
      startTime = stopTime = 0;
      envelope = 0;
    }
  }

  // Make some waves
  uint16_t amplitude = scaleAmplitude(gain * envelope);
  float steps = ((float)freq / SAMPLERATE);
  switch (waveform) {
    case SINE:
      for(uint16_t i = 0; i < BUFLEN; i++) {
        _sbuf[i] = sin((phase + (i * steps)) * TWO_PI) * amplitude;
      }
      break;
    case SQUARE:
      for(uint16_t i = 0; i < BUFLEN; i++) {
        float t = phase + (i * steps);
        t -= (int)t;
        _sbuf[i] = (t > 0.5 ? -1 : 1) * amplitude;
      }
      break;
    case TRIANGLE:
      for(uint16_t i = 0; i < BUFLEN; i++) {
        float t = phase + (i * steps);
        t -= (int)t;
        if (t < 0.25) t *= 4;
         else if (t < 0.75) t = 2 - (t * 4);
          else t = -4 + (t * 4);
        _sbuf[i] = t * amplitude;
      }
      break;
    case SAWTOOTH:
      for(uint16_t i = 0; i < BUFLEN; i++) {
        float t = phase + (i * steps);
        t -= (int)t;
        if (t < 0.5) t *= 2;
         else t = -2 + (t * 2);
        _sbuf[i] = t * amplitude;
      }
      break;
    case NOISE:
      for(uint16_t i = 0; i < BUFLEN; i++) {
        _sbuf[i] = (rand() % (2 * amplitude)) - amplitude;
      }
      break;
  }
  phase += (float)(BUFLEN) * steps;
  phase -= (int)phase;
  return true;
}

void Synth::start() {
  startTime = millis();
  stopTime = 0;
  phase = 0;
}

void Synth::stop() {
  // At least attack, decay and release get to play
  stopTime = startTime + attack + decay;
  if (millis() > stopTime) stopTime = millis() + 10;
}

void Synth::playFor(uint32_t msec) {
  uint32_t duration = attack + decay + release;
  if (msec > duration) duration = msec;
  startTime = millis();
  stopTime  = millis() + duration;
  phase = 0;
}

int16_t Synth::scaleAmplitude(float gain) {
  // 181 is (almost) the square root of 32768.
  return pow(gain * 181, 2);
}
