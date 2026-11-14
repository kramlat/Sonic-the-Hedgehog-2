#define _POSIX_C_SOURCE 200809L
#include "test.h"

#include <string.h>
#include <time.h>

#include "Backend/NetPlay.h"

static void SleepUs(long us) {
    struct timespec ts = { 0, us * 1000 };
    nanosleep(&ts, NULL);
}

// Network play (the engine's NetPlay): a host and a guest in this one process, over a real WebSocket on the loopback. The password is proved without being sent, a wrong one, a second guest and another game are
// refused, and once the match is on the two sides run in lockstep: each gets the same two pads for every frame, the pads held back by the input delay.

static void PumpBoth(NetPlay *a, NetPlay *b, int ms) {
    for (int i = 0; i < ms; i++) {
        NetPlay_Poll(a);
        NetPlay_Poll(b);
        SleepUs(1000);
    }
}

static bool PumpUntil(NetPlay *a, NetPlay *b, NetPlayStatus a_status, NetPlayStatus b_status) {
    for (int i = 0; i < 5000; i++) {
        NetPlay_Poll(a);
        NetPlay_Poll(b);
        if (NetPlay_Status(a) == a_status && NetPlay_Status(b) == b_status)
            return true;
        SleepUs(1000);
    }
    return false;
}

static void Connect(NetPlay *host, NetPlay *guest, const char *password) {
    CHECK(NetPlay_Host(host, "test", "127.0.0.1", 0, password, 3));
    CHECK(NetPlay_Port(host) != 0);
    CHECK(NetPlay_Join(guest, "test", "127.0.0.1", NetPlay_Port(host), password != NULL ? password : NetPlay_Password(host)));
}

static void Netplay_APasswordIsMadeAndProvedWithoutBeingSent(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    Connect(host, guest, NULL);
    CHECK_EQ(strlen(NetPlay_Password(host)), 23); // (4 groups of 5 and 3 dashes)
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    CHECK_EQ(NetPlay_LocalPlayer(host), 0);
    CHECK_EQ(NetPlay_LocalPlayer(guest), 1);
    CHECK_EQ(NetPlay_InputDelay(guest), 3);        // (the guest uses the host's delay)
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
}

static void Netplay_ThePasswordsCaseAndDashesDoNotMatter(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    CHECK(NetPlay_Host(host, "test", "127.0.0.1", 0, "Ab12-Cd34", 2));
    CHECK(NetPlay_Join(guest, "test", "127.0.0.1", NetPlay_Port(host), "ab12 cd34"));
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
}

static void Netplay_AWrongPasswordIsRefused(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    CHECK(NetPlay_Host(host, "test", "127.0.0.1", 0, "RIGHT-PASSWORD", 3));
    CHECK(NetPlay_Join(guest, "test", "127.0.0.1", NetPlay_Port(host), "WRONG-PASSWORD"));
    CHECK(PumpUntil(host, guest, NETPLAY_LISTENING, NETPLAY_CLOSED));
    CHECK(strstr(NetPlay_Reason(guest), "password") != NULL);
    // (and the host still takes the right one)
    NetPlay *other = NetPlay_Create();
    CHECK(NetPlay_Join(other, "test", "127.0.0.1", NetPlay_Port(host), "RIGHT-PASSWORD"));
    CHECK(PumpUntil(host, other, NETPLAY_READY, NETPLAY_READY));
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
    NetPlay_Destroy(other);
}

static void Netplay_ASecondGuestAndAnotherGameAreRefused(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create(), *second = NetPlay_Create(), *other_game = NetPlay_Create();
    Connect(host, guest, "SHARED-SECRET");
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    CHECK(NetPlay_Join(second, "test", "127.0.0.1", NetPlay_Port(host), "SHARED-SECRET"));
    CHECK(PumpUntil(host, second, NETPLAY_READY, NETPLAY_CLOSED));
    CHECK(strstr(NetPlay_Reason(second), "already") != NULL);
    NetPlay_Destroy(host);
    host = NetPlay_Create();
    CHECK(NetPlay_Host(host, "test", "127.0.0.1", 0, "SHARED-SECRET", 3));
    CHECK(NetPlay_Join(other_game, "another game", "127.0.0.1", NetPlay_Port(host), "SHARED-SECRET"));
    CHECK(PumpUntil(host, other_game, NETPLAY_LISTENING, NETPLAY_CLOSED));
    CHECK(strstr(NetPlay_Reason(other_game), "different game") != NULL);
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
    NetPlay_Destroy(second);
    NetPlay_Destroy(other_game);
}

