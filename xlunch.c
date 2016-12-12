// this code is not optimal in many ways, but works just nicely.
// Licence: GNU GPL v3
// Author: Tomas M <www.slax.org>


#define _GNU_SOURCE
/* include X11 stuff */
#include <X11/Xlib.h>
/* include Imlib2 stuff */
#include <Imlib2.h>
/* sprintf include */
#include <stdio.h>
/* strcpy include */
#include <string.h>
/* exit include */
#include <stdlib.h>
/* exec include */
#include <unistd.h>
/* cursor */
#include <X11/Xcursor/Xcursor.h>
/* X utils */
#include <X11/Xutil.h>
#include <X11/Xatom.h>
/* parse commandline arguments */
#include <ctype.h>

/* some globals for our window & X display */
Display *disp;
Window   win;
Visual  *vis;
Colormap cm;
int      depth;

XIM im;
XIC ic;

int screen;
int screen_width;
int screen_height;

Window prev;
int revert_to;


// Let's define a linked list node:

typedef struct node {
    char title[255];
    char icon[255];
    char cmd[255];
    int hovered;
    int clicked;
    int hidden;
    int x;
    int y;
    struct node * next;
} node_t;


typedef struct keynode {
    char key[255];
    struct keynode * next;
} keynode_t;


node_t * apps = NULL;
keynode_t * cmdline = NULL;

int icon_size;
int padding;
int margin;
int border;
int cell_width;
int cell_height;
int font_height;
int useRootImg;
char commandline[10024];
char commandlinetext[10024];
int cmdx;
int cmdy;
int cmdw;
int cmdh;
char * bgfile="";
char * conffile="";
char * runT="";
char * fontname="";
int fullscreen=1;
int useIsTyping=0;
int columns;

#define MOUSE 1
#define KEYBOARD 2
int hoverset=MOUSE;


/* areas to update */
Imlib_Updates updates, current_update;


void init(int argc, char **argv)
{
   // defaults
   useRootImg=0;
   icon_size=48;
   padding=20;
   margin=2;
   border=140;
   font_height=20;
   cmdy=100;

   int c;

   opterr = 0;
   while ((c = getopt(argc, argv, "rm:p:i:b:g:c:f:t:x:n")) != -1)
   switch (c)
   {
      case 'r':
      useRootImg=1;
      break;

      case 'm':
      margin=atoi(optarg);
      break;

      case 'p':
      padding=atoi(optarg);
      break;

      case 'i':
      icon_size=atoi(optarg);
      break;

      case 'b':
      border=atoi(optarg);
      break;

      case 'g':
      bgfile=optarg;
      break;

      case 'c':
      conffile=optarg;
      break;

      case 'n':
      fullscreen=0;
      break;

      case 't':
      cmdy=atoi(optarg);
      break;

      case 'x':
      runT=optarg;
      break;

      case 'f':
      fontname=optarg;
      break;

      case '?':
      {
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
        {
          fprintf (stderr, "Unknown option or missing parameter for option `-%c'.\n", optopt);
          fprintf (stderr,"\nAvailable options:\n\n");
          fprintf (stderr,"   -r         use root window's background image\n");
          fprintf (stderr,"              Fails if your root window has no image set\n");
          fprintf (stderr,"   -g [file]  Image to set as background (jpg/png)\n");
          fprintf (stderr,"   -m [i]     margin (integer) specifies margin in pixels between icons\n");
          fprintf (stderr,"   -p [i]     padding (integer) specifies padding inside icons in pixels\n");
          fprintf (stderr,"   -b [i]     border (integer) specifies spacing border size in pixels\n");
          fprintf (stderr,"   -i [i]     icon size (integer) in pixels\n");
          fprintf (stderr,"   -c [file]  path to config file which describes titles, icons and commands\n");
          fprintf (stderr,"   -n         Disable fullscreen\n");
          fprintf (stderr,"   -t [i]     Top position (integer) in pixels for the Run commandline\n");
          fprintf (stderr,"   -x [text]  String to display instead of 'Run: '\n");
          fprintf (stderr,"   -f [name]  font name including size after slash, for example: DejaVuSans/10\n");

//          fprintf (stderr,"   -d [x]  gradient color\n");
//          fprintf (stderr,"   -s [i]  font size (integer) in pixels\n");
          fprintf (stderr,"\n");
          exit(1);
        }
        else
          fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt);
        exit(1);
      }
   }

   /* connect to X */
   disp = XOpenDisplay(NULL);
   if (!disp)
   {
      fprintf(stderr,"Cannot connect to DISPLAY\n");
      exit(2);
   }

   /* get default visual , colormap etc. you could ask imlib2 for what it */
   /* thinks is the best, but this example is intended to be simple */
   screen = DefaultScreen(disp);
   vis   = DefaultVisual(disp, screen);
   depth = DefaultDepth(disp, screen);
   cm    = DefaultColormap(disp, screen);

   XSynchronize(disp,True);

   // previous window handle
   XGetInputFocus(disp,&prev,&revert_to);

   /* get screen size */
   screen_width=DisplayWidth(disp,screen);
   screen_height=DisplayHeight(disp,screen);

   cell_width=icon_size+padding*2+margin*2;
   cell_height=icon_size+padding*2+margin*2+font_height;
   columns=(screen_width-border*2)/cell_width;
   cell_width=(screen_width-border*2)/columns; // rounded

   cmdx=border+cell_width/2-icon_size/2;
   cmdw=screen_width-cmdx;
   cmdh=40;
}


