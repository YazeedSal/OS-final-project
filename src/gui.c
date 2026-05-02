/*
 * gui.c  —  Milestones 2 + 3: Graph Visualiser + Animation (PCB edition)
 *
 * Compile:
 *   gcc src/main.c src/graph.c src/dijkstra.c src/gui.c -o sim -lraylib -lm
 */

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/graph.h"
#ifdef MILESTONE3
#include "../include/dijkstra.h"
#endif

/* ═══════════════════════════════════════════════════════════════════
   TUNABLES
   ═══════════════════════════════════════════════════════════════════ */
#define WINDOW_W       1280
#define WINDOW_H        800
#define FPS              60
#define MAX_NODES	15

/* CPU chip geometry */
#define CHIP_HALF        28
#define LEG_COUNT         4
#define LEG_LEN          11
#define LEG_THICK         2
#define LEG_SPACING       9

/* Animation timing (all in seconds) */
#ifdef MILESTONE3
#define HOP_DURATION     0.30f   /* 300 ms per hop along an edge  */
#define NODE_WAIT        1.00f   /* 1 s pause at intermediate nodes */

/* Entity (the moving "packet") */
#define ENTITY_RADIUS    10.0f
#endif

/* ═══════════════════════════════════════════════════════════════════
   COLOUR PALETTE
   ═══════════════════════════════════════════════════════════════════ */
#define COL_BG           CLITERAL(Color){  8,  20,  12, 255 }
#define COL_GRID         CLITERAL(Color){ 16,  38,  22, 255 }
#define COL_TRACE_HI     CLITERAL(Color){ 40, 210,  80, 255 }
#define COL_TRACE_LO     CLITERAL(Color){ 18,  90,  35, 255 }
#ifdef MILESTONE3
#define COL_PATH_HI      CLITERAL(Color){ 80, 180, 255, 255 }  /* active path trace  */
#define COL_PATH_LO      CLITERAL(Color){ 20,  60, 120, 255 }
#endif
#define COL_CHIP_BODY    CLITERAL(Color){  6,   6,   6, 255 }
#define COL_CHIP_BORDER  CLITERAL(Color){ 55, 175,  75, 255 }
#define COL_CHIP_INNER   CLITERAL(Color){ 22,  65,  30, 255 }
#define COL_LEG          CLITERAL(Color){170, 165, 130, 255 }
#define COL_NOTCH        CLITERAL(Color){ 22,  22,  22, 255 }
#define COL_LABEL        CLITERAL(Color){200, 255, 200, 255 }
#define COL_WEIGHT       CLITERAL(Color){255, 215,  50, 255 }
#define COL_SRC_RING     CLITERAL(Color){ 80, 200, 255, 255 }
#define COL_DST_RING     CLITERAL(Color){255, 100,  80, 255 }
#define COL_HUD          CLITERAL(Color){ 40, 210,  80, 255 }
#define COL_HUD_DIM      CLITERAL(Color){ 20, 100,  40, 255 }

#define COL_ENTITY       CLITERAL(Color){255, 220,  50, 255 }  /* yellow packet      */
#define COL_ENTITY_GLOW  CLITERAL(Color){255, 220,  50,  60 }
#define COL_BTN_BG       CLITERAL(Color){ 20,  60,  28, 255 }
#define COL_BTN_BORDER   CLITERAL(Color){ 40, 180,  70, 255 }
#define COL_BTN_TEXT     CLITERAL(Color){200, 255, 200, 255 }


/* ═══════════════════════════════════════════════════════════════════
   ANIMATION STATE (milestone 3 only)
   ═══════════════════════════════════════════════════════════════════ */
#ifdef MILESTONE3
typedef enum {
    ANIM_IDLE,        /* waiting at source before play is pressed */
    ANIM_MOVING,      /* travelling along an edge (hop-by-hop)    */
    ANIM_WAITING,     /* pausing 1s at an intermediate node       */
    ANIM_DONE         /* arrived at destination                   */
} AnimState;

