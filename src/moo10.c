#include <pebble.h>

// watchface based on BBC clocks

// keys for settings  
#define KEY_SECONDS 0  
#define KEY_ERA 1
  
 // styles
#define STYLE_1970 0
#define STYLE_1978 1
#define STYLE_1981 2
#define STYLE_1953 3
#define STYLE_1963 4
#define STYLE_1964 5
#define STYLE_1966 6
#define STYLE_1968 7
  
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
  uint32_t tim=(showSeconds)?1000:60000;
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
    gpath_draw_outline_open(ctx,pptr);
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
    graphics_context_set_stroke_width(ctx,1);
 
    shape(ctx,0,-23<<FRACBITS,-30<<FRACBITS,-0x40,0x80,&gp,1); 
    shape(ctx,0,-23<<FRACBITS,30<<FRACBITS,-0x40,0x80,&gp,1); 
    shape(ctx,0,23<<FRACBITS,-30<<FRACBITS,0x40,0x80,&gp,1); 
    shape(ctx,0,23<<FRACBITS,30<<FRACBITS,0x40,0x80,&gp,1);   
  
    graphics_context_set_stroke_width(ctx,2);
   
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),32);
    graphics_context_set_stroke_width(ctx,1);
    free(gp.points);
}

static void block(GContext *ctx,uint8_t ystart,uint8_t ysize,int fill)
{
  // quickly fill a contiguous block of lines with a fill value   
    GBitmap *b=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
    uint8_t *bits=gbitmap_get_data(b);
    uint32_t *lp=(uint32_t *)&bits[ystart*XREZ];
    uint16_t cnt=(XREZ>>2)*ysize;
    for(int j=0;j<cnt;j++) *lp++=fill;
    graphics_release_frame_buffer(ctx,b); 
 
}

