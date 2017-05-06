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
#include <X11/cursorfont.h>
/* X utils */
#include <X11/Xutil.h>
#include <X11/Xatom.h>
/* parse commandline arguments */
#include <ctype.h>
/* one instance */
#include <sys/file.h>
#include <errno.h>

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

// Let's define a linked list node:

typedef struct node
{
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
int bordertop;
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
int disableprompt;
int fullscreen=1;
int columns;
int singleinstance=1;
int uposx;
int uposy;
int uwidth;
int uheight;
int uborder;
int ubordertop;
int utop;

#define MOUSE 1
#define KEYBOARD 2
int hoverset=MOUSE;
int desktopmode=0;
int lock;

/* areas to update */
Imlib_Updates updates, current_update;
/* our background image, rendered only once */
Imlib_Image background = NULL;
/* image variable */
Imlib_Image image = NULL;



void recalc_cells()
{
   border=screen_width/10;
   if (uborder>0) border=uborder;
   bordertop=border;
   if (ubordertop>0) bordertop=ubordertop;

   cell_width=icon_size+padding*2+margin*2;
   cell_height=icon_size+padding*2+margin*2+font_height;
   columns=(screen_width-border*2)/cell_width;
   cell_width=(screen_width-border*2)/columns; // rounded

   cmdx=border+cell_width/2-icon_size/2;
   cmdw=screen_width-cmdx;
   cmdh=40;
   cmdy=bordertop/3*2;
   if (utop) cmdy=utop;
}


void init(int argc, char **argv)
{
   // defaults
   useRootImg=0;
   icon_size=48;
   padding=20;
   margin=2;
   font_height=20;
   cmdy=100;
   disableprompt=0;
   uposx=0;
   uposy=0;
   uwidth=0;
   uheight=0;
   uborder=0;
   ubordertop=0;
   utop=0;

   int c;

   opterr = 0;
   while ((c = getopt(argc, argv, "rm:p:i:b:g:c:f:t:T:x:nkdsa:e:y:z:")) != -1)
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
      uborder=atoi(optarg);
      break;

      case 'T':
      ubordertop=atoi(optarg);
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
      utop=atoi(optarg);
      break;

      case 'x':
      runT=optarg;
      break;

      case 'f':
      fontname=optarg;
      break;

      case 'k':
      disableprompt=1;
      break;

      case 'd':
      desktopmode=1;
      break;

      case 's':
      singleinstance=0;
      break;

      case 'a':
      uposx=atoi(optarg);
      break;

      case 'e':
      uposy=atoi(optarg);
      break;

      case 'y':
      uwidth=atoi(optarg);
      break;

      case 'z':
      uheight=atoi(optarg);
      break;

      case '?':
      {
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
        {
          fprintf (stderr, "Unknown option or missing parameter for option `-%c'.\n\n", optopt);
          fprintf (stderr,"By default, xlunch opens in fullscreen mode, and ends after you run app.\n");
          fprintf (stderr,"It is also possible to cancel xlunch with right mouse click, or Esc key.\n");
          fprintf (stderr,"You can change this by using the following parameter:\n\n");
          fprintf (stderr,"   -d         desktop mode, always keep the launcher at background (behind other windows),\n");
          fprintf (stderr,"              and keep it running after execution of any app or command.\n");
          fprintf (stderr,"              In this mode, xlunch never terminates\n\n");
          fprintf (stderr,"Available options:\n\n");
          fprintf (stderr,"   -r         use root window's background image\n");
          fprintf (stderr,"              (Fails if your root window has no image set)\n");
          fprintf (stderr,"   -k         hide the prompt, and allow user to only run app by icon\n");
          fprintf (stderr,"   -g [file]  Image to set as background (jpg/png)\n");
          fprintf (stderr,"   -m [i]     margin (integer) specifies margin in pixels between icons\n");
          fprintf (stderr,"   -p [i]     padding (integer) specifies padding inside icons in pixels\n");
          fprintf (stderr,"   -b [i]     border (integer) specifies spacing border size in pixels\n");
          fprintf (stderr,"   -i [i]     icon size (integer) in pixels\n");
          fprintf (stderr,"   -c [file]  path to config file which describes titles, icons and commands\n");
          fprintf (stderr,"   -n         Disable fullscreen\n");
          fprintf (stderr,"   -t [i]     Top position (integer) in pixels for the Run commandline\n");
          fprintf (stderr,"   -T [i]     Top position (integer) in pixels for the icons (overides top border)\n");
          fprintf (stderr,"   -x [text]  string to display instead of 'Run: '\n");
          fprintf (stderr,"   -f [name]  font name including size after slash, for example: DejaVuSans/10\n");
          fprintf (stderr,"   -s         disable single-instance check - allow multiple instances running\n\n");
          fprintf (stderr,"Multi monitor setup: xlunch cannot detect your output monitors, it sees your monitors\n");
          fprintf (stderr,"as a big single screen. You can customize this manually by providing the top/left\n");
          fprintf (stderr,"coordinates and width/height of your monitor screen, which effectively positions xlunch\n");
          fprintf (stderr,"on the desired monitor. Use the following options:\n\n");
          fprintf (stderr,"   -a [i]     the x coordinates (integer) of the top left corner of launcher window\n");
          fprintf (stderr,"   -e [i]     the y coordinates (integer) of the top left corner of launcher window\n");
          fprintf (stderr,"   -y [i]     the width (integer) of launcher window on your screen\n");
          fprintf (stderr,"   -z [i]     the height (integer) of launcher window on your screen\n\n");
          fprintf (stderr,"For example, if you have two 800x600 monitors side by side, xlunch sees it ass 1600x800.\n");
          fprintf (stderr,"You can put it to first monitor by: -a 0 -e 0 -y 800 -z 600, or to second monitor by\n");
          fprintf (stderr,"using -a 800 -e 0 -y 800 -z 600. Remember that all these settings may be entirely\n");
          fprintf (stderr,"overiden by your window manager, so you may find it more useful to specify only width\n");
          fprintf (stderr,"and height using these parameters, and then specify desired monitor in your WM config.\n");

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

   /* get/set screen size */
   if (uwidth==0) screen_width=DisplayWidth(disp,screen);
   else screen_width=uwidth;

   if (uheight==0) screen_height=DisplayHeight(disp,screen);
   else screen_height=uheight;

   recalc_cells();
}


void restack()
{
   if (desktopmode) XLowerWindow(disp,win);
   else XRaiseWindow(disp,win);
}


char* strncpyutf8(char* dst, const char* src, size_t num)
{
    if(num)
    {
        size_t sizeSrc = strlen(src); // number of bytes not including null
        while(sizeSrc>num)
        {
            const char* lastByte = src + sizeSrc; // initially \0 at end

            while(lastByte-- > src) // test previous chars
                if((*lastByte & 0xC0) != 0x80) // utf8 start byte found
                    break;

            sizeSrc = lastByte-src;
        }
        memcpy(dst, src, sizeSrc);
        dst[sizeSrc] = '\0';
    }
    return dst;
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
           current->y=j*cell_height+bordertop;
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



void cleanup()
{
   flock(lock, LOCK_UN | LOCK_NB);
   // destroy window, disconnect display, and exit
   XDestroyWindow(disp,win);
   XFlush(disp);
   XCloseDisplay(disp);
}


void parse_app_icons()
{
   FILE * fp;
   char * homeconf = NULL;

   char * home = getenv("HOME");
   if (home!=NULL)
   {
      char* path = "/.xlunch/icons.conf";
      size_t len = strlen(home) + strlen(path) + 1;
      homeconf = malloc(len);
      strcpy(homeconf, home);
      strcat(homeconf, path);
   }

   if (strlen(conffile)==0) conffile=homeconf;
   fp=fopen(conffile,"rb");
   if (fp==NULL)
   {
      fprintf(stderr,"Error opening config file from %s.\nReverting back to system conf.\n",conffile);
      conffile="/etc/xlunch/icons.conf";
      fp=fopen(conffile,"rb");

      if (fp==NULL)
      {
         fprintf(stderr,"Error opening config file %s\n",conffile);
         fprintf(stderr,"You may need to create it. Icon file format is following:\n");
         fprintf(stderr,"title;icon_path;command\n");
         fprintf(stderr,"title;icon_path;command\n");
         fprintf(stderr,"title;icon_path;command\n");
         return;
      }
   }

   char title[255]={0};
   char icon[255]={0};
   char cmd[255]={0};

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
   if (cell->y + cell_height > screen_height) return 0;

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
   if (disableprompt) return;
   if (strlen(runT)==0) runT="Run:  ";
   strcpy(commandlinetext,runT);

   keynode_t * current = cmdline;

   while (current != NULL) {
      strcat(commandlinetext,current->key);
      current = current->next;
   }

   strcat(commandlinetext,"_");
}


void run_command(char * cmd_orig, int excludePercentSign)
{
    char cmd[255];
    strcpy(cmd,cmd_orig);

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

    restack();

    if (desktopmode)
    {
       pid_t pid=fork();
       if (pid==0) // child process
       {
          if (singleinstance) close(lock);
          printf("Forking command: %s\n",cmd);
          int err=execvp(cmd,array);
          fprintf(stderr,"Error forking %s : %d\n",cmd,err);
          exit(0);
       }
       else if (pid<0) // error forking
       {
          fprintf(stderr,"Error running %s\n",cmd);
       }
       else // parent process
       {
          while (cmdline != NULL) pop_key();
          joincmdline();
          joincmdlinetext();
          filter_apps();
          arrange_positions();
       }
    }
    else
    {
       // execute cmd replacing current process
       cleanup();
       printf("Running command: %s\n",cmd);
       int err=execvp(cmd,array);
       fprintf(stderr,"Error running %s : %d\n",cmd, err);
       exit(0);
    }
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


void update_background_image()
{
   /* reset image if exists */
   if (background)
   {
      imlib_context_set_image(background);
      imlib_free_image();
   }

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
         imlib_context_set_color(0, 0, 0, 100);
         imlib_context_set_blend(1);
         imlib_image_fill_rectangle(0,0, screen_width, screen_height);
         imlib_context_set_blend(0);
      }
   }
   else // load background from file
   if (strlen(bgfile)>0)
   {
      image = imlib_load_image(bgfile);
      imlib_context_set_image(image);
      if (image)
      {
         int w = imlib_image_get_width();
         int h = imlib_image_get_height();
         imlib_context_set_image(background);
         imlib_context_set_color(0, 0, 0, 100);
         imlib_context_set_blend(1);
         imlib_blend_image_onto_image(image, 1, 0, 0, w, h,  0,0, screen_width, screen_height);
         imlib_image_fill_rectangle(0,0, screen_width, screen_height);
         imlib_context_set_blend(0);
         imlib_context_set_image(image);
         imlib_free_image();
      }
   }
   else
   {
      imlib_context_set_color(46, 52, 64, 255);
      imlib_image_fill_rectangle(0,0, screen_width, screen_height);
   }
}


/* the program... */
int main(int argc, char **argv)
{
   /* events we get from X */
   XEvent ev;
   /* our virtual framebuffer image we draw into */
   Imlib_Image buffer;
   /* a font */
   Imlib_Font font;
   /* our color range */
   Imlib_Color_Range rangebg;
   Imlib_Color_Range range;
   char title[255];

   /* width and height values */
   int w, h;

   // set initial variables now
   init(argc, argv);
   joincmdlinetext();

   // If an instance is already running, quit
   if (singleinstance)
   {
      lock=open("/tmp/xlunch.lock",O_CREAT | O_RDWR,0666);
      int rc = flock(lock, LOCK_EX | LOCK_NB);
      if (rc) { if (errno == EWOULDBLOCK) fprintf(stderr,"xlunch already running. You may consider -s\nIf this is an error, you may remove /tmp/xlunch.lock\n"); exit(3); }
   }

   win = XCreateSimpleWindow(disp, DefaultRootWindow(disp), uposx, uposy, screen_width, screen_height, 0, 0, 0);

   // absolute fullscreen mode by overide redirect
   if (fullscreen && desktopmode)
   {
      unsigned long valuemask = CWOverrideRedirect;
      XSetWindowAttributes attributes;
      attributes.override_redirect=True;
      XChangeWindowAttributes(disp,win,valuemask,&attributes);
   }

   /* add the ttf fonts dir to our font path */
   imlib_add_path_to_font_path("~/.fonts");
   imlib_add_path_to_font_path("/usr/local/share/fonts");
   imlib_add_path_to_font_path("/usr/share/fonts/truetype");
   imlib_add_path_to_font_path("/usr/share/fonts/TTF");
   imlib_add_path_to_font_path("/usr/share/fonts/truetype/dejavu");
   /* set our cache to 2 Mb so it doesn't have to go hit the disk as long as */
   /* the images we use use less than 2Mb of RAM (that is uncompressed) */
   imlib_set_cache_size(2048 * screen_width);
   /* set the font cache to 512Kb - again to avoid re-loading */
   imlib_set_font_cache_size(512 * screen_width);
   /* set the maximum number of colors to allocate for 8bpp and less to 128 */
   imlib_set_color_usage(128);
   /* dither for depths < 24bpp */
   imlib_context_set_dither(1);
   /* set the display , visual, colormap and drawable we are using */
   imlib_context_set_display(disp);
   imlib_context_set_visual(vis);
   imlib_context_set_colormap(cm);
   imlib_context_set_drawable(win);

   update_background_image();

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
   XSelectInput(disp, win, StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask | KeymapStateMask | FocusChangeMask );
   /* show the window */
   XMapRaised(disp, win);

   // prepare for keyboard UTF8 input
   if (XSetLocaleModifiers("@im=none") == NULL) return 11;
   im = XOpenIM(disp, NULL, NULL, NULL);
   if (im == NULL) { fputs("Could not open input method\n", stdout); return 2; }
   ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL);
   if (ic == NULL) { fprintf(stderr,"Could not open IC, whatever it is, I dont know\n");  return 4; }
   XSetICFocus(ic);

   // send to back or front, depending on settings
   restack();

   // parse config file
   parse_app_icons();


   // prepare message for window manager that we are requesting fullscreen
   XClientMessageEvent msg = {
        .type = ClientMessage,
        .display = disp,
        .window = win,
        .message_type = XInternAtom(disp, "_NET_WM_STATE", True),
        .format = 32,
        .data = { .l = {
            1 /* _NET_WM_STATE_ADD */,
            XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", True),
            None,
            0,
            1
        }}
   };

   // send the message
   if (fullscreen && !desktopmode)
   XSendEvent(disp, DefaultRootWindow(disp), False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*)&msg);


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

               case FocusIn:
                  restack();
               break;

               case FocusOut:
                  restack();
               break;


               case ConfigureNotify:
               {
                  if (screen_width!=ev.xconfigure.width || screen_height!=ev.xconfigure.height)
                  {
                     screen_width=ev.xconfigure.width;
                     screen_height=ev.xconfigure.height;
                     if (!useRootImg) update_background_image();
                     recalc_cells();
                     arrange_positions();
                     updates = imlib_update_append_rect(updates, 0, 0, screen_width, screen_height);
                  }
                  break;
               }

               case ButtonPress:
               {
                  if (ev.xbutton.button==3 && !desktopmode) { cleanup(); exit(0); }
                  if (ev.xbutton.button!=1) break;
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

                  if (keycode==XK_Escape && !desktopmode)
                  {
                     cleanup();
                     exit(0);
                  }

                  if (keycode==XK_Return || keycode==XK_KP_Enter)
                  {
                     // if we have an icon hovered, and the hover was caused by keyboard arrows, run the hovered icon
                     node_t * current = apps;
                     node_t * selected = NULL;
                     node_t * selected_one = NULL;
                     int nb=0;
                     while (current != NULL)
                     {
                       if (!current->hidden)
                       {
                         nb++;
                         selected_one=current;
                       }
                        if (!current->hidden && current->hovered) selected=current;
                        current=current->next;
                     }
                     /* if only 1 app was filtered, consider it selected */
                     if (nb==1 && selected_one!=NULL) run_command(selected_one->cmd,1);
                     else if (hoverset==KEYBOARD && selected!=NULL) run_command(selected->cmd,1);
                     // else run the command entered by commandline, if the command prompt is used
                     else if (!disableprompt) run_command(commandline,0);
                  }

                  if (keycode==XK_Tab || keycode==XK_Up || keycode==XK_Down || keycode==XK_Left || keycode==XK_Right
                  || keycode==XK_KP_Up || keycode==XK_KP_Down || keycode==XK_KP_Left || keycode==XK_KP_Right
                  || keycode==XK_Page_Down || keycode==XK_Page_Up || keycode==XK_Home || keycode==XK_End)
                  {
                     int i=0;
                     if (keycode==XK_KP_Left || keycode==XK_Left) i=-1;
                     if (keycode==XK_Up || keycode==XK_KP_Up || keycode==XK_Page_Up) i=-columns;
                     if (keycode==XK_Down || keycode==XK_KP_Down || keycode==XK_Page_Down) i=columns;
                     if (keycode==XK_Tab || keycode==XK_Right || keycode==XK_KP_Right) i=1;

                     int j=0,n=0;
                     node_t * current = apps;
                     node_t * selected = NULL;
                     while (current != NULL)
                     {
                        if (!current->hidden)
                        {
                           if (current->y+cell_height<screen_height) n++;
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
                        if (!disableprompt) push_key(kbdbuf);

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
             Cursor c = XCreateFontCursor(disp,XC_top_left_arrow);

             while (current != NULL)
             {
                if (!current->hidden && current->y+cell_height<=screen_height)
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
                         c = XCreateFontCursor(disp,XC_hand1);
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
                         size_t sz=strlen(current->title);
                         text_w=cell_width-2*margin-padding+1;

                         imlib_context_set_font(font);

                         while(text_w > cell_width-2*margin-padding && sz>0)
                         {
                            strncpyutf8(title,current->title,sz);
                            imlib_get_text_size(title, &text_w, &text_h);
                            sz--;
                         }

                         // if text was shortened, add dots at the end
                         if (strlen(current->title)!=strlen(title))
                         {
                            char * ptr = title;
                            int len=strlen(ptr);
                            while (len>1 && isspace(ptr[len-1])) ptr[--len]=0;
                            strcat(title,"..");
                            imlib_get_text_size(title, &text_w, &text_h);
                         }

                         int d;
                         if (current->clicked==1) d=4; else d=0;

                         imlib_context_set_color(0, 0, 0, 30);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +1, title);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, title);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +2, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, title);

                         imlib_context_set_color(255, 255, 255, 255);
                         imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2, title);

                         /* free the font */
                         imlib_free_font();
                      }

                      imlib_context_set_image(image);
                      imlib_free_image();
                   }
                   /* render text if icon couldn't be loaded
                   */
                   else {
                     w = 48;
                     h = 48;
                     image = imlib_load_image("/etc/xlunch/ghost.png");
                     //image = imlib_create_image(w, h);

                     imlib_context_set_image(buffer);

                     if (current->hovered)
                     {
                        c = XCreateFontCursor(disp,XC_hand1);
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
                        size_t sz=strlen(current->title);
                        text_w=cell_width-2*margin-padding+1;

                        imlib_context_set_font(font);

                        while(text_w > cell_width-2*margin-padding && sz>0)
                        {
                           strncpyutf8(title,current->title,sz);
                           imlib_get_text_size(title, &text_w, &text_h);
                           sz--;
                        }

                        // if text was shortened, add dots at the end
                        if (strlen(current->title)!=strlen(title))
                        {
                           char * ptr = title;
                           int len=strlen(ptr);
                           while (len>1 && isspace(ptr[len-1])) ptr[--len]=0;
                           strcat(title,"..");
                           imlib_get_text_size(title, &text_w, &text_h);
                        }

                        int d;
                        if (current->clicked==1) d=4; else d=0;

                        imlib_context_set_color(0, 0, 0, 30);
                        imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +1, title);
                        imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +1, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, title);
                        imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x +2, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2 +2, title);

                        imlib_context_set_color(255, 255, 255, 255);
                        imlib_text_draw(current->x +cell_width/2 - (text_w / 2) - up_x, current->y + cell_height - d - font_height/2 - text_h - up_y - padding/2, title);

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
