// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2020 Gerard SAMBLANCAT
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

// Ancient AV versions forgot to set this.
#define __STDC_CONSTANT_MACROS

#include "led-matrix.h"
#include "pixel-mapper.h"
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <map>
#include <string>
#include <Magick++.h>
#include <magick/image.h>
#include <limits.h>

// libav: "U NO extern C in header ?"
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libswscale/swscale.h>
}

using rgb_matrix::GPIO;
using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
 
using namespace Magick;
 
typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
  ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
                  anim_delay_ms(-1), loops(-1), vsync_multiple(1) {}
  tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
  tmillis_t wait_ms;           // Regular image: duration to show.
  tmillis_t anim_delay_ms;     // Animation delay override.
  int loops;
  int vsync_multiple;
};

struct FileInfo {
  ImageParams params;      // Each file might have specific timing settings
  bool is_multi_frame;
};

//Définis le Buffer de l'écran X-Y-rgb qui est scrollé
struct pixcol {
  uint8_t rr,gg,bb;
};

//Quelques couleurs
pixcol black={0,0,0};
pixcol white={255,255,255};

//Variables écran Matrix
unsigned short screenH, screenW;
static RGBMatrix *matrix=NULL;
static FrameCanvas *off_canvas=NULL;
RGBMatrix::Options matrix_options;
rgb_matrix::RuntimeOptions runtime_opt;

//......................................................................
// Variables & Paramètres du Texte PAR DEFAUT 

//Pointeur du texte
unsigned short msg_ptr;
//si boucle : pointeur debut do
unsigned short msg_ptr_do = 0;
//si boucle : nb de boucle restant (-1 => pas de boucle)
short do_loops = -1;

//dernière commande qui a été stripée (avant le ':') (sinon cmd="")
char cmd[256];
//le paramètre de la commande (après le ':')
char cmdparam[256];
//si c'est pas une commande -> char à afficher
char affchar;

//Fait un gros buffer par defaut (taille forfaitaire du double largeur) A VOIR !!!
const unsigned short xmax_buff = 512;
const unsigned short ymax_buff = 512;

//Position X du curseur en écriture dans buff (part de 0 à gauche)
unsigned short wrx_ptr;
//Position Y du curseur en écriture dans buff (part de screenH du haut)
unsigned short wry_ptr;

//Buffer écran
pixcol buff[xmax_buff][ymax_buff];

//Masque du Liseré seul sur les bords
pixcol cadremask[xmax_buff][ymax_buff];
//Masque de l'Image Background
pixcol backmask[xmax_buff][ymax_buff];

const short MAXLENMSG = 2048;
char msg[MAXLENMSG];

//Fonte de char (correspond au dossier à utliser)
char font[128];

//Hauteur de fonte (police de base 64)(si = 0 pas de resize de fonte)
unsigned short fontsize = 0;

//Vitesse de défilement (0=à fond, 100=maxi très lent)
unsigned short speed = 20;
 
//Pas de défilement (1=normal, 5=max)
unsigned short step = 1;

//epaisseur du cadre en pixel
unsigned short lisere = 0;

//Flag si image de fond
unsigned short background = 0;

//Crenage en -/+ pixel par caractere affiché
short crenage = 0;

//Ecriture Chars sur fond opaque (1), ou transparent (0)
short solid = 0;

//Flag du Mode fixe ou shift
bool fixe = 0;

//Paramètre architecture des RGB Matrix en prog
//(prend le dessus sur ligne de commande)
//Toujours necessaire --led-no-hardware-pulse si l'audio est activé.
//Sinon mettre dtparam-audio=off dans /boot/config.txt
short chain=1;
short parallel=1;
short cols=32;
short rows=32;

//Couleurs back/ink d'origine des fichiers fontes (constantes)
pixcol backcolor0={0,0,0};
pixcol inkcolor0={0xff,0xff,0xff};

//Couleur active des Caractères à afficher
pixcol backcolor={0,0,0};
pixcol inkcolor={0xC0,0xC0,0xC8};

//-----------------------------------------------------------------------
volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

void CopyFrame(AVFrame *pFrame, FrameCanvas *canvas,
               int offset_x, int offset_y, int width, int height) {
 for (int y = 0; y < height; ++y) {
   pixcol *pix = (pixcol*) (pFrame->data[0] + y*pFrame->linesize[0]);
   for (int x = 0; x < width; ++x, ++pix)
      canvas->SetPixel(x + offset_x, y + offset_y, pix->rr, pix->gg, pix->bb);
 }
}

// Scale "width" and "height" to fit within target rectangle of given size.
void ScaleToFitKeepAscpet(int fit_in_width, int fit_in_height,
                          int *width, int *height) {
  if (*height < fit_in_height && *width < fit_in_width) return; // Done.
  const float height_ratio = 1.0 * (*height) / fit_in_height;
  const float width_ratio  = 1.0 * (*width) / fit_in_width;
  const float ratio = (height_ratio > width_ratio) ? height_ratio : width_ratio;
  *width = roundf(*width / ratio);
  *height = roundf(*height / ratio);
}

static void add_nanos(struct timespec *accumulator, long nanoseconds) {
  accumulator->tv_nsec += nanoseconds;
  while (accumulator->tv_nsec > 1000000000) {
    accumulator->tv_nsec -= 1000000000;
    accumulator->tv_sec += 1;
  }
}

// Convert deprecated color formats to new and manually set the color range.
// YUV has funny ranges (16-235), while the YUVJ are 0-255. SWS prefers to
// deal with the YUV range, but then requires to set the output range.
// https://libav.org/documentation/doxygen/master/pixfmt_8h.html#a9a8e335cf3be472042bc9f0cf80cd4c5
SwsContext *CreateSWSContext(const AVCodecContext *codec_ctx,
                             int display_width, int display_height) {
  AVPixelFormat pix_fmt;
  bool src_range_extended_yuvj = true;
  // Remap deprecated to new pixel format.
  switch (codec_ctx->pix_fmt) {
  case AV_PIX_FMT_YUVJ420P: pix_fmt = AV_PIX_FMT_YUV420P; break;
  case AV_PIX_FMT_YUVJ422P: pix_fmt = AV_PIX_FMT_YUV422P; break;
  case AV_PIX_FMT_YUVJ444P: pix_fmt = AV_PIX_FMT_YUV444P; break;
  case AV_PIX_FMT_YUVJ440P: pix_fmt = AV_PIX_FMT_YUV440P; break;
  default:
    src_range_extended_yuvj = false;
    pix_fmt = codec_ctx->pix_fmt;
  }
  SwsContext *swsCtx = sws_getContext(codec_ctx->width, codec_ctx->height,
                                      pix_fmt,
                                      display_width, display_height,
                                      AV_PIX_FMT_RGB24, SWS_BILINEAR,
                                      NULL, NULL, NULL);
  if (src_range_extended_yuvj) {
    // Manually set the source range to be extended. Read modify write.
    int dontcare[4];
    int src_range, dst_range;
    int brightness, contrast, saturation;
    sws_getColorspaceDetails(swsCtx, (int**)&dontcare, &src_range,
                             (int**)&dontcare, &dst_range, &brightness,
                             &contrast, &saturation);
    const int* coefs = sws_getCoefficients(SWS_CS_DEFAULT);
    src_range = 1;  // New src range.
    sws_setColorspaceDetails(swsCtx, coefs, src_range, coefs, dst_range,
                             brightness, contrast, saturation);
  }
  return swsCtx;
}


