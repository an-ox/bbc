#include <pebble.h>

// watchface based on BBC clocks

// keys for settings  
#define KEY_SECONDS 0  
#define KEY_ERA 1
  
 // styles
#define STYLE_1970 0
#define STYLE_1978 1
#define STYLE_1981 2
  
#define XREZ 144
#define YREZ 168 
#define CENTREX 72
#define CENTREY 84

  
static Window *window;
static Layer *canvas; 

// using an apptimer allows us to easily change the update rate

static AppTimer *timer;

uint8_t clockDigits[6];  // I will unpack the time into here HHMMSS

// info about the time

uint8_t lastMinute=64;
uint8_t lastHour=64;
uint16_t currentSecond; 
uint16_t currentMinute;
uint16_t currentHour;

// info about the battery and connection state

bool btconnected=false;
uint8_t batteryLevel=0;
bool batteryCharging=false;

// need to be able to see the time so..
struct tm *t;  // Structure holding easily read components of the time.
time_t temp;	 // Raw epoch time.

// Some state.
bool showSeconds=true;
int displayStyle=STYLE_1978;

                
// stuff to update info on battery and BT status

static void handle_battery(BatteryChargeState charge_state) 
{
  batteryCharging=(charge_state.is_charging); 
  batteryLevel=charge_state.charge_percent;
}

static void handle_bluetooth(bool connected)
{
  btconnected=connected;
}

static void timer_callback(void *data)
{	
  //Render
  handle_battery(battery_state_service_peek());
  btconnected=bluetooth_connection_service_peek(); 
  layer_mark_dirty(canvas);
  timer=app_timer_register((showSeconds)?1000:60000,(AppTimerCallback) timer_callback,0);
}


// Crap to handle stuff being sent from the phone

static void process_tuple(Tuple *t)
{
  // Act upon individual tuples
  switch(t->key)
  {
   case KEY_SECONDS:  // turn second hand off and on
    if(strcmp(t->value->cstring,"on")==0)
    {
      showSeconds=true;
      persist_write_bool(KEY_SECONDS,true);
    }
    else if(strcmp(t->value->cstring,"off")==0)
    {
      showSeconds=false;
      persist_write_bool(KEY_SECONDS,false);
    }
    timer=app_timer_register(100,(AppTimerCallback) timer_callback,0); // force quick load after setting change
    break;
    case KEY_ERA:  // set style according to approximate BBC era
     if(strcmp(t->value->cstring,"1970")==0)
     {
       displayStyle=STYLE_1970;
       persist_write_int(KEY_ERA,STYLE_1970);
     }
     else if(strcmp(t->value->cstring,"1978")==0)
     {
       displayStyle=STYLE_1978;
       persist_write_int(KEY_ERA,STYLE_1978);
     }    
     else if(strcmp(t->value->cstring,"1981")==0)
     {
       displayStyle=STYLE_1981;
       persist_write_int(KEY_ERA,STYLE_1981);
     }    
     timer=app_timer_register(100,(AppTimerCallback) timer_callback,0); // force quick load after setting change
    break;
  }
}

static void in_received_handler(DictionaryIterator *iter, void *context) 
{
  // Extract tuples from a dictionary & process them
    (void) context;
    //Get data
    Tuple *t = dict_read_first(iter);
    while(t != NULL)
    {
        process_tuple(t);
        //Get next
        t = dict_read_next(iter);
    }
}


static void hourChanged()
{
  // this will be called every time the hour changes
  // use it to change any state per hour on the hour
  clockDigits[0]=currentHour/10;
  clockDigits[1]=currentHour%10;
}

static void minuteChanged()
{
  // this will be called every time the minute changes
  // use it to change any state per minute on the minute  
 
}

// make stuff needed during run time

static void makeCrap()
{
  int i;

// fetch the time so I can set everything up right at first run

  temp = time(NULL);	   // get raw time
  t = localtime(&temp);  // break it up into readable bits
  // I'll initialise this even though it'll get filled with the time
  // before first draw happens anyway
  for(i=0;i<6;i++) clockDigits[i]=i;
  
}



  

// throw away all the smeg when done

static void destroyCrap()
{
}

	
/****************************** Renderer Lifecycle *****************************/


/*
 * Render 
 */

/*
 * Start rendering loop
 */
static void start()
{
  timer=app_timer_register((showSeconds)?1000:60000,(AppTimerCallback) timer_callback,0);
}



/*
 * Rendering
 */

