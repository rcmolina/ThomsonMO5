/*--------------------------------------------------------*/
/* k5towav (c) Prehisto feb 2015                          */
/* To convert MO K7 file to WAV file                      */
/*--------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef TRUE
#   define  TRUE 1
#endif
#ifndef FALSE
#   define  FALSE 0
#endif

/* WAV constants */
#define WAVE_HEADER_SIZE  0x2c
#define WAVE_RIFF_CHUNK_SIZE  4
#define WAVE_FORMAT_CHUNK_SIZE  16
#define WAVE_FORMAT_PCM            1
#define WAVE_FORMAT_CHANNELS  1
#define WAVE_FORMAT_SAMPLESPERSEC  22050		// antes 44100
#define WAVE_FORMAT_BLOCKALIGN  1
//#define WAVE_FORMAT_AVGBYTEPERSEC  WAVE_FORMAT_SAMPLESPERSEC*WAVE_FORMAT_BLOCKALIGN
#define WAVE_FORMAT_BITPERSAMPLE  8

/* Sample parameters */
#define CYCLES_PER_FRAME    19968
#define MICROSEC_PER_FRAME  20000
#define MICROSEC_PER_SEC    1000000
//#define CURRENT_VALUE_DEFAULT  0x7fff
#define CURRENT_VALUE_DEFAULT  0xFF

static int flag_modified = FALSE;
//static int current_value = CURRENT_VALUE_DEFAULT;
static unsigned char current_value = CURRENT_VALUE_DEFAULT;
static double remain = 0;
static int cycles = 0;

static int samplespersec = WAVE_FORMAT_SAMPLESPERSEC;


static void write_gap (int length, FILE *file)
{
    int i;

    for (i=0; i<length; i++)
    {
        //fputc (current_value&0xff, file );
        //fputc ((current_value>>8)&0xff, file );
		fputc (current_value, file );
    }
}



static void write_motoroff (FILE *file)
{
    write_gap (samplespersec*2, file); // entre 2
}



static void write_motoron (FILE *file)
{
    write_gap (samplespersec, file);
}



static void write_data_gap (FILE *file)
{
    /* Must have time to do at least :
       4+2   NEXT   LDA    ,X+
       4+2          STA    ,U+
       4+1          LEAY   -1,U
       3            BNE    NEXT
       ... a bit more! */
    write_gap (samplespersec/8, file);
}



static void write_signal (FILE *file)
{
    int i;
    double size = ((((double)cycles*(double)MICROSEC_PER_FRAME)
                                   /(double)CYCLES_PER_FRAME)
                                   *(double)samplespersec)
                                   /(double)MICROSEC_PER_SEC;
    
    size += remain-(double)((int)remain);
    remain = size;

    /* write signal */
    for (i=0; i<(int)size; i++)
    {
        //fputc (current_value&0xff, file );
        //fputc ((current_value>>8)&0xff, file );
		fputc (current_value, file );
    }

    /* invert signal */
    current_value=~current_value;

    cycles = 0;
}



static void write_byte_standard (char byte, FILE *file)
{
    int bit;
    
    /* 4     LF1AF  STA    <$2045
       2            LDB    #$08  */
    cycles += 4+2;
    for (bit=0x80; bit>0; bit>>=1)
    {
        /* 7     LF1B3  BSR    LF1CB
           2     LF1CB  LDA    #$40
           4+0          EORA    ,U
           4+0          STA     ,U  */
        cycles += 7+2+4+4;
        write_signal (file);
        /* 5            RTS
           3            LDX    #$002D
           7            BSR    LF1A2
           4+1   LF1A2  LEAX   -$01,X
           3            BNE    LF1A2
           2            CLRA
           5            RTS
           3            LDX    #$0032
           6            ASL    <$2045
           3            BCC    LF1C5  */
        cycles += 5+3+7+(0x2d*(4+1+3))+2+5+3+6+3;

        if (((int)byte & bit) == 0)
        {
            /* 7     LF1C5  BSR    LF1A2
               4+1   LF1A2  LEAX   -$01,X
               3            BNE    LF1A2
               2            CLRA
               5            RTS       */
            cycles += 7+(0x32*(4+1+3))+2+5;
        }
        else
        {
            /* 7            BSR    LF1CB
               2     LF1CB  LDA    #$40
               4+0          EORA   ,U
               4+0          STA    ,U   */
            cycles += 7+2+4+4;
            write_signal (file);
            /* 5            RTS
               4+1          LEAX   -$03,X
               7     LF1C5  BSR    LF1A2
               4+1   LF1A2  LEAX   -$01,X
               3            BNE    LF1A2
               2            CLRA
               5            RTS    */
            cycles += 5+4+1+7+((0x32-3)*(4+1+3))+2+5;
        }
        /* 2            DECB
           3            BNE    LF1B3  */
        cycles += 5;
    }
    /* 5            RTS   */
    cycles += 5;
}