void arrange_positions()
{
    node_t * current = apps;
    int i=0; int j=0;

    while (current != NULL)
    {
        if (current->hidden)
        {
           current->x=0;
           current->y=0;
        }
        else
        {
           current->x=i*cell_width+border;
           if (current->x+cell_width>screen_width-border)
           {
              i=0;
              j++;
              current->x=border;
           }
           i++;
           current->y=j*cell_height+border;
        }
        current = current->next;
    }
}


void push_key(char * key)
{
    keynode_t * current = cmdline;

    if (current==NULL)
    {
       cmdline = malloc(sizeof(keynode_t));
       strcpy(cmdline->key,key);
       cmdline->next = NULL;
       return;
    }

    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new item */
    current->next = malloc(sizeof(keynode_t));
    strcpy(current->next->key,key);
    current->next->next = NULL;
}


void pop_key()
{
    if (cmdline==NULL) return;

    // if there is only one item, remove it
    if (cmdline->next==NULL)
    {
       free(cmdline);
       cmdline=NULL;
       return;
    }

    keynode_t * current = cmdline;

    while (current->next->next != NULL) {
        current = current->next;
    }

    /* now we can remove last item */
    free(current->next);
    current->next=NULL;
}




void push_app(char * title, char * icon, char * cmd, int x, int y)
{
    node_t * current = apps;

    // empty list, add first directly
    if (current==NULL)
    {
       apps = malloc(sizeof(node_t));
       strcpy(apps->title,title);
       strcpy(apps->icon,icon);
       strcpy(apps->cmd,cmd);
       apps->x=0;
       apps->y=0;
       apps->hidden=0;
       apps->hovered=0;
       apps->clicked=0;
       apps->next = NULL;
       return;
    }

    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new variable */
    current->next = malloc(sizeof(node_t));
    strcpy(current->next->title,title);
    strcpy(current->next->icon,icon);
    strcpy(current->next->cmd,cmd);
    current->next->x=x;
    current->next->y=y;
    current->next->hidden=0;
    current->next->hovered=0;
    current->next->clicked=0;
    current->next->next = NULL;
}



int cleanup()
{
   // revert focus to previous window
   XSetInputFocus(disp,prev,RevertToNone,CurrentTime);
   // destroy window, disconnect display, and exit
   XDestroyWindow(disp,win);
   XFlush(disp);
   XCloseDisplay(disp);
}

void parse_app_icons()
{
   FILE * fp;
   if (strlen(conffile)==0) conffile="/etc/xlunch/icons.conf";
   fp=fopen(conffile,"rb");
   if (fp==NULL)
   {
      printf("error opening config file %s\n",conffile);
      printf("Icon file format is following:\n");
      printf("title;icon_path;command\n");
      printf("title;icon_path;command\n");
      printf("title;icon_path;command\n");
      return;
   }

   char title[255];
   char icon[255];
   char cmd[255];

   while(!feof(fp))
   {
      fscanf(fp,"%250[^;];%250[^;];%250[^\n]\n",title,icon,cmd);
      push_app(title,icon,cmd,0,0);
   }

   arrange_positions();
   fclose(fp);
}