// Both sides get the same two pads each frame: the pads are held back by the delay (the first frames are idle)
static void Netplay_TheTwoSidesRunInLockstep(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    Connect(host, guest, NULL);
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    const uint8_t start[3] = { 7, 8, 9 };
    CHECK(!NetPlay_StartMatch(guest, start, 3));   // (only the host starts it)
    CHECK(NetPlay_StartMatch(host, start, 3));
    CHECK(PumpUntil(host, guest, NETPLAY_RUNNING, NETPLAY_RUNNING));
    size_t size = 0;
    const uint8_t *data = NetPlay_StartData(guest, &size);
    CHECK_EQ(size, 3);
    CHECK(size == 3 && memcmp(data, start, 3) == 0);

    const int frames = 400, delay = 3;
    uint8_t host_pad[400], guest_pad[400];
    for (int f = 0; f < frames; f++) {
        host_pad[f] = (uint8_t)(f * 7 + 1);
        guest_pad[f] = (uint8_t)(f * 5 + 2);
    }
    int host_f = 0, guest_f = 0, bad = 0;
    for (int spin = 0; spin < 20000 && (host_f < frames || guest_f < frames); spin++) {
        NetPlay_Poll(host);
        NetPlay_Poll(guest);
        uint8_t p1, p2;
        if (host_f < frames && NetPlay_Frame(host, host_pad[host_f], &p1, &p2)) {
            const uint8_t want1 = host_f >= delay ? host_pad[host_f - delay] : 0, want2 = host_f >= delay ? guest_pad[host_f - delay] : 0;
            // (a frame's pad is the one given `delay` calls before it)
            bad += p1 != want1 || p2 != want2;
            host_f++;
        }
        if (guest_f < frames && NetPlay_Frame(guest, guest_pad[guest_f], &p1, &p2)) {
            const uint8_t want1 = guest_f >= delay ? host_pad[guest_f - delay] : 0, want2 = guest_f >= delay ? guest_pad[guest_f - delay] : 0;
            bad += p1 != want1 || p2 != want2;
            guest_f++;
        }
        if (spin % 7 == 0)
            SleepUs(200);
    }
    CHECK_EQ(host_f, frames);
    CHECK_EQ(guest_f, frames);
    CHECK_EQ(bad, 0);
    CHECK_EQ(NetPlay_FrameNumber(host), frames);
    PumpBoth(host, guest, 20);
    CHECK(NetPlay_PingMs(host) >= -1);
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
}

// Every 60th frame the games' hashes are compared: the same ones go on, a different one ends the match as a desync
static void Netplay_ADifferenceInTheGamesEndsTheMatch(void) {
    for (int different = 0; different < 2; different++) {
        NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
        Connect(host, guest, NULL);
        CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
        CHECK(NetPlay_StartMatch(host, NULL, 0));
        CHECK(PumpUntil(host, guest, NETPLAY_RUNNING, NETPLAY_RUNNING));
        int hf = 0, gf = 0;
        for (int spin = 0; spin < 20000 && (hf < 130 || gf < 130) && NetPlay_Status(host) == NETPLAY_RUNNING; spin++) {
            NetPlay_Poll(host);
            NetPlay_Poll(guest);
            uint8_t p1, p2;
            if (NetPlay_HashDue(host))
                NetPlay_ReportHash(host, 0x1234);
            if (NetPlay_HashDue(guest))
                NetPlay_ReportHash(guest, different && NetPlay_FrameNumber(guest) == 120 ? 0x9999 : 0x1234);
            if (hf < 130 && NetPlay_Frame(host, 0, &p1, &p2))
                hf++;
            if (gf < 130 && NetPlay_Frame(guest, 0, &p1, &p2))
                gf++;
            if (spin % 7 == 0)
                SleepUs(200);
        }
        PumpBoth(host, guest, 50);
        uint32_t frame = 0;
        if (different) {
            CHECK(NetPlay_Desynced(host, &frame) || NetPlay_Desynced(guest, &frame));
            CHECK_EQ(frame, 120);
            CHECK_EQ(NetPlay_Status(host), NETPLAY_CLOSED);
            CHECK_EQ(NetPlay_Status(guest), NETPLAY_CLOSED);
        } else {
            CHECK(!NetPlay_Desynced(host, &frame) && !NetPlay_Desynced(guest, &frame));
            CHECK_EQ(NetPlay_Status(host), NETPLAY_RUNNING);
        }
        NetPlay_Destroy(host);
        NetPlay_Destroy(guest);
    }
}

