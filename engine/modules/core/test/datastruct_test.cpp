/**
 * Unit tests for engine data structures (RingBuffer, DiscreteEventStream).
 *
 * Build: cmake --build . --target datastruct_test
 * Run:   ctest -R datastruct_test
 */

#include <test_macros.hpp>
#include <vector>

#include "oge/event_stream.hpp"
#include "oge/ring_buffer.hpp"


// =========================================================================
// RingBuffer tests
// =========================================================================

// m_head starts at 1.  Push() writes at m_head % Capacity then increments.
// So after N pushes, indices are [1, 1+N).

TEST(rb_push_and_get) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(42);
    CHECK_EQ(rb.Get(1), 42);
}

TEST(rb_multiple_push) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(10); rb.Push(20); rb.Push(30);
    CHECK_EQ(rb.Get(1), 10);
    CHECK_EQ(rb.Get(2), 20);
    CHECK_EQ(rb.Get(3), 30);
}

TEST(rb_head) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(1); rb.Push(2);
    CHECK_EQ(rb.Head(), 2);
}

TEST(rb_wrap_around) {
    oge::RingBuffer<int, 4> rb;
    for (int i = 0; i < 6; ++i) rb.Push(i);
    CHECK_EQ(rb.Head(), 5);
    CHECK(rb.Contains(3));
    CHECK(!rb.Contains(1));
}

TEST(rb_head_cursor) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(10); rb.Push(20);
    CHECK_EQ(rb.HeadCursor(), 3u);  // starts at 1, +2 pushes = 3
}

TEST(rb_non_const_get) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(100);
    rb.Get(1) = 200;
    CHECK_EQ(rb.Get(1), 200);
}

TEST(rb_non_const_head) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(100);
    rb.Head() = 300;
    CHECK_EQ(rb.Head(), 300);
}

// =========================================================================
// DiscreteEventStream tests (inherits RingBuffer; m_head starts at 1)
// =========================================================================

TEST(es_push_poll) {
    oge::DiscreteEventStream<int, 16> es;
    es.Push(10); es.Push(20);
    int out = 0;
    // m_head starts at 1, so first event is at index 1.
    oge::DiscreteEventStream<int, 16>::Cursor c{1};
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 10);
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 20);
    CHECK(!es.PollOne(c, out));
}

TEST(es_advance_cursor) {
    oge::DiscreteEventStream<int, 16> es;
    es.Push(1); es.Push(2); es.Push(3);
    oge::DiscreteEventStream<int, 16>::Cursor c{1};
    int out = 0;
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 1);
    es.AdvanceCursor(c);  // advance to head → nothing left
    CHECK(!es.PollOne(c, out));
}

TEST(es_frontier) {
    oge::DiscreteEventStream<int, 16> es;
    es.Push(100); es.Push(200);
    oge::DiscreteEventStream<int, 16>::Cursor c{1};
    int out = 0;
    CHECK(es.PollOne(c, out, es.HeadCursor()));
    CHECK_EQ(out, 100);
    CHECK(es.PollOne(c, out, es.HeadCursor()));
    CHECK_EQ(out, 200);
}

TEST(es_capacity_boundary) {
    oge::DiscreteEventStream<int, 4> es;
    for (int i = 0; i < 6; ++i) es.Push(i);
    int out = 0;
    // 6 pushes: indices 1..6. Wraps at capacity 4, so 3 and 4 are overwritten.
    // Valid data at indices 3,4,5,6 → values 2,3,4,5.
    oge::DiscreteEventStream<int, 4>::Cursor c{3};
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 2);
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 3);
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 4);
    CHECK(es.PollOne(c, out)); CHECK_EQ(out, 5);
    CHECK(!es.PollOne(c, out));
}

RUN_TESTS("Datastruct Tests")
