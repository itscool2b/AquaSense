#include "filters.h"

#include <math.h>

SmoothedReading::SmoothedReading(SensorLimits limits) : limits_(limits) {}

void SmoothedReading::clear() {
  len_ = 0;
  head_ = 0;
  pending_len_ = 0;
  misses_ = 0;
}

void SmoothedReading::push(float v) {
  window_[head_] = v;
  head_ = (head_ + 1) % kWindow;
  if (len_ < kWindow) {
    len_++;
  }
}

float SmoothedReading::median() const {
  float sorted[kWindow];
  for (int i = 0; i < len_; i++) {
    sorted[i] = window_[i];
  }
  for (int i = 1; i < len_; i++) {
    float v = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > v) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = v;
  }
  if (len_ % 2) {
    return sorted[len_ / 2];
  }
  return (sorted[len_ / 2 - 1] + sorted[len_ / 2]) / 2.0f;
}

float SmoothedReading::value() const {
  if (len_ == 0) {
    return NAN;
  }
  float sum = 0;
  for (int i = 0; i < len_; i++) {
    sum += window_[i];
  }
  return sum / len_;
}

void SmoothedReading::miss() {
  misses_++;
  if (misses_ >= kMaxMisses) {
    len_ = 0;
    head_ = 0;
    pending_len_ = 0;
  }
}

bool SmoothedReading::add(float raw) {
  if (isnan(raw) || isinf(raw) || raw < limits_.min || raw > limits_.max) {
    rejected_++;
    miss();
    return false;
  }
  misses_ = 0;

  // Too few readings to judge a jump yet.
  if (len_ < kConfirm) {
    pending_len_ = 0;
    push(raw);
    return true;
  }

  if (fabsf(raw - median()) <= limits_.max_jump) {
    pending_len_ = 0;
    push(raw);
    return true;
  }

  // A jump. Hold it until we know whether it is noise or a real change.
  pending_[pending_len_++] = raw;
  if (pending_len_ < kConfirm) {
    rejected_++;
    return false;
  }

  float lo = pending_[0];
  float hi = pending_[0];
  for (int i = 1; i < pending_len_; i++) {
    if (pending_[i] < lo) lo = pending_[i];
    if (pending_[i] > hi) hi = pending_[i];
  }
  if (hi - lo <= limits_.max_jump) {
    // Three jumps that agree: the water really changed.
    float confirmed[kConfirm];
    for (int i = 0; i < kConfirm; i++) {
      confirmed[i] = pending_[i];
    }
    len_ = 0;
    head_ = 0;
    pending_len_ = 0;
    for (int i = 0; i < kConfirm; i++) {
      push(confirmed[i]);
    }
    rejected_ -= kConfirm - 1;  // the held readings were counted as rejected
    return true;
  }

  // Scattered jumps: noise. Keep only the newest as a candidate.
  pending_[0] = raw;
  pending_len_ = 1;
  rejected_++;
  return false;
}