typedef struct {
    int  path[MAX_NODES];
    int  pathLen;
    int  totalCost;
    int  segIdx;
    int  hopIdx;
    int  hopTotal;
    float timer;
    Vector2 entityPos;
    AnimState state;
    int       playing;
} Anim;
#endif

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
        float angle = (float)i / n * 2.0f * PI - PI / 2.0f;
        gPos[i].x = cx + r * cosf(angle);
        gPos[i].y = cy + r * sinf(angle);
    }
}

/* ═══════════════════════════════════════════════════════════════════
   HELPER: find the weight of a specific directed edge u->v
   ANIMATION init + tick   (milestone 3 only)
   ═══════════════════════════════════════════════════════════════════ */
#ifdef MILESTONE3
static int edge_weight(const Graph* g, int u, int v)
{
    AdjNode* cur = g->adjLists[u];
    while (cur) {
        if (cur->destination == v) return cur->weight;
        cur = cur->next;
    }
    return 1; /* fallback, should never happen on a valid path */
}

/* ═══════════════════════════════════════════════════════════════════
   ANIMATION: initialise
   ═══════════════════════════════════════════════════════════════════ */
static void anim_init(Anim* a, const Graph* g)
{
    memset(a, 0, sizeof(Anim));
    a->totalCost = dijkstra_path(g, a->path, &a->pathLen);

    if (a->pathLen < 1 || a->totalCost < 0) {
        a->state = ANIM_DONE;   /* no path — jump straight to done */
        a->pathLen = 0;
        return;
    }

    a->state     = ANIM_IDLE;
    a->playing   = 0;
    a->segIdx    = 0;
    a->hopIdx    = 0;
    a->hopTotal  = (a->pathLen > 1)
                   ? edge_weight(g, a->path[0], a->path[1]) : 0;
    a->entityPos = gPos[a->path[0]];
    a->timer     = 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════
   ANIMATION: tick — call once per frame with delta time
   ═══════════════════════════════════════════════════════════════════ */
static void anim_tick(Anim* a, const Graph* g, float dt)
{
    if (!a->playing || a->state == ANIM_DONE || a->state == ANIM_IDLE)
        return;

    a->timer += dt;

    if (a->state == ANIM_WAITING) {
        /* ── pausing at an intermediate node ── */
        if (a->timer >= NODE_WAIT) {
            a->timer = 0.0f;
            /* advance to next segment */
            a->segIdx++;
            if (a->segIdx >= a->pathLen - 1) {
                a->state     = ANIM_DONE;
                a->entityPos = gPos[a->path[a->pathLen - 1]];
                return;
            }
            a->hopIdx   = 0;
            a->hopTotal = edge_weight(g, a->path[a->segIdx],
                                          a->path[a->segIdx + 1]);
            a->state    = ANIM_MOVING;
        }

    } else if (a->state == ANIM_MOVING) {
        /* ── travelling along one hop ── */
        if (a->timer >= HOP_DURATION) {
            a->timer = 0.0f;
            a->hopIdx++;

            if (a->hopIdx >= a->hopTotal) {
                /* finished this edge — land on the next node */
                a->entityPos = gPos[a->path[a->segIdx + 1]];

                int nextNode = a->path[a->segIdx + 1];
                int destNode = a->path[a->pathLen - 1];

                if (nextNode == destNode) {
                    a->state   = ANIM_DONE;
                    a->playing = 0;
                } else {
                    /* intermediate node: wait 1 second */
                    a->state = ANIM_WAITING;
                }
            } else {
                /* interpolate position within the hop */
                Vector2 from = gPos[a->path[a->segIdx]];
                Vector2 to   = gPos[a->path[a->segIdx + 1]];
                float   t    = (float)a->hopIdx / (float)a->hopTotal;
                a->entityPos = (Vector2){ from.x + (to.x - from.x) * t,
                                          from.y + (to.y - from.y) * t };
            }
        } else {
            /* smooth interpolation within the current hop */
            Vector2 from     = gPos[a->path[a->segIdx]];
            Vector2 to       = gPos[a->path[a->segIdx + 1]];
            float   hopStart = (float)a->hopIdx       / (float)a->hopTotal;
            float   hopEnd   = (float)(a->hopIdx + 1) / (float)a->hopTotal;
            float   t        = hopStart + (hopEnd - hopStart) * (a->timer / HOP_DURATION);
            a->entityPos     = (Vector2){ from.x + (to.x - from.x) * t,
                                          from.y + (to.y - from.y) * t };
        }
    }
}
#endif /* MILESTONE3 */

/* ═══════════════════════════════════════════════════════════════════
   DRAW: PCB background
   ═══════════════════════════════════════════════════════════════════ */
static void draw_pcb_background(void)
{
    int sp = 40;
    for (int x = 0; x < WINDOW_W; x += sp)
        DrawLine(x, 0, x, WINDOW_H, COL_GRID);
    for (int y = 0; y < WINDOW_H; y += sp)
        DrawLine(0, y, WINDOW_W, y, COL_GRID);

    int pad = 22;
    Color fid = CLITERAL(Color){28, 75, 38, 255};
    int corners[4][2] = {{pad,pad},{WINDOW_W-pad,pad},
                         {pad,WINDOW_H-pad},{WINDOW_W-pad,WINDOW_H-pad}};
    for (int i = 0; i < 4; i++) {
        DrawCircleLines(corners[i][0], corners[i][1], 9, fid);
        DrawCircleLines(corners[i][0], corners[i][1], 4, fid);
    }
    int m = 8;
    DrawRectangleLines(m, m, WINDOW_W-2*m, WINDOW_H-2*m,
                       CLITERAL(Color){24, 60, 30, 255});
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: directed PCB-trace arrow
   ═══════════════════════════════════════════════════════════════════ */
static void draw_trace_arrow(Vector2 p1, Vector2 p2, int on_path)
{
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    float head = 16.0f, wing = 7.0f;

    float dx = p2.x - p1.x, dy = p2.y - p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 2.0f * margin + head + 4.0f) return;

    float ux = dx/len, uy = dy/len;
    float px = -uy,    py =  ux;

    Vector2 a    = { p1.x + ux * margin, p1.y + uy * margin };
    Vector2 tip  = { p2.x - ux * margin, p2.y - uy * margin };
    Vector2 base = { tip.x - ux * head,  tip.y - uy * head  };

#ifdef MILESTONE3
    Color hi = on_path ? COL_PATH_HI : COL_TRACE_HI;
    Color lo = on_path ? COL_PATH_LO : COL_TRACE_LO;
    float thick = on_path ? 6.0f : 5.0f;
    float thin  = on_path ? 3.0f : 2.0f;
#else
    (void)on_path;
    Color hi = COL_TRACE_HI;
    Color lo = COL_TRACE_LO;
    float thick = 5.0f, thin = 2.0f;
#endif

    DrawLineEx(a, base, thick, lo);
    DrawLineEx(a, base, thin,  hi);

    Vector2 bl = { base.x + px * wing, base.y + py * wing };
    Vector2 br = { base.x - px * wing, base.y - py * wing };
    DrawTriangle(tip, br, bl, hi);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: edge weight label
   ═══════════════════════════════════════════════════════════════════ */
static void draw_edge_weight(Vector2 p1, Vector2 p2, int w, Font font)
{
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    float dx = p2.x - p1.x, dy = p2.y - p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 2.0f * margin + 12.0f) return;

    float ux = dx/len, uy = dy/len;
    float px = -uy, py = ux;

    Vector2 mid = {
        p1.x + dx * 0.70f + px * 18.0f,
        p1.y + dy * 0.70f + py * 18.0f
    };

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", w);
    int fs = 15;
    Vector2 sz = MeasureTextEx(font, buf, fs, 1);

    DrawRectangleRounded(
        (Rectangle){mid.x-sz.x/2-5, mid.y-sz.y/2-3, sz.x+10, sz.y+6},
        0.4f, 4, CLITERAL(Color){0,0,0,255});
    DrawRectangleRoundedLines(
        (Rectangle){mid.x-sz.x/2-5, mid.y-sz.y/2-3, sz.x+10, sz.y+6},
        0.4f, 4, CLITERAL(Color){40,100,50,255});
    DrawTextEx(font, buf,
               (Vector2){mid.x-sz.x/2, mid.y-sz.y/2},
               fs, 1, COL_WEIGHT);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: CPU chip
   ═══════════════════════════════════════════════════════════════════ */
static void draw_cpu(Vector2 p, int id, int is_source, int is_dest,
                     int is_current, Font font)
{
    int h = CHIP_HALF;
#ifdef MILESTONE3
    if (is_source)
        DrawCircle((int)p.x, (int)p.y, h+LEG_LEN+6,
                   CLITERAL(Color){80,200,255,55});
    if (is_dest)
        DrawCircle((int)p.x, (int)p.y, h+LEG_LEN+6,
                   CLITERAL(Color){255,100,80,55});
    if (is_current)
        DrawCircle((int)p.x, (int)p.y, h+LEG_LEN+10,
                   CLITERAL(Color){255,220,50,40});
#endif
    for (int side = 0; side < 4; side++) {
        for (int k = 0; k < LEG_COUNT; k++) {
            float off = (k - (LEG_COUNT-1)/2.0f) * LEG_SPACING;
            Vector2 start, end;
            switch (side) {
                case 0: start=(Vector2){p.x+off,p.y-h};   end=(Vector2){p.x+off,p.y-h-LEG_LEN}; break;
                case 1: start=(Vector2){p.x+off,p.y+h};   end=(Vector2){p.x+off,p.y+h+LEG_LEN}; break;
                case 2: start=(Vector2){p.x-h,p.y+off};   end=(Vector2){p.x-h-LEG_LEN,p.y+off}; break;
                default:start=(Vector2){p.x+h,p.y+off};   end=(Vector2){p.x+h+LEG_LEN,p.y+off}; break;
            }
            DrawLineEx(start, end, LEG_THICK, COL_LEG);
            DrawCircleV(end, 2.5f, COL_LEG);
        }
    }

    DrawRectangle((int)(p.x-h+4),(int)(p.y-h+4),h*2,h*2,
                  CLITERAL(Color){0,25,5,140});
    DrawRectangle((int)(p.x-h),(int)(p.y-h),h*2,h*2,COL_CHIP_BODY);
#ifdef MILESTONE3
    Color border = is_current ? COL_ENTITY  :
                   is_source  ? COL_SRC_RING :
                   is_dest    ? COL_DST_RING :
                                COL_CHIP_BORDER;
#else
	Color border = COL_CHIP_BORDER;
#endif	
    float bthick = (is_source||is_dest||is_current) ? 2.5f : 1.5f;
    DrawRectangleLinesEx(
        (Rectangle){(float)(p.x-h),(float)(p.y-h),(float)(h*2),(float)(h*2)},
        bthick, border);

    DrawCircle((int)p.x,(int)(p.y-h),5,COL_NOTCH);
    DrawCircleLines((int)p.x,(int)(p.y-h),5,border);
    DrawCircle((int)(p.x-h+7),(int)(p.y-h+7),3,
               CLITERAL(Color){55,175,75,255});

    DrawLine((int)(p.x-h+10),(int)p.y,(int)(p.x+h-10),(int)p.y,COL_CHIP_INNER);
    DrawLine((int)p.x,(int)(p.y-h+10),(int)p.x,(int)(p.y+h-10),COL_CHIP_INNER);
    DrawCircle((int)(p.x-9),(int)(p.y-9),2,COL_CHIP_INNER);
    DrawCircle((int)(p.x+9),(int)(p.y+9),2,COL_CHIP_INNER);
    DrawCircle((int)(p.x-9),(int)(p.y+9),2,COL_CHIP_INNER);
    DrawCircle((int)(p.x+9),(int)(p.y-9),2,COL_CHIP_INNER);

    char buf[10];
    snprintf(buf, sizeof(buf), "CPU%d", id);
    int fs = 13;
    Vector2 sz = MeasureTextEx(font, buf, fs, 1);
    DrawTextEx(font, buf,
               (Vector2){p.x-sz.x/2, p.y-sz.y/2},
               fs, 1, COL_LABEL);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: moving entity, play button, path helper (milestone 3 only)
   ═══════════════════════════════════════════════════════════════════ */
#ifdef MILESTONE3
static void draw_entity(Vector2 pos, AnimState state)
{
    if (state == ANIM_IDLE || state == ANIM_DONE) return;

    /* glow ring */
    DrawCircle((int)pos.x, (int)pos.y,
               (int)(ENTITY_RADIUS + 8), COL_ENTITY_GLOW);
    /* body */
    DrawCircle((int)pos.x, (int)pos.y,
               (int)ENTITY_RADIUS, COL_ENTITY);
    /* inner dot */
    DrawCircle((int)pos.x, (int)pos.y, 4,
               CLITERAL(Color){80, 40, 0, 255});
}

static int draw_play_button(int playing, AnimState state)
{
    Rectangle btn = { WINDOW_W - 130, WINDOW_H - 52, 110, 36 };
    int hovered   = CheckCollisionPointRec(GetMousePosition(), btn);
    int clicked   = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    Color bg     = hovered ? CLITERAL(Color){30,90,45,255} : COL_BTN_BG;
    Color border = hovered ? CLITERAL(Color){80,220,100,255} : COL_BTN_BORDER;

    DrawRectangleRounded(btn, 0.3f, 6, bg);
    DrawRectangleRoundedLines(btn, 0.3f, 6, border);

    const char* label;
    if (state == ANIM_DONE)   label = "Restart";
    else if (playing)         label = "[ Stop ]";
    else                      label = "[ Play ]";

    int fs = 16;
    Vector2 sz = MeasureTextEx(GetFontDefault(), label, fs, 1);
    DrawText(label,
             (int)(btn.x + (btn.width  - sz.x) / 2),
             (int)(btn.y + (btn.height - sz.y) / 2),
             fs, COL_BTN_TEXT);

    return clicked && (state != ANIM_DONE);
}

static int draw_play_button_raw(int playing, AnimState state)
{
    (void)playing; (void)state;
    Rectangle btn = { WINDOW_W - 130, WINDOW_H - 52, 110, 36 };
    int hovered   = CheckCollisionPointRec(GetMousePosition(), btn);
    return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static int is_path_edge(const Anim* a, int u, int v)
{
    for (int i = 0; i < a->pathLen - 1; i++)
        if (a->path[i] == u && a->path[i+1] == v) return 1;
    return 0;
}
#endif /* MILESTONE3 */

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC ENTRY POINT
   ═══════════════════════════════════════════════════════════════════ */
void draw_gui(const Graph* g)
{
    if (!g || g->numNodes <= 0) return;

    compute_layout(g->numNodes);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_W, WINDOW_H,
               "OS Project — Graph Visualiser (PCB edition)");
    SetTargetFPS(FPS);

    Font font = GetFontDefault();

#ifdef MILESTONE3
    /* run Dijkstra once and set up animation state */
    Anim anim;
    anim_init(&anim, g);
#endif

    while (!WindowShouldClose()) {

#ifdef MILESTONE3
        float dt = GetFrameTime();

        /* ── button interaction ── */
        if (draw_play_button_raw(anim.playing, anim.state)) {
            if (anim.state == ANIM_DONE) {
                anim_init(&anim, g);
                anim.playing = 1;
                anim.state   = ANIM_MOVING;
            } else if (anim.state == ANIM_IDLE) {
                anim.playing = 1;
                anim.state   = ANIM_MOVING;
            } else {
                anim.playing = !anim.playing;
            }
        }

        /* ── advance animation ── */
        anim_tick(&anim, g, dt);

        /* ── which node is the entity currently sitting on? ── */
        int cur_node = -1;
        if (anim.state == ANIM_WAITING && anim.segIdx + 1 < anim.pathLen)
            cur_node = anim.path[anim.segIdx + 1];
        else if (anim.state == ANIM_IDLE && anim.pathLen > 0)
            cur_node = anim.path[0];
        else if (anim.state == ANIM_DONE && anim.pathLen > 0)
            cur_node = anim.path[anim.pathLen - 1];
#endif /* MILESTONE3 */

        /* ════════════ DRAW ════════════ */
        BeginDrawing();
        ClearBackground(COL_BG);

        draw_pcb_background();

        /* 1. edges */
        for (int i = 0; i < g->numEdges; i++) {
            int u = g->edges[i].source;
            int v = g->edges[i].destination;
            int w = g->edges[i].weight;
#ifdef MILESTONE3
            draw_trace_arrow(gPos[u], gPos[v], is_path_edge(&anim, u, v));
#else
            draw_trace_arrow(gPos[u], gPos[v], 0);
#endif
            draw_edge_weight(gPos[u], gPos[v], w, font);
        }

        /* 2. CPU chips */
        for (int i = 0; i < g->numNodes; i++) {
#ifdef MILESTONE3
            draw_cpu(gPos[i], i,
                     i == g->source,
                     i == g->destination,
                     i == cur_node,
                     font);
#else
            draw_cpu(gPos[i], i,
                     i == g->source,
                     i == g->destination,
                     0,
                     font);
#endif
        }

#ifdef MILESTONE3
        /* 3. moving entity */
        draw_entity(anim.entityPos, anim.state);
#endif

        /* 4. HUD */
        DrawText("OS Project — Graph Visualiser", 16, 14, 18, COL_HUD);

        char info[100];
#ifdef MILESTONE3
        if (anim.totalCost >= 0)
            snprintf(info, sizeof(info),
                     "Nodes: %d   Edges: %d   Query: CPU%d -> CPU%d   "
                     "Shortest path cost: %d",
                     g->numNodes, g->numEdges,
                     g->source, g->destination, anim.totalCost);
        else
            snprintf(info, sizeof(info),
                     "Nodes: %d   Edges: %d   Query: CPU%d -> CPU%d   "
                     "No path found!",
                     g->numNodes, g->numEdges,
                     g->source, g->destination);
#else
        snprintf(info, sizeof(info),
                 "Nodes: %d   Edges: %d",
                 g->numNodes, g->numEdges);
#endif
        DrawText(info, 16, 36, 14, COL_HUD_DIM);
#ifdef MILESTONE3
        /* legend */
        DrawRectangle(WINDOW_W-200, 14, 12, 12, COL_SRC_RING);
        DrawText("Source", WINDOW_W-184, 14, 13, COL_SRC_RING);
        DrawRectangle(WINDOW_W-200, 32, 12, 12, COL_DST_RING);
        DrawText("Dest",   WINDOW_W-184, 32, 13, COL_DST_RING);
#endif

#ifdef MILESTONE3
        /* play/stop button */
        draw_play_button(anim.playing, anim.state);

        /* status line */
        const char* status =
            anim.state == ANIM_IDLE    ? "Press Play to start" :
            anim.state == ANIM_MOVING  ? (anim.playing ? "Transmitting..." : "Paused") :
            anim.state == ANIM_WAITING ? (anim.playing ? "Processing at node..." : "Paused") :
                                          "";
        DrawText(status, 16, WINDOW_H - 48, 14, COL_HUD_DIM);

        /* arrived banner */
        if (anim.state == ANIM_DONE && anim.totalCost >= 0) {
            int bw = 460, bh = 70;
            int bx = (WINDOW_W-bw)/2, by = (WINDOW_H-bh)/2;
            DrawRectangleRounded(
                (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                0.2f, 8, CLITERAL(Color){0,0,0,220});
            DrawRectangleRoundedLines(
                (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                0.2f, 8, COL_ENTITY);
            char msg[80];
            snprintf(msg, sizeof(msg), "Packet arrived! Cost: %d", anim.totalCost);
            int fs = 24;
            Vector2 sz = MeasureTextEx(font, msg, fs, 1);
            DrawTextEx(font, msg,
                       (Vector2){bx+(bw-sz.x)/2, by+(bh-sz.y)/2},
                       fs, 1, COL_ENTITY);
        }

        /* no-path banner */
        if (anim.totalCost < 0 && anim.pathLen == 0) {
            int bw = 380, bh = 60;
            int bx = (WINDOW_W-bw)/2, by = (WINDOW_H-bh)/2;
            DrawRectangleRounded(
                (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                0.2f, 8, CLITERAL(Color){0,0,0,220});
            DrawRectangleRoundedLines(
                (Rectangle){(float)bx,(float)by,(float)bw,(float)bh},
                0.2f, 8, COL_DST_RING);
            DrawText("No path between source and destination!",
                     bx+18, by+18, 16, COL_DST_RING);
        }
#endif /* MILESTONE3 */

        DrawText("[ESC] Quit", 16, WINDOW_H-24, 13, COL_HUD_DIM);

        EndDrawing();
    }

    CloseWindow();
}
