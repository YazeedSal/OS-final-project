/*
 * gui.c  —  Milestone 2: Graph Visualiser (PCB / CPU-chip edition)
 *
 * Plugs directly into the Graph struct defined in include/graph.h.
 * Call draw_gui(graph) from main.c after readGraphFromFile() succeeds.
 *
 * Compile alongside graph.c and main.c, e.g.:
 *   gcc src/main.c src/graph.c src/gui.c -o sim -lraylib -lm
 *
 * Or add to your Makefile:
 *   SRCS = src/main.c src/graph.c src/gui.c
 */

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/graph.h"

/* ═══════════════════════════════════════════════════════════════════
   TUNABLES
   ═══════════════════════════════════════════════════════════════════ */
#define WINDOW_W      1280
#define WINDOW_H       800
#define FPS             60
#define MAX_V           15

/* CPU chip geometry */
#define CHIP_HALF       28     /* half side-length of the black square  */
#define LEG_COUNT        4     /* pins per side                         */
#define LEG_LEN         11     /* how far the pin sticks out            */
#define LEG_THICK        2
#define LEG_SPACING      9

/* ═══════════════════════════════════════════════════════════════════
   PCB COLOUR PALETTE
   ═══════════════════════════════════════════════════════════════════ */
#define COL_BG           CLITERAL(Color){  8,  20,  12, 255 }
#define COL_GRID         CLITERAL(Color){ 16,  38,  22, 255 }
#define COL_TRACE_HI     CLITERAL(Color){ 40, 210,  80, 255 }  /* bright green trace  */
#define COL_TRACE_LO     CLITERAL(Color){ 18,  90,  35, 255 }  /* dim under-trace     */
#define COL_CHIP_BODY    CLITERAL(Color){  6,   6,   6, 255 }
#define COL_CHIP_BORDER  CLITERAL(Color){ 55, 175,  75, 255 }
#define COL_CHIP_INNER   CLITERAL(Color){ 22,  65,  30, 255 }  /* internal line tint  */
#define COL_LEG          CLITERAL(Color){170, 165, 130, 255 }  /* silver pins         */
#define COL_NOTCH        CLITERAL(Color){ 22,  22,  22, 255 }
#define COL_LABEL        CLITERAL(Color){200, 255, 200, 255 }
#define COL_WEIGHT       CLITERAL(Color){255, 215,  50, 255 }
#define COL_SRC_RING     CLITERAL(Color){ 80, 200, 255, 255 }  /* source highlight    */
#define COL_DST_RING     CLITERAL(Color){255, 100,  80, 255 }  /* destination highlight */
#define COL_HUD          CLITERAL(Color){ 40, 210,  80, 255 }
#define COL_HUD_DIM      CLITERAL(Color){ 20, 100,  40, 255 }

/* ═══════════════════════════════════════════════════════════════════
   LAYOUT  — circular, vertices evenly spaced
   ═══════════════════════════════════════════════════════════════════ */
static Vector2 gPos[MAX_V];