void rotate(uint16_t ang,GPoint *p)
{
  ang-=0x4000;
  int16_t ox=p->x;
  int16_t oy=p->y;
  
  p->x=(sin_lookup(ang)*ox/TRIG_MAX_RATIO)+(cos_lookup(ang)*oy/TRIG_MAX_RATIO);
  p->y=(-cos_lookup(ang)*ox/TRIG_MAX_RATIO)+(sin_lookup(ang)*oy/TRIG_MAX_RATIO);
  
}

static void hand(GContext *ctx,uint16_t ang,int16_t w1, int16_t w2,int16_t outerr,uint16_t innerr,int16_t xoff,uint8_t fracbits)
{
  // Usually used to draw rectangular or tapered watch hands. Added the xoff offset
  // option so it can also be used to make the double block hour markers on the BBC
  // style watchfaces.
  //
  // fracbits is the number of fixed point fraction bits it assumes you use.
  
   GPathInfo gp={4,(GPoint[])
                {
                  {0,0},
                  {0,0},
                  {0,0},
                  {0,0}
                }
               };
  GPath *pptr=NULL;
  GPoint a,b,c,d;

  a.x=-w1+xoff;
  a.y=outerr;
  b.x=w1+xoff;
  b.y=outerr;
  c.x=w2+xoff;
  c.y=innerr;
  d.x=-w2+xoff;
  d.y=innerr;   
  rotate(ang,&a);
  rotate(ang,&b);
  rotate(ang,&c);
  rotate(ang,&d);
  gp.points[0].x=a.x>>fracbits;
  gp.points[0].y=a.y>>fracbits;
  gp.points[1].x=b.x>>fracbits;
  gp.points[1].y=b.y>>fracbits;
  gp.points[2].x=c.x>>fracbits;
  gp.points[2].y=c.y>>fracbits;
  gp.points[3].x=d.x>>fracbits;
  gp.points[3].y=d.y>>fracbits;   
  pptr=gpath_create(&gp);
  gpath_move_to(pptr, GPoint(CENTREX,CENTREY));
  gpath_draw_outline(ctx,pptr);
  gpath_draw_filled(ctx,pptr);
  gpath_destroy(pptr);
  
}
static void render(Layer *layer, GContext* ctx) 
{
  int i;

  // Get the current bits of the time

  temp = time(NULL);	   // get raw time
  t = localtime(&temp);  // break it up into readable bits
  currentSecond=t->tm_sec;  // the current second
  currentMinute=t->tm_min;
  currentHour=t->tm_hour;
  if(currentHour>11) currentHour-=12;

  if(currentHour==0) currentHour=12;

// Upon first run copy the current hour and sec ro last hour and sec

  if(lastMinute>60)  // will never naturally occur, set that way at first run
  {
    lastMinute=currentMinute;
    lastHour=currentHour;
  }
  else // in normal runs we can check for changed min or hour and do something
  {
    if(currentHour!=lastHour)
    {
      lastHour=currentHour;
      hourChanged();
    }
    if(currentMinute!=lastMinute)
    {
      lastMinute=currentMinute;
      minuteChanged();
    }    
  }
  if(currentSecond>59) currentSecond=59;  // Can happen, apparently, if there is a leap second. 
  
  // put time in time digits

  clockDigits[0]=currentHour/10;
  clockDigits[1]=currentHour%10;
  clockDigits[2]=t->tm_min/10;
  clockDigits[3]=t->tm_min%10;
  clockDigits[4]=t->tm_sec/10;
  clockDigits[5]=t->tm_sec%10;
  
  // preinit battery info
  handle_battery(battery_state_service_peek());
  btconnected=bluetooth_connection_service_peek();

  const uint8_t fp=8;  // number of fracbits
  // default sizes for the hands
  uint16_t hhThickness=4<<fp;
  uint16_t mhThickness=3<<fp;
  uint16_t shThickness=3<<(fp-1);
  uint8_t innerDotRadius=10;
  uint8_t shortenHH=0;
  // set colours according to era
  // default to blue/orange
  int bgCol=0x000055;
  int fillCol=0xffaa00;
  if(displayStyle==STYLE_1970)
  {
    bgCol=0x000000;
    fillCol=0xffffff;
  }
   if(displayStyle==STYLE_1981)
  {
    bgCol=0x000055;
    fillCol=0xaaff00;
    shThickness=1<<fp;
    mhThickness=shThickness;
    hhThickness=3<<(fp-1);
    innerDotRadius=7;
    shortenHH=5;
  }
  window_set_background_color(window,GColorFromHEX(bgCol));
  graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
  graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));
  // draw smeg
  
  const int16_t outerRadius=70<<fp;
  const int16_t innerRadius=50<<fp;
  const int16_t gapWidth=0x180;
 
  uint16_t phi=0x8000/6;
  uint16_t ph=phi;
 
  
  
  int vw=3<<(fp-1);
  const int baa=2<<fp;  // bar size for 1981 era
  for(i=0;i<12;i++)
  {
    switch(displayStyle)
    {
      // draw the hour pips according to the era
      case STYLE_1970: // use the gradually fattening hour pips
      case STYLE_1978:
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,-(vw+gapWidth),fp);
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,vw+gapWidth,fp);
      break;
      case STYLE_1981: // use single bars except for 12,3,6 and 9 which are double
       if((i%3)==2)
       {
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,-(baa+gapWidth),fp);
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,baa+gapWidth,fp);  
       }
       else
       {
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,0,fp);  
       }
    }
   

    vw+=0x20;
    ph+=phi;
  }
  if(displayStyle==STYLE_1981)  // in this style the hands are white
  {
     graphics_context_set_fill_color(ctx,GColorFromHEX(0xffffff));
     graphics_context_set_stroke_color(ctx,GColorFromHEX(0xffffff));
  }
  int32_t ang=TRIG_MAX_ANGLE*currentSecond/60;
  if(showSeconds) 
  {
    hand(ctx,ang,shThickness,shThickness,70<<fp,10<<fp,0,fp);  
    if(displayStyle==STYLE_1970)  // this old style has a stubby stalk opposite the main second hand
    {
      hand(ctx,ang+0x8000,shThickness,shThickness,25<<fp,10<<fp,0,fp);  
    }
  }
  ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
  hand(ctx,ang,mhThickness,mhThickness,70<<fp,10<<fp,0,fp); 
  ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
  hand(ctx,ang,hhThickness,hhThickness,(48-shortenHH)<<fp,10<<fp,0,fp);
 
  graphics_context_set_fill_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?bgCol:fillCol));
  graphics_context_set_stroke_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?bgCol:fillCol));
  graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),15);
  graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),15);
  
  // I'll draw the inner dot in red if the battery is below or equal to 30%
  
  if(batteryLevel>30)  // normal colours
  {
    graphics_context_set_fill_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?fillCol:bgCol));
    graphics_context_set_stroke_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?fillCol:bgCol));
  }
  else  // red dot
  {
    graphics_context_set_fill_color(ctx,GColorFromHEX(0xff0000));
    graphics_context_set_stroke_color(ctx,GColorFromHEX(0xff0000));
  }
  graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),innerDotRadius);
  graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),innerDotRadius); 
  
  // the watchface is drawn, if BT is disconnected add some 'interference'
  
  if(!btconnected)  // bluetooth disabled
  {
    GBitmap *b=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
    uint8_t *bits=gbitmap_get_data(b);
    for(i=0;i<16;i++)  // count of no. of lines of interference to do
    {
      uint8_t *lp=&bits[(rand()%YREZ)*XREZ];
      for(int j=0;j<XREZ;j++)
      {
        uint8_t p=rand()&3;
        p=p|(p<<2)|(p<<4)|(p<<6);
        *lp++=p;
      }
    }
    graphics_release_frame_buffer(ctx,b); 
  }
  
}