short Video(const char movie_file[128], FrameCanvas *offscreen_canvas) {
  int vsync_multiple = 1;
  bool maintain_aspect_ratio = false;        // !!!!!!!
  unsigned int frame_skip = 0;

  // Initalizing these to NULL prevents segfaults!
  AVFormatContext   *pFormatCtx = NULL;
  int               i, videoStream;
  AVCodecContext    *pCodecCtxOrig = NULL;
  AVCodecContext    *pCodecCtx = NULL;
  AVCodec           *pCodec = NULL;
  AVPacket          packet;
  int               frameFinished;

  // Register all formats and codecs
  av_register_all();
  avformat_network_init();

  if (avformat_open_input(&pFormatCtx, movie_file, NULL, NULL) != 0) {
    perror("Issue opening file: ");
    return -1;
  }

  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    fprintf(stderr, "Couldn't find stream information\n");
    return -1;
  }

  long frame_count = 0;

  // Find the first video stream
  videoStream=-1;
  for (i=0; i < (int)pFormatCtx->nb_streams; ++i) {
    if (pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  }
  if (videoStream == -1)
    return -1; // Didn't find a video stream

  // Get a pointer to the codec context for the video stream
  pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
  double fps = av_q2d(pFormatCtx->streams[videoStream]->avg_frame_rate);
  if (fps < 0) {
    fps = 1.0 / av_q2d(pFormatCtx->streams[videoStream]->codec->time_base);
  }

  // Find the decoder for the video stream
  pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
  if (pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }
  // Copy context
  pCodecCtx = avcodec_alloc_context3(pCodec);
  if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
    fprintf(stderr, "Couldn't copy codec context");
    return -1;
  }

  // Open codec
  if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) return -1;

  /*
   * Prepare frame to hold the scaled target frame to be send to matrix.
   */
  AVFrame *output_frame = av_frame_alloc();  // Target frame for output
  int display_width = pCodecCtx->width;
  int display_height = pCodecCtx->height;
  if (maintain_aspect_ratio) {
    display_width = pCodecCtx->width;
    display_height = pCodecCtx->height;
    // Make display fit within canvas.
    ScaleToFitKeepAscpet(matrix->width(),matrix->height(),&display_width, &display_height);
  } else {
    display_width = matrix->width();
    display_height = matrix->height();
  }
  // Letterbox or pillarbox black bars.
  const int display_offset_x = (matrix->width() - display_width)/2;
  const int display_offset_y = (matrix->height() - display_height)/2;

  // Allocate buffer to meet output size requirements
  const size_t output_size = avpicture_get_size(AV_PIX_FMT_RGB24,
                                           display_width,display_height);
  uint8_t *output_buffer = (uint8_t *) av_malloc(output_size);

  // Assign appropriate parts of buffer to image planes in output_frame.
  // Note that output_frame is an AVFrame, but AVFrame is a superset
  // of AVPicture
  avpicture_fill((AVPicture *)output_frame, output_buffer, AV_PIX_FMT_RGB24,
                 display_width, display_height);

  // initialize SWS context for software scaling
  SwsContext *const sws_ctx = CreateSWSContext(pCodecCtx,
                                               display_width, display_height);
  if (!sws_ctx) {
    fprintf(stderr, "Trouble doing scaling to %dx%d :(\n",
            matrix->width(), matrix->height());
    return 1;
  }

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  const long frame_wait_nanos = 1e9 / fps;
  struct timespec next_frame;

  unsigned int frames_to_skip;
  int readed;

  AVFrame *decode_frame = av_frame_alloc();  // Decode video into this

  do {
    frames_to_skip = frame_skip;
    clock_gettime(CLOCK_MONOTONIC, &next_frame);

    while (!interrupt_received) {
      readed = av_read_frame(pFormatCtx, &packet);
      if (readed<0) break;
      // Is this a packet from the video stream?
      if (packet.stream_index==videoStream) {
        // Determine absolute end of this frame now so that we don't include
        // decoding overhead. TODO: skip frames if getting too slow ?
        add_nanos(&next_frame, frame_wait_nanos);
        
        // Decode video frame
        avcodec_decode_video2(pCodecCtx, decode_frame, &frameFinished, &packet);
        
        if (frames_to_skip) { frames_to_skip--; continue; }
        
        // Did we get a video frame?
        if (frameFinished) {
          // Convert the image from its native format to RGB
          sws_scale(sws_ctx, (uint8_t const * const *)decode_frame->data,
                    decode_frame->linesize, 0, pCodecCtx->height,
                    output_frame->data, output_frame->linesize);
          CopyFrame(output_frame, offscreen_canvas,
                    display_offset_x, display_offset_y, display_width, display_height);
          frame_count++;
          offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, vsync_multiple);
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_frame, NULL);
      }
      // Free the packet that was allocated by av_read_frame
      av_free_packet(&packet);
    }
  } while ( (!interrupt_received)&&(readed>=0) );

  av_free(output_buffer);
  av_frame_free(&output_frame);
  av_frame_free(&decode_frame);

  // Close the codecs
  avcodec_close(pCodecCtx);
  avcodec_close(pCodecCtxOrig);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  return frame_count;
}

