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
//#include "../include/ipc.h"
#include "../include/scheduler.h"   /* SimEvent for draw_gui_m7 */

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
   PUBLIC: MILESTONE 7 — replay an FCFS-scheduled timeline
   --------------------------------------------------------------------
   The parent (scheduler.c) already ran the real, FCFS-scheduled
   simulation and handed us a list of timed events. Here we just play
   that list back on a single clock so the ordering — including
   travelers waiting outside a busy node — is shown exactly as it
   happened.
   ═══════════════════════════════════════════════════════════════════ */
typedef enum { RS_IDLE, RS_WAIT, RS_INSIDE, RS_MOVING, RS_DONE } RState;

/* put every traveler back to its starting node */
static void m7_reset(RState state[], int atNode[], int fromN[], int toN[],
                     float moveStart[], Vector2 pos[], int sources[], int n)
{
    for (int i = 0; i < n; i++) {
        state[i]     = RS_IDLE;
        atNode[i]    = sources[i];
        fromN[i]     = sources[i];
        toN[i]       = sources[i];
        moveStart[i] = 0.0f;
        pos[i]       = gPos[sources[i]];
    }
}

/* apply one timeline event to the render state */
static void m7_apply(SimEvent e, RState state[], int atNode[],
                     int fromN[], int toN[], float moveStart[], Vector2 pos[])
{
    int t = e.traveler;
    switch (e.type) {
        case EV_WAIT:                       /* arrived, queued outside node */
            state[t]  = RS_WAIT;
            atNode[t] = e.node;
            pos[t]    = gPos[e.node];
            break;
        case EV_ENTER:                      /* admitted into the node       */
            state[t]  = RS_INSIDE;
            atNode[t] = e.node;
            pos[t]    = gPos[e.node];
            break;
        case EV_LEAVE:                      /* leaving, travel to next node  */
            if (e.next != -1) {
                state[t]     = RS_MOVING;
                fromN[t]     = e.node;
                toN[t]       = e.next;
                moveStart[t] = e.t;
            }
            break;
        case EV_FINISH:
            state[t] = RS_DONE;
            break;
    }
}

void draw_gui_m7(const Graph* g, SimEvent* events, int numEvents,
                 int sources[], int dests[], int numTravelers,
                 const char* algoName)
{
    if (!g || numTravelers <= 0) return;
    compute_layout(g->numNodes);

    RState  state[MAX_TRAVELERS];
    int     atNode[MAX_TRAVELERS];
    int     fromN[MAX_TRAVELERS], toN[MAX_TRAVELERS];
    float   moveStart[MAX_TRAVELERS];
    Vector2 pos[MAX_TRAVELERS];

    m7_reset(state, atNode, fromN, toN, moveStart, pos, sources, numTravelers);

    int   evIdx    = 0;
    float now      = 0.0f;
    int   playing  = 0;
    int   all_done = (numEvents == 0);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_W, WINDOW_H, "OS Project — Graph Visualiser M7 (FCFS)");
    SetTargetFPS(FPS);
    Font font = GetFontDefault();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* button: Play / Stop / Restart */
        if (draw_button(playing, all_done)) {
            if (all_done) {
                m7_reset(state, atNode, fromN, toN, moveStart, pos,
                         sources, numTravelers);
                evIdx = 0; now = 0.0f; all_done = 0; playing = 1;
            } else {
                playing = !playing;
            }
        }

        if (playing) {
            now += dt;
            while (evIdx < numEvents && events[evIdx].t <= now) {
                m7_apply(events[evIdx], state, atNode, fromN, toN,
                         moveStart, pos);
                evIdx++;
            }
            /* interpolate packets that are moving along an edge */
            for (int i = 0; i < numTravelers; i++) {
                if (state[i] != RS_MOVING) continue;
                int   w   = edge_weight(g, fromN[i], toN[i]);
                float dur = (float)w * HOP_SEC;
                float tt  = (dur > 0.0f) ? (now - moveStart[i]) / dur : 1.0f;
                if (tt > 1.0f) tt = 1.0f;
                Vector2 a = gPos[fromN[i]], b = gPos[toN[i]];
                pos[i] = (Vector2){ a.x + (b.x - a.x) * tt,
                                    a.y + (b.y - a.y) * tt };
            }
            if (evIdx >= numEvents) all_done = 1;
        }

        BeginDrawing();
        ClearBackground(C_BG);
        draw_background();

        /* edges + weights */
        for (int i = 0; i < g->numEdges; i++) {
            int u = g->edges[i].source, v = g->edges[i].destination;
            draw_arrow(gPos[u], gPos[v], 0, C_CHIP_BORDER);
            draw_weight_label(gPos[u], gPos[v], g->edges[i].weight, font);
        }

        /* chips — colour the chip of whoever is INSIDE it */
        for (int node = 0; node < g->numNodes; node++) {
            int   useCol = 0;
            Color col    = C_CHIP_BORDER;
            for (int i = 0; i < numTravelers; i++)
                if (state[i] == RS_INSIDE && atNode[i] == node) {
                    col = TCOLORS[i % MAX_TRAVELERS];
                    useCol = 1;
                    break;
                }
            draw_cpu(gPos[node], node, col, useCol, font);
        }

        /* waiting packets — stacked just outside their node */
        for (int node = 0; node < g->numNodes; node++) {
            float   cx = WINDOW_W / 2.0f, cy = WINDOW_H / 2.0f;
            Vector2 d  = { gPos[node].x - cx, gPos[node].y - cy };
            float   dl = sqrtf(d.x * d.x + d.y * d.y);
            if (dl < 1.0f) { d.x = 0; d.y = -1; dl = 1; }
            d.x /= dl; d.y /= dl;

            int slot = 0;
            for (int i = 0; i < numTravelers; i++) {
                if (state[i] != RS_WAIT || atNode[i] != node) continue;
                float off = CHIP_HALF + LEG_LEN + 20.0f + slot * 24.0f;
                Vector2 wp = { gPos[node].x + d.x * off,
                               gPos[node].y + d.y * off };
                /* hollow ring = "waiting outside" */
                DrawCircleLines((int)wp.x, (int)wp.y, ENTITY_R + 3,
                                TCOLORS[i % MAX_TRAVELERS]);
                draw_packet(wp, TCOLORS[i % MAX_TRAVELERS]);
                slot++;
            }
        }

        /* moving + inside packets */
        for (int i = 0; i < numTravelers; i++)
            if (state[i] == RS_MOVING || state[i] == RS_INSIDE)
                draw_packet(pos[i], TCOLORS[i % MAX_TRAVELERS]);

        draw_hud(g, numTravelers, sources, dests, playing, all_done,
                 "OS Project — Graph Visualiser M7");

        /* show which scheduler is running (milestone 7 requirement) */
        char algo[48];
        snprintf(algo, sizeof(algo), "Scheduler: %s", algoName);
        DrawText(algo, 16, 58, 16, C_WEIGHT);

        draw_button(playing, all_done);
        EndDrawing();
    }

    CloseWindow();
}
