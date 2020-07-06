#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "mac.h"
#ifdef PLAYGROUND
#include "../printf.h"
#else
#include <stdio.h>
#endif


extern uint32_t debug_print_sync;
extern uint32_t debug_print_status;

// uint8_t byte_log[65536];
// uint8_t bit_offset = 8;
// uint32_t byte_log_offset;

// put_bit with a MAC layer on it
int mac_put_bit(struct mac_state *state, int bit, void *buffer,
                unsigned int buffer_size) {
    demod_pkt_t *pkt = (demod_pkt_t *)buffer;
    int packet_done = 0;

    assert(buffer_size >= sizeof(demod_pkt_t));

    // byte_log[byte_log_offset] |= (bit << --bit_offset);
    // if (!bit_offset) {
    //     byte_log_offset++;
    //     if (byte_log_offset > sizeof(byte_log)) {
    //         byte_log_offset = 0;
    //     }
    //     byte_log[byte_log_offset] = 0;
    //     bit_offset = 8;
    // }

    switch (state->mstate) {
    case MAC_IDLE:
        // Search until at least /n/ zeros are found.
        // The next transition /might/ be sync.
        if (state->idle_zeros > 32) {
            if (bit != 0) {
                state->mstate = MAC_SYNC;
                // printf("Got bit, transitioning to MAC_SYNC\n");
                state->bitpos = 6;
                state->curbyte = 0x80;
                state->sync_count = 0;
            } else {
                if (state->idle_zeros < 255) {
                    state->idle_zeros++;
                }
            }
        } else {
            if (bit != 0)
                state->idle_zeros = 0;
            else
                state->idle_zeros++;
        }
        break;

    case MAC_SYNC:
        // acculumate two bytes worth of temp data, then check to see if valid
        // sync
        state->curbyte >>= 1;
        state->bitpos--;
        if (bit) {
            state->curbyte |= 0x80;
        }

        /* We haven't yet read 8 bits */
        if (state->bitpos) {
            break;
        }

        // printf("Got MAC byte: %02x\n", state->curbyte);
        /* 8 bits have been read.  Process the resulting byte. */

        /* Optimization: check to see if we just read an idle value. */
        if (state->curbyte == 0x00) {

            /* False noise trigger, go back to idle. */
            state->mstate = MAC_IDLE;
            state->idle_zeros = 8; /* We just saw 8 zeros, so count those */
            if (debug_print_sync) printf("False sync\n");
            break;
        }

        /* Tally up the sync characters, make sure the sync matches. */

        state->mac_sync[state->sync_count++] = state->curbyte;
        if (state->sync_count >= 3) {
            /* Test for sync sequence. It's one byte less than the # of
             * leading zeros, to allow for the idle escape trick above to work
             * in case of zero-biased noise.
             */
            if ((state->mac_sync[0] == 0xAA) && (state->mac_sync[1] == 0x55) &&
                (state->mac_sync[2] == 0x42)) {
                // found the sync sequence, proceed to packet state
                // osalDbgAssert(pktReady == 0,
                //               "Packet buffer full flag still set "
                //               "while new packet incoming\r\n");
                if (debug_print_sync) printf("Found sync\n");
                state->mstate = MAC_PACKET;
                state->pkt_len = 0;
                state->pkt_read = 0;
            } else {
                if (debug_print_sync)
                    printf("%02x %02x %02x not sync\n", state->mac_sync[0],
                           state->mac_sync[1], state->mac_sync[2]);
                state->mstate = MAC_IDLE;
                state->idle_zeros = 0;
            }
        }

        /* Move on to the next bit */
        state->bitpos = 8;
        state->curbyte = 0;
        break;

    case MAC_PACKET:
        /* If we haven't figured out the length, but we've read the entire
         * header, figure out the length.  Or exit the loop if it's not a
         * valid packet.
         */
        if (!state->pkt_len && state->pkt_read > sizeof(demod_pkt_header_t)) {

            /* If the version number doesn't match, abandon ship. */
            if ((pkt->header.version != PKT_VER_1) &&
                (pkt->header.version != PKT_VER_2) &&
                (pkt->header.version != PKT_VER_3)) {
                if (debug_print_status) {
                    printf("Unrecognized packet version: %d\n", pkt->header.version);
                }
                goto make_idle;
            }

            if (pkt->header.type == PKTTYPE_CTRL)
                state->pkt_len = CTRL_LEN;
            else if (pkt->header.type == PKTTYPE_DATA)
                state->pkt_len = DATA_LEN;
            else {
                /* Unrecognized packet type */
                if (debug_print_status)
                    printf("Unrecognized packet header type %d\n",
                           pkt->header.type);
                goto make_idle;
            }
        }

        /* Load on the next bit. */
        state->curbyte >>= 1;
        state->bitpos--;
        if (bit) {
            state->curbyte |= 0x80;
        }

        /* If we've finished this bit, add it to the packet. */
        if (state->bitpos == 0) {
            ((uint8_t *)buffer)[state->pkt_read++] = state->curbyte;
            state->bitpos = 8;
            state->curbyte = 0;
        }

        /* If we've finished reading the packet, indicate it's ready */
        if (state->pkt_len && state->pkt_read >= state->pkt_len) {
            packet_done = 1; // flag that the packet is ready
            goto make_idle;
            break;
        }

        break;

    default:
        break;
    }

    return 0;

make_idle:
    state->mstate = MAC_IDLE;
    state->idle_zeros = 0;
    return packet_done;
}
