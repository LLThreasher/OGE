/**
 * Unit tests for engine data structures (RingBuffer, DiscreteEventStream).
 *
 * Build: cmake --build . --target datastruct_test
 * Run:   ctest -R datastruct_test
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "oge/event_stream.hpp"
#include "oge/ring_buffer.hpp"

static int g_passed = 0, g_failed = 0;
#define TEST(n) static void n(); struct R##n{R##n(){t.push_back({#n,n});}}r##n; static void n()
#define CHK(e) do{if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);++g_failed;return;}}while(0)
#define CHKEQ(a,b) do{if(!((a)==(b))){std::fprintf(stderr,"FAIL %s:%d: %s!=%s\n",__FILE__,__LINE__,#a,#b);++g_failed;return;}}while(0)
struct T{const char*n;void(*f)();};static std::vector<T> t;

// =========================================================================
// RingBuffer tests
// =========================================================================

// m_head starts at 1.  Push() writes at m_head % Capacity then increments.
// So after N pushes, indices are [1, 1+N).

TEST(rb_push_and_get) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(42);
    CHKEQ(rb.Get(1), 42);
}

TEST(rb_multiple_push) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(10); rb.Push(20); rb.Push(30);
    CHKEQ(rb.Get(1), 10);
    CHKEQ(rb.Get(2), 20);
    CHKEQ(rb.Get(3), 30);
}

TEST(rb_head) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(1); rb.Push(2);
    CHKEQ(rb.Head(), 2);
}

TEST(rb_wrap_around) {
    oge::RingBuffer<int, 4> rb;
    for (int i = 0; i < 6; ++i) rb.Push(i);
    CHKEQ(rb.Head(), 5);
    CHK(rb.Contains(3));
    CHK(!rb.Contains(1));
}

TEST(rb_head_cursor) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(10); rb.Push(20);
    CHKEQ(rb.HeadCursor(), 3u);  // starts at 1, +2 pushes = 3
}

TEST(rb_non_const_get) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(100);
    rb.Get(1) = 200;
    CHKEQ(rb.Get(1), 200);
}

TEST(rb_non_const_head) {
    oge::RingBuffer<int, 8> rb;
    rb.Push(100);
    rb.Head() = 300;
    CHKEQ(rb.Head(), 300);
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
    CHK(es.PollOne(c, out)); CHKEQ(out, 10);
    CHK(es.PollOne(c, out)); CHKEQ(out, 20);
    CHK(!es.PollOne(c, out));
}

TEST(es_advance_cursor) {
    oge::DiscreteEventStream<int, 16> es;
    es.Push(1); es.Push(2); es.Push(3);
    oge::DiscreteEventStream<int, 16>::Cursor c{1};
    int out = 0;
    CHK(es.PollOne(c, out)); CHKEQ(out, 1);
    es.AdvanceCursor(c);  // advance to head → nothing left
    CHK(!es.PollOne(c, out));
}

TEST(es_frontier) {
    oge::DiscreteEventStream<int, 16> es;
    es.Push(100); es.Push(200);
    oge::DiscreteEventStream<int, 16>::Cursor c{1};
    int out = 0;
    CHK(es.PollOne(c, out, es.HeadCursor()));
    CHKEQ(out, 100);
    CHK(es.PollOne(c, out, es.HeadCursor()));
    CHKEQ(out, 200);
}

TEST(es_capacity_boundary) {
    oge::DiscreteEventStream<int, 4> es;
    for (int i = 0; i < 6; ++i) es.Push(i);
    int out = 0;
    // 6 pushes: indices 1..6. Wraps at capacity 4, so 3 and 4 are overwritten.
    // Valid data at indices 3,4,5,6 → values 2,3,4,5.
    oge::DiscreteEventStream<int, 4>::Cursor c{3};
    CHK(es.PollOne(c, out)); CHKEQ(out, 2);
    CHK(es.PollOne(c, out)); CHKEQ(out, 3);
    CHK(es.PollOne(c, out)); CHKEQ(out, 4);
    CHK(es.PollOne(c, out)); CHKEQ(out, 5);
    CHK(!es.PollOne(c, out));
}

// =========================================================================
int main() {
    std::fprintf(stdout,"=== Datastruct Tests ===\n");
    for(auto& e:t){int b=g_failed;e.f();if(g_failed==b){++g_passed;std::fprintf(stdout,"  PASS %s\n",e.n);}}
    std::fprintf(stdout,"\nResults: %d passed, %d failed\n",g_passed,g_failed);
    return g_failed>0?EXIT_FAILURE:EXIT_SUCCESS;
}
