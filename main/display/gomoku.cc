#include "gomoku.h"
#include <cstring>
#include <cstdlib>
#include <esp_log.h>

#define TAG "Gomoku"
#define BOARD_SIZE 15
#define CELL_SIZE  28
#define MARGIN     36
#define STONE_R    12

static int g_board[BOARD_SIZE][BOARD_SIZE];
static int g_current = 1;
static int g_mode = 0;
static bool g_over = false;
static int g_history[BOARD_SIZE*BOARD_SIZE][2];
static int g_hist_cnt = 0;

static lv_obj_t *g_board_cont = nullptr;
static lv_obj_t *g_stones[BOARD_SIZE][BOARD_SIZE];
static lv_obj_t *g_green_ring = nullptr;
static lv_obj_t *g_status_lbl = nullptr;
static lv_obj_t *g_menu_page = nullptr;
static lv_obj_t *g_game_page = nullptr;
static lv_obj_t *g_result_lbl = nullptr;

static int count_dir(int r, int c, int dr, int dc, int p) {
    int cnt = 0;
    for (int i = 1; i < 5; i++) {
        int nr = r+dr*i, nc = c+dc*i;
        if (nr<0||nr>=BOARD_SIZE||nc<0||nc>=BOARD_SIZE) break;
        if (g_board[nr][nc]!=p) break;
        cnt++;
    }
    return cnt;
}
static int check_win(int r, int c) {
    int p = g_board[r][c]; if (p==0) return 0;
    int d[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    for (int i=0;i<4;i++) { if (1+count_dir(r,c,d[i][0],d[i][1],p)+count_dir(r,c,-d[i][0],-d[i][1],p)>=5) return p; }
    return 0;
}

static int eval_line(int r, int c, int dr, int dc, int p) {
    int cnt=1,o0=0,o1=0;
    for (int i=1;i<=5;i++){int nr=r+dr*i,nc=c+dc*i;if(nr<0||nr>=BOARD_SIZE||nc<0||nc>=BOARD_SIZE)break;if(g_board[nr][nc]==p)cnt++;else{if(g_board[nr][nc]==0)o0=1;break;}}
    for (int i=1;i<=5;i++){int nr=r-dr*i,nc=c-dc*i;if(nr<0||nr>=BOARD_SIZE||nc<0||nc>=BOARD_SIZE)break;if(g_board[nr][nc]==p)cnt++;else{if(g_board[nr][nc]==0)o1=1;break;}}
    if(cnt>=5)return 50000;
    int o=o0+o1;
    if(cnt==4)return o==2?10000:(o==1?1000:0);
    if(cnt==3)return o==2?1000:(o==1?100:0);
    if(cnt==2)return o==2?100:(o==1?10:0);
    return o==2?10:(o==1?1:0);
}
static int eval_pos(int r, int c, int p) {
    int d[4][2]={{1,0},{0,1},{1,1},{1,-1}},t=0;
    for(int i=0;i<4;i++)t+=eval_line(r,c,d[i][0],d[i][1],p);
    return t;
}
static int ai_score() {
    int best=-1,br=7,bc=7;
    for(int r=0;r<BOARD_SIZE;r++)for(int c=0;c<BOARD_SIZE;c++){
        if(g_board[r][c]!=0)continue;
        int s=eval_pos(r,c,2)+eval_pos(r,c,1)/2;
        if(s>best||(s==best&&rand()%3==0)){best=s;br=r;bc=c;}
    }
    g_board[br][bc]=2;g_history[g_hist_cnt][0]=br;g_history[g_hist_cnt][1]=bc;g_hist_cnt++;
    int w=check_win(br,bc);if(w){g_over=true;return w;}
    if(g_hist_cnt>=BOARD_SIZE*BOARD_SIZE){g_over=true;return 0;}
    g_current=1;return 0;
}

// stone object
static void add_stone(int r, int c, int player) {
    if(g_stones[r][c]){lv_obj_del(g_stones[r][c]);g_stones[r][c]=nullptr;}
    lv_obj_t *s = lv_obj_create(g_board_cont);
    int px = MARGIN + c*CELL_SIZE, py = MARGIN + r*CELL_SIZE;
    lv_obj_set_size(s, STONE_R*2, STONE_R*2);
    lv_obj_set_pos(s, px-STONE_R, py-STONE_R);
    lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_bg_color(s, player==1?lv_color_hex(0x1A1A1A):lv_color_hex(0xF0F0F0), 0);
    if(player==2){lv_obj_set_style_border_width(s,2,0);lv_obj_set_style_border_color(s,lv_color_hex(0x888888),0);}
    g_stones[r][c]=s;
}

// green ring
static void set_ring(int r, int c) {
    if(r<0||c<0){if(g_green_ring)lv_obj_add_flag(g_green_ring,LV_OBJ_FLAG_HIDDEN);return;}
    if(!g_green_ring){
        g_green_ring=lv_obj_create(g_board_cont);
        lv_obj_set_style_bg_opa(g_green_ring,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(g_green_ring,3,0);
        lv_obj_set_style_border_color(g_green_ring,lv_color_hex(0x00FF00),0);
    }
    int px=MARGIN+c*CELL_SIZE,py=MARGIN+r*CELL_SIZE;
    lv_obj_set_size(g_green_ring,STONE_R*2+8,STONE_R*2+8);
    lv_obj_set_pos(g_green_ring,px-STONE_R-4,py-STONE_R-4);
    lv_obj_set_style_radius(g_green_ring,LV_RADIUS_CIRCLE,0);
    lv_obj_clear_flag(g_green_ring,LV_OBJ_FLAG_HIDDEN);
}

static void clear_stones() {
    for(int r=0;r<BOARD_SIZE;r++)for(int c=0;c<BOARD_SIZE;c++)
        if(g_stones[r][c]){lv_obj_del(g_stones[r][c]);g_stones[r][c]=nullptr;}
    if(g_green_ring)lv_obj_add_flag(g_green_ring,LV_OBJ_FLAG_HIDDEN);
}

static void reset_game() {
    memset(g_board,0,sizeof(g_board));g_current=1;g_over=false;g_hist_cnt=0;
    clear_stones();
}

static void show_result(int w) {
    if(!g_result_lbl)return;
    lv_label_set_text(g_result_lbl,w==0?"Draw!":(w==1?"Black Wins!":"White Wins!"));
    lv_obj_clear_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_result_lbl);
}

static void place_stone(int r, int c) {
    if(g_over||r<0||r>=BOARD_SIZE||c<0||c>=BOARD_SIZE||g_board[r][c]!=0)return;
    g_board[r][c]=g_current;
    g_history[g_hist_cnt][0]=r;g_history[g_hist_cnt][1]=c;g_hist_cnt++;
    add_stone(r,c,g_current);
    set_ring(r,c);
    int w=check_win(r,c);
    if(w){g_over=true;show_result(w);return;}
    if(g_hist_cnt>=BOARD_SIZE*BOARD_SIZE){g_over=true;show_result(0);return;}
    g_current=3-g_current;
    if(g_status_lbl){
        lv_label_set_text(g_status_lbl,g_current==1?"Black's turn":"White's turn");
        lv_obj_set_style_text_color(g_status_lbl,g_current==1?lv_color_hex(0xFFFFFF):lv_color_hex(0xCCCCCC),0);
    }
    if(g_mode==2&&g_current==2&&!g_over){
        lv_label_set_text(g_status_lbl,"AI thinking...");
        lv_timer_create([](lv_timer_t *t){
            int r=ai_score();
            int ar=g_history[g_hist_cnt-1][0],ac=g_history[g_hist_cnt-1][1];
            add_stone(ar,ac,2);set_ring(ar,ac);
            lv_label_set_text(g_status_lbl,r?(r==2?"White Wins!":"Black Wins!"):"Black's turn");
            lv_obj_set_style_text_color(g_status_lbl,r?lv_color_hex(0xFFD700):lv_color_hex(0xFFFFFF),0);
            if(r)show_result(r);
            lv_timer_del(t);
        },300,nullptr);
    }
}
static void board_click_cb(lv_event_t *e) {
    if(g_over||(g_mode==2&&g_current==2))return;
    lv_indev_t *indev=lv_indev_get_act();lv_point_t pt;lv_indev_get_point(indev,&pt);
    lv_area_t coords;lv_obj_get_coords(g_board_cont,&coords);
    int rx=pt.x-coords.x1,ry=pt.y-coords.y1;
    int c=(rx-MARGIN+CELL_SIZE/2)/CELL_SIZE;
    int r=(ry-MARGIN+CELL_SIZE/2)/CELL_SIZE;
    if(r<0||r>=BOARD_SIZE||c<0||c>=BOARD_SIZE)return;
    int cx=MARGIN+c*CELL_SIZE,cy=MARGIN+r*CELL_SIZE;
    if((rx-cx)*(rx-cx)+(ry-cy)*(ry-cy)>(STONE_R+6)*(STONE_R+6))return;
    place_stone(r,c);
}

static void start_game(int mode) {
    g_mode=mode;reset_game();
    lv_obj_add_flag(g_menu_page,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);
    if(g_result_lbl)lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_status_lbl,"Black's turn");
    lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);
}

