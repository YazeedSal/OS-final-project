/*
 * gui_m4.c — Milestones 4, 5 & 6: Graph Visualiser (PCB edition)
 *
 * Entry points:
 *   draw_gui_m4()  — M4: path-driven animation, Play/Stop button
 *   draw_gui_m5()  — M5/M6: pipe-driven animation, Play/Stop pauses visuals
 *
 * M6 additions:
 *   - ANIM_QUEUED state: traveler is blocked outside a node (sem_wait)
 *   - Queued travelers drawn as a dim pulsing ring beside the node
 *   - WAITING pipe messages consumed instantly (no timer delay)
 */

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "../include/graph.h"
#include "../include/travelers.h"
#include "../include/ipc.h"

/* ═══════════════════════════════════════════════════════════════════
   CONSTANTS
   ═══════════════════════════════════════════════════════════════════ */
#define WINDOW_W      1280
#define WINDOW_H       800
#define FPS             60

#define CHIP_HALF       28
#define LEG_COUNT        4
#define LEG_LEN         11
#define LEG_THICK        2
#define LEG_SPACING      9

#define HOP_SEC       0.30f   /* seconds per hop (300ms × weight = total) */
#define NODE_WAIT_SEC 1.00f   /* seconds to wait at a node (M4)           */
#define ENTITY_R      10.0f   /* moving packet radius                      */
#define QUEUED_R       7.0f   /* queued packet radius (smaller)            */

/* buffer per traveler: worst case M6 = 2 messages per node + finished */
#define MSG_BUF_SIZE  (MAX_NODES * 2 + 4)

/* ═══════════════════════════════════════════════════════════════════
   COLOURS
   ═══════════════════════════════════════════════════════════════════ */
#define C_BG          CLITERAL(Color){  8,  20,  12, 255 }
#define C_GRID        CLITERAL(Color){ 16,  38,  22, 255 }
#define C_TRACE_HI    CLITERAL(Color){ 40, 210,  80, 255 }
#define C_TRACE_LO    CLITERAL(Color){ 18,  90,  35, 255 }
#define C_PATH_HI     CLITERAL(Color){ 80, 180, 255, 255 }
#define C_PATH_LO     CLITERAL(Color){ 20,  60, 120, 255 }
#define C_CHIP_BODY   CLITERAL(Color){  6,   6,   6, 255 }
#define C_CHIP_BORDER CLITERAL(Color){ 55, 175,  75, 255 }
#define C_CHIP_INNER  CLITERAL(Color){ 22,  65,  30, 255 }
#define C_LEG         CLITERAL(Color){170, 165, 130, 255 }
#define C_NOTCH       CLITERAL(Color){ 22,  22,  22, 255 }
#define C_LABEL       CLITERAL(Color){200, 255, 200, 255 }
#define C_WEIGHT      CLITERAL(Color){255, 215,  50, 255 }
#define C_HUD         CLITERAL(Color){ 40, 210,  80, 255 }
#define C_HUD_DIM     CLITERAL(Color){ 20, 100,  40, 255 }
#define C_BTN_IDLE    CLITERAL(Color){ 20,  60,  28, 255 }
#define C_BTN_HOV     CLITERAL(Color){ 30,  90,  45, 255 }
#define C_BTN_BORDER  CLITERAL(Color){ 40, 180,  70, 255 }
#define C_BTN_HBORD   CLITERAL(Color){ 80, 220, 100, 255 }
#define C_BTN_TEXT    CLITERAL(Color){200, 255, 200, 255 }

static const Color TCOLORS[MAX_TRAVELERS] = {
    {255, 220,  50, 255},  /* yellow      */
    {255,  80,  80, 255},  /* red         */
    { 80, 180, 255, 255},  /* blue        */
    {180,  80, 255, 255},  /* purple      */
    { 80, 255, 180, 255},  /* mint        */
    {255, 160,  30, 255},  /* orange      */
    {255, 100, 200, 255},  /* pink        */
    {100, 255, 100, 255},  /* light green */
    {255, 255, 120, 255},  /* cream       */
    { 80, 220, 220, 255},  /* cyan        */
    {200, 120,  60, 255},  /* brown       */
    {160, 255,  80, 255},  /* lime        */
    {255,  60, 160, 255},  /* magenta     */
    {120, 160, 255, 255},  /* periwinkle  */
    {220, 200, 255, 255},  /* lavender    */
};

