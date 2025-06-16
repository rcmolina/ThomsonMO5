/*
		    MM   MM    OOOOO    5555555
		    M M M M   O     O   5
		    M  M  M   O     O   555555
		    M     M   O     O         5
		    M     M   O     O         5
		    M     M    OOOOO    555555

			     EMULATEUR

			 Par Edouard FORLER
		    (edouard.forler@di.epfl.ch)
			  Par Sylvain HUET
		    (huet@poly.polytechnique.fr)
			       1997

  k52wav.c : outil de conversion de donnees PC -> MO5.

*/

//#define NO_PARSER
#define NO_BLANKS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define sample_freq 44100
#define sample_freq  4800
#define rec_speed   1200

int longper = sample_freq/(rec_speed)*1.1;
int shortper = sample_freq/(rec_speed*2)*1.1;

#define SPL 127
signed int spl = SPL;
signed int spl2 = SPL;
int mspause = 100;			// pause in ms
long pw,pp;
	  
int head[46] = {0x52,0x49,0x46,0x46, // RIFF
                0x00,0x00,0x00,0x00, // chunk size
                0x57,0x41,0x56,0x45, //WAVE
                0x66,0x6d,0x74,0x20, //fmt_
                0x12,0x00,0x00,0x00, // 16 for PCM 
                0x01,0x00,			 // 1 for LINEAR PCM
                0x01,0x00, 			 // NUM CHANNELS
                //0x44,0xac,0x00,0x00, // SAMPLE RATE
                sample_freq-(sample_freq*256),sample_freq/256,0x00,0x00, // SAMPLE RATE				
                //0x44,0xac,0x00,0x00, // BYTE RATE
                sample_freq-(sample_freq*256),sample_freq/256,0x00,0x00, // BYTE RATE	   	   	   
                0x01,0x00,			 //BLOCK ALIGN
                0x08,0x00,			 // BITS PER SAMPLE
                0x00,0x00,			 // EXTRA PARAM SIZE
                0x64,0x61,0x74,0x61, // DATA
                0x00,0x00,0x00,0x00}; // NUMBER OF BYTES IN DATA

void blank(FILE *fw);


int myfgetc(FILE *fp, FILE *fw)
{
  int c=fgetc(fp),size;

  if (c==EOF)
  {
    printf("Fin du fichier .K5\n");
    blank(fw);

    size = ftell(fw)-8;
    fseek(fw,4,0);
    fputc(size,fw);
    fputc(size>>8,fw);
    fputc(size>>16,fw);
    fputc(size>>24,fw);

    size -= 38;
    fseek(fw,42,0);
    fputc(size,fw);
    fputc(size>>8,fw);
    fputc(size>>16,fw);
    fputc(size>>24,fw);

    fclose(fw);
    fclose(fp);
    exit(1);
  }
  return c;
}

void write(int bit, FILE *fw)
{
  int i;

  if (bit==0)
  {
    for (i=0;i<longper;i++) fputc(128+spl,fw);
    spl=-spl;
  }
  else
  {
    for (i=0;i<shortper;i++) fputc(128+spl,fw);
    spl=-spl;
    for (i=0;i<shortper;i++) fputc(128+spl,fw);
    spl=-spl;
  }

}

void writebyte(FILE *fw, int c)
{
  int i;

  for (i=8;i>0;i--)
    if (c&(1<<(i-1))) write(1,fw); else write(0,fw);
}

void blank(FILE *fw)
{
#ifdef NO_BLANKS
#else
  int i;
  for (i=0;i<(int)(0.5 + sample_freq*mspause/1000.0);i++) fputc(128,fw);
  //for (i=0;i<sample_freq/10;i++) fputc(128,fw);
  //spl=64;
  spl = SPL;
#endif
}

int getnumber(char *s)
{
// Returns the INT number contained in string *s
int i;
sscanf(s,"%d",&i); return(i);
}

