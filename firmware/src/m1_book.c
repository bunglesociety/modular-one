/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* Modular One 0.2: the songs and the sketchbook in flash, for the player (seq.c).
 *
 * Two of Felucca's three user sample slots (eng_sample.c) are taken over, so the sample
 * editor keeps slot 1 only (editor.c):
 *   sketches  0xB4000..0xC7FFF (slot 2): 20 sectors. A sketch = a 32-byte header at the start
 *             of a sector, then its notes (the piece format, seq.c); it takes as many sectors
 *             as it needs; the sketches follow each other from the first sector, the first
 *             sector without a valid header is the free space.
 *   songs     0xC8000..0xDBFFF (slot 3): written from the computer (tools/fm1_songs.py,
 *             editor cmds 40-43): a 512-byte header (count, per song offset, notes, name), then
 *             the data at +512.
 * Everything here runs in the main loop (flash writes stop the audio for a sector erase). */
#define SK_BASE (SMP_USER_BASE + SMP_USER_SIZE)
#define SK_NSECT (SMP_USER_SIZE / 0x1000u)
#define SK_MAGIC 0x314B5346u                       /* "FSK1" */
#define SONG_BASE (SMP_USER_BASE + 2u * SMP_USER_SIZE)
#define SONG_SIZE SMP_USER_SIZE
#define SONG_DATA 512u
#define SONG_MAGIC 0x314E5346u                     /* "FSN1" */
#define SONG_MAX 20u
#define SK_OUT (SK_EV * 6u)

typedef struct {
    uint32_t magic;
    uint16_t nsect, num;                           /* sectors taken; its number when kept */
    uint32_t notes, len, dur10, crc;               /* len: bytes after the header; CRC-32 of them */
    uint32_t flags, rsv;                           /* flags SK_NAMED: the body starts with the name */
} sk_hdr_t;                                        /* 32 B */
#define SK_NAMED 0xFFFFFFFEu                       /* (0xFFFFFFFF: no name, the sketches of 0.2 before names) */
#define SK_NAME 12u                                /* the name: 12 bytes, 0-padded (all 0: "CROQUIS n") */
typedef struct {
    uint32_t off, notes;                           /* off: from the data start (+512) */
    char name[12];
} song_ent_t;                                      /* 20 B */
typedef struct {
    uint32_t magic;
    uint16_t version, count;
    uint32_t data_len, crc, rsv;
    song_ent_t e[SONG_MAX];
} song_hdr_t;                                      /* 20 + 400 = 420 B */
typedef char m1_size_check[sizeof(song_hdr_t) == 420u && sizeof(sk_hdr_t) == 32u ? 1 : -1];   /* fm1_songs.py's layout */

static struct {
    uint8_t scan;                                  /* the list is to be rebuilt (when nothing plays) */
    uint8_t writing;                               /* songs: SONG_BEGIN done, SONG_END not yet */
    uint8_t nsk, free_sect, songs;
    uint8_t last_sect, last_ok;                    /* the last kept sketch: overwritten by a deeper keep */
    uint16_t next_num;
    uint32_t last_w, last_ms, depth;
} m1b;
static uint8_t sk_out[SK_OUT + 256u] __attribute__((aligned(4)));

static const uint8_t *m1_xip(uint32_t off) { return fm1_xip_ptr(off); }
static void m1_inval(uint32_t off, uint32_t n)
{
    fm1_irq_off();
    fl_inval(off, n);
    fm1_irq_on();
}

static int m1_sk_valid(uint32_t s, const sk_hdr_t **hp)
{
    const sk_hdr_t *h = (const sk_hdr_t *)m1_xip(SK_BASE + s * 0x1000u);
    *hp = h;
    if (h->magic != SK_MAGIC || !h->nsect || s + h->nsect > SK_NSECT || !h->notes ||
        h->len > h->nsect * 0x1000u - sizeof(sk_hdr_t) || (h->flags == SK_NAMED && h->len < SK_NAME))
        return 0;
    return st_crc32((const uint8_t *)h + sizeof(sk_hdr_t), h->len) == h->crc;
}

