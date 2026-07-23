#include "gomoku.h"
#include <cstring>
#include <cstdlib>
#include <esp_log.h>
#include <esp_heap_caps.h>

#define TAG "Gomoku"
#define BOARD_SIZE 15
#define CELL_SIZE  28
#define MARGIN     36
#define STONE_R    12
#define CANVAS_PX  (MARGIN*2 + CELL_SIZE*(BOARD_SIZE-1))

static int g_board[BOARD_SIZE][BOARD_SIZE];
static int g_current = 1, g_mode = 0, g_hist_cnt = 0;
static bool g_over = false;
static int g_history[BOARD_SIZE*BOARD_SIZE][2];
static int g_last_r = -1, g_last_c = -1;

static lv_obj_t *g_canvas = nullptr;
static lv_color_t *g_cbuf = nullptr;
static lv_obj_t *g_status_lbl = nullptr;
static lv_obj_t *g_menu_page = nullptr;
static lv_obj_t *g_game_page = nullptr;
static lv_obj_t *g_result_lbl = nullptr;

// ── 胜负 ──
static int cdir(int r,int c,int dr,int dc,int p){int n=0;for(int i=1;i<5;i++){int nr=r+dr*i,nc=c+dc*i;if(nr<0||nr>=15||nc<0||nc>=15)break;if(g_board[nr][nc]!=p)break;n++;}return n;}
static int chk(int r,int c){int p=g_board[r][c];if(!p)return 0;int d[4][2]={{1,0},{0,1},{1,1},{1,-1}};for(int i=0;i<4;i++)if(1+cdir(r,c,d[i][0],d[i][1],p)+cdir(r,c,-d[i][0],-d[i][1],p)>=5)return p;return 0;}

// ── AI ──
static int eline(int r,int c,int dr,int dc,int p){int n=1,o0=0,o1=0;for(int i=1;i<=5;i++){int nr=r+dr*i,nc=c+dc*i;if(nr<0||nr>=15||nc<0||nc>=15)break;if(g_board[nr][nc]==p)n++;else{if(!g_board[nr][nc])o0=1;break;}}for(int i=1;i<=5;i++){int nr=r-dr*i,nc=c-dc*i;if(nr<0||nr>=15||nc<0||nc>=15)break;if(g_board[nr][nc]==p)n++;else{if(!g_board[nr][nc])o1=1;break;}}if(n>=5)return 50000;int o=o0+o1;if(n==4)return o==2?10000:(o==1?1000:0);if(n==3)return o==2?1000:(o==1?100:0);if(n==2)return o==2?100:(o==1?10:0);return o==2?10:(o==1?1:0);}
static int epos(int r,int c,int p){int t=0,d[4][2]={{1,0},{0,1},{1,1},{1,-1}};for(int i=0;i<4;i++)t+=eline(r,c,d[i][0],d[i][1],p);return t;}
static int ai_go(){int best=-1,br=7,bc=7;for(int r=0;r<15;r++)for(int c=0;c<15;c++){if(g_board[r][c])continue;int s=epos(r,c,2)+epos(r,c,1)/2;if(s>best||(s==best&&rand()%3==0)){best=s;br=r;bc=c;}}g_board[br][bc]=2;g_history[g_hist_cnt][0]=br;g_history[g_hist_cnt][1]=bc;g_hist_cnt++;int w=chk(br,bc);if(w){g_over=true;return w;}if(g_hist_cnt>=225){g_over=true;return 0;}g_current=1;return 0;}

// ── Canvas 绘制 ──
static void draw_board_bg() {
    lv_canvas_fill_bg(g_canvas, lv_color_hex(0xDEB887), LV_OPA_COVER);
    lv_draw_rect_dsc_t rd;lv_draw_rect_dsc_init(&rd);rd.bg_opa=LV_OPA_TRANSP;rd.border_color=lv_color_black();rd.border_width=1;
    lv_canvas_draw_rect(g_canvas,MARGIN,MARGIN,CELL_SIZE*(BOARD_SIZE-1),CELL_SIZE*(BOARD_SIZE-1),&rd);
    lv_draw_line_dsc_t ld;lv_draw_line_dsc_init(&ld);ld.color=lv_color_black();ld.width=1;
    for(int i=0;i<BOARD_SIZE;i++){
        lv_coord_t xy=(lv_coord_t)(MARGIN+i*CELL_SIZE),end=(lv_coord_t)(MARGIN+CELL_SIZE*(BOARD_SIZE-1));
        lv_point_t hp[2]={{(lv_coord_t)MARGIN,xy},{end,xy}};lv_canvas_draw_line(g_canvas,hp,2,&ld);
        lv_point_t vp[2]={{xy,(lv_coord_t)MARGIN},{xy,end}};lv_canvas_draw_line(g_canvas,vp,2,&ld);
    }
    int stars[3]={3,7,11};lv_draw_rect_dsc_t dt;lv_draw_rect_dsc_init(&dt);dt.bg_color=lv_color_black();dt.bg_opa=LV_OPA_COVER;
    for(int r=0;r<3;r++)for(int c=0;c<3;c++)lv_canvas_draw_rect(g_canvas,MARGIN+stars[c]*CELL_SIZE-3,MARGIN+stars[r]*CELL_SIZE-3,6,6,&dt);
}

