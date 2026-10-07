#pragma once

/*
 * Averaging and bad-reading rejection. Plain C++ (no Arduino) so it can be
 * unit-tested on a computer: `make -C firmware test`.
 *
 * Each sensor keeps its last 10 accepted readings and reports their mean.
 * A reading is rejected when it is
 *   - not a number, or outside the sensor's possible range, or
 *   - a sudden jump: further than `max_jump` from the median of the window.
 *
 * A real change (moving the probe from pH 7 buffer to pH 4 buffer) also looks
 * like a jump. When 3 jumps in a row agree with each other, they are treated
 * as the new level: the window is cleared and refilled from them.
 */

struct SensorLimits {
  float min;       // lowest believable value
  float max;       // highest believable value
  float max_jump;  // biggest believable change between two readings
};

class SmoothedReading {
 public:
  static const int kWindow = 10;
  static const int kConfirm = 3;
  static const int kMaxMisses = 5;

  explicit SmoothedReading(SensorLimits limits);

  // Adds a raw reading. Returns true if it was accepted.
  bool add(float raw);

  // The sensor did not answer. After kMaxMisses misses in a row the value
  // is dropped so the display shows "--" and the upload sends null.
  void miss();

  bool has_value() const { return len_ > 0; }
  float value() const;  // mean of the window; NaN when empty
  int count() const { return len_; }
  unsigned long rejected() const { return rejected_; }
  void clear();

 private:
  float median() const;
  void push(float v);

  SensorLimits limits_;
  float window_[kWindow];
  int len_ = 0;
  int head_ = 0;
  float pending_[kConfirm];
  int pending_len_ = 0;
  int misses_ = 0;
  unsigned long rejected_ = 0;
};