static const song_hdr_t *m1_songs(void)
{
    const song_hdr_t *h = (const song_hdr_t *)m1_xip(SONG_BASE);
    uint32_t i;
    if (h->magic != SONG_MAGIC || h->version != 1 || !h->count || h->count > SONG_MAX ||
        h->data_len > SONG_SIZE - SONG_DATA)
        return 0;
    for (i = 0; i < h->count; i++)
        if (h->e[i].off >= h->data_len || !h->e[i].notes)
            return 0;
    if (st_crc32(m1_xip(SONG_BASE + SONG_DATA), h->data_len) != h->crc)
        return 0;
    return h;
}

static void m1_name(char *d, const char *s, uint32_t n)   /* printable ASCII, upper case */
{
    uint32_t i, k = 0;
    for (i = 0; i < n && s[i] && k < PL_NAME - 1u; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        d[k++] = c >= 32 && c < 127 ? c : ' ';
    }
    d[k] = 0;
}

/* a sketch's notes; its name (set in the Carnet app, editor cmd 49), else "CROQUIS n" */
static const uint8_t *m1_sk_notes(const sk_hdr_t *h)
{
    return (const uint8_t *)h + sizeof(sk_hdr_t) + (h->flags == SK_NAMED ? SK_NAME : 0u);
}
static void m1_sk_name(const sk_hdr_t *h, char *b)
{
    uint32_t k = 8, v = h->num;
    if (h->flags == SK_NAMED) {
        m1_name(b, (const char *)h + sizeof(sk_hdr_t), SK_NAME);
        if (b[0] && !(b[0] == ' ' && !b[1]))
            return;
    }
    str_cpy(b, "CROQUIS ", PL_NAME);
    if (v >= 10u)
        b[k++] = (char)('0' + v / 10u % 10u);
    b[k++] = (char)('0' + v % 10u);
    b[k] = 0;
}

/* rebuild the player's list: the songs, then the sketches */
static void m1_rescan(void)
{
    const song_hdr_t *sh = flash_ok ? m1_songs() : 0;
    uint32_t n = 0, i, s;
    m1b.songs = 0;
    if (sh) {
        for (i = 0; i < sh->count && n < PL_MAX; i++, n++) {
            pl_item[n].data = m1_xip(SONG_BASE + SONG_DATA + sh->e[i].off);
            pl_item[n].n = sh->e[i].notes;
            m1_name(pl_item[n].name, sh->e[i].name, 12);
            if (!pl_item[n].name[0])
                str_cpy(pl_item[n].name, "SONG", PL_NAME);
            pl_item[n].dur10 = pl_measure(pl_item[n].data, pl_item[n].n);
        }
        m1b.songs = (uint8_t)n;
    }
    pl_songs = m1b.songs;
    m1b.nsk = 0;
    m1b.next_num = 1;
    for (s = 0; flash_ok && s < SK_NSECT;) {
        const sk_hdr_t *h;
        if (!m1_sk_valid(s, &h))
            break;
        if (n < PL_MAX) {
            pl_item[n].data = m1_sk_notes(h);
            pl_item[n].n = h->notes;
            pl_item[n].dur10 = h->dur10;
            m1_sk_name(h, pl_item[n].name);
            n++;
        }
        if (h->num >= m1b.next_num)
            m1b.next_num = (uint16_t)(h->num + 1u);
        m1b.nsk++;
        s += h->nsect;
    }
    m1b.free_sect = (uint8_t)s;
    pl_count = n;
}

/* the player's list may only change while nothing plays */
static int m1_quiet(void) { return !demo_on && !demo_req; }

