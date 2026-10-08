/*
 * G1-R1 negative test: a fault while this hart holds the console lock must
 * still produce a FAIL verdict (not a hang). fx1_log() dereferences an
 * unmapped string pointer; the default trap handler reports the load access
 * fault (mcause 5) and fails with code 0xE005. Registered with an expected
 * exit status of 1 and that code.
 */
#include "fx1_fw.h"

int main(void)
{
    fx1_log("about to fault inside fx1_log");
    fx1_log((const char*)0x60000000u); /* unmapped: load access fault */
    fx1_log("unreachable");
    return 0x7F;
}
