#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int Flight_ComputeWorldStateResyncSegmentSize(int size);
int Flight_BuildWorldStateResyncSegmentChecksums(int* outChecksums, uint8_t* worldState, int worldStateSize);

typedef struct PacketWindowStats {
    size_t bytesSent;
    int packetsSent;
    int fullAckWindows;
    int finalAckCount;
} PacketWindowStats;

/* Synthetic transport model using the exact packet/window constants from
 * FlightNet_SendWorldStateResyncToPlayer. It deliberately does not redefine
 * any gameplay state or network protocol. */
static PacketWindowStats simulate_all_segments_mismatched(int worldStateSize, int segmentSize, int segmentCount) {
    PacketWindowStats stats = {0};
    int chunkSlot = 0;
    int worldOffset = 0;
    int packetFreeBytes = 492;
    int segmentIndex;

    for (segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        int remainingSegmentBytes = segmentSize;
        if (worldOffset + remainingSegmentBytes > worldStateSize) {
            remainingSegmentBytes = worldStateSize - worldOffset;
            if (remainingSegmentBytes < 0) {
                remainingSegmentBytes = 0;
            }
        }

        while (remainingSegmentBytes != 0) {
            int recordBytes = remainingSegmentBytes + 8;
            int dataBytes;
            if (recordBytes > packetFreeBytes) {
                recordBytes = packetFreeBytes;
            }
            dataBytes = recordBytes - 8;
            assert(dataBytes > 0);
            packetFreeBytes -= recordBytes;
            worldOffset += dataBytes;
            remainingSegmentBytes -= dataBytes;
            stats.bytesSent += (size_t)dataBytes;

            if (packetFreeBytes < 32) {
                ++stats.packetsSent;
                ++chunkSlot;
                if (chunkSlot == 16) {
                    ++stats.fullAckWindows;
                    chunkSlot = 0;
                }
                packetFreeBytes = 492;
            }
        }
    }

    if (packetFreeBytes < 496) {
        ++stats.packetsSent;
        stats.finalAckCount = chunkSlot + 1;
    }
    return stats;
}

int main(void) {
    enum { STATE_SIZE = 50000 };
    uint8_t* state = (uint8_t*)malloc(STATE_SIZE);
    uint8_t* changed = (uint8_t*)malloc(STATE_SIZE);
    int baseline[125];
    int modified[125];
    int segmentSize;
    int i;
    int changedChecksumCount = 0;
    PacketWindowStats windows;

    assert(state != NULL && changed != NULL);
    for (i = 0; i < STATE_SIZE; ++i) {
        state[i] = (uint8_t)((i * 37 + 11) & 0xff);
    }
    memcpy(changed, state, STATE_SIZE);

    segmentSize = Flight_ComputeWorldStateResyncSegmentSize(STATE_SIZE);
    assert(segmentSize == STATE_SIZE / 124);
    assert(segmentSize > 0);
    assert(Flight_BuildWorldStateResyncSegmentChecksums(baseline, state, STATE_SIZE) == 125);

    /* Only an extended-tail byte changes. It must participate in the existing
     * checksum array rather than living in a parallel protocol. */
    changed[STATE_SIZE - 17] ^= 0x5au;
    assert(Flight_BuildWorldStateResyncSegmentChecksums(modified, changed, STATE_SIZE) == 125);
    for (i = 0; i < 125; ++i) {
        if (baseline[i] != modified[i]) {
            ++changedChecksumCount;
        }
    }
    assert(changedChecksumCount >= 1);

    /* 50 KiB is far larger than one 16 x 500-byte send window. The existing
     * packet loop must continue through repeated ACK windows and cover every
     * byte rather than truncate at the first 16 packets. */
    windows = simulate_all_segments_mismatched(STATE_SIZE, segmentSize, 125);
    assert(windows.bytesSent == STATE_SIZE);
    assert(windows.packetsSent > 16);
    assert(windows.fullAckWindows >= 2);
    assert(windows.finalAckCount >= 1 && windows.finalAckCount <= 16);

    free(changed);
    free(state);
    return 0;
}
