#include <pebble.h>
#include "netdownload.h"
#define FRAME_MS 250 //Update freq - an optimistic 4 fps
#define HEAP_FREE 10000 //Keep this amount of heap free
static Window *window;
static TextLayer *text_layer;
static BitmapLayer *bitmap_layer;
static GBitmap *current_bmp;
static GBitmap *nxt_bmp;
static AppTimer *timer;
static Layer *video_layer;
bool playing = false;
unsigned int frames = 0;
char fpsTxt[20];

//Linked list for keeping a frame buffer
struct llnode {
  GBitmap *bmp;
  struct llnode *nxt; //Next frame
  struct llnode *prv; //Previous frame
};
typedef struct llnode ll;

struct deq {
  unsigned int len;
  ll *fst; //First Element
  ll *lst; //Last Element
} *fbuff;

void beginstream(void *ctx) {
  //Request URL buffer
  printf("Requesting stream begins");
  netdownload_request(""); //URL no longer necessary - may become useful in future.
}

void show_frame(Layer *layer, GContext *ctx){

  //Draw with transparency
  if(fbuff->fst != NULL){
    //Support for layering:
    graphics_context_set_compositing_mode(ctx, GCompOpSet);

    GRect bounds = gbitmap_get_bounds(fbuff->fst->bmp);
    graphics_draw_bitmap_in_rect(ctx, fbuff->fst->bmp, GRect(bounds.origin.x, bounds.origin.y + 20, bounds.size.w, bounds.size.h));

    if(playing && fbuff->fst->nxt != fbuff->fst){ //Save one frame in memory
      //Free fst and remove reference
      ll *cur = fbuff->fst->nxt;
      gbitmap_destroy(fbuff->fst->bmp);
      fbuff->lst->nxt = cur;
      free(fbuff->fst);
      fbuff->fst = cur;
      fbuff->len -= 1;
      frames++;
    }
  }
}

void play(void *ctx){
  if(playing){
    layer_mark_dirty(video_layer); //Update video layer

    //Timer for next frame
    timer = app_timer_register(FRAME_MS, play, NULL); //Main loop
  }
}

static void fpscalc(struct tm *tick_time, TimeUnits changed){
    snprintf(fpsTxt, 20, "%dbuf %dfps", fbuff->len, frames);
    frames = 0;
    layer_mark_dirty(text_layer_get_layer(text_layer));
}

static void window_load(Window *window) {
  fbuff = malloc(sizeof(struct deq));
  fbuff->len = 0; //No frames loaded yet

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  video_layer = layer_create(bounds);
  layer_set_update_proc(video_layer, show_frame);
  layer_add_child(window_layer, video_layer);

  text_layer = text_layer_create(GRect(0,0, 150, 20));
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  text_layer_set_text(text_layer, fpsTxt);
  text_layer_set_background_color(text_layer, GColorBlack);
  text_layer_set_text_color(text_layer, GColorWhite);

  //Subscribe for FPS
  tick_timer_service_subscribe(SECOND_UNIT, fpscalc);
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  bitmap_layer_destroy(bitmap_layer);
  layer_destroy(video_layer);

  //Free all buffer memory
  fbuff->lst->nxt = NULL; //Linearise
  ll *todel = fbuff->fst;
  while(todel != NULL){
    ll *nxt = todel->nxt;
    gbitmap_destroy(todel->bmp);
    free(todel);
    todel = nxt;
  }
  free(fbuff);
}

void download_complete_handler(NetDownload *download) {
  printf("Loaded image with %lu bytes", download->length);
  printf("Heap free is %u bytes", heap_bytes_free());
  if(heap_bytes_free() < HEAP_FREE){
    printf("Heap too small, dropping frame");
    //TODO: send 'stop' request so phone stops sending frames
    return;
  }
  //Create bmp
  GBitmap *bmp = gbitmap_create_from_png_data(download->data, download->length);

  // Free the memory now
  free(download->data);
  download->data = NULL;

  if(bmp == NULL){ //For some reason this doesn't work
    printf("Out of memory, cannot allocate new frame");
    return;
  }

  //Create new list element for bmp
  ll *newLst = malloc(sizeof(ll));
  newLst->bmp = bmp;

  //Append to fbuff 
  if(fbuff->lst != NULL){
    fbuff->lst->nxt = newLst;
    newLst->prv = fbuff->lst;
  }
  fbuff->lst = newLst;
  if(fbuff->fst != NULL){
    fbuff->fst->prv = newLst;
    newLst->nxt = fbuff->fst;
  }else{ //Nothing in fbuff
    fbuff->fst = newLst;
    newLst->nxt = newLst;
    newLst->prv = newLst;
  }

  //Increment buffer length: if 6, start playing
  fbuff->len += 1;
  if(fbuff->len >= 6 && !playing) {
    //timer = app_timer_register(FRAME_MS, play, NULL); //Play video
  }
  printf("%u Frames in memory", fbuff->len);

  netdownload_destroy(download);
}

static void togglePlayPause(ClickRecognizerRef recognizer, void *context){
  playing = !playing;
  printf("Play/Pause Button");
  if(playing){
    printf("Playing");
    timer = app_timer_register(FRAME_MS, play, NULL);
  }
}

static void click_config_provider(void *context){
  window_single_click_subscribe(BUTTON_ID_SELECT, togglePlayPause);
}

static void init(void) {
  // Need to initialize this first to make sure it is there when
  // the window_load function is called by window_stack_push.
  netdownload_initialize(download_complete_handler);

  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_click_config_provider(window, click_config_provider);
  window_stack_push(window, true);
	window_set_background_color(window, GColorBlack);

  light_enable(true);

  //Give enough time for JS to load
  timer = app_timer_register(1000, beginstream, NULL); //Main loop
}

static void deinit(void) {
  netdownload_deinitialize(); // call this to avoid 20B memory leak
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