int main(int argc, char **argv)
{
  int j,l,i,c,b;
  FILE *fp,*fw;
  int k; // argc position for read file
  char *ext;

  //printf("conversion .k5 mo5 -> wav 44,1KHz 8 bits.\n");
  printf("conversion .k5 mo5 -> wav"); printf(" %dHz 8 bits.\n",sample_freq);  
  printf("version 1.0, par Edouard FORLER\n");

  if (argc<2 || argc > 3) {
    printf("usage : k52wav [mspause] fichier.k5\n");
    return 1;
  }
  else if (argc == 2) {
      k = 1;
  }
  else if (argc == 3) {
	  mspause = getnumber(argv[1]);
      k = 2;
  }

  l=strlen(argv[k])-3;
  ext= strlwr(&argv[k][l]);

  //if ( (l<1)||( (strcmp(&argv[k][l],".k5"))&&(strcmp(&argv[k][l],".K5"))&&(strcmp(&argv[k][l],".k7"))&&(strcmp(&argv[k][l],".K7")) ) )
  if ( (l<1)||( (strcmp(ext,".k5"))&&(strcmp(ext,".k7")) ) )
  {
     printf("erreur: mauvais ce n'est k5 fichier.\n");
     return 1;
  }

  printf("ouverture de %s\n",argv[k]);
  fp=fopen(argv[k],"rb");
  if (fp==NULL)
  {
     printf("erreur: fichier %s introuvable.\n",argv[k]);
     return 0;
  }

  fseek(fp,0,SEEK_END);
  //printf("Taille estimee du fichier .wav:  %i octets. Continuer (Y/N)? ",ftell(fp)*8*longper+ftell(fp)/255*sample_freq); //fsize + blanks
  #ifdef NO_BLANKS
     printf("Taille estimee du fichier .wav:  %i octets \n",ftell(fp)*8*longper);   
  #else
     printf("Taille estimee du fichier .wav:  %i octets \n",ftell(fp)*8*longper + (ftell(fp)/255)*sample_freq/10);  
  #endif
  fseek(fp,0,SEEK_SET);
  fflush(stdout);
//  if (((c=getchar())!='y')&&(c!='Y')) exit(1);

  strcpy(&argv[k][l],".wav\0");
  printf("creation de %s\n",argv[k]);
  fw=fopen(argv[k],"wb");
  if (fw==NULL)
  {
     printf("erreur: fichier %s impossible a ouvrir.\n",argv[k]);
     return 1;
  }

  printf("valeur de pause: %d ms\n", mspause);

  /* Ecriture header WAV */
  for(i=0;i<46;i++) fputc(head[i],fw);

#ifdef NO_PARSER
  do
  {
    c=myfgetc(fp,fw);
    writebyte(fw,c);
  } while (c!=EOF);

#else
  blank(fw);
  c=myfgetc(fp,fw);
  do		//do-while
  {
     do
     {
	   writebyte(fw,c);
       c=myfgetc(fp,fw);
     } while (c==1);

     if (c==60)
     {
         writebyte(fw,60);
         c=myfgetc(fp,fw);
         if (c==90)
         {
            int s=0;
            writebyte(fw,90);
            b=myfgetc(fp,fw);
            writebyte(fw,b);
            l=myfgetc(fp,fw);
            writebyte(fw,l);
            if (l==0) l=256;

            for (i=0;i<l-2;i++)
            {
              c=myfgetc(fp,fw);
              writebyte(fw,c);
              s=s+c;
            }
            c=myfgetc(fp,fw);
		    pp=ftell(fp)-1;
            writebyte(fw,c);
            if (((c+s)&255) && (b!=0xff)) printf("@ 0x%08X erreur checksum!? trouvee=0x%02X should(256-sum)=0x%02X avec sum=0x%02X\n", pp, c, 256-(s&255), s&255);

         } 
         else {	 	 	 	 //c <>90
            pp=ftell(fp)-2;				//addr for 0x3C
            printf("@ 0x%08X *erreur en-Tete de Bloc non 0x3C5A trouvee=0x3C%02X\n", pp, c);

      	    do {
                do {
                    writebyte(fw,c);
                    c=myfgetc(fp,fw);
                } while (c!=1);
			
                pw=ftell(fw);
                pp=ftell(fp)-1;
				spl2=spl;
				
                do {
                    writebyte(fw,c);
                    c=myfgetc(fp,fw);
                } while (c==1);
	
            } while (c!=60);

            fseek(fw,pw,SEEK_SET);	   //Rewind at 01's synch for R/W
            fseek(fp,pp,SEEK_SET);
		    spl=spl2;
/*
            do {
              writebyte(fw,c);
              c=myfgetc(fp,fw);
            } while (c!=EOF);
*/	  	  	  	     	   	   	       
         }

     }
     else {	 	 //c<>60

         pp=ftell(fp)-1;
         printf("@ 0x%08X en-Tete Special de Bloc non 0x3C trouvee=0x%02X\n", pp, c);
		 
         do {
		 
	        do {
	           writebyte(fw,c);
	           c=myfgetc(fp,fw);
	        } while (c!=1);
	
	        pw=ftell(fw);
	        pp=ftell(fp)-1;
			spl2=spl;
			

	        do {
	           writebyte(fw,c);
	           c=myfgetc(fp,fw);
	        } while (c==1);

         } while (c!=60);
        
         fseek(fw,pw,SEEK_SET);		//Rewind at 01's synch for R/W
         fseek(fp,pp,SEEK_SET);
		 spl=spl2;
/*
         do {
              writebyte(fw,c);
              c=myfgetc(fp,fw);
         } while (c!=EOF);	  	  	  	     	   	   	       
*/
     }

     blank(fw);
     c=myfgetc(fp,fw);					//read 01 sync and jump to start
     if (c==0) fseek(fp,0,SEEK_END);

  } while (1);		//do-while

#endif
}

