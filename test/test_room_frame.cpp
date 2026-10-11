// Host test for the Room Sensor frame encoder (data layer only - no hardware/timing involved).
//   g++ -std=c++17 -Wall -I components/thermia_room components/thermia_room/thermia_room_frame.cpp
//   test/test_room_frame.cpp -o test/test_room_frame && ./test/test_room_frame

#include <cstdio>
#include <cstring>

#include "thermia_room_frame.h"

using namespace esphome::thermia_room;

static int g_failed = 0;
static int g_checks = 0;
#define CHECK(cond, ...)                                          \
  do {                                                            \
    g_checks++;                                                   \
    if (!(cond)) {                                                \
      g_failed++;                                                 \
      printf("  FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);  \
      printf(__VA_ARGS__);                                        \
      printf("\n");                                               \
    }                                                              \
  } while (0)

static void test_checksum_formula() {
  // Rows read off the author's own table (thermiq_table.jpg, "MSB oikealla, 1=0 ja 01=1"), plus 20.2 confirmed
  // against the 20.2.png line capture: checksum = integer ^ decimal ^ (setpoint_half & 31), setpoint 21.0 -> 42 -> 0b01010.
  const struct { uint8_t i, d, c; } rows[] = {{20, 1, 31}, {20, 2, 28}, {20, 3, 29}, {20, 4, 26}, {20, 6, 24},
                                              {20, 7, 25}, {20, 8, 22}, {21, 9, 22}, {22, 0, 28}, {22, 1, 29},
                                              {22, 2, 30}, {20, 5, 27}};
  for (auto &r : rows)
    CHECK(frame_checksum_ok(r.i, r.d, r.c), "%u.%u -> checksum should be %u", r.i, r.d, r.c);
  CHECK(!frame_checksum_ok(20, 2, 30), "old (52-i-d) value for 20.2 must be rejected");
}

static void test_field_layout() {
  RoomFrame f = encode_room_temperature(20.5f);
  CHECK(f.integer_part == 20, "integer_part got %u", f.integer_part);
  CHECK(f.decimal_part == 5, "decimal_part got %u", f.decimal_part);
  CHECK(f.checksum == 27, "checksum got %u (20^5^10=27)", f.checksum);

  // The author's own 20.5 C debug string (esp32_implementation.png): "I" = raw 1, "_I" = raw 0.
  const char *capture = "____II_II_II_IIIII_II_IIII_II_II_I_II_I_I_III";
  char got[FRAME_BITS * 2 + 5] = "____";
  int pos = 4;
  for (uint8_t i = 0; i < FRAME_BITS; i++) {
    if (!f.bits[i] && i != 0)  // a leading long space merges into the idle line
      got[pos++] = '_';
    got[pos++] = 'I';
  }
  got[pos] = '\0';
  CHECK(std::strcmp(got, capture) == 0, "20.5 frame\n    got      %s\n    expected %s", got, capture);
}

// Real-sensor line captures from the blog (20.1.png, 20.2.png), pulse gaps measured off the images.
static void test_line_captures() {
  const struct { float t; const char *wave; } caps[] = {
      {20.1f, "II_II_II_IIIII_II_IIII_IIII_I_I_I_I_I_III"},
      {20.2f, "II_II_II_IIIII_II_IIIII_IIIII_I_I_I_III"},
  };
  for (auto &c : caps) {
    RoomFrame f = encode_room_temperature(c.t);
    char got[FRAME_BITS * 2 + 1];
    int pos = 0;
    for (uint8_t i = 0; i < FRAME_BITS; i++) {
      if (!f.bits[i] && i != 0)
        got[pos++] = '_';
      got[pos++] = 'I';
    }
    got[pos] = '\0';
    CHECK(std::strcmp(got, c.wave) == 0, "%.1f frame\n    got      %s\n    expected %s", c.t, got, c.wave);
  }
}

static void test_round_trip() {
  // Every representable value (0.0 .. 31.9 in 0.1 steps) must survive encode -> decode, and its checksum must
  // verify - this is the same round trip a receiver would perform.
  for (int tenths = 0; tenths <= 319; tenths++) {
    float temp = tenths / 10.0f;
    RoomFrame f = encode_room_temperature(temp);
    uint8_t sp, i, d, c;
    decode_room_frame(f.bits, sp, i, d, c);
    CHECK(sp == 42, "round-trip setpoint mismatch at %.1f (got %u, want 42 = 21.0 C)", temp, sp);
    CHECK(i == f.integer_part, "round-trip integer_part mismatch at %.1f", temp);
    CHECK(d == f.decimal_part, "round-trip decimal_part mismatch at %.1f", temp);
    CHECK(c == f.checksum, "round-trip checksum mismatch at %.1f", temp);
    CHECK(frame_checksum_ok(i, d, c), "round-tripped frame should self-verify at %.1f", temp);
  }
}

static void test_setpoint() {
  // Setpoint is carried in half degrees; the default 21.0 C (42) must leave every frame bit-identical to the
  // captures - covered by test_line_captures/test_field_layout.
  for (int half = (int) (SETPOINT_MIN * 2); half <= (int) (SETPOINT_MAX * 2); half++) {
    RoomFrame f = encode_room_temperature(22.3f, half / 2.0f);
    uint8_t s2, i, d, c;
    decode_room_frame(f.bits, s2, i, d, c);
    CHECK(s2 == half && i == 22 && d == 3, "setpoint %.1f round-trip got half=%u %u.%u", half / 2.0f, s2, i, d);
    CHECK(c == ((22 ^ 3 ^ half) & 0x1F), "setpoint %.1f checksum %u", half / 2.0f, c);
    CHECK(frame_checksum_ok(i, d, c, (uint8_t) half), "setpoint %.1f frame should self-verify", half / 2.0f);
  }
  // Accepted by the pump in the 2026-10-08 brute force (payload 23.0): setpoint 20 -> chk 31, 22 -> chk 27.
  CHECK(encode_room_temperature(23.0f, 20.0f).checksum == 31, "20.0 C setpoint must give the accepted chk 31");
  CHECK(encode_room_temperature(23.0f, 22.0f).checksum == 27, "22.0 C setpoint must give the accepted chk 27");
  // Rounds to 0.5 C and clamps.
  CHECK(encode_room_temperature(20.0f, 21.3f).setpoint_half == 43, "21.3 rounds to 21.5");
  CHECK(encode_room_temperature(20.0f, 1.0f).setpoint_half == (uint8_t) (SETPOINT_MIN * 2), "setpoint clamps low");
  CHECK(encode_room_temperature(20.0f, 99.0f).setpoint_half == (uint8_t) (SETPOINT_MAX * 2), "setpoint clamps high");

  // Constant start/tail bits.
  RoomFrame d = encode_room_temperature(20.2f);
  CHECK(d.bits[0] == 1 && d.bits[26] == 0 && d.bits[27] == 1 && d.bits[28] == 1, "start/tail must be 1/011");
}

static void test_clamping() {
  RoomFrame low = encode_room_temperature(-5.0f);
  CHECK(low.integer_part == 0 && low.decimal_part == 0, "negative temp clamps to 0.0");

  RoomFrame high = encode_room_temperature(99.0f);
  CHECK(high.integer_part == 31 && high.decimal_part == 9, "out-of-range temp clamps to 31.9");
}

int main() {
  test_checksum_formula();
  test_field_layout();
  test_line_captures();
  test_round_trip();
  test_setpoint();
  test_clamping();

  printf("%d/%d checks passed\n", g_checks - g_failed, g_checks);
  return g_failed == 0 ? 0 : 1;
}