int mouse_over_cell(node_t * cell, int mouse_x, int mouse_y)
{
   if (cell->hidden) return 0;

   if (mouse_x>=cell->x+margin
      && mouse_x<=cell->x+cell_width-margin
      && mouse_y>=cell->y+margin
      && mouse_y<=cell->y+cell_height-margin) return 1;
   else return 0;
}


Pixmap GetRootPixmap(Display* display, Window *root)
{
    Pixmap currentRootPixmap;
    Atom act_type;
    int act_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    Atom _XROOTPMAP_ID;

    _XROOTPMAP_ID = XInternAtom(display, "_XROOTPMAP_ID", False);

    if (XGetWindowProperty(display, *root, _XROOTPMAP_ID, 0, 1, False,
                XA_PIXMAP, &act_type, &act_format, &nitems, &bytes_after,
                &data) == Success) {

        if (data) {
            currentRootPixmap = *((Pixmap *) data);
            XFree(data);
        }
    }

    return currentRootPixmap;
}


int get_root_image_to_imlib_data(DATA32 * direct)
{
    int screen;
    Window root;
    Display* display;
    XWindowAttributes attrs;
    Pixmap bg;
    XImage* img;

    display = XOpenDisplay(NULL);
    if (display == NULL) return 0;

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    XGetWindowAttributes(display, root, &attrs);

    bg = GetRootPixmap(display, &root);
    img = XGetImage(display, bg, 0, 0, attrs.width, attrs.height, ~0, ZPixmap);
    XFreePixmap(display, bg);

    if (!img) { XCloseDisplay(display); return 0; }

    unsigned long pixel;
    int x, y;

    for (y = 0; y < img->height; y++)
    {
          for (x = 0; x < img->width; x++)
          {
              pixel = XGetPixel(img,x,y);
              direct[y*img->width+x+0] = 0xffffffff&pixel;
          }
    }

    XDestroyImage(img);
    return 1;
}


void filter_apps()
{
   node_t * current = apps;

   while (current != NULL)
   {
      if (strlen(commandline)==0 || strcasestr(current->title,commandline)!=NULL || strcasestr(current->cmd,commandline)!=NULL) current->hidden=0;
      else { current->hidden=1; current->hovered=0; current->clicked=0; }
      current=current->next;
   }
}


void run_command(char * cmd, int excludePercentSign)
{
    printf("Running command: %s\n",cmd);
    cleanup();

    // split arguments into pieces
    int i = 0;
    char *p = strtok (cmd, " ");
    char *array[100] = {0};

    if (p==NULL) array[0]=cmd;

    while (p != NULL)
    {
        if (strncmp("%",p,1)!=0 || !excludePercentSign) array[i++]=p;
        p = strtok (NULL, " ");
        if (i>=99) break;
    }

    // execute cmd replacing current process
    int err=execvp(cmd,array);
    printf("Error running %s : %d",cmd,err);
    exit(0);
}

void joincmdline()
{
   strcpy(commandline,"");

   keynode_t * current = cmdline;

   while (current != NULL) {
      strcat(commandline,current->key);
      current = current->next;
   }
}

void joincmdlinetext()
{
   if (strlen(runT)==0) runT="Run:  ";
   strcpy(commandlinetext,runT);

   keynode_t * current = cmdline;

   while (current != NULL) {
      strcat(commandlinetext,current->key);
      current = current->next;
   }

   strcat(commandlinetext,"_");
}


void sethover(node_t * cell, int hover)
{
   if (cell==NULL) return;
   if (cell->hidden) return;
   if (cell->hovered!=hover) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
   cell->hovered=hover;
}

void setclicked(node_t * cell, int clicked)
{
   if (cell==NULL) return;
   if (cell->hidden) return;
   if (cell->clicked!=clicked) updates = imlib_update_append_rect(updates, cell->x, cell->y, cell_width, cell_height);
   cell->clicked=clicked;
}


Imlib_Font loadfont()
{
   if (strlen(fontname)==0) fontname="OpenSans-Regular/10";
   Imlib_Font font;
   font=imlib_load_font(fontname);
   if (!font) font=imlib_load_font("DejaVuSans/10");
   return font;
}


