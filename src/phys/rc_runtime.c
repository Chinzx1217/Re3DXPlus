#include "rc_runtime.h"

#include <stdio.h>
#include <stdlib.h>

rc_cpu_t rc_cpu;
fpt rc_fst[8];
int rc_ftop, rc_fc0, rc_fc2, rc_fc3;
uint8_t *rc_mem;
uint8_t rc_low[0x10000];
uint32_t rc_heap_next = RC_HEAP_BASE;
uint32_t rc_rand_seed;
void (*rc_on_hook)(uint32_t at);
int (*rc_on_other_setup)(uint32_t slot, uint32_t car_type);
void (*rc_on_other_free)(uint32_t slot);
uint32_t rc_link_pcb, rc_link_tx, rc_link_rx;
void (*rc_on_link_receive)(void);
void (*rc_on_link_send)(void);
#ifdef RC_TRACE
FILE *rc_trace;
#endif

void rc_fail(const char *msg) {
  fprintf(stderr, "rc: %s\n", msg);
  abort();
}

void rc_fail_addr(const char *msg, uint32_t a) {
  fprintf(stderr, "rc: %s %#x\n", msg, a);
  abort();
}

uint8_t *rc_va_slow(uint32_t a) {
  if (a < sizeof rc_low) return rc_low + a;
  rc_fail_addr("bad address", a);
}

void rc_unknown(const char *name) {
  fprintf(stderr, "rc: unimplemented import %s\n", name);
}

uint32_t rc_alloc(uint32_t n) {
  uint32_t p = (rc_heap_next + 15) & ~15u;
  uint32_t heap_end = RC_HEAP_BASE + RC_HEAP_SIZE;
  if (n == 0) n = 1;
  if (!rc_mem) rc_fail("memory image is not loaded");
  if (p < RC_HEAP_BASE || p > heap_end || n > heap_end - p) {
    fprintf(stderr, "rc: heap exhausted (request=%u used=%u capacity=%u)\n",
            n, p >= RC_HEAP_BASE ? p - RC_HEAP_BASE : 0u, RC_HEAP_SIZE);
    rc_fail("heap exhausted");
  }
  rc_heap_next = p + n;
  memset(VA(p), 0, n);
  return p;
}

void rc_reset_heap(void) { rc_heap_next = RC_HEAP_BASE; }

