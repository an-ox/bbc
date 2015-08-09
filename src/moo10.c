#include <pebble.h>

// watchface based on BBC clocks

// keys for settings  
#define KEY_SECONDS 0  
#define KEY_ERA 1
#define KEY_XCOL 2
  
 // styles
#define STYLE_1970 0
#define STYLE_1978 1
#define STYLE_1981 2
#define STYLE_1953 3
#define STYLE_1963 4
#define STYLE_1964 5
#define STYLE_1966 6
#define STYLE_1968 7
#define STYLE_1991 8
  
 // for test mode
  
//#define TEST
  
#define XREZ 144
#define YREZ 168 
#define CENTREX 72
#define CENTREY 84
#define FRACBITS 8
  
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
bool exCol=true;
int displayStyle=STYLE_1978;
bool firstBoot=true;
uint32_t testcol=0xffffffff;


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
#ifdef TEST
    uint32_t tim=1000;
#else
    uint32_t tim=(showSeconds)?1000:60000;
#endif
  if(firstBoot) 
   {
   tim=100;
   firstBoot=false;
 }
    timer=app_timer_register(tim,(AppTimerCallback) timer_callback,0);

  

}


// Crap to handle stuff being sent from the phone

static void process_tuple(Tuple *t)
{
  // Act upon individual tuples
  switch(t->key)
  {
   case KEY_SECONDS:  // turn second hand off and on
    //if(strcmp(t->value->cstring,"on")==0)
    if(t->value->int32>0) 
    {
      showSeconds=true;
      persist_write_bool(KEY_SECONDS,true);
    }
    else 
    {
      showSeconds=false;
      persist_write_bool(KEY_SECONDS,false);
    }
    app_timer_reschedule(timer,100);
    break;
 case KEY_XCOL:  // turn second hand off and on
    //if(strcmp(t->value->cstring,"on")==0)
    if(t->value->int32>0) 
    {
      exCol=true;
      persist_write_bool(KEY_XCOL,true);
    }
    else 
    {
      exCol=false;
      persist_write_bool(KEY_XCOL,false);
    }
    app_timer_reschedule(timer,100);
    break;    
    case KEY_ERA:  // set style according to approximate BBC era
     if(strcmp(t->value->cstring,"0")==0) displayStyle=STYLE_1970;
     else if(strcmp(t->value->cstring,"1")==0) displayStyle=STYLE_1978;
     else if(strcmp(t->value->cstring,"2")==0) displayStyle=STYLE_1981;
     else if(strcmp(t->value->cstring,"3")==0) displayStyle=STYLE_1953;
     else if(strcmp(t->value->cstring,"4")==0) displayStyle=STYLE_1963;
     else if(strcmp(t->value->cstring,"5")==0) displayStyle=STYLE_1964;
     else if(strcmp(t->value->cstring,"6")==0) displayStyle=STYLE_1966;
     else if(strcmp(t->value->cstring,"7")==0) displayStyle=STYLE_1968;
    else if(strcmp(t->value->cstring,"8")==0) displayStyle=STYLE_1991;
     persist_write_int(KEY_ERA,displayStyle);
     app_timer_reschedule(timer,100);
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
  timer=app_timer_register(10,(AppTimerCallback) timer_callback,0);
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

static void shape(GContext *ctx,uint16_t ang,int16_t ox,int16_t oy,int16_t mx,int16_t my,GPathInfo *gp,int func)
{
  // draw 'shape' rotated to 'ang' and translated by 'ox' and 'oy'
  // use this instead of the library functions as they have shit precision
  
  GPoint *oldpoints=gp->points;
  gp->points=malloc(sizeof(GPoint)*gp->num_points);
  for(uint16_t i=0;i<gp->num_points;i++)
  {
    gp->points[i].x=((oldpoints[i].x*mx)>>FRACBITS)+ox;
    gp->points[i].y=((oldpoints[i].y*my)>>FRACBITS)+oy;
    rotate(ang,&gp->points[i]);
    gp->points[i].x=(gp->points[i].x>>FRACBITS)+CENTREX;
    gp->points[i].y=(gp->points[i].y>>FRACBITS)+CENTREY;
  }
  GPath *pptr=gpath_create(gp);
  switch(func)
  {
  case 0:  // std filled in  
    gpath_draw_outline(ctx,pptr);
    gpath_draw_filled(ctx,pptr);
  break;
  case 1: // open line draw
#ifdef PBL_COLOR
    gpath_draw_outline_open(ctx,pptr);
#else
    gpath_draw_outline(ctx,pptr);
#endif
  break;
  }
  gpath_destroy(pptr);
  free(gp->points);
  gp->points=oldpoints;
}

static void hand(GContext *ctx,uint16_t ang,int16_t w1, int16_t w2,int16_t outerr,uint16_t innerr,int16_t xoff)
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
  gp.points[0].x=a.x>>FRACBITS;
  gp.points[0].y=a.y>>FRACBITS;
  gp.points[1].x=b.x>>FRACBITS;
  gp.points[1].y=b.y>>FRACBITS;
  gp.points[2].x=c.x>>FRACBITS;
  gp.points[2].y=c.y>>FRACBITS;
  gp.points[3].x=d.x>>FRACBITS;
  gp.points[3].y=d.y>>FRACBITS;   
  pptr=gpath_create(&gp);
  gpath_move_to(pptr, GPoint(CENTREX,CENTREY));
  gpath_draw_outline(ctx,pptr);
  gpath_draw_filled(ctx,pptr);
  gpath_destroy(pptr);
  
}

static void faceElement(GContext *ctx,uint16_t ang,int16_t rad,int16_t s1,int16_t s2,uint8_t type)
{
  GPoint a;
  a.x=0;
  a.y=rad;
  rotate(ang,&a);
  switch(type)
  {
   case 0:  // a dot
    graphics_draw_circle(ctx,GPoint((a.x>>FRACBITS)+CENTREX,(a.y>>FRACBITS)+CENTREY),s1);
    graphics_fill_circle(ctx,GPoint((a.x>>FRACBITS)+CENTREX,(a.y>>FRACBITS)+CENTREY),s1);
  break;
  }
}

void set_stroke_width(GContext *ctx,uint8_t w)
  {
  #ifdef PBL_COLOR
    graphics_context_set_stroke_width(ctx,w);
    #endif
}

uint32_t getcol4(int col)
{
#ifndef PBL_COLOR
  return 0xffffffff;
#else
  uint32_t res=GColorFromHEX(col).argb;
  return res|(res<<8)|(res<<16)|(res<<24);

#endif
}

static void paint(GContext *ctx,uint8_t ystart,uint8_t ysize,int fill)
{
#ifdef PBL_COLOR
  if(ystart>=YREZ) return;
  
   fill=getcol4(fill); 
  // quickly paint a contiguous block of lines with a fill value   
    GBitmap *b=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
    uint8_t *bits=gbitmap_get_data(b);
    uint32_t *lp=(uint32_t *)&bits[ystart*XREZ];
    uint16_t cnt=(XREZ>>2)*ysize;
    for(int j=0;j<cnt;j++) *lp++&=fill;
    graphics_release_frame_buffer(ctx,b); 
#endif
 
}

static void bband(GContext *ctx,int sch)
{
 // Paint background with a "battery band", sch denoting the colour scheme.
  // Let the "band" be 8 colours, in order empty... full.
  
  uint32_t bband[4][8]=
  {
    {0xff0000,0xff0055,0xff00aa,0xaa00ff,0x5500aa,0x005555,0x00aa00,0x00ff00},
    {0x0000aa,0x0000ff,0x0055ff,0x00aaff,0x00ffff,0xaaff55,0xffff00,0xaa5500},
    {0x0000aa,0x0000ff,0x5500ff,0xaa00ff,0xff00ff,0xaa00ff,0x5500ff,0x0055ff},
    {0x000055,0x0000aa,0x0000ff,0x5500ff,0x0000ff,0x0000aa,0x0000aa,0x000055}
  };
  
  // Range over which it is going to move goes from 0 (Full) to (YRez-2) (Empty).
  
  uint16_t startlin=((YREZ-2)*batteryLevel)/100;
  startlin=(YREZ-2)-startlin;
  
  // fill to just before startlin using empty colour
  paint(ctx,0,startlin,bband[sch][0]);
  // fill in the band in 2 pix high lines
  for(uint8_t i=0;i<6;i++)
  {
    paint(ctx,startlin,2,bband[sch][i+1]);
    startlin+=2;  
  }
  // paint the rest in full colour
  if(startlin<YREZ)
    {
    paint(ctx,startlin,YREZ-startlin,bband[sch][7]);
  }
  
}


 void set_fill_color(GContext *ctx,int c)
  {
  #ifdef PBL_COLOR
   graphics_context_set_fill_color(ctx,GColorFromHEX(c));
  #else
    if(c!=0)
      graphics_context_set_fill_color(ctx,GColorWhite); 
   else 
      graphics_context_set_fill_color(ctx,GColorBlack); 
  #endif
}

void set_stroke_color(GContext *ctx,int c)
  {
  #ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx,GColorFromHEX(c));
  #else
    if(c!=0)
      graphics_context_set_stroke_color(ctx,GColorWhite); 
    else
      graphics_context_set_stroke_color(ctx,GColorBlack); 
  #endif
}
  
void set_background_color(Window *window,int bgCol)
{
  #ifdef PBL_COLOR
    window_set_background_color(window,GColorFromHEX(bgCol));
  #else
    window_set_background_color(window,GColorBlack); 
  #endif
}


void setColour(GContext *ctx,int c)
  {
  #ifdef PBL_COLOR
    graphics_context_set_stroke_color(ctx,GColorFromHEX(c));
    graphics_context_set_fill_color(ctx,GColorFromHEX(c));
  #else
    if(c!=0)
      {
     graphics_context_set_stroke_color(ctx,GColorWhite); 
      graphics_context_set_fill_color(ctx,GColorWhite); 
  }
  else
  { 
    graphics_context_set_stroke_color(ctx,GColorBlack); 
  }
  #endif
}
  
static void batwings(GContext *ctx)
{
  GPathInfo gp;
  #define ARCPOINTS 20
  gp.num_points=ARCPOINTS*2;
  gp.points=malloc(sizeof(GPoint)*ARCPOINTS*2);
  uint16_t ang=0x7000;
  uint16_t ia=0x6000/ARCPOINTS;
  int16_t ss=-1;
  uint16_t arcwidth=0;
  uint8_t breakPoint=6;
  uint16_t arcinc1=0x3000/(ARCPOINTS-breakPoint);
  uint16_t arcinc2=0x3000/breakPoint;
  
  for(uint8_t i=0;i<ARCPOINTS;i++)
  {
    if(i<=ARCPOINTS-breakPoint)
    {
     arcwidth+=arcinc1; 
    }
    else
    {
     arcwidth-=arcinc2; 
    }
    uint16_t uw=arcwidth>>8;
    gp.points[i].x=0;
    gp.points[i].y=65<<FRACBITS;
    rotate(ang,&gp.points[i]);
    gp.points[ARCPOINTS+(ARCPOINTS-i-1)].x=gp.points[i].x-(gp.points[i].x>>9)*ss*uw;
    gp.points[ARCPOINTS+(ARCPOINTS-i-1)].y=gp.points[i].y-(gp.points[i].y>>8)*ss*uw;
      ang-=ia;
    // kinda fudge the correct angle on the lightning bolt thingy
    if(i==ARCPOINTS-breakPoint)
    {
      ss=-ss;
      gp.points[ARCPOINTS+(ARCPOINTS-i-1)].x-=5<<FRACBITS;
      gp.points[ARCPOINTS+(ARCPOINTS-i-1)].y-=6<<FRACBITS;
    }
     if(i==ARCPOINTS-(breakPoint-1))
    {
      gp.points[ARCPOINTS+(ARCPOINTS-i-1)].x+=5<<FRACBITS;
      gp.points[ARCPOINTS+(ARCPOINTS-i-1)].y+=6<<FRACBITS;
    } 
  }
    shape(ctx,0,0,0,0x100,0x108,&gp,0);
    shape(ctx,0,0,0,-0x100,0x108,&gp,0);
    gp.num_points=ARCPOINTS;
  #ifdef PBL_COLOR  
  graphics_context_set_stroke_width(ctx,1);
 #endif
    shape(ctx,0,-23<<FRACBITS,-30<<FRACBITS,-0x40,0x80,&gp,1); 
    shape(ctx,0,-23<<FRACBITS,30<<FRACBITS,-0x40,0x80,&gp,1); 
    shape(ctx,0,23<<FRACBITS,-30<<FRACBITS,0x40,0x80,&gp,1); 
    shape(ctx,0,23<<FRACBITS,30<<FRACBITS,0x40,0x80,&gp,1);   
    set_stroke_width(ctx,2);
  if(exCol) 
  {
    bband(ctx,0);
    set_stroke_color(ctx,0x00aaff);
  }
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),32);
    set_stroke_width(ctx,1);
    free(gp.points);
}