static void draw_stone(int r,int c,int player){
    lv_draw_arc_dsc_t a;lv_draw_arc_dsc_init(&a);a.start_angle=0;a.end_angle=3600;
    int cx=MARGIN+c*CELL_SIZE,cy=MARGIN+r*CELL_SIZE;
    a.color=player==1?lv_color_black():lv_color_white();a.width=STONE_R;
    lv_canvas_draw_arc(g_canvas,cx,cy,STONE_R,0,3600,&a);
    if(player==2){a.color=lv_color_black();a.width=1;lv_canvas_draw_arc(g_canvas,cx,cy,STONE_R-2,0,3600,&a);}
}

static void draw_ring(int r,int c){
    if(r<0||c<0)return;
    int cx=MARGIN+c*CELL_SIZE,cy=MARGIN+r*CELL_SIZE;
    lv_draw_arc_dsc_t a;lv_draw_arc_dsc_init(&a);a.start_angle=0;a.end_angle=3600;
    a.color=lv_color_hex(0x00FF00);a.width=3;
    lv_canvas_draw_arc(g_canvas,cx,cy,STONE_R+3,0,3600,&a);
}

static void redraw_all(){
    draw_board_bg();
    for(int r=0;r<BOARD_SIZE;r++)for(int c=0;c<BOARD_SIZE;c++)if(g_board[r][c])draw_stone(r,c,g_board[r][c]);
    draw_ring(g_last_r,g_last_c);
    lv_obj_invalidate(g_canvas);
}

static void reset_game(){memset(g_board,0,sizeof(g_board));g_current=1;g_over=false;g_hist_cnt=0;g_last_r=-1;g_last_c=-1;}

static void show_result(int w){
    if(!g_result_lbl)return;
    lv_label_set_text(g_result_lbl,w==0?"Draw!":(w==1?"Black Wins!":"White Wins!"));
    lv_obj_clear_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(g_result_lbl);
}

static void place_stone(int r,int c){
    if(g_over||r<0||r>=BOARD_SIZE||c<0||c>=BOARD_SIZE||g_board[r][c]!=0)return;
    // 擦除旧绿圈：用棋盘底色覆盖
    if(g_last_r>=0&&g_last_c>=0){
        int cx=MARGIN+g_last_c*CELL_SIZE,cy=MARGIN+g_last_r*CELL_SIZE;
        lv_draw_arc_dsc_t cl;lv_draw_arc_dsc_init(&cl);cl.start_angle=0;cl.end_angle=3600;
        cl.color=lv_color_hex(0xDEB887);cl.width=4;
        lv_canvas_draw_arc(g_canvas,cx,cy,STONE_R+4,0,3600,&cl);
        // 重新画那个位置原来的棋子（底色弧线覆盖了棋子边缘）
        if(g_board[g_last_r][g_last_c]!=0)draw_stone(g_last_r,g_last_c,g_board[g_last_r][g_last_c]);
    }
    g_board[r][c]=g_current;g_history[g_hist_cnt][0]=r;g_history[g_hist_cnt][1]=c;g_hist_cnt++;
    g_last_r=r;g_last_c=c;
    draw_stone(r,c,g_current);draw_ring(r,c);
    lv_obj_invalidate(g_canvas);
    int w=chk(r,c);if(w){g_over=true;show_result(w);return;}
    if(g_hist_cnt>=BOARD_SIZE*BOARD_SIZE){g_over=true;show_result(0);return;}
    g_current=3-g_current;
    if(g_status_lbl){lv_label_set_text(g_status_lbl,g_current==1?"Black's turn":"White's turn");lv_obj_set_style_text_color(g_status_lbl,g_current==1?lv_color_hex(0xFFFFFF):lv_color_hex(0xCCCCCC),0);}
    if(g_mode==2&&g_current==2&&!g_over){lv_label_set_text(g_status_lbl,"AI thinking...");lv_timer_create([](lv_timer_t *t){int r=ai_go();if(g_last_r>=0&&g_last_c>=0){int cx=MARGIN+g_last_c*CELL_SIZE,cy=MARGIN+g_last_r*CELL_SIZE;lv_draw_arc_dsc_t cl;lv_draw_arc_dsc_init(&cl);cl.start_angle=0;cl.end_angle=3600;cl.color=lv_color_hex(0xDEB887);cl.width=4;lv_canvas_draw_arc(g_canvas,cx,cy,STONE_R+4,0,3600,&cl);if(g_board[g_last_r][g_last_c]!=0)draw_stone(g_last_r,g_last_c,g_board[g_last_r][g_last_c]);}int ar=g_history[g_hist_cnt-1][0],ac=g_history[g_hist_cnt-1][1];g_last_r=ar;g_last_c=ac;draw_stone(ar,ac,2);draw_ring(ar,ac);lv_obj_invalidate(g_canvas);lv_label_set_text(g_status_lbl,r?(r==2?"White Wins!":"Black Wins!"):"Black's turn");lv_obj_set_style_text_color(g_status_lbl,r?lv_color_hex(0xFFD700):lv_color_hex(0xFFFFFF),0);if(r)show_result(r);lv_timer_del(t);},300,nullptr);}
}