const char *rc_config_dir;
static int rc_space_c(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
static FILE *rc_files[4];

char rc_last_path[128];

uint32_t rc_fopen(uint32_t path) {
  /* data/carconfig (the loaders) and data/rtmovie (the realtime movies), under rc_config_dir */
  static const char *const prefix[] = { "data/carconfig/", "data/rtmovie/", "data/cutin/" };
  const char *p = (const char *)VA(path), *rest = NULL;
  snprintf(rc_last_path, sizeof rc_last_path, "%s", p);
  for (int k = 0; k < 3 && !rest; ++k) {
    int i = 0;
    while (prefix[k][i] && p[i] == prefix[k][i]) i++;
    if (!prefix[k][i]) rest = p + 5; /* past "data/" */
  }
  if (!rest || !rc_config_dir) return 0;
  char full[1200];
  snprintf(full, sizeof full, "%s/%s", rc_config_dir, rest);
  for (uint32_t k = 0; k < 4; ++k)
    if (!rc_files[k]) {
      rc_files[k] = fopen(full, "rb");
      if (getenv("RC_TRACE_FILES") || !strncmp(p, "data/rtmovie/", 13))
        fprintf(stderr, "rc_fopen %s: %s\n", full, rc_files[k] ? "ok" : "missing");
      return rc_files[k] ? k + 1 : 0;
    }
  return 0;
}

static FILE *rc_file(uint32_t f) { return f >= 1 && f <= 4 ? rc_files[f - 1] : NULL; }

uint32_t rc_fread(uint32_t buf, uint32_t size, uint32_t n, uint32_t f) {
  FILE *fp = rc_file(f);
  if (!fp || !size || !n) return 0;
  return (uint32_t)fread(VA(buf), size, n, fp);
}

uint32_t rc_feof(uint32_t f) {
  FILE *fp = rc_file(f);
  return !fp || feof(fp) ? 1 : 0;
}

uint32_t rc_fscanf(uint32_t f, uint32_t fmt, uint32_t args) {
  FILE *fp = rc_file(f);
  const char *q = (const char *)VA(fmt);
  uint32_t n = 0;
  if (!fp) return 0xffffffffu;
  while (*q) {
    if (rc_space_c(*q)) {
      if (fscanf(fp, " ") < 0) return n ? n : 0xffffffffu;
      q++;
      continue;
    }
    if (*q != '%') {
      int c = fgetc(fp);
      if (c != *q) {
        if (c != EOF) ungetc(c, fp);
        break;
      }
      q++;
      continue;
    }
    char conv = q[1];
    uint32_t dst = M32(args + 4 * n);
    int r;
    q += 2;
    if (conv == 'd') {
      int v;
      r = fscanf(fp, "%d", &v);
      if (r == 1) W32(dst, (uint32_t)v);
    } else if (conv == 'f') {
      float v;
      r = fscanf(fp, "%f", &v);
      if (r == 1) WF(dst, v);
    } else if (conv == 's') {
      char w[512];
      r = fscanf(fp, "%511s", w);
      if (r == 1) strcpy((char *)VA(dst), w);
    } else if (conv == 'c') {
      int c = fgetc(fp);
      r = c == EOF ? EOF : 1;
      if (r == 1) *(uint8_t *)VA(dst) = (uint8_t)c;
    } else {
      rc_fail("fscanf conversion");
    }
    if (r != 1) return n ? n : r == EOF ? 0xffffffffu : 0;
    n++;
  }
  return n;
}

static size_t rc_format(char *out, uint32_t fmt, uint32_t args);

uint32_t rc_sprintf(uint32_t buf, uint32_t fmt, uint32_t args) {
  char out[1024];
  size_t o = rc_format(out, fmt, args);
  memcpy(VA(buf), out, o + 1);
  return (uint32_t)o;
}

/* yt::SCarryover's file (/tmp/ytCarryover.txt: CSeq::End saves what a story race hands the next
 * one, CSeq::Init loads it for a series that carries over), kept in memory as handle 5 */
#define RC_CARRY 5u
static char rc_carry[4096];
static size_t rc_carry_len, rc_carry_at;
static int rc_carry_open;

static int rc_is_carry(const char *p) {
  const char *c = "/tmp/ytCarryover.txt";
  while (*c && *p == *c) p++, c++;
  return !*c && !*p;
}

uint32_t rc_fopen_mode(uint32_t path, uint32_t mode) {
  if (rc_is_carry((const char *)VA(path))) {
    const char *m = (const char *)VA(mode);
    if (m[0] == 'w') rc_carry_len = 0;
    else if (!rc_carry_len) return 0; /* (none saved yet: as a missing file) */
    rc_carry_at = 0;
    rc_carry_open = 1;
    return RC_CARRY;
  }
  return rc_fopen(path);
}

uint32_t rc_fprintf(uint32_t f, uint32_t fmt, uint32_t args) {
  if (f != RC_CARRY || !rc_carry_open) return 0;
  char out[1024];
  size_t o = rc_format(out, fmt, args);
  if (rc_carry_len + o < sizeof rc_carry) memcpy(rc_carry + rc_carry_len, out, o), rc_carry_len += o;
  return (uint32_t)o;
}

void rc_carry_clear(void) { rc_carry_len = 0; }

static size_t rc_format(char *out, uint32_t fmt, uint32_t args) {
  /* the movies' file names: %d (with flags and width), %s, %f */
  const char *q = (const char *)VA(fmt);
  char spec[16];
  size_t o = 0;
  while (*q && o < 1024 - 64) {
    if (*q != '%') {
      out[o++] = *q++;
      continue;
    }
    size_t k = 0;
    spec[k++] = *q++;
    while (((*q >= '0' && *q <= '9') || *q == '.' || *q == '-' || *q == '+' || *q == ' ') && k < sizeof spec - 2)
      spec[k++] = *q++;
    char conv = *q++;
    spec[k++] = conv;
    spec[k] = 0;
    if (conv == 'd' || conv == 'i' || conv == 'x' || conv == 'u' || conv == 'c') {
      o += (size_t)snprintf(out + o, 1024 - o, spec, (int)M32(args));
      args += 4;
    } else if (conv == 's') {
      o += (size_t)snprintf(out + o, 1024 - o, spec, (const char *)VA(M32(args)));
      args += 4;
    } else if (conv == 'f') {
      double v;
      memcpy(&v, VA(args), 8);
      o += (size_t)snprintf(out + o, 1024 - o, spec, v);
      args += 8;
    } else if (conv == '%') {
      out[o++] = '%';
    } else {
      rc_fail("sprintf conversion");
    }
    if (o > 1024 - 64) o = 1024 - 64;
  }
  out[o] = 0;
  return o;
}

uint32_t rc_fgets(uint32_t buf, uint32_t n, uint32_t f) {
  if (f == RC_CARRY) {
    char *d = (char *)VA(buf);
    uint32_t k = 0;
    if (!rc_carry_open || rc_carry_at >= rc_carry_len || n < 2) return 0;
    while (k + 1 < n && rc_carry_at < rc_carry_len) {
      char c = rc_carry[rc_carry_at++];
      d[k++] = c;
      if (c == '\n') break;
    }
    d[k] = 0;
    return buf;
  }
  if (f < 1 || f > 4 || !rc_files[f - 1]) return 0;
  return fgets((char *)VA(buf), (int)n, rc_files[f - 1]) ? buf : 0;
}

void rc_fclose(uint32_t f) {
  if (f == RC_CARRY) {
    rc_carry_open = 0;
    return;
  }
  if (f < 1 || f > 4 || !rc_files[f - 1]) return;
  fclose(rc_files[f - 1]);
  rc_files[f - 1] = NULL;
}

uint32_t rc_strstr(uint32_t s, uint32_t find) {
  const char *h = (const char *)VA(s), *n = (const char *)VA(find);
  for (uint32_t i = 0; h[i]; ++i) {
    uint32_t k = 0;
    while (n[k] && h[i + k] == n[k]) k++;
    if (!n[k]) return s + i;
  }
  return n[0] ? 0 : s;
}

uint32_t rc_strcat(uint32_t dst, uint32_t src) {
  char *d = (char *)VA(dst);
  const char *p = (const char *)VA(src);
  while (*d) d++;
  while ((*d++ = *p++)) {}
  return dst;
}

static int rc_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

uint32_t rc_sscanf(uint32_t s, uint32_t fmt, uint32_t args) {
  const char *p = (const char *)VA(s), *f = (const char *)VA(fmt);
  uint32_t n = 0;
  while (*f) {
    if (*f != '%') {
      if (rc_space(*f)) {
        while (rc_space(*p)) p++;
      } else if (*p++ != *f) {
        break;
      }
      f++;
      continue;
    }
    char conv = f[1];
    f += 2;
    while (rc_space(*p)) p++;
    if (!*p) return n ? n : 0xffffffffu; /* EOF before the first conversion */
    uint32_t dst = M32(args + 4 * n);
    int used = 0;
    if (conv == 'd') {
      int v;
      if (sscanf(p, "%d%n", &v, &used) != 1) break;
      W32(dst, (uint32_t)v);
    } else if (conv == 'f') {
      float v;
      if (sscanf(p, "%f%n", &v, &used) != 1) break;
      WF(dst, v);
    } else if (conv == 's') {
      char *d = (char *)VA(dst);
      while (p[used] && !rc_space(p[used])) d[used] = p[used], used++;
      d[used] = 0;
    } else {
      rc_fail("sscanf conversion");
    }
    p += used;
    n++;
  }
  return n;
}

int rc_strcmp(const char *a, const char *b) {
  while (*a && *a == *b) a++, b++;
  return (unsigned char)*a - (unsigned char)*b;
}

/* std::wstring::compare(const wchar_t *) of libstdc++'s wstring (its characters at the pointer
 * it holds, their count 12 bytes before them; 4-byte wchar_t) */
uint32_t rc_wcompare(uint32_t str, uint32_t s) {
  uint32_t p = M32(str), n = M32(p - 12), on = 0;
  while (M32(s + 4 * on)) on++;
  for (uint32_t i = 0; i < n && i < on; ++i) {
    int32_t a = (int32_t)M32(p + 4 * i), b = (int32_t)M32(s + 4 * i);
    if (a != b) return a < b ? (uint32_t)-1 : 1u;
  }
  return (uint32_t)((int32_t)n - (int32_t)on);
}

int rc_cutin_mode;
uint32_t rc_cutin_lines;
void (*rc_on_cutin_speak)(uint32_t pos, uint32_t tag, int flash, int x);
void (*rc_on_cutin_end)(uint32_t pos);

uint32_t rc_string(uint32_t s, uint32_t n, uint32_t size) {
  uint32_t rep = rc_alloc(12 + (n + 1) * size);
  W32(rep, n);
  W32(rep + 4, n);
  W32(rep + 8, 0);
  if (n) memcpy(VA(rep + 12), VA(s), n * size);
  return rep + 12;
}

uint32_t rc_utf8_line(void) {
  uint32_t c = ++rc_cutin_lines;
  uint32_t rep = rc_alloc(12 + 8);
  W32(rep, 1);
  W32(rep + 4, 1);
  W32(rep + 12, c);
  return rep + 12;
}

uint32_t rc_wcslen(uint32_t s) {
  uint32_t n = 0;
  while (M32(s + 4 * n)) n++;
  return n;
}

uint32_t rc_strtol(uint32_t s, uint32_t end, uint32_t base) {
  const char *p = (const char *)VA(s);
  char *e;
  long v = strtol(p, &e, (int)base);
  if (end) W32(end, s + (uint32_t)(e - p));
  return (uint32_t)v;
}

double rc_strtod(uint32_t s, uint32_t end) {
  const char *p = (const char *)VA(s);
  char *e;
  double v = strtod(p, &e);
  if (end) W32(end, s + (uint32_t)(e - p));
  return v;
}

uint32_t rc_rand(void) {
  if (!rc_rand_seed) return 12345;
  rc_rand_seed = rc_rand_seed * 1103515245u + 12345u;
  return (rc_rand_seed >> 16) & 0x7fff;
}

int rc_load_image(const char *path) {
  uint32_t n = RC_IMG_END - RC_IMG_BASE;
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "rc: cannot open physics image %s\n", path ? path : "(null)");
    return -1;
  }
  if (!rc_mem) {
    rc_mem = calloc(1, RC_MEM_SIZE);
    if (!rc_mem) {
      fprintf(stderr, "rc: cannot allocate %u-byte emulated memory for %s\n",
              (unsigned)RC_MEM_SIZE, path);
      fclose(f);
      return -1;
    }
  }
  size_t got = fread(rc_mem + (RC_IMG_BASE - RC_MEM_BASE), 1, n, f);
  fclose(f);
  if (got != n) {
    fprintf(stderr, "rc: incomplete physics image %s (read %lu of %u bytes)\n",
            path, (unsigned long)got, n);
    return -1;
  }
  return 0;
}