/****************************** Window Lifecycle *****************************/


static void window_load(Window *window)
{
	//Setup window
	window_set_background_color(window,GColorFromHEX(0x000055));
	
	//Setup canvas
	canvas = layer_create(GRect(0, 0, 144, 168));
	layer_set_update_proc(canvas, (LayerUpdateProc) render);
	layer_add_child(window_get_root_layer(window), canvas);
	
  //subscribe to battery & BT services
  battery_state_service_subscribe(handle_battery);
  bluetooth_connection_service_subscribe(handle_bluetooth);
  
  // load persistent state
  showSeconds=persist_read_bool(KEY_SECONDS);
  displayStyle=persist_read_int(KEY_ERA);
  
	//Start rendering
	start();
}

static void window_unload(Window *window) 
{
	//Cancel timer
	app_timer_cancel(timer);
  // Unsubscribe services
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
	//Destroy canvas
	layer_destroy(canvas);
}

/****************************** App Lifecycle *****************************/


static void init(void) {
	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) 
	{
		.load = window_load,
		.unload = window_unload,
	});
	
	//Prepare everything
	makeCrap();
  // register to receive messages from external
  app_message_register_inbox_received(in_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());    //Largest possible input and output buffer sizes
	//Finally
	window_stack_push(window, true);
}

static void deinit(void) 
{
	//De-init everything
	destroyCrap();
	//Finally
	window_destroy(window);
}

int main(void) 
{
	init();
	app_event_loop();
	deinit();
}
