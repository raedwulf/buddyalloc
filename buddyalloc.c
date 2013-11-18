#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MEM_BITS 20
#define BLK_BITS 10
#define MEM_SIZE (1 << MEM_BITS)
#define BLK_SIZE (1 << BLK_BITS) 
#define BT_SIZE  ((MEM_SIZE/(8*BLK_SIZE))*2)

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
		if (!(btree[b >> 3] & (1 << (b & 7)))) {
			/* check allocation of ancestors */
			for (uint32_t a = b >> 1; a; a >>= 1)
				/* if node is allocated */
				if (btree[a >> 3] & (1 << (a & 7))) {
					uint32_t l = a << 1, r = (a << 1) + 1;
					/* if either child is allocated, but not both nor neither
					 * means that the block is free */
					if ((btree[l >> 3] & (1 << (l & 7))) ^
							(btree[r >> 3] & (1 << (r & 7)))) {
						for (; b; b >>= 1)
							btree[b >> 3] |= (1 << (b & 7));
						return bmem + blk;
					} else
						break;
				}
			if (!(btree[0] & 2)) {
				for (; b; b >>= 1)
					btree[b >> 3] |= (1 << (b & 7));
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
	if (!(btree[n >> 3] & (1 << (n & 7))) && size == s)
		return n;
	else {
		if (s <= BLK_SIZE)
			return 0xffffffff;
		if ((btree[l >> 3] & (1 << (l & 7))) <=
				(btree[r >> 3] & (1 << (r & 7)))) {
			if ((a = btrav(size, l, s >> 1)) != 0xffffffff)
				return a;
			if ((a = btrav(size, r, s >> 1)) != 0xffffffff)
				return a;
		} else {
			if ((a = btrav(size, r, s >> 1)) != 0xffffffff)
				return a;
			if ((a = btrav(size, l, s >> 1)) != 0xffffffff)
				return a;
		}
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
		for (uint32_t b = n; b; b >>= 1)
			btree[b >> 3] |= (1 << (b & 7));
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
		fprintf(stderr, "%s: %p not on minimum blocksize boundary\n", __func__, mem);
		return;
	}

	/* find the lowest layer and work up until we find a block
	 * that is allocated */
	uint32_t b = (1 << (MEM_BITS - BLK_BITS)) + (y >> BLK_BITS);
	if (btree[b >> 3] & (1 << (b & 7)))
		btree[b >> 3] &= ~(1 << (b & 7));
	else
		for (uint32_t a = b >> 1; a; a >>= 1) {
			if (btree[a >> 3] & (1 << (a & 7))) {
				uint32_t l = a << 1, r = (a << 1) + 1;
				/* if either child is still allocated,
				 * don't go any further */
				if ((btree[l >> 3] & (1 << (l & 7))) ||
						(btree[r >> 3] & (1 << (r & 7))))
					break;
				btree[a >> 3] &= ~(1 << (a & 7));
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
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n", n, s, btree[n >> 3] & (1 << (n & 7)) ? "grey" : "white");
	}
	
	uint32_t l = n << 1, r = (n << 1) + 1;
	if ((btree[l >> 3] & (1 << (l & 7))) || (btree[r >> 3] & (1 << (r & 7)))) {
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n", l, s >> 1, btree[l >> 3] & (1 << (l & 7)) ? "grey" : "white");
		fprintf(fd, "n%d [label=\"%d\",style=filled,fillcolor=%s];\n", r, s >> 1, btree[r >> 3] & (1 << (r & 7)) ? "grey" : "white");
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
	void *a = balloc(100 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *b = balloc(240 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *c = balloc(64  * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *d = balloc(256 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(b);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(a);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	void *e = balloc(75 * 1024);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(c);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(e);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);
	bfree(d);
	bdebugtree(1, MEM_SIZE / BLK_SIZE);

	return 0;
}
