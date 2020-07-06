#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "bpsk.h"

#include "bpsk/demod.h"
#include "bpsk/mac.h"
#include "bpsk/murmur3.h"

#include "arm_math.h"

#include "printf.h"

// static struct mac_state mac_state;
// static int packet_count = 0;
// static int corrupt_count = 0;
// static demod_pkt_t packet;

volatile uint32_t debug_print_sync = 1;
volatile uint32_t debug_print_status = 1;

// static int validate_packet(demod_pkt_t *pkt, int should_print)
// {
//     unsigned int i;
//     demod_pkt_ctrl_t *cpkt = &pkt->ctrl_pkt;
//     demod_pkt_data_t *dpkt = &pkt->data_pkt;
//     uint32_t hash;

//     if (should_print)
//     {
//         printf("Got packet:\n");
//         printf("    Header:\n");
//         printf("        Version: %d\n", pkt->header.version);
//         printf("           Type: %" PRId32 "  ", pkt->header.type);
//     }
//     switch (pkt->header.type)
//     {

//     case PKTTYPE_CTRL:
//         if (should_print)
//         {
//             printf("Ctrl Packet\n");
//             printf("       Reserved: %d\n", cpkt->reserved);
//             printf("         Length: %" PRId32 "\n", cpkt->length);
//             printf("      Full Hash: %" PRIx32 "\n", cpkt->fullhash);
//             printf("           GUID: ");
//             for (i = 0; i < 16; i++)
//                 printf("%" PRIx8 " ", cpkt->guid[i]);
//             printf("\n");
//             printf("    Packet Hash: %" PRIx32 " ", cpkt->hash);
//         }
//         MurmurHash3_x86_32((uint8_t *)cpkt, sizeof(*cpkt) -
//         sizeof(cpkt->hash),
//                            MURMUR_SEED_BLOCK, &hash);
//         if (hash != cpkt->hash)
//         {
//             if (should_print)
//                 printf("!= %" PRIx32 "\n", hash);
//             return 0;
//         }
//         if (should_print)
//             printf("Ok\n");
//         break;

//     case PKTTYPE_DATA:
//         // unstripe the transition xor's used to keep baud sync. We
//         // don't xor the header or the ending hash, but xor
//         // everything else..
//         for (i = sizeof(pkt->header); i < sizeof(*dpkt) - sizeof(dpkt->hash);
//              i++)
//         {
//             if (pkt->header.version == PKT_VER_1)
//             {
//                 // baud striping on alpha and before
//                 if ((i % 16) == 7)
//                     ((uint8_t *)pkt)[i] ^= 0x55;
//                 else if ((i % 16) == 15)
//                     ((uint8_t *)pkt)[i] ^= 0xAA;
//             }
//             else if (pkt->header.version == PKT_VER_2)
//             {
//                 // more dense baud striping to be used on beta and
//                 // beyond
//                 if ((i % 3) == 0)
//                     ((uint8_t *)pkt)[i] ^= 0x35;
//                 else if ((i % 3) == 1)
//                     ((uint8_t *)pkt)[i] ^= 0xac;
//                 else if ((i % 3) == 2)
//                     ((uint8_t *)pkt)[i] ^= 0x95;
//             }
//             else if (pkt->header.version == PKT_VER_3)
//             {
//                 // Nothing to do
//             }
//         }

//         if (should_print)
//         {
//             printf("Data Packet\n");
//             printf("   Block Number: %" PRId32 "\n", dpkt->block);
//             printf("    Packet Hash: %" PRIx32 " ", dpkt->hash);
//         }
//         /* Make sure the packet's hash is correct. */
//         MurmurHash3_x86_32((uint8_t *)dpkt, sizeof(*dpkt) -
//         sizeof(dpkt->hash),
//                            MURMUR_SEED_BLOCK, &hash);
//         if (hash != dpkt->hash)
//         {
//             if (should_print)
//                 printf("!= %" PRIx32 "\n", hash);
//             return 0;
//         }
//         if (should_print)
//             printf("Ok\n");
//         break;

//     default:
//         if (should_print)
//             printf(" (unknown type)\n");
//         return 0;
//         break;
//     }
//     return 1;
// }

void bpsk_init(void) {
    bpsk_demod_init();
}

extern char varcode_to_char(uint32_t c);
static void print_char(uint32_t c) {
    printf("%c", varcode_to_char(c >> 2));
}

uint32_t bit_acc = 0;
void bpsk_run(int32_t *samples, uint32_t nsamples) {
    uint32_t processed_samples;

    uint32_t bit = 0;
    while (nsamples > 0) {
        if (bpsk_demod(&bit, samples, nsamples, &processed_samples)) {
            bit_acc = (bit_acc << 1) | bit;

            // In varcode, a stop condition is indicated by to consecutive
            // bits of `0`. If we encounter this, print out the accumulated
            // character.
            if ((bit_acc & 3) == 0) {
                print_char(bit_acc);
                bit_acc = 0;
            }
            // if (mac_put_bit(&mac_state, bit, &packet, sizeof(packet)))
            // {
            //     if (validate_packet(&packet, 1))
            //     {
            //         packet_count++;
            //     }
            //     else
            //     {
            //         corrupt_count++;
            //     }
            // }
        }
        if (processed_samples) {
            assert(processed_samples <= nsamples);
            nsamples -= processed_samples;
            samples += processed_samples;
        }
    }
}