/* the program... */
int main(int argc, char **argv)
{
   /* events we get from X */
   XEvent ev;
   /* our virtual framebuffer image we draw into */
   Imlib_Image buffer;
   /* our background image, rendered only once */
   Imlib_Image background;
   /* image variable */
   Imlib_Image image;
   /* a font */
   Imlib_Font font;
   /* our color range */
   Imlib_Color_Range rangebg;
   Imlib_Color_Range range;

   /* width and height values */
   int w, h;

   // set initial variables now
   init(argc, argv);
   joincmdlinetext();

   // fullscreen
   unsigned long valuemask = CWOverrideRedirect;
   XSetWindowAttributes attributes;
   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), 0, 0, screen_width, screen_height, 0, 0, 0);
   attributes.override_redirect=fullscreen;
   XChangeWindowAttributes(disp,win,valuemask,&attributes);


   /* set our cache to 2 Mb so it doesn't have to go hit the disk as long as */
   /* the images we use use less than 2Mb of RAM (that is uncompressed) */
   imlib_set_cache_size(2048 * screen_width);
   /* set the font cache to 512Kb - again to avoid re-loading */
   imlib_set_font_cache_size(512 * screen_width);
   /* add the ttf fonts dir to our font path */
   imlib_add_path_to_font_path("~/.fonts");
   imlib_add_path_to_font_path("/usr/local/share/fonts");
   imlib_add_path_to_font_path("/usr/share/fonts/truetype");
   imlib_add_path_to_font_path("/usr/share/fonts/TTF");
   imlib_add_path_to_font_path("/usr/share/fonts/truetype/dejavu");
   imlib_add_path_to_font_path(".");
   /* set the maximum number of colors to allocate for 8bpp and less to 128 */
   imlib_set_color_usage(128);
   /* dither for depths < 24bpp */
   imlib_context_set_dither(1);
   /* set the display , visual, colormap and drawable we are using */
   imlib_context_set_display(disp);
   imlib_context_set_visual(vis);
   imlib_context_set_colormap(cm);
   imlib_context_set_drawable(win);


   // create gradient. It will be later used to shade things
   rangebg = imlib_create_color_range();
   imlib_context_set_color_range(rangebg);
   /* add white opaque as the first color */
   imlib_context_set_color(0, 0, 0, 100);
   imlib_add_color_to_color_range(0);
   /* add black, fully transparent at the end 20 units away */
   imlib_context_set_color(0, 0, 0, 100);
   imlib_add_color_to_color_range(120);


   /* fill the window background */
   background = imlib_create_image(screen_width, screen_height);
   imlib_context_set_image(background);

   if (useRootImg)
   {
      DATA32 * direct = imlib_image_get_data();
      int ok = get_root_image_to_imlib_data(direct);
      if (ok)
      {
         imlib_image_put_back_data(direct);
         imlib_image_fill_color_range_rectangle(0,0, screen_width, screen_height, -45.0);
      }
   }
   else // load background from file
   if (strlen(bgfile)>0)
   {
      image = imlib_load_image(bgfile);
      imlib_context_set_image(image);
      if (image)
      {
         w = imlib_image_get_width();
         h = imlib_image_get_height();
         imlib_context_set_image(background);
         imlib_context_set_blend(1);
         imlib_blend_image_onto_image(image, 1, 0, 0, w, h,  0,0, screen_width, screen_height);
         imlib_image_fill_color_range_rectangle(0,0, screen_width, screen_height, -45.0);
         imlib_context_set_image(image);
         imlib_free_image();
         imlib_context_set_blend(0);
      }
   }

   // create gradient. It will be later used to shade things
   range = imlib_create_color_range();
   imlib_context_set_color_range(range);
   /* add white opaque as the first color */
   imlib_context_set_color(255, 255, 255, 100);
   imlib_add_color_to_color_range(0);
   /* add black, fully transparent at the end 20 units away */
   imlib_context_set_color(255, 255, 255, 100);
   imlib_add_color_to_color_range(120);


   /* tell X what events we are interested in */
   XSelectInput(disp, win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask | KeymapStateMask);
   /* show the window */
   XMapRaised(disp, win);


   // prepare for keyboard UTF8 input
   if (XSetLocaleModifiers("@im=none") == NULL) return 11;
   im = XOpenIM(disp, NULL, NULL, NULL);
   if (im == NULL) { fputs("Could not open input method\n", stdout); return 2; }
   ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL);
   if (ic == NULL) { printf("Could not open IC, whatever it is, I dont know\n");  return 4; }
   XSetICFocus(ic);

   // I noticed this sometimes fails with BadMatch (invalid parameter attributes).
   // So lets call it only if fullscreen mode is active
   if (fullscreen)
   XSetInputFocus(disp,win,RevertToNone,CurrentTime);

   // parse config file
   parse_app_icons();


   /* infinite event loop */
   for (;;)
   {
        /* init our updates to empty */
        updates = imlib_updates_init();

        /* while there are events form X - handle them */
        do
          {
             XNextEvent(disp, &ev);

             // allow through only UTF8 events
             if (XFilterEvent(&ev, win)) continue;

             switch (ev.type)
             {

               case Expose:
                  /* window rectangle was exposed - add it to the list of */
                  /* rectangles we need to re-render */
                  updates = imlib_update_append_rect(updates, ev.xexpose.x, ev.xexpose.y, ev.xexpose.width, ev.xexpose.height);
                  break;

               case ButtonPress:
               {
                  node_t * current = apps;

                  while (current != NULL)
                  {
                      if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) setclicked(current,1);
                      else setclicked(current,0);
                      current = current->next;
                  }

                  break;
               }

               case ButtonRelease:
               {
                  node_t * current = apps;

                  while (current != NULL)
                  {
                      if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) if (current->clicked==1) run_command(current->cmd,1);
                      setclicked(current,0); // button release means all cells are not clicked
                      current = current->next;
                  }

                  break;
               }

               // refresh keyboard layout if changed
               case KeymapNotify:
                  XRefreshKeyboardMapping(&ev.xmapping);
                  break;

               case KeyPress:
               {
                  // keyboard events
                  int count = 0;
                  KeySym keycode = 0;
                  Status status = 0;
                  char kbdbuf[20]={0};
                  count = Xutf8LookupString(ic, (XKeyPressedEvent*)&ev, kbdbuf, 20, &keycode, &status);

                  if (keycode==XK_Escape)
                  {
                     cleanup();
                     exit(0);
                  }

                  if (keycode==XK_Return || keycode==XK_KP_Enter)
                  {
                     // if we have an icon hovered, and the hover was caused by keyboard arrows, run the hovered icon
                     node_t * current = apps;
                     node_t * selected = NULL;
                     while (current != NULL)
                     {
                        if (!current->hidden && current->hovered) selected=current;
                        current=current->next;
                     }
                     if (hoverset==KEYBOARD && selected!=NULL) run_command(selected->cmd,0);
                     else run_command(commandline,0);
                  }

                  if (keycode==XK_Tab || keycode==XK_Up || keycode==XK_Down || keycode==XK_Left || keycode==XK_Right
                  || keycode==XK_KP_Up || keycode==XK_KP_Down || keycode==XK_KP_Left || keycode==XK_KP_Right
                  || keycode==XK_Page_Down || keycode==XK_Page_Up || keycode==XK_Home || keycode==XK_End)
                  {
                     int i=0;
                     if (keycode==XK_Tab || keycode==XK_KP_Left || keycode==XK_Left) i=-1;
                     if (keycode==XK_Up || keycode==XK_KP_Up || keycode==XK_Page_Up) i=-columns;
                     if (keycode==XK_Down || keycode==XK_KP_Down || keycode==XK_Page_Down) i=columns;
                     if (keycode==XK_Right || keycode==XK_KP_Right) i=1;

                     int j=0,n=0;
                     node_t * current = apps;
                     node_t * selected = NULL;
                     while (current != NULL)
                     {
                        if (!current->hidden)
                        {
                           n++;
                           if (selected==NULL) j++;
                           if (current->hovered) selected=current;
                           sethover(current,0);
                        }
                        current=current->next;
                     }

                     if (selected==NULL) { selected=apps; i=0; j=0; }
                     current=apps;

                     int k=i+j;
                     if (k>n || keycode==XK_End) k=n;
                     if (k<1 || keycode==XK_Home) k=1;
                     while (current != NULL)
                     {
                        if (!current->hidden)
                        {
                           k--;
                           if (k==0) { sethover(current,1); hoverset=KEYBOARD; }
                        }
                        current=current->next;
                     }

                     continue; // do not conitnue
                  }

                  if (keycode==XK_Delete || keycode==XK_BackSpace)
                     pop_key();
                  else
                     if (count>1 || (count==1 && kbdbuf[0]>=32)) // ignore unprintable characterrs
                        push_key(kbdbuf);

                  joincmdline();
                  joincmdlinetext();
                  filter_apps();
                  arrange_positions();

                  // we used keyboard to type command. So unselect all icons.
                  node_t * current = apps;
                  while (current != NULL)
                  {
                     sethover(current,0);
                     setclicked(current,0);
                     current = current->next;
                  }

                  updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                  break;
               }

               case KeyRelease:
                  break;

               case MotionNotify:
               {
                  node_t * current = apps;

                  while (current != NULL)
                  {
                      if (mouse_over_cell(current, ev.xmotion.x, ev.xmotion.y)) { sethover(current,1); hoverset=MOUSE; }
                      else { sethover(current,0); setclicked(current,0); }
                      current = current->next;
                  }

                  break;
               }

               default:
                  /* any other events - do nothing */
                  break;
             }
          }


        while (XPending(disp));
        /* no more events for now ? ok - idle time so lets draw stuff */

        /* take all the little rectangles to redraw and merge them into */
        /* something sane for rendering */
        updates = imlib_updates_merge_for_rendering(updates, screen_width, screen_height);

        for (current_update = updates;   current_update;    current_update = imlib_updates_get_next(current_update))
        {
             int up_x, up_y, up_w, up_h;

             /* find out where the first update is */
             imlib_updates_get_coordinates(current_update, &up_x, &up_y, &up_w, &up_h);

             /* create our buffer image for rendering this update */
             buffer = imlib_create_image(up_w, up_h);

             /* we can blend stuff now */
             imlib_context_set_blend(1);

             imlib_context_set_image(buffer);
             /* blend background image onto the buffer */
             imlib_blend_image_onto_image(background, 1, 0, 0, screen_width, screen_height, - up_x, - up_y, screen_width, screen_height);

             node_t * current = apps;
             Cursor c = XcursorLibraryLoadCursor(disp,"top_left_arrow");

             while (current != NULL)
             {
                if (!current->hidden)
                {
                   image=imlib_load_image(current->icon);
                   if (image)
                   {
                      imlib_context_set_image(image);
                      w = imlib_image_get_width();
                      h = imlib_image_get_height();
                      imlib_context_set_image(buffer);

                      if (current->hovered)
                      {
                         c = XcursorLibraryLoadCursor(disp,"hand1");
                         imlib_image_fill_color_range_rectangle(current->x -up_x+margin, current->y- up_y+margin, cell_width-2*margin, cell_height-2*margin, -45.0);
                      }

                      int d;
                      if (current->clicked) d=2; else d=0;

                      imlib_blend_image_onto_image(image, 0, 0, 0, w, h,
                                                  current->x - up_x + cell_width/2-icon_size/2+d, current->y - up_y +padding+margin+d, icon_size-d*2, icon_size-d*2);

                      /* draw text under icon */
                      font = loadfont();
                      if (font)
                      {
                         int text_w; int text_h;

                         imlib_context_set_font(font);
                         imlib_get_text_size(current->title, &text_w, &text_h);
                         int d;
                         if (current->clicked==1) d=4; else d=0;

                         imlib_context_set_color(0, 0, 0, 30);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +1, current->title);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, current->title);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +2, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, current->title);

                         imlib_context_set_color(255, 255, 255, 255);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2, current->title);

                         /* free the font */
                         imlib_free_font();
                      }

                      imlib_context_set_image(image);
                      imlib_free_image();
                   }
                }
                current = current->next;
             }

             XDefineCursor(disp,win,c);

             /* set the buffer image as our current image */
             imlib_context_set_image(buffer);

             /* draw text */
             font = loadfont();
             if (font)
             {
                imlib_context_set_font(font);
                imlib_context_set_color(0, 0, 0, 30);
                imlib_text_draw(cmdx+1 - up_x, cmdy+1 - up_y, commandlinetext);
                imlib_text_draw(cmdx+1 - up_x, cmdy+2 - up_y, commandlinetext);
                imlib_text_draw(cmdx+2 - up_x, cmdy+2 - up_y, commandlinetext);

                imlib_context_set_color(255, 255, 255, 255);
                imlib_text_draw(cmdx-up_x, cmdy-up_y, commandlinetext); 

                /* free the font */
                imlib_free_font();
             }


             /* don't blend the image onto the drawable - slower */
             imlib_context_set_blend(0);
             /* render the image at 0, 0 */
             imlib_render_image_on_drawable(up_x, up_y);
             /* don't need that temporary buffer image anymore */
             imlib_free_image();
        }
        /* if we had updates - free them */
        if (updates)
           imlib_updates_free(updates);
        /* loop again waiting for events */
   }
   return 0;
}