/* write a sketch at sector s (its sectors are erased first; the header goes last) */
static int m1_sk_write(uint32_t s, uint32_t num, uint32_t flags, const uint8_t *data, uint32_t len, uint32_t notes,
                       uint32_t dur10)
{
    static sk_hdr_t h;
    static uint8_t page[256] __attribute__((aligned(4)));
    uint32_t ns = (sizeof h + len + 0xFFFu) / 0x1000u, i, off, took;
    if (s + ns > SK_NSECT)
        return 1;
    for (i = 0; i < ns; i++) {
        audio_silence();
        if (fl_erase4k(SK_BASE + (s + i) * 0x1000u, &took))
            return 2;
        fm1_wdt_feed();
    }
    for (off = 0; off < sizeof h + len; off += 256u) {   /* the header's bytes stay 0xFF for now */
        uint32_t k;
        for (k = 0; k < 256u; k++) {
            uint32_t p = off + k;
            page[k] = p < sizeof h || p >= sizeof h + len ? 0xFFu : data[p - sizeof h];
        }
        if (fl_write(SK_BASE + s * 0x1000u + off, page, 256u))
            return 3;
    }
    h.magic = SK_MAGIC;
    h.nsect = (uint16_t)ns;
    h.num = (uint16_t)num;
    h.notes = notes;
    h.len = len;
    h.dur10 = dur10;
    h.crc = st_crc32(data, len);
    h.flags = flags;
    h.rsv = 0xFFFFFFFFu;
    if (fl_write(SK_BASE + s * 0x1000u, (const uint8_t *)&h, sizeof h))
        return 3;
    m1_inval(SK_BASE + s * 0x1000u, ns * 0x1000u);
    return 0;
}

static void m1_say_kept(uint32_t num, uint32_t dur10)
{
    char b[24] = "CROQUIS ";
    uint32_t k = 8, sec = dur10 / 100u;
    if (num >= 10u)
        b[k++] = (char)('0' + num / 10u % 10u);
    b[k++] = (char)('0' + num % 10u);
    b[k++] = ' ';
    b[k++] = ' ';
    if (sec >= 600u)
        b[k++] = (char)('0' + sec / 600u % 10u);
    b[k++] = (char)('0' + sec / 60u % 10u);
    b[k++] = ':';
    b[k++] = (char)('0' + sec % 60u / 10u);
    b[k++] = (char)('0' + sec % 10u);
    b[k] = 0;
    ui_message(b);
}