static void block(GContext *ctx,uint8_t ystart,uint8_t ysize,int fill)
{
#ifdef PBL_COLOR
  // quickly fill a contiguous block of lines with a fill value   
    GBitmap *b=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
    uint8_t *bits=gbitmap_get_data(b);
    uint32_t *lp=(uint32_t *)&bits[ystart*XREZ];
    uint16_t cnt=(XREZ>>2)*ysize;
    for(int j=0;j<cnt;j++) *lp++=fill;
    graphics_release_frame_buffer(ctx,b); 
#endif
 
}



static void render(Layer *layer, GContext* ctx) 
{
  int i;
  static uint16_t lastSecond;
#ifdef TEST
  displayStyle=STYLE_1991;
  showSeconds=true;
  exCol=true;
#endif
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



  GPathInfo gp={3,(GPoint[])
    {
      {0,0},
      {0,0},
      {0,0}
    }
  }; 
  
  // default sizes for the hands
  uint16_t hhThickness=4<<FRACBITS;
  uint16_t mhThickness=3<<FRACBITS;
  uint16_t shThickness=3<<(FRACBITS-1);
  uint8_t innerDotRadius=10;
  int16_t outerRadius=70<<FRACBITS;
  int16_t innerRadius=50<<FRACBITS;
  uint8_t shortenHH=0;
    uint16_t shLength=55<<FRACBITS;
uint16_t shOrigin=3;
    
  // set colours according to era
  // default to blue/orange

  
 int bgCol=0x000055;
  int fillCol=0xffaa00;
  int innerDotCol=bgCol;  
  
  switch(displayStyle)
    {
     case STYLE_1970:
      bgCol=0x000000;
      fillCol=0xffffff;
      innerDotCol=bgCol;  
      set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);   
    if(exCol)
      {
      block(ctx,0,YREZ,0xffffffff);
      bband(ctx,3);
       innerDotCol=0xffff00;
    }
     break;
    case STYLE_1978:
      set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);  
 if(exCol)
      {
      block(ctx,0,YREZ,0xffffffff);
      bband(ctx,3);
       innerDotCol=0xffff00;
    }    
    break;
     case STYLE_1981:
      bgCol=0x000055;
      fillCol=0xaaff00;
      shThickness=1<<FRACBITS;
      mhThickness=shThickness;
      hhThickness=3<<(FRACBITS-1);
      innerDotRadius=7;
      shortenHH=5;
      set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);
    innerDotCol=fillCol;
    if(exCol)
      {
      block(ctx,0,YREZ,0xffffffff);
      bband(ctx,3);
       innerDotCol=0x00aaff;
    }    
     break;
    case STYLE_1953:
      bgCol=0x000000;
      fillCol=0xffffff;
      outerRadius=32<<FRACBITS;
      innerRadius=27<<FRACBITS;
      innerDotRadius=2;
      set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);
      shThickness=1<<FRACBITS;
     hhThickness=3<<(FRACBITS-1);
     set_background_color(window,bgCol);
     batwings(ctx);  
     innerDotCol=fillCol;
   break;
  case STYLE_1963:
     bgCol=0x000000;
      fillCol=0xffffff;
      outerRadius=45<<FRACBITS;
      innerRadius=38<<FRACBITS;
      innerDotRadius=8;
      set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);   
      shThickness=mhThickness=hhThickness=1<<(FRACBITS-1);
      set_background_color(window,bgCol);  
    if(exCol)
      innerDotCol=0xffaa00;
    else
      innerDotCol=fillCol;
    shLength=70<<FRACBITS;
    set_stroke_width(ctx,2);
    if(exCol) setColour(ctx,0xffffff);
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),70);
     if(exCol) 
     {
       bband(ctx,0);
      set_fill_color(ctx,0x5500aa);
     }
    else
    {  
      set_fill_color(ctx,0x555555);
    }
    #ifdef PBL_COLOR
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),46);
    #endif
     if(exCol) setColour(ctx,0xff00aa);
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),46);
  
    set_fill_color(ctx,fillCol);
    set_stroke_width(ctx,1);
    shortenHH=0;
    
    break;
   case STYLE_1964:
     bgCol=0xffffffff;
      fillCol=0xffffff;
      outerRadius=50<<FRACBITS;
      innerRadius=43<<FRACBITS;
      innerDotRadius=4;
     shLength=55<<FRACBITS;

      shThickness=mhThickness=hhThickness=1<<(FRACBITS-1);
     set_background_color(window,bgCol);  
 innerDotCol=fillCol;
    set_stroke_width(ctx,8);
    set_stroke_color(ctx,0x555555);
 
     if(exCol) 
     {
       setColour(ctx,0xff00aa);
       bband(ctx,1);
       innerDotCol=0xffaa00;
     }
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),60);
    set_fill_color(ctx,0);
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),50);
    set_fill_color(ctx,fillCol);
    set_stroke_color(ctx,fillCol);    
    set_stroke_width(ctx,1);
    shortenHH=-15;
    break;   

   case STYLE_1966:
     bgCol=0;
      fillCol=0xffffff;
      outerRadius=55<<FRACBITS;
      innerRadius=51<<FRACBITS;
      innerDotRadius=9;
     shLength=55<<FRACBITS;

      shThickness=mhThickness=hhThickness=1<<(FRACBITS-1);
     set_background_color(window,bgCol);  
     innerDotCol=0;
    block(ctx,(YREZ>>1)-60,120,0xffffffff);

    set_stroke_width(ctx,8);
    set_stroke_color(ctx,0);
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),65);
   
    set_fill_color(ctx,0);
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),55);
    set_fill_color(ctx,fillCol);
     graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),12);
      if(exCol)
      {
       bband(ctx,0);
    }  
    set_stroke_color(ctx,fillCol);    
    set_stroke_width(ctx,1);
    shortenHH=-15;
    break;   
    case STYLE_1968:
     bgCol=0;
      fillCol=0xffffff;
      outerRadius=55<<FRACBITS;
      innerRadius=45<<FRACBITS;
      innerDotRadius=0;
     shLength=55<<FRACBITS;

      shThickness=mhThickness=hhThickness=1<<(FRACBITS-1);
     set_background_color(window,bgCol);  
     innerDotCol=0;
    block(ctx,(YREZ>>1)-45,30,0xaaaaaaaa);
    block(ctx,(YREZ>>1)-15,30,0xffffffff);
    block(ctx,(YREZ>>1)+15,30,0xaaaaaaaa);
   
    set_fill_color(ctx,0);
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),60);
    set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);
    set_stroke_width(ctx,3);

    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),55);
    set_stroke_width(ctx,1);
     graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),9);
    
     if(exCol)
      {
       bband(ctx,2);
    }    
    //graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
    //graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));    
    set_stroke_width(ctx,1);
    shortenHH=-10;
    shOrigin=10;
    break;      
  case STYLE_1991:
     bgCol=0;
      fillCol=0xffffff;
      outerRadius=65<<FRACBITS;
      innerRadius=50<<FRACBITS;
      innerDotRadius=4;
     shLength=67<<FRACBITS;

      mhThickness=hhThickness=1<<(FRACBITS-0);
      shThickness=1<<(FRACBITS-1); 
    set_background_color(window,bgCol);  
     innerDotCol=0;
    set_fill_color(ctx,fillCol);
      set_stroke_color(ctx,fillCol);
    set_stroke_width(ctx,3);

    //set_stroke_width(ctx,1);
  if(exCol)
      {
      block(ctx,0,YREZ,0xffffffff);
      bband(ctx,3);
       innerDotCol=0x00ff00;
      setColour(ctx,0xff5500);
    }
      
     graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),9);
    set_stroke_width(ctx,1);
    shortenHH=25;
    shOrigin=-10;
    
    break;          
  }  
  
  set_background_color(window,bgCol);

  // draw the hour pips for current style
  

  const int16_t gapWidth=0x180;
 
  uint16_t phi=0x8000/6;
  uint16_t ph=phi;
  
  int vw=3<<(FRACBITS-1);
  const int baa=2<<FRACBITS;  // bar size for 1981 era
  const uint32_t prands[6]={0xff0000,0xff0000,0xff5500,0xffaa00,0xffff00,0xffff55};
  const uint32_t prands2[6]={0xff0000,0xff8000,0xffff00,0x00ff00,0x00ffff,0x8000ff};
  for(i=0;i<12;i++)
  {
    switch(displayStyle)
    {
      // draw the hour pips according to the era
      case STYLE_1991:  // alternate bars and circles
      if(i&1)
        {
         if(exCol) setColour(ctx,0xff5500);
           hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
      }
      else
        {
        if(exCol) setColour(ctx,0x00ff00);
          faceElement(ctx,ph,60<<FRACBITS,2,0,0);
      }
      break;
      case STYLE_1970: // use the gradually fattening hour pips
      if(exCol) setColour(ctx,prands[i>>1]);
      hand(ctx,ph,vw,vw,outerRadius,innerRadius,-(vw+gapWidth));
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,vw+gapWidth);
      break;      
      case STYLE_1978:
       if(exCol) setColour(ctx,prands2[i>>1]);
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,-(vw+gapWidth));
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,vw+gapWidth);
      break;
      case STYLE_1981: // use single bars except for 12,3,6 and 9 which are double
       if((i%3)==2)
       {
          if(exCol) setColour(ctx,0x00ffff);
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,-(baa+gapWidth));
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,baa+gapWidth);  
       }
       else
       {
          if(exCol) setColour(ctx,0xff00ff);
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
       }
      break;
      case STYLE_1953:
      if(i==11)
      {
        if(exCol) setColour(ctx,0x00ffff);
        hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius-(5<<FRACBITS),0);
      }
      else
      {
        if(exCol) setColour(ctx,0xaaffff);
         hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius,0);
      }
      break;
      case STYLE_1963:  // 4 ordinal bars are slightly thicker
      if((i%3)==2)
       {
         if(exCol) setColour(ctx,0xaaffff);
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
       }
       else
       {
         if(exCol) setColour(ctx,0x00ffff);
        hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius,0);  
       }
      break;
      case STYLE_1964:  // evenly spaced thick pips
        if(exCol) setColour(ctx,0x00ffff);
      hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
      break;
      case STYLE_1966:  // ordinal bars very slightly longer
      set_stroke_width(ctx,2);
      if((i%3)==2)
        {
        if(exCol) setColour(ctx,0x00ffff);
        hand(ctx,ph,baa>0,baa>0,outerRadius,innerRadius-(5<<FRACBITS),0);  
      }
      else
      {  
        if(exCol) setColour(ctx,0x0055ff);
        hand(ctx,ph,baa>0,baa>0,outerRadius,innerRadius,0);  
      }
      break;
       case STYLE_1968:  // evenly spaced thin pips
       if(exCol) setColour(ctx,0xff55ff);
      hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius,0);  
      break;     
    }
   

    vw+=0x20;
    ph+=phi;
  }
  
  // drawing of hands and outer dots can depend on style 
  int32_t ang=TRIG_MAX_ANGLE*currentSecond/60;
  switch(displayStyle)
  {
    case STYLE_1991:
    if(showSeconds)
    {
      set_stroke_width(ctx,1);
      hand(ctx,ang,shThickness,shThickness,shLength,shOrigin<<FRACBITS,0);
    }
     set_stroke_width(ctx,2);
    
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
      hand(ctx,ang,mhThickness,mhThickness,outerRadius,0,0);
     if(exCol)
          setColour(ctx,0x00ff00);
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
      hand(ctx,ang,hhThickness,hhThickness,outerRadius-(shortenHH<<FRACBITS),0,0);    
    break;
    case STYLE_1963:  // all styles with no extra blobs in the middle
    case STYLE_1964:
      case STYLE_1966:
    case STYLE_1968:
      // here there is a comedy long second hand too but it has a square tip
    
    if(showSeconds)
    {
      if(exCol)
          setColour(ctx,0xff00aa);
      set_stroke_width(ctx,1+(shortenHH==0));
      hand(ctx,ang,shThickness,shThickness,shLength,(shOrigin+shortenHH)<<FRACBITS,0);
    }
     set_stroke_width(ctx,2);
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
          if(exCol)
        setColour(ctx,0xffff00);
      hand(ctx,ang,mhThickness,mhThickness,outerRadius,shOrigin<<FRACBITS,0);
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
         if(exCol)
        setColour(ctx,0xffff00);
      hand(ctx,ang,hhThickness,hhThickness,outerRadius-(15<<FRACBITS),shOrigin<<FRACBITS,0);    
    break;
    case STYLE_1953:  // this one has weird pointy hands and odd ring arrangements
      // I'll set the weird long pointy second hand up by hand in the gpinfo and use SHAPE
       if(showSeconds) 
       {
        gp.points[0].x=0;
        gp.points[0].y=70<<FRACBITS;
        gp.points[1].x=-2<<FRACBITS;
        gp.points[1].y=-16<<FRACBITS;
        gp.points[2].x=2<<FRACBITS;
        gp.points[2].y=-16<<FRACBITS;
        if(exCol)
          setColour(ctx,0xff00aa);
        shape(ctx,ang,0,0,0x100,0x100,&gp,0); 
       }
      // then a ring of background colour
      set_fill_color(ctx,bgCol);
      graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),8);
      // now the minute hand just using the old hand() proc
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
      if(exCol)
        setColour(ctx,0xffaa00);
      else
        set_fill_color(ctx,fillCol);
      hand(ctx,ang,1<<(FRACBITS-0),mhThickness,28<<FRACBITS,3<<FRACBITS,0);
      // and now the hour 
     if(exCol)
        setColour(ctx,0xffff00);
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
      hand(ctx,ang,3<<(FRACBITS-1),hhThickness,22<<FRACBITS,3<<FRACBITS,0);    
      // then a ring of foreground colour
     set_stroke_width(ctx,2);
     if(exCol)
        setColour(ctx,0xff00aa);
    else
      set_fill_color(ctx,fillCol);
      graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),9);    
    break;  
    
    default:  // for the main non weird styles
    if(exCol) setColour(ctx,0xffffaa);
    if(displayStyle==STYLE_1981)  // in this style the hands are white
      {
      if(exCol)
        {
          setColour(ctx,0xffff00);
      }
      else
        {
         set_fill_color(ctx,0xffffff);
      set_stroke_color(ctx,0xffffff);
      }
      }
   
      if(showSeconds) 
      {
        hand(ctx,ang,shThickness,shThickness,70<<FRACBITS,10<<FRACBITS,0);  
        if(displayStyle==STYLE_1970)  // this old style has a stubby stalk opposite the main second hand
        {
          hand(ctx,ang+0x8000,shThickness,shThickness,25<<FRACBITS,10<<FRACBITS,0);  
        }
      }
 
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
      hand(ctx,ang,mhThickness,mhThickness,70<<FRACBITS,10<<FRACBITS,0); 
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
      hand(ctx,ang,hhThickness,hhThickness,(48-shortenHH)<<FRACBITS,10<<FRACBITS,0);
     
      set_fill_color(ctx,(displayStyle==STYLE_1981)?bgCol:fillCol);
      set_stroke_color(ctx,(displayStyle==STYLE_1981)?bgCol:fillCol);
      graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),15);
      graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),15);
    break;

  }
  
  
 
  
  // I'll draw the inner dot in red if the battery is below or equal to 30%
    uint8_t dr=innerDotRadius;
  
  if(batteryLevel>30||exCol)  // normal colours
  {
    set_fill_color(ctx,innerDotCol);
    set_stroke_color(ctx,innerDotCol);
  }
  else  // red dot
  {
      if(dr==0) dr=10;  // batt low always shows a nonzero doe
    set_fill_color(ctx,0xff0000);
    set_stroke_color(ctx,0xff0000);
  }
  graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),dr);
  graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),dr); 
  
  // debug test, enable to easily see frame rate
  //block(ctx,0,30,testcol);
  //testcol^=0xffffff;
  
  // removing the interference on BT disconnect, no bugger seems to like it
  
  /*
  
  // the watchface is drawn, if BT is disconnected add some 'interference'
  #ifdef PBL_COLOR
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
   #endif
   */
}



/****************************** Window Lifecycle *****************************/


static void window_load(Window *window)
{
	//Setup window
	set_background_color(window,0x000055);
	
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
  exCol=persist_read_bool(KEY_XCOL);
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
