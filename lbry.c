#include "miner.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "sha3/sph_sha2.h"
#include "sha3/sph_ripemd.h"

//#define DEBUG_ALGO

/* Move init out of loop, so init once externally, and then use one single memcpy with that bigger memory block */
typedef struct {
	sph_sha256_context	sha256;
	sph_sha512_context	sha512;
	sph_ripemd160_context	ripemd;
} lbryhash_context_holder;

/* no need to copy, because close reinit the context */
static THREADLOCAL lbryhash_context_holder ctx;

void init_lbry_contexts(void *dummy)
{
	sph_sha256_init(&ctx.sha256);
	sph_sha512_init(&ctx.sha512);
	sph_ripemd160_init(&ctx.ripemd);
}

void lbryhash(void* output, const void* input)
{
	uint32_t hashA[16], hashB[16], hashC[16];

	memset(hashA, 0, 16 * sizeof(uint32_t));
	memset(hashB, 0, 16 * sizeof(uint32_t));
	memset(hashC, 0, 16 * sizeof(uint32_t));

	sph_sha256 (&ctx.sha256, input, 112);
	sph_sha256_close(&ctx.sha256, hashA);

	sph_sha256 (&ctx.sha256, hashA, 32);
	sph_sha256_close(&ctx.sha256, hashA);

	sph_sha512 (&ctx.sha512, hashA, 32);
	sph_sha512_close(&ctx.sha512, hashA);

	sph_ripemd160 (&ctx.ripemd, hashA, 32);
	sph_ripemd160_close(&ctx.ripemd, hashB);

	sph_ripemd160 (&ctx.ripemd, hashA+8, 32);
	sph_ripemd160_close(&ctx.ripemd, hashC);

	sph_sha256 (&ctx.sha256, hashB, 20);
	sph_sha256 (&ctx.sha256, hashC, 20);
	sph_sha256_close(&ctx.sha256, hashA);

	sph_sha256 (&ctx.sha256, hashA, 32);
	sph_sha256_close(&ctx.sha256, hashA);

	memcpy(output, hashA, 32);
}

int scanhash_lbry(int thr_id, uint32_t *pdata, const uint32_t *ptarget,
					uint32_t max_nonce, uint64_t *hashes_done)
{
	uint32_t n = pdata[27] - 1;
	const uint32_t first_nonce = pdata[27];
	const uint32_t Htarg = ptarget[7];

	uint32_t hash64[8] __attribute__((aligned(32)));
	uint32_t endiandata[32];

	uint64_t htmax[] = {
		0,
		0xF,
		0xFF,
		0xFFF,
		0xFFFF,
		0x10000000
	};
	uint32_t masks[] = {
		0xFFFFFFFF,
		0xFFFFFFF0,
		0xFFFFFF00,
		0xFFFFF000,
		0xFFFF0000,
		0
	};

	// we need bigendian data...
	for (int kk=0; kk < 32; kk++) {
		be32enc(&endiandata[kk], ((uint32_t*)pdata)[kk]);
	};
#ifdef DEBUG_ALGO
	printf("[%d] Htarg=%X\n", thr_id, Htarg);
#endif
	for (int m=0; m < sizeof(masks); m++) {
		if (Htarg <= htmax[m]) {
			uint32_t mask = masks[m];
			do {
				pdata[27] = ++n;
				be32enc(&endiandata[27], n);
				lbryhash(hash64, &endiandata);
#ifndef DEBUG_ALGO
				if ((!(hash64[7] & mask)) && fulltest(hash64, ptarget)) {
					*hashes_done = n - first_nonce + 1;
					return true;
				}
#else
				if (!(n % 0x1000) && !thr_id) printf(".");
				if (!(hash64[7] & mask)) {
					printf("[%d]",thr_id);
					if (fulltest(hash64, ptarget)) {
						*hashes_done = n - first_nonce + 1;
						return true;
					}
				}
#endif
			} while (n < max_nonce && !work_restart[thr_id].restart);
			// see blake.c if else to understand the loop on htmax => mask
			break;
		}
	}

	*hashes_done = n - first_nonce + 1;
	pdata[27] = n;
	return 0;
}
