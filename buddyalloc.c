#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MEM_BITS 20
#define BLK_BITS 10
#define MEM_SIZE (1 << MEM_BITS)
#define BLK_SIZE (1 << BLK_BITS) 
#define BT_SIZE  ((MEM_SIZE/(8*BLK_SIZE))*2)
#define BIT_TEST(x) (btree[(x) >> 3] & (1 << ((x) & 7)))
#define BIT_SET(x)  btree[(x) >> 3] |= (1 << ((x) & 7))
#define BIT_CLR(x)  btree[(x) >> 3] &= ~(1 << ((x) & 7))

static char bmem[MEM_SIZE];
static char btree[BT_SIZE];
static int bdebug = 0;

/* wc-complexity: O(N log N) */
/* closer to... (0.5 * N) * (log N - 1) + log N */
void *balloc(uint32_t size)
{
	if (!size) return NULL;
	if (size < BLK_SIZE) size = BLK_SIZE;

	uint32_t x = MEM_BITS - (32 - __builtin_clz(size-1));
	uint32_t bs = MEM_SIZE >> x;

	uint32_t blk = 0;
	for (uint32_t i = 0; i < (1 << x); i++, blk += bs) {
		uint32_t b = (1 << x) + i;
		/* check if slot appears free */
		if (!BIT_TEST(b)) {
			/* check allocation of ancestors */
			for (uint32_t a = b >> 1; a; a >>= 1)
				/* if node is allocated */
				if (BIT_TEST(a)) {
					uint32_t l = a << 1, r = (a << 1) + 1;
					/* if either child is allocated, but not both
					 * nor neither means that the block is free */
					if (BIT_TEST(l) ^ BIT_TEST(r)) {
						for (; b; b >>= 1) BIT_SET(b);
						return bmem + blk;
					} else
						break;
				}
			if (!(btree[0] & 2)) {
				for (; b; b >>= 1) BIT_SET(b);
				return bmem + blk;
			}
		}
	}
	return NULL;
}

/* wc-complexity: O(log N) */
uint32_t btrav(uint32_t size, uint32_t n, uint32_t s)
{
	uint32_t a;
	uint32_t l = n << 1, r = (n << 1) + 1;
	if (s < BLK_SIZE) return 0xffffffff;

	if (!BIT_TEST(n) && size == s)
		return n;
	else if (BIT_TEST(n) && !(BIT_TEST(l) || BIT_TEST(r)))
		return 0xffffffff;
	else {
		if ((a = btrav(size, l, s >> 1)) != 0xffffffff)
			return a;
		if ((a = btrav(size, r, s >> 1)) != 0xffffffff)
			return a;
		return 0xffffffff;
	}
}

/* wc-complexity: O(2 * log N) */
void *balloc2(uint32_t size)
{
	if (!size) return NULL;
	if (size < BLK_SIZE) size = BLK_SIZE;

	uint32_t x = MEM_BITS - (32 - __builtin_clz(size-1));
	uint32_t blk = MEM_SIZE >> x, n;
	if ((n = btrav(MEM_SIZE >> x, 1, MEM_SIZE)) != 0xffffffff) {
		for (uint32_t b = n; b; b >>= 1) BIT_SET(b);
		uint32_t y = 31 - __builtin_clz(n);
		return bmem + (MEM_SIZE >> y) * (n - (1 << y));
	}
	return NULL;
}

/* wc-complexity: O(log N) */
void bfree(void *mem)
{
	if (mem < (void *)bmem || mem > (void *)bmem + MEM_SIZE) {
		fprintf(stderr, "%s: %p out of range\n", __func__, mem);
		return;
	}

	uint32_t y = (uint32_t)(mem - (void *)bmem);

	if (y & (BLK_SIZE - 1)) {
		fprintf(stderr, "%s: %p not on minimum blocksize boundary\n",
				__func__, mem);
		return;
	}

	/* find the lowest layer and work up until we find a block
	 * that is allocated */
	uint32_t b = (1 << (MEM_BITS - BLK_BITS)) + (y >> BLK_BITS);
	if (BIT_TEST(b))
		BIT_CLR(b);
	else
		for (uint32_t a = b >> 1; a; a >>= 1) {
			if (BIT_TEST(a)) {
				uint32_t l = a << 1, r = (a << 1) + 1;
				/* if either child is still allocated,
				 * don't go any further */
				if (BIT_TEST(l) || BIT_TEST(r)) break;
				BIT_CLR(a);
			}
		}
}

void bdebugtree(uint32_t n, uint32_t s)
{
	static FILE *fd;
	if (n == 1) {
		char cmd[32];
		snprintf(cmd, sizeof(cmd), "dot -Tpng -oballoc_%04d.png", bdebug);
		fd = popen(cmd, "w");
		fprintf(fd, "digraph G {\n");
		fprintf(fd, "graph [fontname = \"Bitstream Sans Vera\"];\n");
		fprintf(fd, "node [fontname = \"Bitstream Sans Vera\"];\n");
		fprintf(fd, "edge [fontname = \"Bitstream Sans Vera\"];\n");
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n",
				n, s, BIT_TEST(n) ? "grey" : "white");
	}
	
	uint32_t l = n << 1, r = (n << 1) + 1;
	if ((btree[l >> 3] & (1 << (l & 7))) || (btree[r >> 3] & (1 << (r & 7)))) {
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n",
				l, s >> 1, BIT_TEST(l) ? "grey" : "white");
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n",
				r, s >> 1, BIT_TEST(r) ? "grey" : "white");
		fprintf(fd, "n%d -> n%d;\n", n, l);
		fprintf(fd, "n%d -> n%d;\n", n, r);
		bdebugtree(l, s >> 1);
		bdebugtree(r, s >> 1);
	}

	if (n == 1) {
		fprintf(fd, "}\n");
		pclose(fd);
		bdebug++;
	}
}

int main(int argc, char **argv)
{
	memset(btree, 0, BT_SIZE);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *a = balloc2(100 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *b = balloc2(240 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *c = balloc2(64  * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *d = balloc2(256 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(b);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(a);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *e = balloc2(75 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(c);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(e);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(d);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);

	return 0;
}