/* REC held: keep the last phrase (again right away: one phrase further back) */
static void m1_keep(void)
{
    uint32_t w = sk_w, deeper, depth, len, notes, dur10, got, s, num, rc;
    if (!flash_ok) {
        ui_message("NO FLASH");
        return;
    }
    deeper = m1b.last_ok && w == m1b.last_w && fm1_ms - m1b.last_ms < 20000u;
    depth = deeper ? m1b.depth + 1u : 1u;
    len = sk_extract(depth, sk_out + SK_NAME, SK_OUT, &notes, &dur10, &got);   /* after an empty name */
    if (!len) {
        ui_message("NOTHING TO KEEP");
        return;
    }
    if (deeper && got < depth) {                     /* no older phrase: that one stays as it is */
        ui_message("NO EARLIER PHRASE");
        m1b.last_ms = fm1_ms;
        return;
    }
    if (demo_on || demo_req) {                       /* the list changes under the player */
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (deeper) {
        s = m1b.last_sect;
        num = m1b.next_num ? m1b.next_num - 1u : 1u;
    } else {
        s = m1b.free_sect;
        num = m1b.next_num;
    }
    memset(sk_out, 0, SK_NAME);
    rc = m1_sk_write(s, num, SK_NAMED, sk_out, SK_NAME + len, notes, dur10);
    m1_rescan();
    if (rc == 1u) {
        ui_message("SKETCHBOOK FULL");
        m1b.last_ok = 0;
        return;
    }
    if (rc) {
        ui_message("FLASH ERROR");
        m1b.last_ok = 0;
        return;
    }
    m1b.last_ok = 1;
    m1b.last_sect = (uint8_t)s;
    m1b.last_w = w;
    m1b.last_ms = fm1_ms;
    m1b.depth = got;
    m1_say_kept(num, dur10);
}

/* erase the sketch j (0 = the first one): the ones after it move down, the sectors freed at the end are
 * erased (after the last sketch the flash stays blank: the scan stops there) */
static int m1_sk_erase(uint32_t j)
{
    const sk_hdr_t *h;
    uint32_t s = 0, i, dst, took;
    for (i = 0; i < j; i++) {
        if (!m1_sk_valid(s, &h))
            return 1;
        s += h->nsect;
    }
    if (!m1_sk_valid(s, &h))
        return 1;
    dst = s;
    s += h->nsect;
    while (s < SK_NSECT && m1_sk_valid(s, &h)) {
        uint32_t ns = h->nsect, num = h->num, notes = h->notes, len = h->len, dur10 = h->dur10, fl = h->flags;
        if (len > sizeof sk_out)
            return 2;
        memcpy(sk_out, (const uint8_t *)h + sizeof(sk_hdr_t), len);   /* (the copy may land on itself) */
        if (m1_sk_write(dst, num, fl, sk_out, len, notes, dur10))
            return 3;
        dst += (sizeof(sk_hdr_t) + len + 0xFFFu) / 0x1000u;
        s += ns;
    }
    for (i = dst; i < s; i++) {
        audio_silence();
        if (fl_erase4k(SK_BASE + i * 0x1000u, &took))
            return 3;
        fm1_wdt_feed();
    }
    m1_inval(SK_BASE + dst * 0x1000u, (s - dst) * 0x1000u);
    m1b.last_ok = 0;
    return 0;
}

/* the sketch j renamed (editor cmd 49; name: up to 12 characters, "" = back to "CROQUIS n"): written
 * again in place. A sketch from before names gets the 12 bytes of its name if its sectors have room */
static int m1_sk_rename(uint32_t j, const char *name)
{
    const sk_hdr_t *h;
    uint32_t s = 0, i, nl, num, notes, dur10, rc;
    for (i = 0; i < j; i++) {
        if (!m1_sk_valid(s, &h))
            return 1;
        s += h->nsect;
    }
    if (!m1_sk_valid(s, &h))
        return 1;
    nl = h->len - (h->flags == SK_NAMED ? SK_NAME : 0u);
    if (sizeof(sk_hdr_t) + SK_NAME + nl > h->nsect * 0x1000u)
        return 4;                                    /* no room for the name */
    memset(sk_out, 0, SK_NAME);
    for (i = 0; i < SK_NAME && name[i]; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        sk_out[i] = (uint8_t)(c >= 32 && c < 127 ? c : ' ');
    }
    memcpy(sk_out + SK_NAME, m1_sk_notes(h), nl);
    num = h->num;
    notes = h->notes;
    dur10 = h->dur10;
    rc = (uint32_t)m1_sk_write(s, num, SK_NAMED, sk_out, SK_NAME + nl, notes, dur10);
    if (m1b.last_ok && m1b.last_sect == s)
        m1b.last_ok = 0;                             /* a deeper keep would overwrite the renamed one */
    return (int)rc;
}

/* the list's item k erased (a sketch; ui_input.c, REC held twice in the list): the player stops,
 * the list is rebuilt, the player goes on from the same place */
static void m1_del(uint32_t k)
{
    char b[PL_NAME + 8];
    int was = demo_on;
    if (!flash_ok || k < pl_songs || k >= pl_count)
        return;
    str_cpy(b, pl_item[k].name, PL_NAME);
    if (demo_on || demo_req) {
        demo_req = 2;
        while (demo_on)
            fm1_wdt_feed();
    }
    if (m1_sk_erase(k - pl_songs)) {
        m1_rescan();
        ui_message("FLASH ERROR");
        return;
    }
    m1_rescan();
    str_cpy(b + str_len(b), " ERASED", 8);
    ui_message(b);
    if (was && pl_count) {
        pl_start = (uint8_t)(k < pl_count ? k : 0u);
        pl_cursor = pl_start;
        demo_req = 1;
    }
}

/* erase every sketch (editor cmd 46) */
static int m1_sk_clear(void)
{
    uint32_t i, took;
    int rc = 0;
    for (i = 0; i < SK_NSECT && !rc; i++) {
        audio_silence();
        rc = fl_erase4k(SK_BASE + i * 0x1000u, &took);
        fm1_wdt_feed();
    }
    m1_inval(SK_BASE, SK_NSECT * 0x1000u);
    m1b.last_ok = 0;
    m1b.scan = 1;
    return rc;
}

static void m1_boot(void)
{
    m1_rescan();
}

/* main loop */
static void m1_service(void)
{
    if (m1_del_req) {
        uint32_t k = m1_del_req - 1u;
        m1_del_req = 0;
        m1_del(k);
    }
    if (m1_keep_req) {
        m1_keep_req = 0;
        m1_keep();
    }
    if (m1b.scan && m1_quiet()) {
        m1b.scan = 0;
        m1_rescan();
    }
}