static void write_byte_modified (char byte, FILE *file)
{
    int bit;
    
    /* 4     LF1AF  STA    <$2045
       2            LDB    #$08  */
    cycles += 4+2;
    for (bit=0x80; bit>0; bit>>=1)
    {
        /* 7     LF1B3  BSR    LF1CB
           2     LF1CB  LDA    #$40
           4+0          EORA    ,U
           4+0          STA     ,U  */
        cycles += 7+2+4+4;
        write_signal (file);
        cycles += 269;

        if (((int)byte & bit) == 1)
        {
            write_signal (file);
        }
        cycles += 564;

        /* 2            DECB
           3            BNE    LF1B3  */
        cycles += 5;
    }
    /* 5            RTS   */
    cycles += 5;
}



static void write_byte (char byte, FILE *file)
{
    if (flag_modified == FALSE)
        write_byte_standard (byte, file);
    else
        write_byte_modified (byte, file);
}



#define STOCK_LENGTH  40
#define MOTOR_STOPS_AFTER_THIS_BLOCK 0x01
#define MOTOR_STOPS_AFTER_DATA_BLOCK 0x02

static void convert_file (FILE *bin_file, FILE *wav_file)
{
    static char stock[STOCK_LENGTH];
    static size_t length = 0;
    static size_t size = 0;
    static int motor_status = 0;
    char dcmoto_mark1[] = "DCMOTO\1\1\1\1\1\1\1\1\1\1\x3c\x5a";
    char dcmoto_mark2[] = "\xdc\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\x3c\x5a";
    char dcmoto_mark3[] = "DCMO6\1\1\1\1\1\1\1\1\1\1\1\x3c\x5a";		// nuevo
    char basic_mark[]   = "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\x3c\x5a";

    do
    {
        if ((length < STOCK_LENGTH) && (feof (bin_file) == 0))
        {
            size = fread(stock+length, 1, STOCK_LENGTH-length, bin_file);
            length += size;
        }

        if (length > 19)
        {
            if ((memcmp (dcmoto_mark1, stock, 18) == 0)
             || (memcmp (dcmoto_mark2, stock, 18) == 0)
             || (memcmp (dcmoto_mark3, stock, 18) == 0) //nuevo
             || (memcmp (basic_mark, stock, 18) == 0))
            {
                if (motor_status != 0)
                {
                    write_motoroff (wav_file);
                    motor_status &= ~MOTOR_STOPS_AFTER_THIS_BLOCK;
                }
                memset (stock, 0x01, 6);

                cycles = 0;
                remain = 0;

                if (stock[18] == '\0')
                {
                    write_motoron (wav_file);
                    motor_status &= ~MOTOR_STOPS_AFTER_DATA_BLOCK;
                    if ((length > (18+2+0x0d)) && (stock[18+2+0x0d] != '\0'))
                    {
                        motor_status |= MOTOR_STOPS_AFTER_DATA_BLOCK;
                    }
                    motor_status |= MOTOR_STOPS_AFTER_THIS_BLOCK;
                }
                else
                {
                    if ((motor_status & MOTOR_STOPS_AFTER_DATA_BLOCK) != 0)
                    {
                        write_motoron (wav_file);
                    }
                    else
                    {
                        write_data_gap (wav_file);
                    }

                    if (stock[18] == '\xff')
                    {
                        motor_status &= ~MOTOR_STOPS_AFTER_DATA_BLOCK;
                        motor_status |= MOTOR_STOPS_AFTER_THIS_BLOCK;
                    }
                    else
                    {
                        motor_status &= ~MOTOR_STOPS_AFTER_THIS_BLOCK;
                    }
                }
            }
        }
        cycles += 4+2+7;  /* For preparing data to be written */
        write_byte (*stock, wav_file);
        cycles += 6+3;    /* For the loop */

        memmove (stock, stock+1, STOCK_LENGTH-1);
        length--;
    } while (length > 0);
}



static void write_le4 (int value, FILE *file)
{
    fputc( value&0xff, file );
    fputc( (value>>8)&0xff, file );
    fputc( (value>>16)&0xff, file );
    fputc( (value>>24)&0xff, file );
}



static void write_le2 (int value, FILE *file)
{
    fputc( value&0xff, file );
    fputc( (value>>8)&0xff, file );
}



static void write_id (char *id, FILE *file)
{
    int size = fwrite (id, 1, 4, file);
    (void)size;
}