static void canvas_click(lv_event_t*e){
    if(g_over||(g_mode==2&&g_current==2))return;
    lv_indev_t *indev=lv_indev_get_act();lv_point_t pt;lv_indev_get_point(indev,&pt);
    lv_area_t ca;lv_obj_get_coords(g_canvas,&ca);
    int rx=pt.x-ca.x1,ry=pt.y-ca.y1;
    int c=(rx-MARGIN+CELL_SIZE/2)/CELL_SIZE;
    int r=(ry-MARGIN+CELL_SIZE/2)/CELL_SIZE;
    if(r<0||r>=BOARD_SIZE||c<0||c>=BOARD_SIZE)return;
    place_stone(r,c);
}

static void start_game(int m){g_mode=m;reset_game();redraw_all();lv_obj_add_flag(g_menu_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);if(g_result_lbl)lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(g_status_lbl,"Black's turn");lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);}
static void undo_move(){
    if(g_over||g_hist_cnt==0)return;
    int s=(g_mode==2&&g_current==2&&g_hist_cnt>=2)?2:1;
    for(int i=0;i<s&&g_hist_cnt>0;i++){g_hist_cnt--;g_board[g_history[g_hist_cnt][0]][g_history[g_hist_cnt][1]]=0;}
    g_last_r=g_hist_cnt>0?g_history[g_hist_cnt-1][0]:-1;g_last_c=g_hist_cnt>0?g_history[g_hist_cnt-1][1]:-1;
    g_current=1;redraw_all();
    lv_label_set_text(g_status_lbl,"Black's turn");lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);
}
static void back_menu(){lv_obj_add_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(g_menu_page,LV_OBJ_FLAG_HIDDEN);if(g_result_lbl)lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN);g_mode=0;g_over=false;}