static void undo_move() {
    if(g_over||g_hist_cnt==0)return;
    int steps=(g_mode==2&&g_current==2&&g_hist_cnt>=2)?2:1;
    for(int i=0;i<steps&&g_hist_cnt>0;i++){
        g_hist_cnt--;
        int r=g_history[g_hist_cnt][0],c=g_history[g_hist_cnt][1];
        g_board[r][c]=0;
        if(g_stones[r][c]){lv_obj_del(g_stones[r][c]);g_stones[r][c]=nullptr;}
    }
    int lr=g_hist_cnt>0?g_history[g_hist_cnt-1][0]:-1,lc=g_hist_cnt>0?g_history[g_hist_cnt-1][1]:-1;
    set_ring(lr,lc);g_current=1;
    lv_label_set_text(g_status_lbl,"Black's turn");
    lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);
}

static void back_to_menu() {
    lv_obj_add_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_menu_page,LV_OBJ_FLAG_HIDDEN);
    if(g_result_lbl)lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);
    g_mode=0;g_over=false;
}

lv_obj_t* gomoku_create(lv_obj_t *parent) {
    // menu page
    g_menu_page=lv_obj_create(parent);
    lv_obj_set_size(g_menu_page,lv_pct(100),lv_pct(100));
    lv_obj_set_style_border_width(g_menu_page,0,0);
    lv_obj_set_style_bg_opa(g_menu_page,LV_OPA_TRANSP,0);
    lv_obj_t *t=lv_label_create(g_menu_page);
    lv_label_set_text(t,"Gomoku");lv_obj_set_style_text_font(t,&lv_font_montserrat_32,0);
    lv_obj_set_style_text_color(t,lv_color_hex(0xFFFFFF),0);
    lv_obj_align(t,LV_ALIGN_TOP_MID,0,60);
    lv_obj_t *b1=lv_btn_create(g_menu_page);lv_obj_set_size(b1,220,70);
    lv_obj_align(b1,LV_ALIGN_CENTER,0,-60);lv_obj_set_style_radius(b1,16,0);
    lv_obj_set_style_bg_color(b1,lv_color_hex(0x1565c0),0);
    lv_obj_t *l1=lv_label_create(b1);lv_label_set_text(l1,"Two Players");
    lv_obj_set_style_text_font(l1,&lv_font_montserrat_20,0);lv_obj_center(l1);
    lv_obj_add_event_cb(b1,[](lv_event_t*){start_game(1);},LV_EVENT_CLICKED,nullptr);
    lv_obj_t *b2=lv_btn_create(g_menu_page);lv_obj_set_size(b2,220,70);
    lv_obj_align(b2,LV_ALIGN_CENTER,0,30);lv_obj_set_style_radius(b2,16,0);
    lv_obj_set_style_bg_color(b2,lv_color_hex(0x7B1FA2),0);
    lv_obj_t *l2=lv_label_create(b2);lv_label_set_text(l2,"VS Computer");
    lv_obj_set_style_text_font(l2,&lv_font_montserrat_20,0);lv_obj_center(l2);
    lv_obj_add_event_cb(b2,[](lv_event_t*){start_game(2);},LV_EVENT_CLICKED,nullptr);

    // game page
    g_game_page=lv_obj_create(parent);
    lv_obj_set_size(g_game_page,lv_pct(100),lv_pct(100));
    lv_obj_set_style_border_width(g_game_page,0,0);
    lv_obj_set_style_bg_opa(g_game_page,LV_OPA_TRANSP,0);
    lv_obj_add_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);

    // Board container (fixed, non-scrollable, native LVGL objects)
    g_board_cont=lv_obj_create(g_game_page);
    int bp=MARGIN*2+CELL_SIZE*(BOARD_SIZE-1);
    lv_obj_set_size(g_board_cont,bp,bp);
    lv_obj_set_style_bg_color(g_board_cont,lv_color_hex(0xDEB887),0);
    lv_obj_set_style_border_width(g_board_cont,0,0);
    lv_obj_set_style_radius(g_board_cont,4,0);
    lv_obj_align(g_board_cont,LV_ALIGN_CENTER,-90,0);
    lv_obj_add_flag(g_board_cont,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_board_cont,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_game_page,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_board_cont,board_click_cb,LV_EVENT_CLICKED,nullptr);

    // Grid lines — pushed to back so stones render on top
    for(int i=0;i<BOARD_SIZE;i++){
        int xy=MARGIN+i*CELL_SIZE;
        lv_obj_t *hl=lv_obj_create(g_board_cont);lv_obj_set_size(hl,CELL_SIZE*(BOARD_SIZE-1),1);
        lv_obj_set_pos(hl,MARGIN,xy);lv_obj_set_style_bg_color(hl,lv_color_black(),0);
        lv_obj_set_style_border_width(hl,0,0);lv_obj_move_background(hl);
        lv_obj_t *vl=lv_obj_create(g_board_cont);lv_obj_set_size(vl,1,CELL_SIZE*(BOARD_SIZE-1));
        lv_obj_set_pos(vl,xy,MARGIN);lv_obj_set_style_bg_color(vl,lv_color_black(),0);
        lv_obj_set_style_border_width(vl,0,0);lv_obj_move_background(vl);
    }
    // Star points
    int stars[3]={3,7,11};
    for(int r=0;r<3;r++)for(int c=0;c<3;c++){
        lv_obj_t *dot=lv_obj_create(g_board_cont);lv_obj_set_size(dot,6,6);
        lv_obj_set_pos(dot,MARGIN+stars[c]*CELL_SIZE-3,MARGIN+stars[r]*CELL_SIZE-3);
        lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_color(dot,lv_color_black(),0);lv_obj_set_style_border_width(dot,0,0);
        lv_obj_move_background(dot);
    }

    // Right sidebar
    lv_obj_t *side=lv_obj_create(g_game_page);
    lv_obj_set_size(side,170,lv_pct(90));lv_obj_align(side,LV_ALIGN_RIGHT_MID,-10,0);
    lv_obj_set_style_border_width(side,0,0);lv_obj_set_style_bg_opa(side,LV_OPA_TRANSP,0);
    lv_obj_set_flex_flow(side,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(side,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(side,10,0);

    g_status_lbl=lv_label_create(side);lv_label_set_text(g_status_lbl,"Black's turn");
    lv_obj_set_style_text_font(g_status_lbl,&lv_font_montserrat_18,0);
    lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_align(g_status_lbl,LV_TEXT_ALIGN_CENTER,0);

    struct{const char*t;uint32_t c;lv_event_cb_t cb;}btns[]={
        {"Undo",0x42426A,[](lv_event_t*){undo_move();}},
        {"Restart",0x2E7D32,[](lv_event_t*){start_game(g_mode);}},
        {"Back",0x333355,[](lv_event_t*){back_to_menu();}},
    };
    for(auto&b:btns){
        lv_obj_t *btn=lv_btn_create(side);lv_obj_set_size(btn,140,46);
        lv_obj_set_style_radius(btn,12,0);lv_obj_set_style_bg_color(btn,lv_color_hex(b.c),0);
        lv_obj_t *l=lv_label_create(btn);lv_label_set_text(l,b.t);
        lv_obj_set_style_text_font(l,&lv_font_montserrat_18,0);lv_obj_center(l);
        lv_obj_add_event_cb(btn,b.cb,LV_EVENT_CLICKED,nullptr);
    }

    g_result_lbl=lv_label_create(g_game_page);
    lv_obj_set_style_text_font(g_result_lbl,&lv_font_montserrat_28,0);
    lv_obj_set_style_text_color(g_result_lbl,lv_color_hex(0xFFD700),0);
    lv_obj_set_style_bg_color(g_result_lbl,lv_color_hex(0x1A1A2E),0);
    lv_obj_set_style_bg_opa(g_result_lbl,LV_OPA_80,0);
    lv_obj_set_style_pad_all(g_result_lbl,20,0);lv_obj_set_style_radius(g_result_lbl,12,0);
    lv_obj_align(g_result_lbl,LV_ALIGN_CENTER,-90,0);
    lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_result_lbl,[](lv_event_t *e){
        lv_obj_add_flag(lv_event_get_target(e),LV_OBJ_FLAG_HIDDEN);
    },LV_EVENT_CLICKED,nullptr);

    reset_game();return parent;
}