static void render(Layer *layer, GContext* ctx) 
{
  int i;
  static uint16_t lastSecond;
  
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
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));   
     break;
    case STYLE_1978:
 graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));   
    break;
     case STYLE_1981:
      bgCol=0x000055;
      fillCol=0xaaff00;
      shThickness=1<<FRACBITS;
      mhThickness=shThickness;
      hhThickness=3<<(FRACBITS-1);
      innerDotRadius=7;
      shortenHH=5;
      graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));
    innerDotCol=fillCol;
     break;
    case STYLE_1953:
      bgCol=0x000000;
      fillCol=0xffffff;
      outerRadius=32<<FRACBITS;
      innerRadius=27<<FRACBITS;
      innerDotRadius=2;
      graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));   
      shThickness=1<<FRACBITS;
     hhThickness=3<<(FRACBITS-1);
     window_set_background_color(window,GColorFromHEX(bgCol));
     batwings(ctx);  
     innerDotCol=fillCol;
   break;
  case STYLE_1963:
     bgCol=0x000000;
      fillCol=0xffffff;
      outerRadius=45<<FRACBITS;
      innerRadius=38<<FRACBITS;
      innerDotRadius=8;
      graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));   
      shThickness=mhThickness=hhThickness=1<<(FRACBITS-1);
     window_set_background_color(window,GColorFromHEX(bgCol));  
 innerDotCol=fillCol;
    shLength=70<<FRACBITS;
    graphics_context_set_stroke_width(ctx,2);
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),70);
    graphics_context_set_fill_color(ctx,GColorFromHEX(0x555555));
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),46);
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),46);
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
        graphics_context_set_stroke_width(ctx,1);
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
     window_set_background_color(window,GColorFromHEX(bgCol));  
 innerDotCol=fillCol;
    graphics_context_set_stroke_width(ctx,8);
    graphics_context_set_stroke_color(ctx,GColorFromHEX(0x555555));
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),60);
    graphics_context_set_fill_color(ctx,GColorFromHEX(0));
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),50);
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
    graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));    
        graphics_context_set_stroke_width(ctx,1);
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
     window_set_background_color(window,GColorFromHEX(bgCol));  
     innerDotCol=0;
    block(ctx,(YREZ>>1)-60,120,0xffffffff);
    graphics_context_set_stroke_width(ctx,8);
    graphics_context_set_stroke_color(ctx,GColorFromHEX(0));
    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),65);
   
    graphics_context_set_fill_color(ctx,GColorFromHEX(0));
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),55);
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
     graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),12);
    graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));    
        graphics_context_set_stroke_width(ctx,1);
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
     window_set_background_color(window,GColorFromHEX(bgCol));  
     innerDotCol=0;
    block(ctx,(YREZ>>1)-45,30,0xaaaaaaaa);
    block(ctx,(YREZ>>1)-15,30,0xffffffff);
    block(ctx,(YREZ>>1)+15,30,0xaaaaaaaa);
   
    graphics_context_set_fill_color(ctx,GColorFromHEX(0));
    graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),60);
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
     graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));
    graphics_context_set_stroke_width(ctx,3);

    graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),55);
    graphics_context_set_stroke_width(ctx,1);
     graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),9);
    graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
    graphics_context_set_stroke_color(ctx,GColorFromHEX(fillCol));    
    graphics_context_set_stroke_width(ctx,1);
    shortenHH=-10;
    shOrigin=10;
    break;      
    
  }  
  
  window_set_background_color(window,GColorFromHEX(bgCol));

  // draw the hour pips for current style
  

  const int16_t gapWidth=0x180;
 
  uint16_t phi=0x8000/6;
  uint16_t ph=phi;
  
  int vw=3<<(FRACBITS-1);
  const int baa=2<<FRACBITS;  // bar size for 1981 era
  for(i=0;i<12;i++)
  {
    switch(displayStyle)
    {
      // draw the hour pips according to the era
      case STYLE_1970: // use the gradually fattening hour pips
      case STYLE_1978:
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,-(vw+gapWidth));
       hand(ctx,ph,vw,vw,outerRadius,innerRadius,vw+gapWidth);
      break;
      case STYLE_1981: // use single bars except for 12,3,6 and 9 which are double
       if((i%3)==2)
       {
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,-(baa+gapWidth));
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,baa+gapWidth);  
       }
       else
       {
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
       }
      break;
      case STYLE_1953:
      if(i==11)
        hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius-(5<<FRACBITS),0);
      else
        hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius,0);
      break;
      case STYLE_1963:  // 4 ordinal bars are slightly thicker
      if((i%3)==2)
       {
        hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
       }
       else
       {
        hand(ctx,ph,baa>>1,baa>>1,outerRadius,innerRadius,0);  
       }
      break;
      case STYLE_1964:  // evenly spaced thick pips
      hand(ctx,ph,baa,baa,outerRadius,innerRadius,0);  
      break;
      case STYLE_1966:  // ordinal bars very slightly longer
      graphics_context_set_stroke_width(ctx,2);
      if((i%3)==2)
        hand(ctx,ph,baa>0,baa>0,outerRadius,innerRadius-(5<<FRACBITS),0);  
      else
        hand(ctx,ph,baa>0,baa>0,outerRadius,innerRadius,0);  
      break;
       case STYLE_1968:  // evenly spaced thin pips
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
    case STYLE_1963:
    case STYLE_1964:
      case STYLE_1966:
    case STYLE_1968:
      // here there is a comedy long second hand too but it has a square tip
    
    if(showSeconds)
    {
      graphics_context_set_stroke_width(ctx,1+(shortenHH==0));
      hand(ctx,ang,shThickness,shThickness,shLength,(shOrigin+shortenHH)<<FRACBITS,0);
    }
     graphics_context_set_stroke_width(ctx,2);
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
      hand(ctx,ang,mhThickness,mhThickness,outerRadius,shOrigin<<FRACBITS,0);
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
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
        shape(ctx,ang,0,0,0x100,0x100,&gp,0); 
       }
      // then a ring of background colour
      graphics_context_set_fill_color(ctx,GColorFromHEX(bgCol));
      graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),8);
      // now the minute hand just using the old hand() proc
      ang=TRIG_MAX_ANGLE*currentMinute/60+(ang/60);
      graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      hand(ctx,ang,1<<(FRACBITS-0),mhThickness,28<<FRACBITS,3<<FRACBITS,0);
      // and now the hour 
      ang=TRIG_MAX_ANGLE*currentHour/12+(ang/12);
      hand(ctx,ang,3<<(FRACBITS-1),hhThickness,18<<FRACBITS,3<<FRACBITS,0);    
      // then a ring of foreground colour
     graphics_context_set_stroke_width(ctx,2);
      graphics_context_set_fill_color(ctx,GColorFromHEX(fillCol));
      graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),9);    
    break;  
    
    default:  // for the main non weird styles
     if(displayStyle==STYLE_1981)  // in this style the hands are white
      {
         graphics_context_set_fill_color(ctx,GColorFromHEX(0xffffff));
         graphics_context_set_stroke_color(ctx,GColorFromHEX(0xffffff));
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
     
      graphics_context_set_fill_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?bgCol:fillCol));
      graphics_context_set_stroke_color(ctx,GColorFromHEX((displayStyle==STYLE_1981)?bgCol:fillCol));
      graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),15);
      graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),15);
    break;

  }
  
  
 
  
  // I'll draw the inner dot in red if the battery is below or equal to 30%
    uint8_t dr=innerDotRadius;
  
  if(batteryLevel>30)  // normal colours
  {
    graphics_context_set_fill_color(ctx,GColorFromHEX(innerDotCol));
    graphics_context_set_stroke_color(ctx,GColorFromHEX(innerDotCol));
  }
  else  // red dot
  {
      if(dr==0) dr=10;  // batt low always shows a nonzero doe
    graphics_context_set_fill_color(ctx,GColorFromHEX(0xff0000));
    graphics_context_set_stroke_color(ctx,GColorFromHEX(0xff0000));
  }
  graphics_draw_circle(ctx,GPoint(CENTREX,CENTREY),dr);
  graphics_fill_circle(ctx,GPoint(CENTREX,CENTREY),dr); 
  
  // debug test, enable to easily see frame rate
  //block(ctx,0,30,testcol);
  //testcol^=0xffffff;
  
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