lv_obj_t* gomoku_create(lv_obj_t*parent){
    g_menu_page=lv_obj_create(parent);lv_obj_set_size(g_menu_page,lv_pct(100),lv_pct(100));lv_obj_set_style_border_width(g_menu_page,0,0);lv_obj_set_style_bg_opa(g_menu_page,LV_OPA_TRANSP,0);
    lv_obj_t*t=lv_label_create(g_menu_page);lv_label_set_text(t,"Gomoku");lv_obj_set_style_text_font(t,&lv_font_montserrat_32,0);lv_obj_set_style_text_color(t,lv_color_hex(0xFFFFFF),0);lv_obj_align(t,LV_ALIGN_TOP_MID,0,60);
    lv_obj_t*b1=lv_btn_create(g_menu_page);lv_obj_set_size(b1,220,70);lv_obj_align(b1,LV_ALIGN_CENTER,0,-60);lv_obj_set_style_radius(b1,16,0);lv_obj_set_style_bg_color(b1,lv_color_hex(0x1565c0),0);
    lv_obj_t*l1=lv_label_create(b1);lv_label_set_text(l1,"Two Players");lv_obj_set_style_text_font(l1,&lv_font_montserrat_20,0);lv_obj_center(l1);lv_obj_add_event_cb(b1,[](lv_event_t*){start_game(1);},LV_EVENT_CLICKED,nullptr);
    lv_obj_t*b2=lv_btn_create(g_menu_page);lv_obj_set_size(b2,220,70);lv_obj_align(b2,LV_ALIGN_CENTER,0,30);lv_obj_set_style_radius(b2,16,0);lv_obj_set_style_bg_color(b2,lv_color_hex(0x7B1FA2),0);
    lv_obj_t*l2=lv_label_create(b2);lv_label_set_text(l2,"VS Computer");lv_obj_set_style_text_font(l2,&lv_font_montserrat_20,0);lv_obj_center(l2);lv_obj_add_event_cb(b2,[](lv_event_t*){start_game(2);},LV_EVENT_CLICKED,nullptr);

    g_game_page=lv_obj_create(parent);lv_obj_set_size(g_game_page,lv_pct(100),lv_pct(100));lv_obj_set_style_border_width(g_game_page,0,0);lv_obj_set_style_bg_opa(g_game_page,LV_OPA_TRANSP,0);lv_obj_add_flag(g_game_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(g_game_page,LV_OBJ_FLAG_SCROLLABLE);

    // 棋盘 Canvas — 全部在 Canvas 上绘制，坐标统一无偏移
    g_canvas=lv_canvas_create(g_game_page);
    g_cbuf=(lv_color_t*)heap_caps_malloc(CANVAS_PX*CANVAS_PX*2,MALLOC_CAP_SPIRAM);
    lv_canvas_set_buffer(g_canvas,g_cbuf,CANVAS_PX,CANVAS_PX,LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(g_canvas,LV_ALIGN_CENTER,-90,0);
    lv_obj_add_flag(g_canvas,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_canvas,canvas_click,LV_EVENT_CLICKED,nullptr);

    // 右侧面板
    lv_obj_t*side=lv_obj_create(g_game_page);lv_obj_set_size(side,170,lv_pct(90));lv_obj_align(side,LV_ALIGN_RIGHT_MID,-10,0);lv_obj_set_style_border_width(side,0,0);lv_obj_set_style_bg_opa(side,LV_OPA_TRANSP,0);lv_obj_set_flex_flow(side,LV_FLEX_FLOW_COLUMN);lv_obj_set_flex_align(side,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER);lv_obj_set_style_pad_all(side,10,0);
    g_status_lbl=lv_label_create(side);lv_label_set_text(g_status_lbl,"Black's turn");lv_obj_set_style_text_font(g_status_lbl,&lv_font_montserrat_18,0);lv_obj_set_style_text_color(g_status_lbl,lv_color_hex(0xFFFFFF),0);lv_obj_set_style_text_align(g_status_lbl,LV_TEXT_ALIGN_CENTER,0);
    struct{const char*t;uint32_t c;lv_event_cb_t cb;}btns[]={{"Undo",0x42426A,[](lv_event_t*){undo_move();}},{"Restart",0x2E7D32,[](lv_event_t*){start_game(g_mode);}},{"Back",0x333355,[](lv_event_t*){back_menu();}}};
    for(auto&b:btns){lv_obj_t*btn=lv_btn_create(side);lv_obj_set_size(btn,140,46);lv_obj_set_style_radius(btn,12,0);lv_obj_set_style_bg_color(btn,lv_color_hex(b.c),0);lv_obj_t*l=lv_label_create(btn);lv_label_set_text(l,b.t);lv_obj_set_style_text_font(l,&lv_font_montserrat_18,0);lv_obj_center(l);lv_obj_add_event_cb(btn,b.cb,LV_EVENT_CLICKED,nullptr);}
    g_result_lbl=lv_label_create(g_game_page);lv_obj_set_style_text_font(g_result_lbl,&lv_font_montserrat_28,0);lv_obj_set_style_text_color(g_result_lbl,lv_color_hex(0xFFD700),0);lv_obj_set_style_bg_color(g_result_lbl,lv_color_hex(0x1A1A2E),0);lv_obj_set_style_bg_opa(g_result_lbl,LV_OPA_80,0);lv_obj_set_style_pad_all(g_result_lbl,20,0);lv_obj_set_style_radius(g_result_lbl,12,0);lv_obj_align(g_result_lbl,LV_ALIGN_CENTER,-90,0);lv_obj_add_flag(g_result_lbl,LV_OBJ_FLAG_HIDDEN|LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(g_result_lbl,[](lv_event_t*e){lv_obj_add_flag(lv_event_get_target(e),LV_OBJ_FLAG_HIDDEN);},LV_EVENT_CLICKED,nullptr);
    reset_game();return parent;
}