//-----------------------------------------------------------------------
bool file_exist(char *fname) {
  struct stat buffer;
  return (stat(fname, &buffer)==0);
}
//-----------------------------------------------------------------------
static int usage(const char *progname) {
  printf("usage: %s [options] \n",progname);
  printf("\nGeneral LED matrix options:\n");
  rgb_matrix::PrintMatrixFlags(stderr);
  printf("\nThe -w, -t and -l options apply to the following images "
          "until a new instance of one of these options is seen.\n"
          "So you can choose different durations for different images.\n");
  return 1;
}
//-----------------------------------------------------------------------
char str[8];
char * BytetoHex(unsigned char val) {
  char matrix[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  str[0]='0';
  str[1]='x';
  str[2]=matrix[(val >> 4) & 0x0f];
  str[3]=matrix[val & 0x0f];
  str[4]=0;
  return str;
}
//-----------------------------------------------------------------------
pixcol Hex6toRGB(char text[]) {
  pixcol col;
  short int i, val, digit=0, p;
  
  for(i=1, p=0, val=0; i>=0; i--, p++) {
     if(text[i]>='0' && text[i]<='9') digit=text[i]-0x30;
     else if((text[i]>='A' && text[i]<='F') || (text[i]>='a' && text[i]<='f')){
       switch(text[i])   {
         case 'A': case 'a': digit=10; break;
         case 'B': case 'b': digit=11; break;
         case 'C': case 'c': digit=12; break;
         case 'D': case 'd': digit=13; break;
         case 'E': case 'e': digit=14; break;
         case 'F': case 'f': digit=15; break;
       }
     }
     val+= digit*pow(16,p);
  }
  col.rr = val;

  for(i=3, p=0, val=0; i>=2; i--, p++) {
     if(text[i]>='0' && text[i]<='9') digit=text[i]-0x30;
     else if((text[i]>='A' && text[i]<='F') || (text[i]>='a' && text[i]<='f')){
       switch(text[i])   {
         case 'A': case 'a': digit=10; break;
         case 'B': case 'b': digit=11; break;
         case 'C': case 'c': digit=12; break;
         case 'D': case 'd': digit=13; break;
         case 'E': case 'e': digit=14; break;
         case 'F': case 'f': digit=15; break;
       }
     }
     val+= digit*pow(16,p);
  }
  col.gg = val;

  for(i=5, p=0, val=0; i>=4; i--, p++) {
     if(text[i]>='0' && text[i]<='9') digit=text[i]-0x30;
     else if((text[i]>='A' && text[i]<='F') || (text[i]>='a' && text[i]<='f')){
       switch(text[i])   {
         case 'A': case 'a': digit=10; break;
         case 'B': case 'b': digit=11; break;
         case 'C': case 'c': digit=12; break;
         case 'D': case 'd': digit=13; break;
         case 'E': case 'e': digit=14; break;
         case 'F': case 'f': digit=15; break;
       }
     }
     val+= digit*pow(16,p);
  }
  col.bb = val;
  return col;
}
//-----------------------------------------------------------------------
//Lit la prochaine cmd dans msg(msg_ptr).
//La cmd doit etre du type '<CMD:nn>', si le ':' ou le '>' est omis ce n'est une cmd et
//le tout est simplement affiché comme du texte.
//La cmd est renvoyée dans 'cmd', le parametre nn dans 'cmdparam' (string).
void getcmd(void) {
const char separ=':';  // séparateur entre cmd et param
short separpos=0;

  //par défaut cmd = 0
  cmd[0]='\0';
  
  //sauve ptr si pas bonne cmd
  short msg_ptr0=msg_ptr;

  //next char ligne de texte ou de cmd ?
  char c=msg[msg_ptr++];
    
  if (c=='<') {
     short n,p;
     for (n=0 ; n<256 ; n++) {
       c=msg[msg_ptr++];
       cmd[n]=c;
       //teste si separateur
       if (c==separ) separpos=n;
       //teste si fin de la cmd
       if (c=='>') {
         //si la commande est valide - délimite cmd
         cmd[separpos]='\0';
         break;
       }
     }
     
     //strippe le cmdparam
     if ( separpos > 0) {
       for (p=0, n=separpos+1 ; n<256 ; p++, n++) {
         c=cmd[n];
         if (c=='>') break; 
         cmdparam[p]=cmd[n];
         cmdparam[p+1]='\0';
       }
     }
     else {
       //cmd pas correcte (pas de :) revient en arrière
       msg_ptr=msg_ptr0;
       affchar=msg[msg_ptr++];
       //remet cmd = 0
       cmd[0]='\0';
     }
  }
  else {
    //si c'est pas une commande on renvoie un seul char à afficher
    affchar=c;
  }
}
//-----------------------------------------------------------------------
//Fait un Cadre liseré sur le tour de l'écran (InkColor), lisere=epaisseur
//Paint le masque cadremask[][] de la taille de l'écran dans buff
//avec un décalage de screenH vers le bas
void Cadre(void) {
unsigned short n,x,y;

 //Init le masque
 for (x = 0; x<screenW; x++) {
   for (y = screenH; y < screenH+screenH; y++) {
     cadremask[x][y] = black;
   }
 }

 //Fait traits horiz haut & bas
 for (n=0; n < lisere ; n++) {
   for (x = 0; x<screenW; x++) {
     cadremask[x][screenH+n] = inkcolor;
     cadremask[x][screenH+screenH-1-n] = inkcolor;
   }
 }
 //Fait traits Verticaux gauche & droite
 for (n=0; n < lisere ; n++) {
   for (y = screenH; y < screenH+screenH; y++) {
     cadremask[n][y] = inkcolor;
     cadremask[screenW-n-1][y] = inkcolor;
   }
 }
}
//-----------------------------------------------------------------------
//Rafraichit la matrix canvas avec buff|]
void Fresh(FrameCanvas *canv) {
unsigned short x, y, ybuff;
pixcol col;

 //x,y balaie la surface écran dans Buff
 for (y = 0, ybuff=screenH; y < screenH; ++y, ++ybuff) {
   for (x = 0; x < screenW; ++x) {
     //recalage sur buff en y
     col = buff[x][ybuff];
     
     //Si applique l'image de fond ?
     if (background != 0)
       if ( (abs(col.rr-backcolor.rr)<3)&&(abs(col.gg-backcolor.gg)<3)&&(abs(col.bb-backcolor.bb)<3) )
         col = backmask[x][ybuff];

     //Si applique le masque du cadre ?
     if (lisere != 0)
       if ((cadremask[x][ybuff].rr)||(cadremask[x][ybuff].gg)||(cadremask[x][ybuff].bb))
         col = cadremask[x][ybuff];
     
     //Paint canvas avec DECALAGE Y de screenH
     canv->SetPixel(x, y, col.rr, col.gg, col.bb);
   }
 }
 //Applique les modif
 canv = matrix->SwapOnVSync(canv, 1);
}
//-----------------------------------------------------------------------
//Inverse Toutes les couleurs RVB
void Invert(RGBMatrix *matx, FrameCanvas *canv, short nb) {
unsigned short n,x,y;
 //Scrolle 'n' coups vers le bas juste de largeur buff écran
 for (n=0; n<nb ; n++) {
   for ( x=0; x<screenW; x++ ) {
     for ( y=screenH; y<screenH+screenH; y++ ) {
       buff[x][y].rr = 255-buff[x][y].rr;
       buff[x][y].gg = 255-buff[x][y].gg;
       buff[x][y].bb = 255-buff[x][y].bb;
     }
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed*4000);
 }
}
//-----------------------------------------------------------------------
//Fait une rotation du buff de ang degrés (sens=1 ou -1)(sens=0 effet acceleration)
void Rotate(RGBMatrix *matx, FrameCanvas *canvas, short nb, short sens) {
 const float deg_to_rad = 2 * 3.14159265 / 360;
 float rot_x, rot_y;
 float ang, cx, cy;
 short x, y, nx, ny, nbrot;
 //buffer carré pour rotation complete
 pixcol buff2[screenW][screenW];
 //image d'origine pour recalcul sans distorsions (effets)
 pixcol buff0[screenW][screenW];

 // centre image (le centre Y est décalé d'une haut d'écran)
 const int cent_x = screenW / 2;
 const int cent_y = screenH + screenH / 2;

 //Sauve image origine
 for (x = 0; x < screenW; ++x) {
   for (y = screenH; y < screenH+screenH; ++y) buff0[x][y] = buff[x][y];
 }

 //commence rotation
 for (nbrot=0; nbrot<nb; nbrot++) {
   if (sens==0) {
     nbrot *= 1.05;
     ang = (float)nbrot * deg_to_rad;
   }
   else ang = (float)nbrot * deg_to_rad * sens;
  
   //préremplit buff2 (en carré!)
   for (x = 0; x < screenW; ++x) {
    for (y = 0; y < screenW; ++y) buff2[x][y] = backcolor;
   }
   
   for (x = 0; x < screenW; ++x) {
     for (y = screenH; y < screenH+screenH; ++y) {
       cx = x - cent_x;  cy = y - cent_y; 
       rot_x = cx * cosf(ang) - cy * sinf(ang);
       rot_y = cx * sinf(ang) + cy * cosf(ang);
       nx = (rot_x + cent_x);  ny = (rot_y + cent_y);
       //test si dans zone de dessin 
       if ( (nx>=0) && (nx<screenW) && (ny>=0) && (ny<3*screenH) ) {
         //sauve couleur du pixel
         buff2[nx][ny] = buff0[x][y];
       }
     }
   }
   //recopie image tournée propre
   for (x = 0; x < screenW; ++x) {
     for (y = screenH; y < screenH+screenH; ++y) buff[x][y] = buff2[x][y];
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canvas);
   usleep(2000);
 }
 //Finit par image droite
 for (x = 0; x < screenW; ++x) {
   for (y = screenH; y < screenH+screenH; ++y) buff[x][y] = buff0[x][y];
 }
}
//-----------------------------------------------------------------------
//Fait une rotation du buff de ang degrés (sens=1 ou -1)(sens=0 effet acceleration)
void Twirl(RGBMatrix *matx, FrameCanvas *canvas, short nb) {
 const float deg_to_rad = 2 * 3.14159265 / 360;
 float rot_x, rot_y;
 float ang, ang2, dist, cx, cy;
 short x, y, nx, ny;
 //buffer carré pour rotation complete
 pixcol buff2[screenW][screenW];
 //image d'origine pour recalcul sans distorsions (effets)
 pixcol buff0[screenW][screenW];

 // centre image (le centre Y est décalé d'une haut d'écran)
 const int cent_x = screenW / 2;
 const int cent_y = screenH + screenH / 2;

 //Sauve image origine
 for (x = 0; x < screenW; ++x) {
   for (y = screenH; y < screenH+screenH; ++y) buff0[x][y] = buff[x][y];
 }

 //commence rotation
 for (short nbrot=0; nbrot<nb; nbrot+=3) {
   //angle de base en rad
   ang = (float)nbrot * deg_to_rad;
   
   //préremplit buff2
   for (x = 0; x < screenW; ++x)
     for (y = 0; y < screenW; ++y) buff2[x][y] = backcolor;
   
   for (x = 0; x < screenW; ++x) {
     for (y = screenH; y < screenH+screenH; ++y) {
       cx = x - cent_x;  cy = y - cent_y;
       dist = sqrt(cx*cx + cy*cy);
       //recalc angle en fonction distance au centre
       ang2 = ang/(1+dist/15);
       rot_x = cx * cosf(ang2) - cy * sinf(ang2);
       rot_y = cx * sinf(ang2) + cy * cosf(ang2);
       nx = (rot_x + cent_x);  ny = (rot_y + cent_y);
       //test si dans zone de dessin 
       if ( (nx>=0) && (nx<screenW) && (ny>=0) && (ny<3*screenH) ) {
         //sauve couleur du pixel
         buff2[nx][ny] = buff0[x][y];
       }
     }
   }
   //recopie image tournée propre
   for (x = 0; x < screenW; ++x) {
     for (y = screenH; y < screenH+screenH; ++y) buff[x][y] = buff2[x][y];
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canvas);
   usleep(150);
 }
}
//-----------------------------------------------------------------------
//Fait un Zoom explode du buff de durée n
void Explode(RGBMatrix *matx, FrameCanvas *canvas, short nb) {
 short x, y, nx, ny;
 pixcol p0;

 short fx = 4;

 //commence effet
 for (short nn=0; nn<nb; nn++) {
   
  for (x = 0; x < screenW; ++x) {
    for (y = screenH; y < screenH+screenH; ++y) {
      nx = x+-(fx/2) + rand()%fx;
      ny = y+-(fx/2) + rand()%fx;
      //test si dans zone de dessin 
      if ( (nx>=0) && (nx<screenW) && (ny>=0) && (ny<3*screenH) ) {
        //interverti couleur in->out
        p0=buff[nx][ny];
        buff[nx][ny].rr = (buff[x][y].rr>7)?buff[x][y].rr-8:0;
        buff[nx][ny].gg = (buff[x][y].gg>7)?buff[x][y].gg-8:0;
        buff[nx][ny].bb = (buff[x][y].bb>7)?buff[x][y].bb-8:0;
        buff[x][y] = p0;
      }
      else
        buff[x][y] = backcolor;
    }
  }
  
  fx *= 1.25;
  
  //Recopie portion du buff a afficher dans off_canvas
  Fresh(canvas);
  usleep(speed*2000);
 }
}
//-----------------------------------------------------------------------
//Fait un scroll vers le bas de n pixels (y=0 est en haut)
void Scroll_Down(RGBMatrix *matx, FrameCanvas *canv, short nb) {
unsigned short n,x,y;
 //Scrolle 'n' coups vers le bas juste de largeur buff écran
 for (n=0; n<nb ; n++) {
   for (x = 0; x<screenW; x++) {    // A VOIR !!!
     for (y = ymax_buff-1; y>0; y--) {
       buff[x][y] = buff[x][y-1];
     }
   }
   //efface ligne 0
   for (x = 0; x<screenW; x++) buff[x][0] = backcolor;

   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);

   //Réglage de SPEED
   usleep(speed * 750);
 }
}
//-----------------------------------------------------------------------
//Fait un scroll vers le Haut de n pixels (y=0 est en haut)
void Scroll_Up(RGBMatrix *matx, FrameCanvas *canv, short nb) {
unsigned short n,x,y;
 //Scrolle 'n' en haut juste de la largeur buff écran
 for (n=0; n<nb ; n++) {
   for (x = 0; x<screenW; x++) {        // A VOIR !!!
     for (y = 0; y<ymax_buff; y++) {
       buff[x][y] = buff[x][y+1];
     }
   }
   //efface ligne top bas
   for (x = 0; x<screenW; x++) buff[x][ymax_buff-1] = backcolor;
   
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   
   //Réglage de SPEED
   usleep(speed * 750);
 }
}
//-----------------------------------------------------------------------
//Fait un Clear fondu progressif (n = vitesse) sur le noir
void Fade(RGBMatrix *matx, FrameCanvas *canv, short spd) {
unsigned short n,x,y;
 for (n=0; n<255 ; n++) {
   short b=0;
   for (x = 0; x<screenW; x++) {
     for (y = screenH; y<screenH+screenH; y++) {
       if (buff[x][y].rr>3) { buff[x][y].rr-=4; b++; }
       if (buff[x][y].gg>3) { buff[x][y].gg-=4; b++; }
       if (buff[x][y].bb>3) { buff[x][y].bb-=4; b++; }
     }
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   if (b<1) break;

   //Réglage de SPEED
   usleep(spd * 1000);
 }
 for (x = 0; x<screenW; x++) {
   for (y = screenH; y<screenH+screenH; y++) {
     buff[x][y]=black;
   }
 }
 //Recopie portion du buff a afficher dans off_canvas
 Fresh(canv);
}
//-----------------------------------------------------------------------
//Fait un effet Changement de couleur du Fond (n = vitesse)
void RainbowBack(RGBMatrix *matx, FrameCanvas *canv, short spd) {
unsigned short x,y;
const float deg_to_rad = 2 * 3.14159265 / 360;
//couleur avant decalage
short a0 = backcolor.rr;
short b0 = backcolor.gg;
short c0 = backcolor.bb;
//nouvelle couleur de chaque boucle
short a1, b1, c1;
//pour rotation des couleurs
short col;

 for (col=0; col<720 ; col+=3) {
    a1=128+(short)(127*sinf((col)*deg_to_rad));
    b1=128+(short)(127*sinf((col+120)*deg_to_rad));
    c1=128+(short)(127*sinf((col+240)*deg_to_rad));
    
    for (x=0; x < screenW; x++) {
     for (y=screenH; y < screenH+screenH; y++) {
       //Test si Substitution Couleur INK ?
       short delta = abs(a0-buff[x][y].rr)+abs(b0-buff[x][y].gg)+abs(c0-buff[x][y].bb);
       if (delta < 10) {
         //Ecrit couleur du pixel
         buff[x][y].rr = a1;
         buff[x][y].gg = b1;
         buff[x][y].bb = c1;
       }
     }
    }
    a0=a1; b0=b1; c0=c1;
    
    //Recopie portion du buff a afficher dans off_canvas
    Fresh( canv);
    //Réglage de SPEED
    usleep(spd * 500);
 }

 //Remet la couleur Ink d'origine
  for (x = 0; x<screenW; x++) {
    for (y = screenH; y<screenH+screenH; y++) {
      //Test si Substitution Couleur Back ?
      short delta = abs(a1-buff[x][y].rr)+abs(b1-buff[x][y].gg)+abs(c1-buff[x][y].bb);
      if (delta < 10) {
        //Ecrit couleur du pixel back1
        buff[x][y] = backcolor;
      }
    }
  }
 //Recopie portion du buff a afficher dans off_canvas
 Fresh(canv);
}
//-----------------------------------------------------------------------
//BLUR - Fait un effet de Flou progressif (n = vitesse)
void Blur(RGBMatrix *matx, FrameCanvas *canv, short spd) {
unsigned short x,y;
short a, b, c;

 for (x=0; x < screenW; x++) {
   buff[x][0].rr = 0; buff[x][0].gg = 0; buff[x][0].bb = 0;
   buff[x][screenH-1].rr = 0; buff[x][screenH-1].gg = 0; buff[x][screenH-1].bb = 0;
 }
 for (y=screenH; y < screenH+screenH; y++) {
   buff[0][y].rr = 0; buff[0][y].gg = 0; buff[0][y].bb = 0;
   buff[screenW-1][y].rr = 0; buff[screenW-1][y].gg = 0; buff[screenW-1][y].bb = 0;
 }

 for (int nn=0; nn<30 ; nn++) {
    for (x=1; x < screenW-1; x++) {
     for (y=screenH; y < screenH+screenH-1; y++) {
       //fais une moyenne des couleurs
       a = (buff[x-1][y].rr+buff[x+1][y].rr+buff[x][y-1].rr+buff[x][y+1].rr+
              buff[x][y].rr+buff[x+1][y+1].rr+buff[x-1][y-1].rr)>>3;
       buff[x][y].rr = a;
       b = (buff[x-1][y].gg+buff[x+1][y].gg+buff[x][y-1].gg+buff[x][y+1].gg+
              buff[x][y].gg+buff[x+1][y+1].gg+buff[x-1][y-1].gg)>>3;
       buff[x][y].gg = b;
       c = (buff[x-1][y].bb+buff[x+1][y].bb+buff[x][y-1].bb+buff[x][y+1].bb+
              buff[x][y].bb+buff[x+1][y+1].bb+buff[x-1][y-1].bb)>>3;
       buff[x][y].bb = c;
     }
    }
    //Recopie portion du buff a afficher dans off_canvas
    Fresh(canv);
    //Réglage de SPEED
    usleep(spd * 5000);
 }
}
//-----------------------------------------------------------------------
//Fait un effet Changement de couleur d'encre (n = vitesse)
void RainbowInk(RGBMatrix *matx, FrameCanvas *canv, short spd) {
const float deg_to_rad = 2 * 3.14159265 / 360;
unsigned short x,y;
//couleur avant decalage
short a0=inkcolor.rr;
short b0=inkcolor.gg;
short c0=inkcolor.bb;
//nouvelle couleur de chaque boucle
short a1, b1, c1;
//pour rotation des couleurs
short col;

 for (col=0; col<720 ; col+=3) {
    a1=128+(short)(127*sinf((col)*deg_to_rad));
    b1=128+(short)(127*sinf((col+120)*deg_to_rad));
    c1=128+(short)(127*sinf((col+240)*deg_to_rad));
    
    for (x=0; x < screenW; x++) {
     for (y=screenH; y < screenH+screenH; y++) {
       //Test si Substitution Couleur INK ?
       short delta = abs(a0-buff[x][y].rr)+abs(b0-buff[x][y].gg)+abs(c0-buff[x][y].bb);
       if (delta < 10) {
         //Ecrit couleur du pixel
         buff[x][y].rr = a1;
         buff[x][y].gg = b1;
         buff[x][y].bb = c1;
       }
     }
    }
    a0=a1; b0=b1; c0=c1;

    //Recopie portion du buff a afficher dans off_canvas
    Fresh(canv);
    //Réglage de SPEED 
    usleep(spd * 500);
 }

  //Remet la couleur Ink d'origine
  for (x = 0; x<screenW; x++) {
    for (y = screenH; y<screenH+screenH; y++) {
      //Test si Substitution Couleur INK ?
      short delta = abs(a1-buff[x][y].rr)+abs(b1-buff[x][y].gg)+abs(c1-buff[x][y].bb);
      if (delta < 10) {
        //Ecrit couleur du pixel ink1
        buff[x][y] = inkcolor;
      }
    }
  }
 //Recopie portion du buff a afficher dans off_canvas
 Fresh(canv);
}
 
//-----------------------------------------------------------------------
//BLINK - Fait clignoter que le Texte Inkcolor (n = fois)
void Blink(RGBMatrix *matx, FrameCanvas *canv, short nb) {
unsigned short x, y, nn;
pixcol buff0[screenW][ymax_buff];
 //sauve l'écran
 for (x=0; x<screenW; x++) {
   for (y=0; y<ymax_buff; y++)
     //Ecrit couleur du pixel
     buff0[x][y] = buff[x][y];
 }

 //Debut du clignotement
 for (nn=0; nn<nb ; nn++) {

   for (x=0; x<screenW; x++) {
     for (y=0; y<ymax_buff; y++)
       //Ecrit couleur du pixel
       buff[x][y] = backcolor;
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed*5000);
   
   //Remet la couleur Ink d'origine
   for (x=0; x<screenW; x++) {
     for (y=0; y<ymax_buff; y++)
       //Re Ecrit couleur du pixel
       buff[x][y] = buff0[x][y];
   }

  //Recopie portion du buff a afficher dans off_canvas
  Fresh(canv);
  //Réglage de SPEED
  usleep(speed*12000);
 }
}
//-----------------------------------------------------------------------
//Décale le buffer , 1 pixel à GAUCHE 
void Scroll_Left(short nb) {
unsigned short n,x,y;
 //Scrolle 'n' coups à gauche tout le buffer a afficher
 for (n=0 ; n<nb ; n++) {
   for (y = 0; y<ymax_buff; y++)
     for (x = 0; x < xmax_buff-1; x++) buff[x][y] = buff[x+1][y];
 }
}
//-----------------------------------------------------------------------
//Décale le buffer , 1 pixel à DROITE 
void Scroll_Right(short nb) {
unsigned short n,x,y;
 //Scrolle 'n' coups à droite
 for (n=0 ; n<nb ; n++) {
   for (y = 0; y < ymax_buff; y++)
     for (x = xmax_buff-1; x>0; x--) buff[x][y] = buff[x-1][y];
 }
}
//-----------------------------------------------------------------------
//Fait un effet de SECOUSSE gauche-droite (n fois)
void Shake(RGBMatrix *matx, FrameCanvas *canv, short nb) {
unsigned short n;
 //Scrolle 'n' en haut juste de la largeur buff écran
 for (n=0; n<nb ; n++) {
 
   Scroll_Left(1);  
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed * 1500);

   Scroll_Right(1);  
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed * 1500);

//   Scroll_Up(matx, canv,1);  
//   Scroll_Down(matx, canv,1);  
 }
}
//----------------------------------------------------------------------
//Recopie une image(ou portion)(calée sur dx,dy) sur l'écran à (destx,desty)
void ImgtoBuff(const Magick::Image img,short dx,short dy,short destx,short desty,short solid) {
short x, y, nx, ny, delta;
uint8_t xr,xg,xb;
short maxx = img.columns();
short maxy = img.rows();

  for (y = 0; y < screenH; ++y) {
    ny=dy+y;
    if (ny>=maxy) break;
    short wry = y+wry_ptr;
    if (wry >= ymax_buff) break;

    for (x = 0; x < screenW; ++x) {
      nx= dx + x;
      if (nx>=maxx) break;
      short wrx = x+wrx_ptr;
      if (wrx >= xmax_buff) break;

      const Magick::Color &c = img.pixelColor(nx, ny);
      xr = ScaleQuantumToChar(c.redQuantum());
      xg = ScaleQuantumToChar(c.greenQuantum());
      xb = ScaleQuantumToChar(c.blueQuantum());      

      short dnx=destx+x;
      short dny=desty+y;
       //Test pour dessin transparent pas sur Couleur fond ?
      if (solid==0) {
        //mode transparent
        delta = abs(xr-backcolor0.rr)+abs(xg-backcolor0.gg)+abs(xb-backcolor0.bb);
        if (delta > 9) {          
           buff[dnx][dny].rr = xr;
           buff[dnx][dny].gg = xg;
           buff[dnx][dny].bb = xb;
        }
      } else {
        //mode solide
        buff[dnx][dny].rr = xr;
        buff[dnx][dny].gg = xg;
        buff[dnx][dny].bb = xb;
      }
    }
  }
}
//-----------------------------------------------------------------------
//Barbouille l'écran avec une image motif (60 fois)
//Dessin en mode transparent = couleur de fond backcolor0 (noir par défaut)
void Splash1(RGBMatrix *matx, FrameCanvas *canv, char *file) {
unsigned short n, nx, ny;

 Magick::Image img;
 img.read(file);

 for (n=0; n<50 ; n++) {
   //recopie que si pas au bout du buff
   nx = -img.columns() + rand()%screenW;
   ny = screenH -(img.rows()/2) + rand()%screenH;
   
   //copie img transparente new position
   ImgtoBuff(img, 0, 0, nx, ny, 0);
   
   //Affiche le buff dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed * 5000);
 }
}
//-----------------------------------------------------------------------
//Barbouille l'écran avec une image motif
//Dessin en mode transparent = couleur de fond backcolor0 (noir par défaut)
void Splash2(RGBMatrix *matx, FrameCanvas *canv, char *file) {
unsigned short nx, ny;

 Magick::Image img;
 img.read(file);

 for (ny=screenH; ny<screenH+screenH ; ) {
   for (nx=0; nx<screenW ; ) {
     //copie img transparente new position
     ImgtoBuff(img, 0, 0, nx, ny, 0);

     //Affiche le buff dans off_canvas
     Fresh(canv);
     //Réglage de SPEED
     usleep(speed * 6000);

     nx += img.columns()*0.75;
   }
   ny += img.rows()*0.75;
 }
}
//----------------------------------------------------------------------
//Fondu enchainé sur l'image a venir
void Fondu(RGBMatrix *matx, FrameCanvas *canv, char *file) {
unsigned short n, x, y, col;
uint8_t xr,xg,xb;
//sauve image d'origine
pixcol buff0[xmax_buff][ymax_buff];

 Magick::Image img;
 img.read(file);

 //Sauve image origine
 for (x = 0; x < xmax_buff; ++x)
   for (y = 0; y < ymax_buff; ++y)
     buff0[x][y] = buff[x][y];

 for (n=1; n<22; n++) {
   for (y = 0; y < img.rows(); ++y) {
      for (x = 0; x < img.columns(); ++x) {
        const Magick::Color &c = img.pixelColor(x, y);
        short ny = y + screenH;
        xr = ScaleQuantumToChar(c.redQuantum());
        col = ((n*xr) +  10*buff0[x][ny].rr)/(n+10);
        buff[x][ny].rr = col;
        
        xg = ScaleQuantumToChar(c.greenQuantum());
        col = ((n*xg) +  10*buff0[x][ny].gg)/(n+10);
        buff[x][ny].gg = col;
        
        xb = ScaleQuantumToChar(c.blueQuantum());
        col = ((n*xb) +  10*buff0[x][ny].bb)/(n+10);
        buff[x][ny].bb = col;    
      }
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(speed * 2500);
 }
 //Finis avec image finale
 ImgtoBuff(img, 0, 0, wrx_ptr, wry_ptr, solid);
 //Recopie portion du buff a afficher dans off_canvas
 Fresh(canv);
}
//-----------------------------------------------------------------------
//Effet de pixelisation jusqu'au gris total
void Pixelise(RGBMatrix *matx, FrameCanvas *canv, short s) {
short sumr=0,sumg=0,sumb=0, x, y, nx, ny;
short pix;

 //Largeur du carré de pixelise grossisant
 for (pix=2 ; pix<screenW/4 ; pix=pix*1.5) {
 
   for (x = 0; x<screenW; x+=pix) {
     for (y = screenH; y<screenH+screenH; y+=pix) {
       //calc moyenne du pixel
       for (nx=0; nx<pix; nx++)
         for (ny=0; ny<pix; ny++) {
           sumr += buff[x+nx][y+ny].rr;
           sumg += buff[x+nx][y+ny].gg;
           sumb += buff[x+nx][y+ny].bb;
       }
       //Calc la moyenne
       short pixpix=pix*pix*1.25;
       sumr = sumr/pixpix;
       sumg = sumg/pixpix;
       sumb = sumb/pixpix;
       //applique la moyenne
       for (nx=0; nx<pix; nx++)
         for (ny=0; ny<pix; ny++) {
           buff[x+nx][y+ny].rr=sumr;
           buff[x+nx][y+ny].gg=sumg;
           buff[x+nx][y+ny].bb=sumb;
       }
     }
   }
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(s * 12000);
 }
}
//-----------------------------------------------------------------------
//Divise l'écran en deux en grandissant et efface tout
void Divide2(RGBMatrix *matx, FrameCanvas *canv, short s) {
unsigned short n, x, y;

 //Scrolle 1/2 buffer à gauche
 for (n=0 ; n<screenW/2 ; n++) {

   //efface colonne du milieu
   for (y = screenH; y<screenH+screenH; y++) {
     buff[screenW/2][y] = backcolor;
     //moitié gauche -> gauche
     for (x = 0; x < screenW/2; x++) buff[x][y] = buff[x+1][y];
     //moitié droite -> droite 
     for (x = screenW; x>screenW/2; x--) buff[x][y] = buff[x-1][y];
   }
    
   //Recopie portion du buff a afficher dans off_canvas
   Fresh(canv);
   //Réglage de SPEED
   usleep(s * 1000);
  }
}
//-----------------------------------------------------------------------
//Efface l'écran (et le buffer) - change pas wr_xptr
void Clear(void) {
unsigned short x,y;
  //raz
  for (y = 0; y<ymax_buff; y++) {
    for (x = 0; x<xmax_buff-1; x++) buff[x][y] = backcolor;
  }
}
//-----------------------------------------------------------------------
// IMGSWP UP/DWN : Affiche une grande image en balayage (images/xxx.xxx)
void ImgSwp(RGBMatrix *matx, FrameCanvas *canv, char *file, short sens) {
short dx, dy;
short nx=0, ny=0;
const short spd0=500;

 Magick::Image img;
 img.read(file);
 
 dx = img.columns()-screenW;
 if (dx<0) dx=0;
 dy = img.rows()-screenH;
 if (dy<0) dy=0;

  //focus vers la droite 
  if (sens==2) {
   for (nx=0; nx<=dx; nx++) {
     ImgtoBuff(img, nx, ny, wrx_ptr, wry_ptr, solid);
     //Recopie portion du buff a afficher dans off_canvas
     Fresh(canv);
     usleep(speed * spd0);
   }
  }
  //focus vers le bas 
  if (sens==0) {
   for (ny=0; ny<dy; ny++) {
     ImgtoBuff(img, nx, ny, wrx_ptr, wry_ptr, solid);
     //Recopie portion du buff a afficher dans off_canvas
     Fresh(canv);
     usleep(speed * spd0);
   }
  }
  //focus vers la gauche 
  if (sens==3) {
   for (nx=dx; nx>0; nx--) {
     ImgtoBuff(img, nx, ny, wrx_ptr, wry_ptr, solid);
     //Recopie portion du buff a afficher dans off_canvas
     Fresh(canv);
     usleep(speed * spd0);
   }
  }
  //focus vers le Haut 
  if (sens==1) {
   for (ny=dy; ny>0; ny--) {
     ImgtoBuff(img, nx, ny, wrx_ptr, wry_ptr, solid);
     //Recopie portion du buff a afficher dans off_canvas
     Fresh(canv);
     usleep(speed * spd0);
   }
  }
}
//-----------------------------------------------------------------------
//Lecture du texte complet à afficher
char* getINI(const char fname[128], const char cmdname[128]) {
FILE *fp;
char *line;
static char cmd[MAXLENMSG];
size_t len=0;
ssize_t read;
short q,r;

  fp=fopen(fname,"r");
  if (fp!=NULL) {
    while ((read = getline(&line,&len,fp))!=-1) {
      //teste presence de cmdname dans la ligne
      char *ret = strstr(line,cmdname);
      if (ret) {
        //extrait le msg entre les " "
        strcpy(cmd,ret);     

        for (q=0; q<MAXLENMSG; q++)
          if (ret[q] ==  '"') break;

        for (r=0, q++; r<MAXLENMSG; r++, q++) {
          if (ret[q]=='"') break;
          cmd[r]=ret[q];   
        }
        cmd[r]='\0';
        fclose(fp);
        return(cmd);
      }
    }
    fclose(fp);
  }
  return('\0');
}
//-----------------------------------------------------------------------
//Lecture du texte complet à afficher
void getMsg(const char fname[128]) {
FILE *fp;
char *line;
size_t len=0;
ssize_t read;

 fp=fopen(fname,"r");
 if (fp!=NULL) {
   while ((read = getline(&line,&len,fp))!=-1) {
     //teste presence de cmdname dans la ligne
     strcat(msg, line);
   }
   fclose(fp);
 }
}
//-----------------------------------------------------------------------
void ImgPaint(FrameCanvas *canv, char *para, short scaling) {
char file[128];
std::vector<Magick::Image> img0, imgs;
uint32_t delai_us=100*speed;
short cy=0;   // recentrage en y si imgH grande
  
  //construit le nom du fichier
  strcpy(file, "../images/");
  strcat(file, para);
  if ( !file_exist(file) ) return;

  readImages(&img0, file);
  
  //Test si Gif-animé ?
  if (img0.size()>1) {
     //GIF animé - mise au propre
     Magick::coalesceImages(&imgs, img0.begin(), img0.end());
      
     //recentrage image en y si imgH > screenH
     if (imgs[0].rows() > screenH) cy=(imgs[0].rows() - screenH)/2;
     
     //recopie que si pas au bout du buff
     for(unsigned short w=0; w<img0.size(); w++) {
        //scaling sur la Largeur=écran
        if (scaling & 2) {
          short nh=(screenW * imgs[w].rows()) / imgs[w].columns();
          short ny=(nh>screenH)?(nh-screenH):0;
          imgs[w].resize(Geometry(screenW,nh,0,ny));
          //recentrage image en y si imgH > screenH
          if (nh > screenH) cy=(nh - screenH)/2;
        }
        //scaling sur la hauteur=écran
        if (scaling & 4) {
          short nw=(screenH * imgs[w].columns()) / imgs[w].rows();
          imgs[w].resize(Geometry(nw, screenH, 0, 0));
          //pas de centrage en y !
          cy=0;
        }
        delai_us = 10000*imgs[w].animationDelay();

        //copie l'image
        ImgtoBuff(imgs[w], 0, cy, wrx_ptr, wry_ptr, solid);
        //Recopie portion du buff a afficher dans off_canvas
        Fresh(canv);   
        //SPEED selon le gifanim
        usleep(delai_us);
     }
     //Va vers la droite dans 'buff'
     wrx_ptr += imgs[0].columns();     // !!!!!!!!!!!!
  }
  else {
     //recentrage image en y si imgH > screenH
     if (img0[0].rows() > screenH) cy=(img0[0].rows() - screenH)/2;
     
     //Image simple - copie que si pas au bout du buff
     //scaling sur la Largeur=écran
     if (scaling & 2) {
       short nh=(screenW * img0[0].rows()) / img0[0].columns();
       short dy=(nh>screenH)?(nh-screenH):0;
       img0[0].resize(Geometry(screenW,nh,0,dy));
       //recentrage image en y si imgH > screenH
       if (nh > screenH) cy=(nh - screenH)/2;
     }
     //scaling sur la hauteur=écran
     if (scaling & 4) {
       short nw=(screenH * img0[0].columns()) / img0[0].rows();
       img0[0].resize(Geometry(nw, screenH, 0, 0));
       //pas de centrage en y !
       cy=0;
     }
     //copie l'image
     ImgtoBuff(img0[0], 0, cy, wrx_ptr, wry_ptr, solid);

    //Va vers la droite dans 'buff'
    wrx_ptr += img0[0].columns();     // !!!!!!!!!!!
  }
}
//-----------------------------------------------------------------------
//Acchiche le car dans buff[] et avance le curseur
//Tiens compte transparence, fonte...
void PrintChar(char car) {
unsigned short delta, x, y;
uint8_t xr,xg,xb;
//pour eviter dentelures apres resize des chars)
//ecart de couleur entre back/ink (rgb cumulé)
const short DELTACOL = 230;
//nom d'un fich de char
char filename[128];
Magick::Image img;
  
  //Remplace le "€" en vrai ascii par le code 0x7F !!!
  if ((int)car==226) car=(char)0x7f;
  
  //une seule lettre à afficher
  strcpy(filename, "../fonts/");
  strcat(filename, font);
  strcat(filename, "/");
  strcat(filename, BytetoHex((int)car));
  strcat(filename,".gif");

  if ( file_exist(filename) ) {
    img.read(filename);
    //Rescale eventuel à Fontsize (si fontsize est redéfini)
    if (fontsize!=0) {
       short nw=(img.columns()*fontsize)/img.rows();
       img.scale(Geometry(nw, fontsize, 0, 0));
    }
    //recopie que si pas au bout du buff
    if (wrx_ptr+img.columns() < xmax_buff) {
      for (x = 0; x < img.columns(); ++x) {
       for (y = 0; y < img.rows(); ++y) {
         const Magick::Color &c = img.pixelColor(x, y);
         
         //xr,g,b couleur de la lettre de base
         xr = ScaleQuantumToChar(c.redQuantum());
         xg = ScaleQuantumToChar(c.greenQuantum());
         xb = ScaleQuantumToChar(c.blueQuantum());
         
         if (solid > 0) {
           //Test si changement de Couleur fond ?
           delta = abs(xr-backcolor0.rr)+abs(xg-backcolor0.gg)+abs(xb-backcolor0.bb);
           if (delta > DELTACOL) {
             xr=inkcolor.rr; xg=inkcolor.gg; xb=inkcolor.bb;
           }
           else {
             //Sinon Couleur fond (bloque texte à 2 couleurs)
             xr=backcolor.rr; xg=backcolor.gg; xb=backcolor.bb;
           }
           //Ecrit couleur du pixel
           short nx = x+wrx_ptr+crenage;
           short ny = y+wry_ptr;
           buff[nx][ny].rr = xr;
           buff[nx][ny].gg = xg;
           buff[nx][ny].bb = xb;
         }
         else {
           //mode transparent opaque=0
           short nx = x+wrx_ptr + crenage;
           short ny = y+wry_ptr;
           delta = abs(xr-backcolor0.rr)+abs(xg-backcolor0.gg)+abs(xb-backcolor0.bb);
           //on est pas sur fond de caractere ?
           if (delta > DELTACOL) {
             //Sinon Couleur INK (bloque texte à 2 couleurs)
             xr=inkcolor.rr; xg=inkcolor.gg; xb=inkcolor.bb;
             buff[nx][ny].rr = xr;
             buff[nx][ny].gg = xg;
             buff[nx][ny].bb = xb;
           }
         }
       }
      }
    }
    //va vers la droite dans 'buff'
    wrx_ptr += img.columns()+crenage;
  }
}
//-----------------------------------------------------------------------
//
//            MAIN    PROG    PRINCIPAL
//
//-----------------------------------------------------------------------
int main(int argc, char *argv[]) {
 Magick::InitializeMagick(*argv);
 //si attente que wr ptrx à telle position
 unsigned short x, y;
 //flag pour ralentissement
 short slowing=0;
 //nom d'un fich de char
 char filename[128];
 Magick::Image img;

  srand(time(NULL));

  if (!rgb_matrix::ParseOptionsFromFlags(&argc,&argv,&matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }
  
  int opt;
  while ((opt = getopt(argc, argv, "c:P:LhCR")) != -1) {
    switch (opt) {
    case 'c':
      printf("Instead of deprecated -c, use --led-chain=%s instead.\n",optarg);
      matrix_options.chain_length = atoi(optarg);
      break;
    case 'P':
      matrix_options.parallel = atoi(optarg);
      break;
    case 'L':
      printf("-L is deprecated. Use\n\t--led-pixel-mapper=\"U-mapper\" --led-chain=4\ninstead.\n");
      return 1;
      break;
    case 'R':
      printf("-R is deprecated.Use --led-pixel-mapper=\"Rotate:%s\" instead.\n", optarg);
      return 1;
      break;
    default:
      return usage(argv[0]);
    }
  }

  //Reprend éventuel réglages architecture en fichier message
  chain = atoi(getINI("panel-config.txt", "led-chain"));
  chain = (chain>0)? chain : 1;  
  matrix_options.chain_length = chain;
  
  parallel = atoi(getINI("panel-config.txt", "led-parallel"));
  parallel = (parallel>0)? parallel : 1;
  matrix_options.parallel = parallel;

  //Reprend éventuel réglages architecture en fichier message
  rows = atoi(getINI("panel-config.txt", "led-rows"));
  rows = (rows>0)? rows : 32;  
  matrix_options.rows = rows;
  cols = atoi(getINI("panel-config.txt", "led-cols"));
  cols = (cols>0)? cols : 32;  
  matrix_options.cols = cols;

  // Prepare matrix
  matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL) return 1;
  
  off_canvas = matrix->CreateFrameCanvas();
  off_canvas->Clear();

  //Init les variables dimensions
  screenW = matrix->width();
  screenH = matrix->height();

  printf("Size: %dx%d. Hardware gpio mapping: %s\n\n",
         matrix->width(), matrix->height(), matrix_options.hardware_mapping);
  printf("Matrices en chaine par ligne : %d\n\n", chain);
  printf("Lignes paralleles : %d\n\n", parallel);
  printf("Matrices %d X %d Leds\n\n", rows, rows);

  //Fonte par défaut
  strcpy(font, getINI("panel-config.txt", "fonte"));
  printf("Fonte = %s\n\n", font);

  //Lecture du Message complet à afficher
  getMsg("message.txt"); 
  printf("Texte = %s\n\n",msg);
 
 //Pointeur de dessin dans le buffer 'buff'
 // Commence à droite de l'écran / une hauteur d'écran)
 wrx_ptr = screenW;
 wry_ptr = screenH;
 
 signal(SIGTERM, InterruptHandler);
 signal(SIGINT, InterruptHandler);
 
 //Ptr du texte 
 msg_ptr=0;
 
 //Vitesse de défilement (0=à fond, 100=maxi très lent)
 speed = 25;
 
 //epaisseur du cadre en pixel
 lisere = 0;

 //...................................................
 while (!interrupt_received ) {
    
   //vas chercher next affchar/cmd
   getcmd();
   
   //Sort si fin de texte  
   if ( (cmd[0]==0) && (affchar=='\0') ) break;
   
   // Lit Une lettre seule (font\xxx.gif) de affchar
   if ( (strlen(cmd)==0) && (affchar!='\0') ) {
     PrintChar(affchar);
   }
   //sinon une commande a traiter
   else {
       //========= BLUR:n = fait un flou progressif (speed = n)
       if (!strcmp(cmd,"BLUR")) {
         Blur(matrix, off_canvas, atoi(cmdparam));
       }
       //========= BACK:rrggbb = 'ABEB72' couleur de fond rgb des chars en hexa
       if (!strcmp(cmd,"BACK")) {
         pixcol col = Hex6toRGB(cmdparam);
         backcolor = col;
       }
       //========= BACKGRD:imgname : fixe une image de background
       if (!strcmp(cmd,"BACKGRD")) {
         strcpy(filename, "../images/");
         strcat(filename, cmdparam);
         if ( file_exist(filename) ) {
           //Flag que background ok
           background = 1;
           img.read(filename);
           //printf("img= %s\n",filename);
           //recopie que si pas au bout du buff
           if (img.columns() < xmax_buff) {           
            for (y = 0; y < img.rows(); ++y) {
              if (y>=ymax_buff) break;
              for (x = 0; x < img.columns(); ++x) {
                const Magick::Color &c = img.pixelColor(x, y);
                backmask[x][y+screenH].rr = ScaleQuantumToChar(c.redQuantum());
                backmask[x][y+screenH].gg = ScaleQuantumToChar(c.greenQuantum());
                backmask[x][y+screenH].bb = ScaleQuantumToChar(c.blueQuantum());
              }
            }
           }
         }
         else {
           //Sinon plus de background
           background = 0;
           for (y = 0; y < ymax_buff; ++y) {
             for (x = 0; x < xmax_buff; ++x)
               backmask[x][y] = backcolor;
           }
         }
       }
       //========= BLINK:n = fait clignoter n fois le Texte seul
       if (!strcmp(cmd,"BLINK")) {
         Blink(matrix, off_canvas, atoi(cmdparam));
       }
       //========= CADRE:n = dessin liseré autour ecran (epaisseur n)
       if (!strcmp(cmd,"CADRE")) {
         lisere = atoi(cmdparam);
         Cadre();
       }
       //========= CLEAR:x = efface l'écran complet
       // si n=0 remet pointeur write à gauche
       // si n=1 change pas position
       if (!strcmp(cmd,"CLEAR")) {
         if (atoi(cmdparam) == 0) { wrx_ptr=0; wry_ptr=screenH; }
         Clear();
       }
       //========= CRENAGE:n = retrait en -/+ en pixel par carac
       if (!strcmp(cmd,"CRENAGE")) {
         crenage = atoi(cmdparam);
         // printf("cren= %d\n", crenage);
       }
       //========= DIVIDE:n = divise ecran en 2 parties et vide tout (n=vitesse)
       if (!strcmp(cmd,"DIVIDE")) {
         Divide2(matrix, off_canvas,atoi(cmdparam));
       }
       //========= DO: = début d'une boucle LOOP:n
       if (!strcmp(cmd,"DO")) {
         msg_ptr_do = msg_ptr;
       }     
       //========= DOWN:n = scroll vers le bas de n pixels
       if (!strcmp(cmd,"DOWN")) {
         Scroll_Down(matrix, off_canvas, atoi(cmdparam));
       }
       //========= EXPLODE:n = effet d'explosion de n fois
       if (!strcmp(cmd,"EXPLODE")) {
         Explode(matrix, off_canvas, atoi(cmdparam));
       }
       //========= FADE:n = fait un clear fade progressif (vers le noir fond compris)
       if (!strcmp(cmd,"FADE")) {
         Fade(matrix, off_canvas, atoi(cmdparam));
       }
       //========= FIXE:n = mode Fixe=1 ou Shift droite = 0
       if (!strcmp(cmd,"FIXE")) {
         fixe = (atoi(cmdparam)==0)?0:1;
         //printf( "fixe= %d\n",fixe);
       }
       //--------- FONDU: Fondu-enchainé sur une Image (images/xxx.xxx)
       if (!strcmp(cmd,"FONDU")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) Fondu(matrix, off_canvas, filename);
       }
       //========= FONT:n = change de Fonte (doit correspondre au répertoire)
       if (!strcmp(cmd,"FONT")) {
         strcpy(font, cmdparam);
         //printf("fonte= %s\n",font);
       }
       //========= FONTSIZE:n = change Taille de Fonte (rescale la hauteur +/-)
       if (!strcmp(cmd,"FONTSIZE")) {
         fontsize = (atoi(cmdparam)>8)?atoi(cmdparam):0;
       }
       //--------- IMG: Affiche une image (images/xxx.xxx)
       if (!strcmp(cmd,"IMG")) {
          ImgPaint(off_canvas, cmdparam, 0);
       }
       //--------- IMGW: Affiche une image rescalée sur Screen Width (images/xxx.xxx)
       if (!strcmp(cmd,"IMGW")) {
          ImgPaint(off_canvas, cmdparam, 2);
       }
       //--------- IMGH: Affiche une image rescalée sur Screen Height (images/xxx.xxx)
       if (!strcmp(cmd,"IMGH")) {
          ImgPaint(off_canvas, cmdparam, 4);
       }
       //--------- IMGSWPUP: Balaie une image (bas vers le haut) (images/xxx.xxx)
       if (!strcmp(cmd,"IMGSWPUP")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) ImgSwp(matrix, off_canvas, filename, 1);
       }
       //--------- IMGSWPDWN: Balaie une image (haut vers le bas) (images/xxx.xxx)
       if (!strcmp(cmd,"IMGSWPDWN")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) ImgSwp(matrix, off_canvas, filename, 0);
       }
       //========= INK:rrggbb = 'ABEB72' couleur écriture rgb des chars en hexa
       if (!strcmp(cmd,"INK")) {
         pixcol col = Hex6toRGB(cmdparam);
         inkcolor = col;
       }
       //========= INVERT:n = image en négatif (n fois)
       if (!strcmp(cmd,"INVERT")) {
         Invert(matrix, off_canvas, atoi(cmdparam));
       }
       //========= LEFT:n = scroll gauche de n pixels
       if (!strcmp(cmd,"LEFT")) {
         for (short q=0; q<atoi(cmdparam); q++) {
           //scroll buff
           Scroll_Left(1);
           //MAJ le buff a afficher dans off_canvas
           Fresh(off_canvas);
           //Réglage de SPEED
           usleep(speed * 500);
         }
       }
       //========= LOOP: = fin d'une boucle LOOP:n
       if (!strcmp(cmd,"LOOP")) {
         //voit si premier passage (ou loop infinie)
         if (do_loops < 0) do_loops = atoi(cmdparam);
         //si boucle infinie ?
         if (do_loops == 0) msg_ptr=msg_ptr_do;
         //Si pas premier passage
         if (do_loops >= 1) {
           do_loops--;
           if (do_loops >= 1) msg_ptr=msg_ptr_do;
           else do_loops = -1;
         }
       }     
       //=========== PAUSE:n =  pause n seconde
       if (!strcmp(cmd,"PAUSE")) {
         sleep(atoi(cmdparam));  
       }
       //========= PIXELISE:n = effet pixelise (speed n)
       if (!strcmp(cmd,"PIXELISE")) {
         Pixelise(matrix, off_canvas, atoi(cmdparam));
       }
       //========= RAINBOWBACK:n = effet couleurs changeantes sur le Fond (n fois)
       if (!strcmp(cmd,"RAINBOWBACK")) {
         RainbowBack(matrix, off_canvas, atoi(cmdparam));
       }
       //========= RAINBOWINK:n = effet couleurs Encre changeantes (n fois)
       if (!strcmp(cmd,"RAINBOWINK")) {
         RainbowInk(matrix, off_canvas, atoi(cmdparam));
       }
       //========= RESET:0 = repart au debut msg
       if (!strcmp(cmd,"RESET")) {
         msg_ptr = 0;
       }
       //========= RIGHT:n = scroll droite de n pixels
       if (!strcmp(cmd,"RIGHT")) {
         for (short q=0; q<atoi(cmdparam); q++) {
           //scroll buff
           Scroll_Right(1);
           //MAJ le buff a afficher dans off_canvas
           Fresh( off_canvas);
           //Réglage de SPEED
           usleep(speed * 500);
         }
       }
       //========= ROTD:n = Rotation Droite de l'image complete (n degres)
       if (!strcmp(cmd,"ROTD")) {
         Rotate(matrix, off_canvas, atoi(cmdparam), 1);
       }
       //========= ROTG:n = Rotation Gauche de l'image complete (n degres)
       if (!strcmp(cmd,"ROTG")) {
         Rotate(matrix, off_canvas, atoi(cmdparam), -1);
       }
       //========= ROTSPEED:n = Rotation de + en + rapide
       if (!strcmp(cmd,"ROTSPEED")) {
         Rotate(matrix, off_canvas, atoi(cmdparam), 0);
       }
       //========= SETX:n = fixe le pointeur X ecriture en pixel
       if (!strcmp(cmd,"SETX")) {
         wrx_ptr = atoi(cmdparam);
       }
       //========= SETY:n = fixe le pointeur Y ecriture en pixel
       if (!strcmp(cmd,"SETY")) {
         //le y0 est en fait à screenH !
         wry_ptr = atoi(cmdparam);
       }
       //========= SHAKE:n = effet de Secousse (n fois)
       if (!strcmp(cmd,"SHAKE")) {
         Shake(matrix, off_canvas, atoi(cmdparam));
       }
       //========= SLOW:n = ralenti progressif du defilement
       if (!strcmp(cmd,"SLOW")) {
         slowing=atoi(cmdparam);
       }
       //========= SOLID:n = mode Fond solide (1) ou transparent (0)
       if (!strcmp(cmd,"SOLID")) {
         solid = (atoi(cmdparam)==0)?0:1;
         //printf("solid= %d\n",solid);
       }
       //========= SPEED:n = vitesse de 0 (à fond) à 250 très lent
       if (!strcmp(cmd,"SPEED")) {
         speed =atoi(cmdparam);
         speed = (speed>250)?250:speed;
         //printf("speed= %d\n",speed);
       }
       //--------- SPLASH1: Barbouille l'écran avec une Image au hasard (images/xxx.xxx)
       if (!strcmp(cmd,"SPLASH1")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) Splash1(matrix, off_canvas, filename);
       }
       //--------- SPLASH: Barbouille l'écran avec une Image en balayant (images/xxx.xxx)
       if (!strcmp(cmd,"SPLASH2")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) Splash2(matrix, off_canvas, filename);
       }
       //========= STEP:n = pas de scroll left (=1)
       if (!strcmp(cmd,"STEP")) {
         step =atoi(cmdparam);
         step = (step>5)?5:step;
         //printf("step= %d\n",step);
       }
       //========= TIME:n = Affiche l'heure hh:mm
       if (!strcmp(cmd,"TIME")) {
         char heure[256];
         struct timespec next_time;
         next_time.tv_sec = time(NULL);
         struct tm tm;
         localtime_r(&next_time.tv_sec, &tm);
         strftime(heure, sizeof(heure), "%H:%M", &tm);
         for (x=0; heure[x]!='\0'; x++)
            PrintChar( heure[x] );
       }
       //========= TWIRL:n = Effet de Tourbillon de l'image complete (n degres)
       if (!strcmp(cmd,"TWIRL")) {
         Twirl(matrix, off_canvas, atoi(cmdparam));
       }
       //========= UP:n = scroll vers le Haut n pixels
       if (!strcmp(cmd,"UP")) {
         Scroll_Up(matrix, off_canvas, atoi(cmdparam));
       }
       //========= VIDEO:file = affiche video mp4
       if (!strcmp(cmd,"VIDEO")) {
         strcpy(filename, "../images/");
         strcat(filename,cmdparam);
         if ( file_exist(filename) ) Video(filename, off_canvas);
       }
       //Raz la commande
       cmd[0]='\0';
   }  // end else commande a traiter

   //(Si Fixe=0) Scrolle à gauche tout le buffer a afficher
   if ( fixe==0 ) {
     while ( wrx_ptr > screenW ) {     
       //Réglage de SPEED
       usleep(speed * 100);
       //scroll buff
       Scroll_Left(step);
       //MAJ le buff a afficher dans off_canvas
       Fresh(off_canvas);
       //wrx_ptr à gauche le write ptr
       wrx_ptr -= step;
       //Si ralenti (SLOW) avant stop
       if (slowing>0) { slowing--; speed = speed+12; }
     }
   } else {
     //Réglage de SPEED
     usleep(speed * 100);
   }

  //MAJ le buff a afficher dans off_canvas
  Fresh(off_canvas);
 
 }  // end while
 //...................................................
  
 // Animation finished. Shut down the RGB matrix.
 matrix->Clear();
 delete matrix;
 return 0;
}