/* ═══════════════════════════════════════════════════════════════════
   ANIMATION STATE
   ANIM_IDLE    — not started yet
   ANIM_MOVING  — travelling along an edge
   ANIM_WAITING — inside a node, waiting 1s (M4)
   ANIM_QUEUED  — blocked outside a node waiting for semaphore (M6)
   ANIM_DONE    — reached destination
   ═══════════════════════════════════════════════════════════════════ */
typedef enum {
    ANIM_IDLE,
    ANIM_MOVING,
    ANIM_WAITING,
    ANIM_QUEUED,
    ANIM_DONE
} AnimState;

typedef struct {
    AnimState state;
    int       segIdx;
    int       hopIdx;
    int       hopTotal;
    float     timer;
    Vector2   pos;
    int       queued_at;   /* node index where traveler is queued (-1 if not) */
} TravAnim;

/* ═══════════════════════════════════════════════════════════════════
   LAYOUT
   ═══════════════════════════════════════════════════════════════════ */
static Vector2 gPos[MAX_NODES];

static void compute_layout(int n)
{
    float cx = WINDOW_W / 2.0f, cy = WINDOW_H / 2.0f;
    float r  = fminf(cx, cy) - CHIP_HALF - LEG_LEN - 60.0f;
    if (n == 1) { gPos[0] = (Vector2){cx, cy}; return; }
    for (int i = 0; i < n; i++) {
        float a = (float)i / (float)n * 2.0f * PI - PI / 2.0f;
        gPos[i] = (Vector2){ cx + r * cosf(a), cy + r * sinf(a) };
    }
}

/* ═══════════════════════════════════════════════════════════════════
   HELPERS
   ═══════════════════════════════════════════════════════════════════ */
static int edge_weight(const Graph* g, int u, int v)
{
    AdjNode* cur = g->adjLists[u];
    while (cur) {
        if (cur->destination == v) return cur->weight;
        cur = cur->next;
    }
    return 1;
}

static int on_path(const Traveler* t, int u, int v)
{
    for (int i = 0; i < t->pathLength - 1; i++)
        if (t->path[i] == u && t->path[i+1] == v) return 1;
    return 0;
}

