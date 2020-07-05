#ifndef __ORCHARD_MAC__
#define __ORCHARD_MAC__

#define PAYLOAD_LEN 256

#define MURMUR_SEED_BLOCK (0xdeadbeef)
#define MURMUR_SEED_TOTAL (0x32d0babe)

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__)                                                  \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

PACK(struct demod_ph {
    /* Protocol version */
    uint8_t version;

    /* Packet type */
    uint8_t type;
});
typedef struct demod_ph demod_pkt_header_t;

PACK(struct demod_dp {
    demod_pkt_header_t header;

    /* Block number (offset is block * PAYLOAD_LEN) */
    uint16_t block;

    /* Payload contents */
    uint8_t payload[PAYLOAD_LEN];

    /* Hash of this data packet */
    uint32_t hash;
});
typedef struct demod_dp demod_pkt_data_t;

PACK(struct demod_cp {
    demod_pkt_header_t header;

    /* Padding */
    uint16_t reserved;

    /* Total length of program in bytes (# blocks = ceil(length / blocksize) */
    uint32_t length;

    /* Lightweight data integrity hash, computed across "length"
     * of program given above.
     */
    uint32_t fullhash;

    /* UID code for identifying a program uniquely, and globally */
    uint8_t guid[16];

    /* Hash check of /this/ packet */
    uint32_t hash;
});
typedef struct demod_cp demod_pkt_ctrl_t;

PACK(union demod_packet {
    demod_pkt_header_t header;
    demod_pkt_ctrl_t ctrl_pkt;
    demod_pkt_data_t data_pkt;
});
typedef union demod_packet demod_pkt_t;

// size of the largest packet we could receive
#define DATA_LEN (sizeof(demod_pkt_data_t))
#define CTRL_LEN (sizeof(demod_pkt_ctrl_t))

// bit 7 defines packet type on the version code (first byte received)
#define PKTTYPE_CTRL 0x01
#define PKTTYPE_DATA 0x02

#define PKT_VER_1 0x01
#define PKT_VER_2 0x02 /* Improved baud striping */
#define PKT_VER_3 0x03 /* Stuffed bits and transition-is-0 */

/// Internal state of the MAC
typedef enum current_mac_state {
    /// Looking for a string of zeroes followed by a bit
    MAC_IDLE = 0,

    /// Found bits, looking for the sync word
    MAC_SYNC = 1,

    /// Found sync word, filling the packet buffer
    MAC_PACKET = 2,
} current_mac_state_t;

struct mac_state {
    /// Where we are inside the current bit
    int bitpos; // Maybe default to 9?

    /// Accumulator for the current bit
    uint8_t curbyte;

    /// Current state machine's state
    current_mac_state_t mstate;

    /// Length of the run of zeroes seen during MAC_IDLE
    uint8_t idle_zeros;

    /// Contents of the current sync byte
    uint8_t mac_sync[4];

    /// Number of sync bytes received so far
    uint8_t sync_count;

    /// Expected length of this packet
    uint32_t pkt_len;

    /// Number of bytes we've read so far
    uint32_t pkt_read;
};

int mac_put_bit(struct mac_state *state, int bit, void *buffer,
                unsigned int buffer_size);

#endif