// Ending it tells the other side
static void Netplay_StoppingTellsTheOtherSide(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    Connect(host, guest, NULL);
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    NetPlay_Stop(guest);
    CHECK(PumpUntil(host, guest, NETPLAY_CLOSED, NETPLAY_CLOSED));
    CHECK(strstr(NetPlay_Reason(host), "other side") != NULL);
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
}

// Finding the host by the session code: the host announces on a UDP port (the tests use their own); the guest hears the tag of its code and joins
static void Netplay_TheGuestFindsTheHostByTheCode(void) {
    NetPlay *host = NetPlay_Create(), *guest = NetPlay_Create();
    NetPlay_SetDiscoveryPort(host, 47933);
    NetPlay_SetDiscoveryPort(guest, 47933);
    CHECK(NetPlay_Host(host, "test", NULL, 0, "sad90-oopx8-7680b-rxptr", 2)); // (every address: the guest joins the one the announcement came from)
    CHECK(NetPlay_JoinByCode(guest, "test", "SAD90 OOPX8 7680B RXPTR")); // (as the code screen shows it: two rows of two groups) // (typed without dashes, in capitals)
    CHECK_EQ(NetPlay_Status(guest), NETPLAY_SEARCHING);
    CHECK(PumpUntil(host, guest, NETPLAY_READY, NETPLAY_READY));
    NetPlay_Destroy(host);
    NetPlay_Destroy(guest);
}

static void Netplay_AWrongCodeOrAnotherGameFindsNobody(void) {
    NetPlay *host = NetPlay_Create(), *wrong = NetPlay_Create(), *other = NetPlay_Create();
    NetPlay_SetDiscoveryPort(host, 47934);
    NetPlay_SetDiscoveryPort(wrong, 47934);
    NetPlay_SetDiscoveryPort(other, 47934);
    CHECK(NetPlay_Host(host, "test", NULL, 0, "sad90-oopx8-7680b-rxptr", 2));
    CHECK(NetPlay_JoinByCode(wrong, "test", "sad90-oopx8-7680b-rxpts"));
    CHECK(NetPlay_JoinByCode(other, "another", "sad90-oopx8-7680b-rxptr"));
    PumpBoth(host, wrong, 1500);
    NetPlay_Poll(other);
    CHECK_EQ(NetPlay_Status(wrong), NETPLAY_SEARCHING); // (it still looks)
    CHECK_EQ(NetPlay_Status(other), NETPLAY_SEARCHING);
    CHECK_EQ(NetPlay_Status(host), NETPLAY_LISTENING);
    NetPlay_Stop(wrong);
    CHECK_EQ(NetPlay_Status(wrong), NETPLAY_CLOSED);
    NetPlay_Destroy(host);
    NetPlay_Destroy(wrong);
    NetPlay_Destroy(other);
}

void RegisterNetplayTests(void) {
    RUN_TEST(Netplay_APasswordIsMadeAndProvedWithoutBeingSent);
    RUN_TEST(Netplay_ThePasswordsCaseAndDashesDoNotMatter);
    RUN_TEST(Netplay_AWrongPasswordIsRefused);
    RUN_TEST(Netplay_ASecondGuestAndAnotherGameAreRefused);
    RUN_TEST(Netplay_TheTwoSidesRunInLockstep);
    RUN_TEST(Netplay_ADifferenceInTheGamesEndsTheMatch);
    RUN_TEST(Netplay_StoppingTellsTheOtherSide);
    RUN_TEST(Netplay_TheGuestFindsTheHostByTheCode);
    RUN_TEST(Netplay_AWrongCodeOrAnotherGameFindsNobody);
}
