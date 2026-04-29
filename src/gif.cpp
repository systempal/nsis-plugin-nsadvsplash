#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>


// My gratitute to Andy Key. His GBM lib served as a base
// for this short gif decoder. I only added extraction
// of transparansy color and comments.
//			Takhir
 
#define	byte		unsigned char
#define	word		unsigned short

#define	make_word(a,b)	(((word)a) + (((word)b) << 8))


typedef int GBM_ERR;

#define	GBM_ERR_GIF_BPP		((GBM_ERR) 1100)
#define	GBM_ERR_GIF_TERM	((GBM_ERR) 1101)
#define	GBM_ERR_GIF_CODE_SIZE	((GBM_ERR) 1102)
#define	GBM_ERR_GIF_CORRUPT	((GBM_ERR) 1103)
#define	GBM_ERR_GIF_HEADER	((GBM_ERR) 1104)

#define	GBM_ERR_OK		((GBM_ERR) 0)
#define	GBM_ERR_BAD_MAGIC	((GBM_ERR) 5)
#define	GBM_ERR_READ		((GBM_ERR) 7)
#define	GBM_ERR_WRITE		((GBM_ERR) 8)



typedef unsigned int cword;

typedef struct
	{
	BOOLEAN ilace, errok;
	int bpp;
	byte pal[0x100*3];
	} GIF_PRIV;


typedef struct
	{
	int fd;				/* File descriptor to read */
	int inx, size;			/* Index and size in bits */
	byte buf[255+3];		/* Buffer holding bits */
	int code_size;			/* Number of bits to return at once */
	cword read_mask;		/* 2^code_size-1 */
	} READ_CONTEXT;

typedef struct
	{
	int x, y, w, h, bpp, pass;
	BOOLEAN ilace;
	int stride;
	byte *data, *data_this_line;
	} OUTPUT_CONTEXT;

typedef unsigned int cword;

typedef struct { byte r, g, b; } GBMRGB;

#define	PRIV_SIZE 2000

typedef struct
	{
	int w, h, bpp;			/* Bitmap dimensions                 */
	byte priv[PRIV_SIZE];		/* Private internal buffer           */
   int trc;
   char *szUrl;
	} GBM;