static int any_on_path(const Traveler* travelers, int count, int u, int v)
{
    for (int i = 0; i < count; i++)
        if (on_path(&travelers[i], u, v)) return 1;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
   ANIMATION TICK  (M4 path-driven only)
   ═══════════════════════════════════════════════════════════════════ */
static void trav_anim_init(TravAnim* a, const Traveler* t, const Graph* g)
{
    memset(a, 0, sizeof(TravAnim));
    a->queued_at = -1;
    if (t->pathLength < 1) { a->state = ANIM_DONE; return; }
    a->state    = ANIM_IDLE;
    a->segIdx   = 0;
    a->hopIdx   = 0;
    a->hopTotal = (t->pathLength > 1)
                  ? edge_weight(g, t->path[0], t->path[1]) : 1;
    a->pos      = gPos[t->path[0]];
    a->timer    = 0.0f;
}

static void trav_anim_tick(TravAnim* a, const Traveler* t,
                           const Graph* g, float dt)
{
    if (a->state == ANIM_IDLE || a->state == ANIM_DONE) return;

    a->timer += dt;

    if (a->state == ANIM_WAITING) {
        if (a->timer < NODE_WAIT_SEC) return;
        a->timer = 0.0f;
        a->segIdx++;
        if (a->segIdx >= t->pathLength - 1) {
            a->state = ANIM_DONE;
            a->pos   = gPos[t->path[t->pathLength - 1]];
            return;
        }
        a->hopIdx   = 0;
        a->hopTotal = edge_weight(g, t->path[a->segIdx],
                                     t->path[a->segIdx + 1]);
        a->state = ANIM_MOVING;
        return;
    }

    /* ANIM_MOVING: advance along current segment */
    Vector2 from = gPos[t->path[a->segIdx]];
    Vector2 to   = gPos[t->path[a->segIdx + 1]];

    /* travel time for this segment = weight × HOP_SEC */
    int w = edge_weight(g, t->path[a->segIdx], t->path[a->segIdx + 1]);
    float seg_dur = (float)w * HOP_SEC;

    if (a->timer >= HOP_SEC) {
        a->timer -= HOP_SEC;
        a->hopIdx++;
        if (a->hopIdx >= w) {
            /* arrived at next node */
            a->pos    = to;
            a->hopIdx = 0;
            if (t->path[a->segIdx + 1] == t->path[t->pathLength - 1])
                a->state = ANIM_DONE;
            else
                a->state = ANIM_WAITING;
        } else {
            float tf = (float)a->hopIdx / (float)w;
            a->pos = (Vector2){ from.x + (to.x - from.x) * tf,
                                from.y + (to.y - from.y) * tf };
        }
    } else {
        float t0 = (float)a->hopIdx       / (float)w;
        float t1 = (float)(a->hopIdx + 1) / (float)w;
        float tf = t0 + (t1 - t0) * (a->timer / HOP_SEC);
        a->pos = (Vector2){ from.x + (to.x - from.x) * tf,
                            from.y + (to.y - from.y) * tf };
    }
    (void)seg_dur;
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: background
   ═══════════════════════════════════════════════════════════════════ */
static void draw_background(void)
{
    for (int x = 0; x < WINDOW_W; x += 40)
        DrawLine(x, 0, x, WINDOW_H, C_GRID);
    for (int y = 0; y < WINDOW_H; y += 40)
        DrawLine(0, y, WINDOW_W, y, C_GRID);

    int p = 22;
    Color fid = CLITERAL(Color){28, 75, 38, 255};
    int c[4][2] = {{p,p},{WINDOW_W-p,p},{p,WINDOW_H-p},{WINDOW_W-p,WINDOW_H-p}};
    for (int i = 0; i < 4; i++) {
        DrawCircleLines(c[i][0], c[i][1], 9, fid);
        DrawCircleLines(c[i][0], c[i][1], 4, fid);
    }
    DrawRectangleLines(8, 8, WINDOW_W-16, WINDOW_H-16,
                       CLITERAL(Color){24, 60, 30, 255});
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: arrow
   ═══════════════════════════════════════════════════════════════════ */
static void draw_arrow(Vector2 p1, Vector2 p2, int highlighted, Color hi_col)
{
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    float head = 16.0f, wing = 7.0f;
    float dx = p2.x-p1.x, dy = p2.y-p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 2.0f*margin + head + 4.0f) return;

    float ux = dx/len, uy = dy/len;
    float px = -uy,    py =  ux;

    Vector2 a    = { p1.x + ux*margin, p1.y + uy*margin };
    Vector2 tip  = { p2.x - ux*margin, p2.y - uy*margin };
    Vector2 base = { tip.x - ux*head,  tip.y - uy*head  };
    Vector2 bl   = { base.x + px*wing, base.y + py*wing };
    Vector2 br   = { base.x - px*wing, base.y - py*wing };

    Color hi    = highlighted ? hi_col   : C_TRACE_HI;
    Color lo    = highlighted ? CLITERAL(Color){(unsigned char)(hi_col.r/4),
                                                (unsigned char)(hi_col.g/4),
                                                (unsigned char)(hi_col.b/4), 255}
                              : C_TRACE_LO;
    float thick = highlighted ? 6.0f : 5.0f;
    float thin  = highlighted ? 3.0f : 2.0f;

    DrawLineEx(a, base, thick, lo);
    DrawLineEx(a, base, thin,  hi);
    DrawTriangle(tip, br, bl, hi);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: weight label
   ═══════════════════════════════════════════════════════════════════ */
static void draw_weight_label(Vector2 p1, Vector2 p2, int w, Font font)
{
    float dx = p2.x-p1.x, dy = p2.y-p1.y;
    float len = sqrtf(dx*dx+dy*dy);
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    if (len < 2.0f*margin + 12.0f) return;

    float px = -dy/len, py = dx/len;
    Vector2 mid = { p1.x + dx*0.70f + px*18.0f,
                    p1.y + dy*0.70f + py*18.0f };

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", w);
    int fs = 15;
    Vector2 sz  = MeasureTextEx(font, buf, fs, 1);
    Rectangle pill = { mid.x-sz.x/2-5, mid.y-sz.y/2-3, sz.x+10, sz.y+6 };

    DrawRectangleRounded(pill, 0.4f, 4, CLITERAL(Color){0,0,0,255});
    DrawRectangleRoundedLines(pill, 0.4f, 4, CLITERAL(Color){40,100,50,255});
    DrawTextEx(font, buf, (Vector2){mid.x-sz.x/2, mid.y-sz.y/2}, fs, 1, C_WEIGHT);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: CPU chip
   ═══════════════════════════════════════════════════════════════════ */
static void draw_cpu(Vector2 p, int id, Color border_col,
                     int use_override, Font font)
{
    int h = CHIP_HALF;

    for (int side = 0; side < 4; side++) {
        for (int k = 0; k < LEG_COUNT; k++) {
            float off = (k - (LEG_COUNT-1)/2.0f) * LEG_SPACING;
            Vector2 s, e;
            switch (side) {
                case 0: s=(Vector2){p.x+off,p.y-h};   e=(Vector2){p.x+off,p.y-h-LEG_LEN}; break;
                case 1: s=(Vector2){p.x+off,p.y+h};   e=(Vector2){p.x+off,p.y+h+LEG_LEN}; break;
                case 2: s=(Vector2){p.x-h,  p.y+off}; e=(Vector2){p.x-h-LEG_LEN,p.y+off}; break;
                default:s=(Vector2){p.x+h,  p.y+off}; e=(Vector2){p.x+h+LEG_LEN,p.y+off}; break;
            }
            DrawLineEx(s, e, LEG_THICK, C_LEG);
            DrawCircleV(e, 2.5f, C_LEG);
        }
    }

    DrawRectangle((int)(p.x-h+4),(int)(p.y-h+4),h*2,h*2,CLITERAL(Color){0,25,5,140});
    DrawRectangle((int)(p.x-h),(int)(p.y-h),h*2,h*2,C_CHIP_BODY);

    Color border = use_override ? border_col : C_CHIP_BORDER;
    float bt     = use_override ? 2.5f : 1.5f;
    DrawRectangleLinesEx(
        (Rectangle){(float)(p.x-h),(float)(p.y-h),(float)(h*2),(float)(h*2)},
        bt, border);

    DrawCircle((int)p.x,(int)(p.y-h),5,C_NOTCH);
    DrawCircleLines((int)p.x,(int)(p.y-h),5,border);
    DrawCircle((int)(p.x-h+7),(int)(p.y-h+7),3,CLITERAL(Color){55,175,75,255});

    DrawLine((int)(p.x-h+10),(int)p.y,(int)(p.x+h-10),(int)p.y,C_CHIP_INNER);
    DrawLine((int)p.x,(int)(p.y-h+10),(int)p.x,(int)(p.y+h-10),C_CHIP_INNER);
    DrawCircle((int)(p.x-9),(int)(p.y-9),2,C_CHIP_INNER);
    DrawCircle((int)(p.x+9),(int)(p.y+9),2,C_CHIP_INNER);
    DrawCircle((int)(p.x-9),(int)(p.y+9),2,C_CHIP_INNER);
    DrawCircle((int)(p.x+9),(int)(p.y-9),2,C_CHIP_INNER);

    char buf2[10];
    snprintf(buf2, sizeof(buf2), "CPU%d", id);
    int fs = 13;
    Vector2 sz = MeasureTextEx(font, buf2, fs, 1);
    DrawTextEx(font, buf2, (Vector2){p.x-sz.x/2, p.y-sz.y/2}, fs, 1, C_LABEL);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: moving packet (solid, full brightness)
   ═══════════════════════════════════════════════════════════════════ */
static void draw_packet(Vector2 pos, Color col)
{
    Color glow = col; glow.a = 60;
    DrawCircle((int)pos.x,(int)pos.y,(int)(ENTITY_R+8),glow);
    DrawCircle((int)pos.x,(int)pos.y,(int)ENTITY_R,col);
    DrawCircle((int)pos.x,(int)pos.y,4,CLITERAL(Color){30,15,0,255});
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: queued packet (dim ring outside node, offset by queue index)
   Shows clearly that this traveler is waiting outside the node.
   ═══════════════════════════════════════════════════════════════════ */
static void draw_queued_packet(Vector2 node_pos, Color col,
                               int queue_idx, float time)
{
    /* offset each queued traveler in a small arc around the node */
    float angle = -PI / 2.0f + (float)queue_idx * (PI / 4.0f);
    float dist  = CHIP_HALF + LEG_LEN + ENTITY_R + 10.0f;
    Vector2 pos = {
        node_pos.x + cosf(angle) * dist,
        node_pos.y + sinf(angle) * dist
    };

    /* dim colour */
    Color dim = col; dim.a = 120;
    /* pulsing outer ring */
    float pulse = 0.5f + 0.5f * sinf(time * 4.0f);
    Color ring = col; ring.a = (unsigned char)(80.0f * pulse);

    DrawCircle((int)pos.x,(int)pos.y,(int)(QUEUED_R+6),ring);
    DrawCircle((int)pos.x,(int)pos.y,(int)QUEUED_R,dim);
    /* dashed border to distinguish from moving packet */
    DrawCircleLines((int)pos.x,(int)pos.y,(int)(QUEUED_R+2),col);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: Play/Stop/Restart button — returns 1 if clicked
   ═══════════════════════════════════════════════════════════════════ */
static int draw_button(int playing, int all_done)
{
    Rectangle btn = { WINDOW_W-130, WINDOW_H-52, 110, 36 };
    int hov     = CheckCollisionPointRec(GetMousePosition(), btn);
    int clicked = hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    DrawRectangleRounded(btn, 0.3f, 6, hov ? C_BTN_HOV   : C_BTN_IDLE);
    DrawRectangleRoundedLines(btn, 0.3f,6, hov ? C_BTN_HBORD : C_BTN_BORDER);

    const char* lbl = all_done ? "Restart" : playing ? "[ Stop ]" : "[ Play ]";
    int fs = 16;
    Vector2 sz = MeasureTextEx(GetFontDefault(), lbl, fs, 1);
    DrawText(lbl,
             (int)(btn.x + (btn.width  - sz.x) / 2),
             (int)(btn.y + (btn.height - sz.y) / 2),
             fs, C_BTN_TEXT);

    return clicked;
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: HUD
   ═══════════════════════════════════════════════════════════════════ */
static void draw_hud(const Graph* g, int numTravelers,
                     int sources[], int dests[],
                     int playing, int all_done,
                     const char* title)
{
    DrawText(title, 16, 14, 18, C_HUD);
    char info[80];
    snprintf(info, sizeof(info), "Nodes: %d   Edges: %d   Travelers: %d",
             g->numNodes, g->numEdges, numTravelers);
    DrawText(info, 16, 36, 14, C_HUD_DIM);

    for (int i = 0; i < numTravelers; i++) {
        Color col = TCOLORS[i % MAX_TRAVELERS];
        int lx = WINDOW_W-210, ly = 14 + i*18;
        DrawRectangle(lx, ly, 12, 12, col);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "T%d: CPU%d -> CPU%d", i, sources[i], dests[i]);
        DrawText(lbl, lx+16, ly, 12, col);
    }

    const char* status = all_done ? "" : playing ? "Transmitting..." : "Paused — Press Play";
    DrawText(status, 16, WINDOW_H-48, 14, C_HUD_DIM);
    DrawText("[ESC] Quit", 16, WINDOW_H-24, 13, C_HUD_DIM);

    if (all_done) {
        int bw=420, bh=64, bx=(WINDOW_W-bw)/2, by=(WINDOW_H-bh)/2;
        DrawRectangleRounded(
            (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
            0.2f, 8, CLITERAL(Color){0,0,0,220});
        DrawRectangleRoundedLines(
            (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
            0.2f, 8, TCOLORS[0]);
        const char* msg = "All travelers arrived!";
        int fs = 22;
        Vector2 sz = MeasureTextEx(GetFontDefault(), msg, fs, 1);
        DrawText(msg,
                 (int)((float)bx + ((float)bw - sz.x) / 2.0f),
                 (int)((float)by + ((float)bh - sz.y) / 2.0f),
                 fs, TCOLORS[0]);
    }
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW FRAME — shared by M4 and M5/M6
   ═══════════════════════════════════════════════════════════════════ */
static void draw_frame(const Graph* g,
                       TravAnim anims[], int numTravelers,
                       Traveler* travelers,   /* M4: non-NULL, M5/M6: NULL */
                       int cur_nodes[],       /* M5/M6: non-NULL, M4: NULL */
                       int nxt_nodes[],       /* M5/M6: non-NULL, M4: NULL */
                       Font font, float time)
{
    /* ── edges ── */
    for (int i = 0; i < g->numEdges; i++) {
        int u = g->edges[i].source;
        int v = g->edges[i].destination;
        int   hi     = 0;
        Color hi_col = C_PATH_HI;   /* fallback, overridden below */

        if (travelers) {
            /* M4: find which traveler is on this edge and use their colour */
            for (int t = 0; t < numTravelers; t++) {
                if (on_path(&travelers[t], u, v)) {
                    hi     = 1;
                    hi_col = TCOLORS[t % MAX_TRAVELERS];
                    break;
                }
            }
        } else if (cur_nodes && nxt_nodes) {
            /* M5/M6: find the traveler currently traversing this edge */
            for (int t = 0; t < numTravelers; t++) {
                if (cur_nodes[t] == u && nxt_nodes[t] == v) {
                    hi     = 1;
                    hi_col = TCOLORS[t % MAX_TRAVELERS];
                    break;
                }
            }
        }

        draw_arrow(gPos[u], gPos[v], hi, hi_col);
        draw_weight_label(gPos[u], gPos[v], g->edges[i].weight, font);
    }

    /* ── chips — tint border for travelers inside the node ── */
    for (int node = 0; node < g->numNodes; node++) {
        int   use_col = 0;
        Color col     = C_CHIP_BORDER;

        for (int t = 0; t < numTravelers; t++) {
            int inside = 0;

            if (travelers) {
                /* M4: WAITING state = inside node */
                if (anims[t].state == ANIM_WAITING
                    && anims[t].segIdx+1 < travelers[t].pathLength
                    && travelers[t].path[anims[t].segIdx+1] == node)
                    inside = 1;
                if (anims[t].state == ANIM_IDLE
                    && travelers[t].pathLength > 0
                    && travelers[t].path[0] == node)
                    inside = 1;
            } else if (cur_nodes) {
                /* M5/M6: MOVING or WAITING (inside) state at this node */
                if (cur_nodes[t] == node
                    && anims[t].state != ANIM_DONE
                    && anims[t].state != ANIM_QUEUED)
                    inside = 1;
            }

            if (inside) {
                col     = TCOLORS[t % MAX_TRAVELERS];
                use_col = 1; break;
            }
        }
        draw_cpu(gPos[node], node, col, use_col, font);
    }

    /* ── moving packets (not queued, not idle, not done) ── */
    for (int i = 0; i < numTravelers; i++) {
        if (anims[i].state == ANIM_IDLE   ||
            anims[i].state == ANIM_DONE   ||
            anims[i].state == ANIM_QUEUED) continue;
        draw_packet(anims[i].pos, TCOLORS[i % MAX_TRAVELERS]);
    }

    /* ── queued packets: one per node, offset by queue position ── */
    for (int node = 0; node < g->numNodes; node++) {
        int q_idx = 0;
        for (int t = 0; t < numTravelers; t++) {
            if (anims[t].state == ANIM_QUEUED && anims[t].queued_at == node) {
                draw_queued_packet(gPos[node],
                                   TCOLORS[t % MAX_TRAVELERS],
                                   q_idx, time);
                q_idx++;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC: MILESTONE 4
   ═══════════════════════════════════════════════════════════════════ */
void draw_gui_m4(const Graph* g, Traveler* travelers, int numTravelers)
{
    if (!g || !travelers || numTravelers <= 0) return;

    compute_layout(g->numNodes);

    TravAnim anims[MAX_TRAVELERS];
    for (int i = 0; i < numTravelers; i++)
        trav_anim_init(&anims[i], &travelers[i], g);

    int sources[MAX_TRAVELERS], dests[MAX_TRAVELERS];
    for (int i = 0; i < numTravelers; i++) {
        sources[i] = travelers[i].source;
        dests[i]   = travelers[i].destination;
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_W, WINDOW_H, "OS Project — Graph Visualiser M4 (PCB edition)");
    SetTargetFPS(FPS);
    Font font = GetFontDefault();
    int   playing = 0;
    float time    = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time += dt;

        int all_done = 1;
        for (int i = 0; i < numTravelers; i++)
            if (anims[i].state != ANIM_DONE) { all_done = 0; break; }

        if (draw_button(playing, all_done)) {
            if (all_done) {
                for (int i = 0; i < numTravelers; i++)
                    trav_anim_init(&anims[i], &travelers[i], g);
                playing = 1;
            } else {
                playing = !playing;
            }
        }

        if (playing) {
            for (int i = 0; i < numTravelers; i++)
                if (anims[i].state == ANIM_IDLE)
                    anims[i].state = ANIM_MOVING;

            for (int i = 0; i < numTravelers; i++)
                trav_anim_tick(&anims[i], &travelers[i], g, dt);
        }

        BeginDrawing();
        ClearBackground(C_BG);
        draw_background();
        draw_frame(g, anims, numTravelers, travelers, NULL, NULL, font, time);
        draw_hud(g, numTravelers, sources, dests, playing, all_done,
                 "OS Project — Graph Visualiser M4");
        draw_button(playing, all_done);
        EndDrawing();
    }

    CloseWindow();
}

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC: MILESTONE 5 & 6
   ═══════════════════════════════════════════════════════════════════ */
void draw_gui_m5(const Graph* g,
                 int read_fds[], int sources[], int dests[],
                 int numTravelers)
{
    if (!g || !read_fds || numTravelers <= 0) return;

    compute_layout(g->numNodes);

    /* ── drain ALL pipe messages before opening the window ──
       Children start immediately on fork. Buffer everything so
       nothing is lost, then replay on Play press.              */
    TravelMessage buf[MAX_TRAVELERS][MSG_BUF_SIZE];
    int           buf_len[MAX_TRAVELERS];
    memset(buf_len, 0, sizeof(buf_len));

    {
        int closed[MAX_TRAVELERS];
        memset(closed, 0, sizeof(closed));
        int active = numTravelers;

        while (active > 0) {
            fd_set rfds;
            FD_ZERO(&rfds);
            int maxfd = 0;
            for (int i = 0; i < numTravelers; i++) {
                if (!closed[i]) {
                    FD_SET(read_fds[i], &rfds);
                    if (read_fds[i] > maxfd) maxfd = read_fds[i];
                }
            }
            if (select(maxfd+1, &rfds, NULL, NULL, NULL) <= 0) break;

            for (int i = 0; i < numTravelers; i++) {
                if (closed[i] || !FD_ISSET(read_fds[i], &rfds)) continue;
                TravelMessage msg;
                ssize_t n = read(read_fds[i], &msg, sizeof(msg));
                if (n <= 0) { closed[i] = 1; active--; continue; }

                if (buf_len[i] < MSG_BUF_SIZE)
                    buf[i][buf_len[i]++] = msg;

                if (msg.finished) { closed[i] = 1; active--; }
            }
        }
    }

    /* ── animation state ── */
    TravAnim anims[MAX_TRAVELERS];
    int cur_node[MAX_TRAVELERS], nxt_node[MAX_TRAVELERS];
    int buf_pos[MAX_TRAVELERS];
    float next_msg_timer[MAX_TRAVELERS];

    /*
     * inside_node[i] = the node traveler i is currently occupying
     * (holding the semaphore). Set when ENTERED is consumed, cleared
     * when the next ENTERED or DESTINATION is consumed.
     * This is the ground truth for "is this node occupied?" —
     * independent of animation state which changes as soon as
     * the traveler starts moving toward the next node.
     */
    int inside_node[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        memset(&anims[i], 0, sizeof(TravAnim));
        anims[i].state     = ANIM_IDLE;
        anims[i].pos       = gPos[sources[i]];
        anims[i].queued_at = -1;
        cur_node[i]        = sources[i];
        nxt_node[i]        = -1;
        buf_pos[i]         = 0;
        next_msg_timer[i]  = 0.0f;
        inside_node[i]     = -1;   /* not inside any node yet */
    }

    int   playing  = 0;
    int   all_done = 0;
    float time     = 0.0f;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_W, WINDOW_H, "OS Project — Graph Visualiser M5/M6 (PCB edition)");
    SetTargetFPS(FPS);
    Font font = GetFontDefault();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time += dt;

        /* ── button ── */
        if (draw_button(playing, all_done)) {
            if (all_done) {
                /* restart */
                for (int i = 0; i < numTravelers; i++) {
                    memset(&anims[i], 0, sizeof(TravAnim));
                    anims[i].state     = ANIM_IDLE;
                    anims[i].pos       = gPos[sources[i]];
                    anims[i].queued_at = -1;
                    cur_node[i]        = sources[i];
                    nxt_node[i]        = -1;
                    buf_pos[i]         = 0;
                    next_msg_timer[i]  = 0.0f;
                    inside_node[i]     = -1;
                }
                all_done = 0;
                playing  = 1;
            } else {
                playing = !playing;
            }
        }

        if (!playing) goto draw;

        /* ── replay buffered messages ── */
        {
            int done_count = 0;
            for (int i = 0; i < numTravelers; i++) {
                if (anims[i].state == ANIM_DONE) { done_count++; continue; }

                next_msg_timer[i] -= dt;
                if (next_msg_timer[i] > 0.0f) continue;
                if (buf_pos[i] >= buf_len[i])  { done_count++; continue; }

                /* ── helper: is 'node' occupied by any OTHER traveler? ──
                   Uses inside_node[] which reflects semaphore ownership,
                   not animation state. This correctly blocks even after
                   the occupant has started moving toward its next node. */
                #define node_is_occupied(node, skip_i) ({ \
                    int _occ = 0; \
                    for (int _j = 0; _j < numTravelers; _j++) { \
                        if (_j == (skip_i)) continue; \
                        if (inside_node[_j] == (node)) { _occ = 1; break; } \
                    } \
                    _occ; \
                })

                /* ── if QUEUED, block until node is free AND we are first ──
                   To avoid two queued travelers entering simultaneously,
                   also check that no other QUEUED traveler has a lower
                   buffer position (i.e. arrived earlier). */
                if (anims[i].state == ANIM_QUEUED) {
                    int node = anims[i].queued_at;

                    /* still occupied? */
                    if (node_is_occupied(node, i)) {
                        next_msg_timer[i] = 0.05f;
                        continue;
                    }

                    /* is there another queued traveler at the same node
                       who arrived earlier (lower buf_pos)? */
                    int earlier_queued = 0;
                    for (int j = 0; j < numTravelers; j++) {
                        if (j == i) continue;
                        if (anims[j].state == ANIM_QUEUED &&
                            anims[j].queued_at == node &&
                            buf_pos[j] < buf_pos[i]) {
                            earlier_queued = 1; break;
                        }
                    }
                    if (earlier_queued) {
                        next_msg_timer[i] = 0.05f;
                        continue;
                    }
                }

                TravelMessage* msg = &buf[i][buf_pos[i]++];

                /* Is this the very first message for this traveler? */
                int is_first = (buf_pos[i] == 1);

                if (msg->finished) {
                    printf("[PID=%d] finished\n", (int)msg->pid);
                    fflush(stdout);
                    anims[i].state     = ANIM_DONE;
                    anims[i].queued_at = -1;
                    inside_node[i]     = -1;
                    done_count++;

                } else if (msg->waiting) {
                    /* ── WAITING: traveler wants to enter a node ──
                       Only show QUEUED if the node is actually occupied.
                       If free, skip straight to ENTERED next frame.    */
                    int node = msg->current_node;

                    int next_is_dest = (buf_pos[i] < buf_len[i] &&
                                        buf[i][buf_pos[i]].next_node == -1 &&
                                        !buf[i][buf_pos[i]].waiting);

                    if (!node_is_occupied(node, i) || is_first || next_is_dest) {
                        /* node is free — skip QUEUED */
                        anims[i].pos      = gPos[node];
                        next_msg_timer[i] = 0.0f;
                    } else {
                        /* node is occupied — show as queued */
                        anims[i].state     = ANIM_QUEUED;
                        anims[i].queued_at = node;
                        anims[i].pos       = gPos[node];
                        next_msg_timer[i]  = NODE_WAIT_SEC;
                    }

                } else if (msg->next_node == -1) {
                    /* ── DESTINATION reached ── */
                    printf("[PID=%d] arrived at node %d | DESTINATION\n",
                           (int)msg->pid, msg->current_node);
                    fflush(stdout);
                    anims[i].pos       = gPos[msg->current_node];
                    anims[i].state     = ANIM_DONE;
                    anims[i].queued_at = -1;
                    cur_node[i]        = msg->current_node;
                    nxt_node[i]        = -1;
                    inside_node[i]     = -1;   /* released the semaphore */
                    done_count++;

                } else {
                    /* ── ENTERED node, now moving toward next ── */
                    printf("[PID=%d] arrived at node %d | next node: %d\n",
                           (int)msg->pid, msg->current_node, msg->next_node);
                    fflush(stdout);
                    cur_node[i]        = msg->current_node;
                    nxt_node[i]        = msg->next_node;
                    anims[i].state     = ANIM_MOVING;
                    anims[i].timer     = 0.0f;
                    anims[i].pos       = gPos[msg->current_node];
                    anims[i].queued_at = -1;
                    inside_node[i]     = msg->current_node;  /* holds semaphore */

                    /* delay until next message = W × HOP_SEC
                       (this is when the semaphore is released in the child) */
                    int w = edge_weight(g, msg->current_node, msg->next_node);
                    next_msg_timer[i] = (float)w * HOP_SEC;
                }

                #undef node_is_occupied
            }
            all_done = (done_count == numTravelers);
        }

        /* ── smooth interpolation for MOVING travelers ── */
        for (int i = 0; i < numTravelers; i++) {
            if (anims[i].state != ANIM_MOVING || nxt_node[i] < 0) continue;

            anims[i].timer += dt;
            int w = edge_weight(g, cur_node[i], nxt_node[i]);
            float hop_dur = (float)w * HOP_SEC;
            float t = anims[i].timer / hop_dur;
            if (t > 1.0f) t = 1.0f;
            Vector2 from = gPos[cur_node[i]];
            Vector2 to   = gPos[nxt_node[i]];
            anims[i].pos = (Vector2){
                from.x + (to.x - from.x) * t,
                from.y + (to.y - from.y) * t
            };
        }

        draw:
        BeginDrawing();
        ClearBackground(C_BG);
        draw_background();
        draw_frame(g, anims, numTravelers, NULL, cur_node, nxt_node, font, time);
        draw_hud(g, numTravelers, sources, dests, playing, all_done,
                 "OS Project — Graph Visualiser M5/M6");
        draw_button(playing, all_done);
        EndDrawing();
    }

    CloseWindow();
}
