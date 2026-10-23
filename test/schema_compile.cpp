#include <saitekikuukan/soa.hpp>
struct Row { int first, second; };
struct Other { int field; };
struct Flags { bool active; };

#if TEST_CASE == 1
using Invalid = kk::SoA<Row, kk::MemberList<&Row::first, &Row::first>>;
#elif TEST_CASE == 2
using Invalid = kk::SoA<Row, kk::MemberList<&Other::field>>;
#elif TEST_CASE == 3
using Invalid = kk::SoA<Flags, kk::MemberList<&Flags::active>>;
#elif TEST_CASE == 4
using Invalid = kk::SoA<Row, kk::MemberList<&Row::first>>;
#endif
int main() {
    Invalid rows;
#if TEST_CASE == 4
    (void)rows.column<&Row::second>();
#endif
}