/*...sgif_rhdr:0:*/
GBM_ERR gif_rhdr(int fd, GBM *gbm)
	{
	GIF_PRIV *gif_priv = (GIF_PRIV *) gbm->priv;
	byte signiture[6], scn_desc[7], image_desc[10];
	int img = -1, img_want = 0;
	int bits_gct;

	/* Read and validate signiture block */

	if ( _read(fd, signiture, 6) != 6 )
		return GBM_ERR_READ;
	if ( memcmp(signiture, "GIF87a", 6) &&
	     memcmp(signiture, "GIF89a", 6) )
		return GBM_ERR_BAD_MAGIC;

	/* Read screen descriptor */

	if ( _read(fd, scn_desc, 7) != 7 )
		return GBM_ERR_READ;
	bits_gct = gif_priv->bpp = (scn_desc[4] & 7) + 1;
//	bits_gct = ((scn_desc[4] & 0x70) >> 4) + 1;

	if ( scn_desc[4] & 0x80 )
		/* Global colour table follows screen descriptor */
		{
		if ( _read(fd, gif_priv->pal, 3 << bits_gct) != (3 << bits_gct) )
			return GBM_ERR_READ;
		}
	else
		/* Blank out palette, but make entry 1 white */
		{
		memset(gif_priv->pal, 0, 3 << bits_gct);
		gif_priv->pal[3] =
		gif_priv->pal[4] =
		gif_priv->pal[5] = 0xff;
		}

	/* Expected image descriptors / extension blocks / terminator */

	while ( img < img_want )
		{
		if ( _read(fd, image_desc, 1) != 1 )
			return GBM_ERR_READ;
		switch ( image_desc[0] )
			{
/*...s0x2c \45\ image descriptor:24:*/
case 0x2c:
	if ( _read(fd, image_desc + 1, 9) != 9 )
		return GBM_ERR_READ;
	gbm->w = make_word(image_desc[5], image_desc[6]);
	gbm->h = make_word(image_desc[7], image_desc[8]);
	gif_priv->ilace = ( (image_desc[9] & 0x40) != 0 );

	if ( image_desc[9] & 0x80 )
		/* Local colour table follows */
		{
		gif_priv->bpp = (image_desc[9] & 7) + 1;
		if ( _read(fd, gif_priv->pal, 3 << gif_priv->bpp) != (3 << gif_priv->bpp) )
			return GBM_ERR_READ;
		}

	if ( ++img != img_want )
		/* Skip the image data */
		{
		byte code_size, block_size;

		if ( _read(fd, &code_size, 1) != 1 )
			return GBM_ERR_READ;
		do
			{
			if ( _read(fd, &block_size, 1) != 1 )
				return GBM_ERR_READ;
			_lseek(fd, block_size, SEEK_CUR);
			}
		while ( block_size );
		}

	break;
/*...e*/
/*...s0x21 \45\ extension block:24:*/
/* Ignore all extension blocks */

case 0x21:
	{
/* Graphics and Comment ControlExtensions added by Takhir */
      byte func_code, byte_count, rslt;
	if ( _read(fd, &func_code, 1) != 1 )
		return GBM_ERR_READ;
	_read(fd, &byte_count, 1);
	switch (func_code)
	{
	case 0xF9: // GraphicsControlExtensions
      byte_count++;
		for(;byte_count>0;byte_count--)
		{
         if ( _read(fd, &rslt, 1) != 1 )
				return GBM_ERR_READ;
			if((byte_count==5)&&(rslt&1)) gbm->trc=1;
			if((byte_count==2)&&(gbm->trc==1))
				gbm->trc=rslt;
		}
		break;
	case 0xFE: // CommentControlExtensions
		if(gbm->szUrl==NULL)
			_lseek(fd, byte_count+1, SEEK_CUR);
		else
      {
		   gbm->szUrl[0] = 0;
		   if (_read(fd, gbm->szUrl, byte_count+1) != ((int)byte_count+1) ) 
			   return GBM_ERR_READ;
      }
	break;
	default: // Other Extensions
		do
		{	_lseek(fd, byte_count, SEEK_CUR);
			if ( _read(fd, &byte_count, 1) != 1 )
				return GBM_ERR_READ;
		}
		while ( byte_count );
	}
/* end of changes */
	}
	break;
/*...e*/
/*...s0x3b \45\ terminator:24:*/
/* Oi, we were hoping to get an image descriptor! */

case 0x3b:
	return GBM_ERR_GIF_TERM;
/*...e*/
/*...sdefault:24:*/
default:
	return GBM_ERR_GIF_HEADER;
/*...e*/
			}
		}

	switch ( gif_priv->bpp )
		{
		case 1:		gbm->bpp = 1;		break;
		case 2:
		case 3:
		case 4:		gbm->bpp = 4;		break;
		case 5:
		case 6:
		case 7:
		case 8:		gbm->bpp = 8;		break;
		default:	return GBM_ERR_GIF_BPP;
		}

	return GBM_ERR_OK;
	}

/*...e*/
/*...sgif_rdata:0:*/
/*...sread context:0:*/


int getTransparencyColor(TCHAR* fn_src)
{
	int	fd;
	GBM	gbm;
   GBMRGB *gbmrgb = (GBMRGB*)(gbm.priv + 8);

	if(lstrcmpi(fn_src + lstrlen(fn_src) - 4, _T(".gif")) != 0) return -1;
   gbm.trc = -1;
   gbm.szUrl = NULL;
	if ((fd = _topen(fn_src, O_RDONLY | O_BINARY))==-1) return 0;
	gif_rhdr( fd, &gbm);
	_close(fd);
	if(gbm.trc == -1 || gbm.trc > 255) return -1;
	fd = 0;
	memcpy(&fd, &gbmrgb[gbm.trc], 3);
	/*char b[64];
	wsprintf(b, "Index %d, color=0x%06x", gbm.trc, fd);
	MessageBox(NULL, b, "Index & color", 0);*/
	return fd;
}