static void compute_layout(int n)
{
    float cx = WINDOW_W / 2.0f;
    float cy = WINDOW_H / 2.0f;
    float r  = fminf(cx, cy) - CHIP_HALF - LEG_LEN - 60.0f;

    if (n == 1) { gPos[0] = (Vector2){cx, cy}; return; }

    for (int i = 0; i < n; i++) {
        float angle = (float)i / n * 2.0f * PI - PI / 2.0f;
        gPos[i].x = cx + r * cosf(angle);
        gPos[i].y = cy + r * sinf(angle);
    }
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: PCB grid background + fiducial marks
   ═══════════════════════════════════════════════════════════════════ */
static void draw_pcb_background(void)
{
    int sp = 40;
    for (int x = 0; x < WINDOW_W; x += sp)
        DrawLine(x, 0, x, WINDOW_H, COL_GRID);
    for (int y = 0; y < WINDOW_H; y += sp)
        DrawLine(0, y, WINDOW_W, y, COL_GRID);

    /* corner fiducials */
    int pad = 22;
    Color fid = CLITERAL(Color){28, 75, 38, 255};
    int corners[4][2] = {{pad,pad},{WINDOW_W-pad,pad},
                         {pad,WINDOW_H-pad},{WINDOW_W-pad,WINDOW_H-pad}};
    for (int i = 0; i < 4; i++) {
        DrawCircleLines(corners[i][0], corners[i][1], 9, fid);
        DrawCircleLines(corners[i][0], corners[i][1], 4, fid);
    }

    /* board edge lines */
    int m = 8;
    DrawRectangleLines(m, m, WINDOW_W-2*m, WINDOW_H-2*m,
                       CLITERAL(Color){24, 60, 30, 255});
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: directed PCB-trace arrow between two chips
   ═══════════════════════════════════════════════════════════════════ */
static void draw_trace_arrow(Vector2 p1, Vector2 p2)
{
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    float head   = 16.0f;
    float wing   =  7.0f;

    float dx = p2.x - p1.x, dy = p2.y - p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 2.0f * margin + head + 4.0f) return;

    float ux = dx/len, uy = dy/len;   /* unit vector along edge    */
    float px = -uy,    py =  ux;      /* perpendicular (left side) */

    Vector2 a    = { p1.x + ux * margin, p1.y + uy * margin };
    Vector2 tip  = { p2.x - ux * margin, p2.y - uy * margin };
    Vector2 base = { tip.x - ux * head,  tip.y - uy * head  };

    /* trace: runs from a up to the arrowhead base */
    DrawLineEx(a, base, 5.0f, COL_TRACE_LO);
    DrawLineEx(a, base, 2.0f, COL_TRACE_HI);

    /* arrowhead wings */
    Vector2 bl = { base.x + px * wing, base.y + py * wing };
    Vector2 br = { base.x - px * wing, base.y - py * wing };

    /* raylib DrawTriangle needs counter-clockwise winding.
       CCW from tip: tip -> br -> bl  */
    DrawTriangle(tip, br, bl, COL_TRACE_HI);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: edge weight label (mid-point, slightly offset)
   ═══════════════════════════════════════════════════════════════════ */
static void draw_edge_weight(Vector2 p1, Vector2 p2, int w, Font font)
{
    float margin = CHIP_HALF + LEG_LEN + 3.0f;
    float dx = p2.x - p1.x, dy = p2.y - p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 2.0f * margin + 12.0f) return;

    float ux = dx/len, uy = dy/len;
    float px = -uy,    py =  ux;     /* perpendicular */

    /* Place label 70% of the way from p1 to p2, offset sideways by 18px */
    Vector2 mid = {
        p1.x + dx * 0.70f + px * 18.0f,
        p1.y + dy * 0.70f + py * 18.0f
    };

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", w);
    int fs = 15;
    Vector2 sz = MeasureTextEx(font, buf, fs, 1);

    /* fully opaque dark background so it always reads clearly */
    DrawRectangleRounded(
        (Rectangle){mid.x - sz.x/2 - 5, mid.y - sz.y/2 - 3, sz.x + 10, sz.y + 6},
        0.4f, 4,
        CLITERAL(Color){0, 0, 0, 255});
    /* bright border so the pill is visible against dark traces */
    DrawRectangleRoundedLines(
        (Rectangle){mid.x - sz.x/2 - 5, mid.y - sz.y/2 - 3, sz.x + 10, sz.y + 6},
        0.4f, 4,
        CLITERAL(Color){40, 100, 50, 255});
    DrawTextEx(font, buf,
               (Vector2){mid.x - sz.x/2, mid.y - sz.y/2},
               fs, 1, COL_WEIGHT);
}

/* ═══════════════════════════════════════════════════════════════════
   DRAW: CPU chip at position p
     id        — vertex number (0-based)
     is_source / is_dest — highlight rings for the Dijkstra query
   ═══════════════════════════════════════════════════════════════════ */
static void draw_cpu(Vector2 p, int id, int is_source, int is_dest, Font font)
{
    int h = CHIP_HALF;

    /* ── highlight ring behind the chip ────────────────────────── */
    if (is_source)
        DrawCircle((int)p.x, (int)p.y, h + LEG_LEN + 6,
                   CLITERAL(Color){80, 200, 255, 55});
    if (is_dest)
        DrawCircle((int)p.x, (int)p.y, h + LEG_LEN + 6,
                   CLITERAL(Color){255, 100, 80, 55});

    /* ── silver legs on all 4 sides ────────────────────────────── */
    for (int side = 0; side < 4; side++) {
        for (int k = 0; k < LEG_COUNT; k++) {
            float off = (k - (LEG_COUNT - 1) / 2.0f) * LEG_SPACING;
            Vector2 start, end;
            switch (side) {
                case 0: /* top    */
                    start = (Vector2){p.x + off, p.y - h};
                    end   = (Vector2){p.x + off, p.y - h - LEG_LEN};
                    break;
                case 1: /* bottom */
                    start = (Vector2){p.x + off, p.y + h};
                    end   = (Vector2){p.x + off, p.y + h + LEG_LEN};
                    break;
                case 2: /* left   */
                    start = (Vector2){p.x - h,          p.y + off};
                    end   = (Vector2){p.x - h - LEG_LEN, p.y + off};
                    break;
                default: /* right  */
                    start = (Vector2){p.x + h,          p.y + off};
                    end   = (Vector2){p.x + h + LEG_LEN, p.y + off};
                    break;
            }
            DrawLineEx(start, end, LEG_THICK, COL_LEG);
            DrawCircleV(end, 2.5f, COL_LEG);   /* solder pad */
        }
    }

    /* ── drop shadow ────────────────────────────────────────────── */
    DrawRectangle((int)(p.x - h + 4), (int)(p.y - h + 4),
                  h*2, h*2, CLITERAL(Color){0, 25, 5, 140});

    /* ── chip body ───────────────────────────────────────────────── */
    DrawRectangle((int)(p.x - h), (int)(p.y - h), h*2, h*2, COL_CHIP_BODY);

    /* ── chip border (colour-coded for source / dest) ───────────── */
    Color border = is_source ? COL_SRC_RING :
                   is_dest   ? COL_DST_RING :
                               COL_CHIP_BORDER;
    DrawRectangleLinesEx(
        (Rectangle){(float)(p.x - h), (float)(p.y - h), (float)(h*2), (float)(h*2)},
        is_source || is_dest ? 2.5f : 1.5f, border);

    /* ── alignment notch (top-centre) ───────────────────────────── */
    DrawCircle((int)p.x, (int)(p.y - h), 5, COL_NOTCH);
    DrawCircleLines((int)p.x, (int)(p.y - h), 5, border);

    /* ── pin-1 dot (top-left) ────────────────────────────────────── */
    DrawCircle((int)(p.x - h + 7), (int)(p.y - h + 7), 3,
               CLITERAL(Color){55, 175, 75, 255});

    /* ── decorative inner circuit lines ─────────────────────────── */
    DrawLine((int)(p.x - h + 10), (int)p.y,
             (int)(p.x + h - 10), (int)p.y, COL_CHIP_INNER);
    DrawLine((int)p.x, (int)(p.y - h + 10),
             (int)p.x, (int)(p.y + h - 10), COL_CHIP_INNER);
    DrawCircle((int)(p.x - 9), (int)(p.y - 9), 2, COL_CHIP_INNER);
    DrawCircle((int)(p.x + 9), (int)(p.y + 9), 2, COL_CHIP_INNER);
    DrawCircle((int)(p.x - 9), (int)(p.y + 9), 2, COL_CHIP_INNER);
    DrawCircle((int)(p.x + 9), (int)(p.y - 9), 2, COL_CHIP_INNER);

    /* ── vertex label ────────────────────────────────────────────── */
    char buf[10];
    snprintf(buf, sizeof(buf), "CPU%d", id);
    int fs = 13;
    Vector2 sz = MeasureTextEx(font, buf, fs, 1);
    DrawTextEx(font, buf,
               (Vector2){p.x - sz.x/2, p.y - sz.y/2},
               fs, 1, COL_LABEL);
}

/* ═══════════════════════════════════════════════════════════════════
   PUBLIC ENTRY POINT
   Call this from main.c after readGraphFromFile() succeeds.
   ═══════════════════════════════════════════════════════════════════ */
void draw_gui(const Graph* g)
{
    if (g == NULL || g->numNodes <= 0) return;

    compute_layout(g->numNodes);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(WINDOW_W, WINDOW_H,
               "OS Project — Graph Visualiser (PCB edition)");
    SetTargetFPS(FPS);

    Font font = GetFontDefault();

    while (!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(COL_BG);

        draw_pcb_background();

        /* ── 1. draw all directed edges (behind chips) ── */
        for (int i = 0; i < g->numEdges; i++) {
            int  u = g->edges[i].source;
            int  v = g->edges[i].destination;
            int  w = g->edges[i].weight;
            draw_trace_arrow(gPos[u], gPos[v]);
            draw_edge_weight(gPos[u], gPos[v], w, font);
        }

        /* ── 2. draw CPU chips ── */
        for (int i = 0; i < g->numNodes; i++) {
            int is_src  = (i == g->source);
            int is_dst  = (i == g->destination);
            draw_cpu(gPos[i], i, is_src, is_dst, font);
        }

        /* ── 3. HUD ── */
        DrawText("OS Project — Graph Visualiser", 16, 14, 18, COL_HUD);

        char info[80];
        snprintf(info, sizeof(info),
                 "Nodes: %d   Edges: %d   Query: CPU%d  →  CPU%d",
                 g->numNodes, g->numEdges, g->source, g->destination);
        DrawText(info, 16, 36, 14, COL_HUD_DIM);

        /* legend for source / dest colours */
        DrawRectangle(WINDOW_W - 200, 14, 12, 12, COL_SRC_RING);
        DrawText("Source",  WINDOW_W - 184, 14, 13, COL_SRC_RING);
        DrawRectangle(WINDOW_W - 200, 32, 12, 12, COL_DST_RING);
        DrawText("Dest",    WINDOW_W - 184, 32, 13, COL_DST_RING);

        DrawText("[ESC] Quit", 16, WINDOW_H - 24, 13, COL_HUD_DIM);

        EndDrawing();
    }

    CloseWindow();
}