static void update_wav_header (char *filename)
{
    FILE *file;
    struct stat st;

    stat( filename, &st );

    file = fopen (filename, "rb+");
    if (file != NULL)
    {
        write_id  ("RIFF", file);
        write_le4 ((int)st.st_size-WAVE_RIFF_CHUNK_SIZE, file);
        write_id  ("WAVE", file);
        write_id  ("fmt ", file);
        write_le4 (WAVE_FORMAT_CHUNK_SIZE, file);
        write_le2 (WAVE_FORMAT_PCM, file);
        write_le2 (WAVE_FORMAT_CHANNELS, file);
        write_le4 (samplespersec, file); //multiplica *2
        write_le4 (samplespersec*WAVE_FORMAT_BLOCKALIGN, file); //avgbytepersec
        write_le2 (WAVE_FORMAT_BLOCKALIGN, file);
        write_le2 (WAVE_FORMAT_BITPERSAMPLE, file);
        write_id  ("data", file);
        write_le4 ((int)st.st_size-WAVE_HEADER_SIZE, file);
        fclose (file);
    }
}



static void write_wav (char *bin_name, char *wav_name)
{
    int i;
    FILE *bin_file;
    FILE *wav_file;

    bin_file = fopen (bin_name, "rb");
    if (bin_file != NULL)
    {
        wav_file = fopen (wav_name, "wb");
        if (wav_file != NULL)
        {
            /* Create WAV header gap */
            for (i=0; i<WAVE_HEADER_SIZE; i++)
            {
                fputc (0x00, wav_file);
            }

            /* Write WAV data */
            write_motoron (wav_file);
            convert_file (bin_file, wav_file);
            write_motoroff (wav_file);
            fclose (wav_file);
            update_wav_header (wav_name);
        }
        else
        {
            printf ("*** Can not write file %s\n", wav_name); 
        }
        fclose (bin_file);
    }
    else
    {
        printf ("*** Can not read file %s\n", bin_name); 
    }
}

int getnumber(char *s) {
// Returns the INT number contained in string *s
int i;

sscanf(s,"%d",&i); return(i);
}

static int info (void)
{
    printf ("k5towav (c) Prehisto feb 2015\n");
    printf ("     Usage : k5towav [freq] [--modified] <filename>\n");
    return EXIT_FAILURE;
}

/* Changes the File Extension of String *str to *ext */
void ChangeFileExtension (char *str, char *ext)
{
  int n;
  
  n = strlen(str); 

  while (str[n] != '.') 
    n--;

  n++; 
  str[n] = 0;
  strcat (str, ext);
}

int main (int argc, char *argv[])
{

    int i;

    char bin_name[256] = "";
    char wav_name[256] = "";
/*
	int n;
	char *str;

    for (i=1; i<argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp (argv[i], "--modified") == 0)
            {
                flag_modified = TRUE;
            }
            else
            {
                printf ("*** option '%s' unknown\n", argv[i]);
                return info ();
            }
        }   
        else
        {
            if (*bin_name == '\0')
            {
                strcpy(bin_name, argv[i]);
            }
            else
            {
                return info ();
            }
        }
    }

	  str = strdup(bin_name);
	  n = strlen(str); 
	
	  while (str[n] != '.') 
	    n--;
	
	  n++; 
	  str[n] = 0;
    sprintf (wav_name, "%s.wav", str);
*/

int k; // argc position for read file

  if (argc < 2 || argc > 4) {
    return info();
  }
  else if (argc == 2) {
  	k = 1; 
	strcpy(bin_name, argv[k]);
	strcpy(wav_name, argv[k]);
    ChangeFileExtension (wav_name, "wav");
  }
  else if (argc == 3) {

    if ((argv[1][0] >= 48) && (argv[1][0] <= 57)) {
        samplespersec=getnumber(argv[1]);
    }
    else if (strcmp (argv[1], "--modified") == 0) {
        flag_modified = TRUE;
    }
    else {
        printf ("*** option '%s' unknown\n", argv[1]);
        return info ();
    }

	k = 2;
    strcpy (bin_name, argv[k]); 
	strcpy(wav_name, argv[k]);
    ChangeFileExtension (wav_name, "wav");
  }
  else if (argc == 4) {
  
    if ((argv[1][0] >= 48) && (argv[1][0] <= 57)) {
        samplespersec=getnumber(argv[1]);	 
    } else {
        printf ("*** option '%s' unknown\n", argv[1]);
        return info ();
    }

    if (strcmp (argv[2], "--modified") == 0) {
        flag_modified = TRUE;
    } else {
        printf ("*** option '%s' unknown\n", argv[1]);
        return info ();
    }	     
	  	  	  
	k = 3;
    strcpy (bin_name, argv[k]); 
	strcpy(wav_name, argv[k]);
    ChangeFileExtension (wav_name, "wav");	  	   
  }

    write_wav (bin_name, wav_name);

    return EXIT_SUCCESS;
}

